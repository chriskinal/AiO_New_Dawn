#include "ADProcessor.h"
#include "EventLogger.h"
#include "HardwareManager.h"
#include "ConfigManager.h"

// Static instance
ADProcessor* ADProcessor::instance = nullptr;

ADProcessor::ADProcessor() : 
    wasRaw(0),
    wasOffset(0),
    wasCountsPerDegree(1.0f),
    kickoutAnalogRaw(0),
    pressureReading(0.0f),
    motorCurrentRaw(0),
    currentReading(0.0f),
    analogWorkSwitchEnabled(false),
    workSwitchAnalogRaw(0),
    workSwitchSetpoint(50.0f),      // 50% default
    workSwitchHysteresis(20.0f),    // 20% default
    invertWorkSwitch(false),
    debounceDelay(50),  // 50ms default debounce
    lastProcessTime(0),
    currentBufferIndex(0),
    teensyADC(nullptr)
{
    // Initialize switch states
    workSwitch = {false, false, 0, false};
    steerSwitch = {false, false, 0, false};
    
    // Initialize current buffer
    for (int i = 0; i < CURRENT_BUFFER_SIZE; i++) {
        currentBuffer[i] = 0.0f;
    }
    currentRunningSum = 0.0f;
    
    // Initialize JD PWM data
    jdPWMMode = false;
    jdPWMDutyTime = 0;
    jdPWMDutyTimePrev = 0;
    jdPWMRiseTime = 0;
    jdPWMPrevRiseTime = 0;
    jdPWMPeriod = 0;
    jdPWMRollingAverage = 0;
    jdPWMDelta = 0;
    jdPWMDutyPercent = 0.0f;
    jdPWMDutyPercentPrev = 0.0f;
    
    instance = this;
}

ADProcessor* ADProcessor::getInstance()
{
    if (instance == nullptr) {
        instance = new ADProcessor();
    }
    return instance;
}

bool ADProcessor::init()
{
    LOG_INFO(EventSource::AUTOSTEER, "=== A/D Processor Initialization ===");
    
    // Load analog work switch settings from ConfigManager
    extern ConfigManager configManager;
    analogWorkSwitchEnabled = configManager.getAnalogWorkSwitchEnabled();
    workSwitchSetpoint = configManager.getWorkSwitchSetpoint();
    workSwitchHysteresis = configManager.getWorkSwitchHysteresis();
    invertWorkSwitch = configManager.getInvertWorkSwitch();
    
    LOG_INFO(EventSource::AUTOSTEER, "Analog work switch config: Enabled=%d, SP=%d%%, H=%d%%, Inv=%d",
             analogWorkSwitchEnabled, workSwitchSetpoint, workSwitchHysteresis, invertWorkSwitch);
    
    // Check for JD PWM mode
    jdPWMMode = configManager.getJDPWMEnabled();
    if (jdPWMMode) {
        LOG_INFO(EventSource::AUTOSTEER, "JD PWM encoder mode enabled (uses AOG pressure threshold)");
        LOG_DEBUG(EventSource::AUTOSTEER, "JD_PWM_INIT: Mode ENABLED (uses AOG pressure threshold)");
    } else {
        LOG_DEBUG(EventSource::AUTOSTEER, "JD_PWM_INIT: Mode DISABLED (using analog pressure mode)");
    }
    
    // Configure pins with ownership tracking
    pinMode(AD_STEER_PIN, INPUT_PULLUP);      // Steer switch with internal pullup
    
    // Configure work pin based on mode
    configureWorkPin();
    
    pinMode(AD_WAS_PIN, INPUT_DISABLE);       // WAS analog input (no pullup)
    
    // Request ownership of appropriate kickout pin based on mode
    HardwareManager* hwMgr = HardwareManager::getInstance();
    if (jdPWMMode) {
        // JD PWM mode uses digital pin
        if (hwMgr->requestPinOwnership(AD_KICKOUT_D_PIN, HardwareManager::OWNER_ADPROCESSOR, "ADProcessor-JDPWM")) {
            pinMode(AD_KICKOUT_D_PIN, INPUT_PULLUP);
            hwMgr->updatePinMode(AD_KICKOUT_D_PIN, INPUT_PULLUP);
            
            attachInterrupt(digitalPinToInterrupt(AD_KICKOUT_D_PIN), jdPWMRisingISR, RISING);
            LOG_INFO(EventSource::AUTOSTEER, "JD_ENC: Mode enabled on pin %d", AD_KICKOUT_D_PIN);
        } else {
            LOG_WARNING(EventSource::AUTOSTEER, "JD_ENC: Failed to get ownership of KICKOUT_D pin %d - may be in use by encoder", AD_KICKOUT_D_PIN);
        }
    } else {
        // Analog pressure sensor mode uses analog pin
        if (hwMgr->requestPinOwnership(AD_KICKOUT_A_PIN, HardwareManager::OWNER_ADPROCESSOR, "ADProcessor")) {
            pinMode(AD_KICKOUT_A_PIN, INPUT_DISABLE);
            hwMgr->updatePinMode(AD_KICKOUT_A_PIN, INPUT_DISABLE);
            LOG_INFO(EventSource::AUTOSTEER, "KICKOUT_A pin configured for analog pressure sensor");
        } else {
            LOG_WARNING(EventSource::AUTOSTEER, "Failed to get ownership of KICKOUT_A pin");
        }
    }
    
    pinMode(AD_CURRENT_PIN, INPUT_DISABLE);   // Current sensor analog input
    
    // Test immediately after setting
    LOG_DEBUG(EventSource::AUTOSTEER, "After pinMode: Pin %d digital=%d", AD_STEER_PIN, digitalRead(AD_STEER_PIN));
    
    // Initialize Teensy ADC library
    teensyADC = new ADC();
    
    // Register ADC configuration with HardwareManager
    
    // Register ADC0 config (WAS reading)
    if (!hwMgr->requestADCConfig(HardwareManager::ADC_MODULE_0, 12, 4, "ADProcessor")) {
        LOG_WARNING(EventSource::AUTOSTEER, "Failed to register ADC0 configuration");
    }
    
    // Register ADC1 config (other sensors)
    if (!hwMgr->requestADCConfig(HardwareManager::ADC_MODULE_1, 12, 1, "ADProcessor")) {
        LOG_WARNING(EventSource::AUTOSTEER, "Failed to register ADC1 configuration");
    }
    
    // Configure ADC0 for WAS reading - optimized settings
    teensyADC->adc0->setAveraging(4);                                      // Reduced from 16 for faster reads
    teensyADC->adc0->setResolution(12);                                    // 12-bit resolution
    teensyADC->adc0->setConversionSpeed(ADC_CONVERSION_SPEED::HIGH_SPEED); // Faster than MED_SPEED
    teensyADC->adc0->setSamplingSpeed(ADC_SAMPLING_SPEED::HIGH_SPEED);     // Faster than MED_SPEED
    
    // Configure ADC1 for other sensors if needed
    teensyADC->adc1->setAveraging(1);  // No averaging
    teensyADC->adc1->setResolution(12);
    teensyADC->adc1->setConversionSpeed(ADC_CONVERSION_SPEED::MED_SPEED);
    teensyADC->adc1->setSamplingSpeed(ADC_SAMPLING_SPEED::MED_SPEED);
    
    // Take initial readings
    updateWAS();
    updateSwitches();
    
    // Clear any initial change flags
    workSwitch.hasChanged = false;
    steerSwitch.hasChanged = false;
    
    LOG_DEBUG(EventSource::AUTOSTEER, "Pin configuration complete");
    LOG_DEBUG(EventSource::AUTOSTEER, "Initial WAS reading: %d (%.2fV)", wasRaw, getWASVoltage());
    LOG_DEBUG(EventSource::AUTOSTEER, "Work switch: %s (pin A17)", workSwitch.debouncedState ? "ON" : "OFF");
    LOG_DEBUG(EventSource::AUTOSTEER, "Steer switch: %s (pin %d)", steerSwitch.debouncedState ? "ON" : "OFF", AD_STEER_PIN);
    
    LOG_INFO(EventSource::AUTOSTEER, "A/D Processor initialization SUCCESS");
    
    return true;
}

void ADProcessor::process()
{
    uint32_t now = millis();
    
    // Update WAS at 200Hz (every 5ms) - more than enough for 100Hz autosteer
    static uint32_t lastWASUpdate = 0;
    if (now - lastWASUpdate >= 5) {
        lastWASUpdate = now;
        updateWAS();
    }
    
    // Fast current sensor sampling (every 1ms like test sketch)
    static uint32_t lastCurrentSample = 0;
    
    if (now - lastCurrentSample >= 1) {
        lastCurrentSample = now;
        
        // Read current sensor and store in buffer
        uint16_t reading = teensyADC->adc1->analogRead(AD_CURRENT_PIN);
        
        // Simple approach from test sketch - subtract baseline offset
        float adjusted = (float)(reading - 77);  // 77 is our baseline
        float newValue = (adjusted > 0) ? adjusted : 0.0f;
        
        // Update running sum by removing old value and adding new value
        currentRunningSum -= currentBuffer[currentBufferIndex];
        currentRunningSum += newValue;
        currentBuffer[currentBufferIndex] = newValue;
        currentBufferIndex = (currentBufferIndex + 1) % CURRENT_BUFFER_SIZE;
        
        // Calculate average from running sum (much faster!)
        currentReading = currentRunningSum / CURRENT_BUFFER_SIZE;
    }
    
    // Read other sensors at reduced rate (every 10ms = 100Hz)
    static uint32_t lastSlowRead = 0;
    
    if (now - lastSlowRead >= 10) {
        lastSlowRead = now;
        
        // Update switches
        updateSwitches();
        
        if (jdPWMMode) {
            // In JD PWM mode, calculate motion value from duty cycle
            uint32_t now = millis();
            
            
            // Log JD PWM status periodically
            static uint32_t lastStatusLog = 0;
            static uint32_t lastMotionLog = 0;
            static bool wasMoving = false;
            
            // Log basic status every 5 seconds if signal present
            if (now - lastStatusLog > 5000 && jdPWMPeriod > 0) {
                LOG_INFO(EventSource::AUTOSTEER, "JD_ENC Status: duty=%dus, avg=%.0fus, delta=%.0fus, pressure=%.0f", 
                         jdPWMDutyTime, jdPWMRollingAverage, abs(jdPWMDelta), pressureReading);
                lastStatusLog = now;
            }
            
            // Log motion events immediately
            bool isMoving = (pressureReading > 25.0f);  // ~10% of 255 threshold
            if (isMoving != wasMoving) {
                if (isMoving) {
                    LOG_INFO(EventSource::AUTOSTEER, "JD_ENC Motion START: duty=%dus, delta=%.0fus, pressure=%.0f", 
                             jdPWMDutyTime, abs(jdPWMDelta), pressureReading);
                } else {
                    LOG_INFO(EventSource::AUTOSTEER, "JD_ENC Motion STOP: duty=%dus", jdPWMDutyTime);
                }
                wasMoving = isMoving;
            }
            
            // Log high motion values
            if (isMoving && now - lastMotionLog > 1000) {
                LOG_DEBUG(EventSource::AUTOSTEER, "JD_ENC Moving: duty=%dus, avg=%.0fus, delta=%.0fus, pressure=%.0f", 
                          jdPWMDutyTime, jdPWMRollingAverage, abs(jdPWMDelta), pressureReading);
                lastMotionLog = now;
            }
            
            // Check if we have valid duty cycle data
            // Full encoder range: 4% to 94% (90% total range)
            if (jdPWMDutyPercent >= 2.0f && jdPWMDutyPercent <= 96.0f && jdPWMPeriod > 0) {
                // Store previous value before updating
                if (jdPWMDutyPercentPrev == 0.0f) {
                    jdPWMDutyPercentPrev = jdPWMDutyPercent;
                }
                
                // Update rolling average (80% old + 20% new)
                // This smooths out noise and provides a stable baseline
                if (jdPWMRollingAverage == 0) {
                    // Initialize on first reading
                    jdPWMRollingAverage = jdPWMDutyTime;
                } else {
                    jdPWMRollingAverage = jdPWMRollingAverage * 0.8f + jdPWMDutyTime * 0.2f;
                }
                
                // Calculate delta from rolling average
                jdPWMDelta = jdPWMDutyTime - jdPWMRollingAverage;
                
                // Motion is the absolute delta from average in microseconds
                float motionMicros = abs(jdPWMDelta);
                
                // Simple scaling: multiply delta by 5
                float sensorReading = motionMicros * 5.0f;
                sensorReading = min(sensorReading, 255.0f);
                
                
                // Debug logging
                static uint32_t lastDebugTime = 0;
                if (millis() - lastDebugTime > 500) {
                    LOG_DEBUG(EventSource::AUTOSTEER, "JD_PWM: duty=%dus, avg=%.0fus, delta=%.0fus (x5=%.0f)", 
                              jdPWMDutyTime, jdPWMRollingAverage, jdPWMDelta, sensorReading);
                    lastDebugTime = millis();
                }
                
                // Update pressure reading directly (0-255 range for PGN)
                pressureReading = sensorReading;
            } else {
                // Invalid duty cycle
                if (jdPWMDutyPercent > 0 && (jdPWMDutyPercent < 2.0f || jdPWMDutyPercent > 96.0f)) {
                    static uint32_t lastInvalidLog = 0;
                    if (now - lastInvalidLog > 2000) {
                        LOG_WARNING(EventSource::AUTOSTEER, "JD_ENC Invalid duty: %.1f%% (valid: 2-96%%)", 
                                    jdPWMDutyPercent);
                        lastInvalidLog = now;
                    }
                }
                pressureReading = 0;
            }
        } else {
            // Normal analog pressure sensor mode
            kickoutAnalogRaw = analogRead(AD_KICKOUT_A_PIN);
            
            // Debug current sensor reading
            static uint32_t lastCurrentDebug = 0;
            
            if (millis() - lastCurrentDebug > 2000) {  // Every 2 seconds
                lastCurrentDebug = millis();
                LOG_DEBUG(EventSource::AUTOSTEER, "Current sensor: Averaged reading=%.1f (from %d samples)", 
                          currentReading, CURRENT_BUFFER_SIZE);
            }
            
            // Update pressure sensor reading with filtering
            // Scale 12-bit ADC (0-4095) to match NG-V6 behavior
            float sensorSample = (float)kickoutAnalogRaw;
            sensorSample *= 0.15f;  // Scale down to try matching old AIO
            sensorSample = min(sensorSample, 255.0f);  // Limit to 1 byte (0-255)
            pressureReading = pressureReading * 0.8f + sensorSample * 0.2f;  // 80/20 filter
        }
    }
    
    lastProcessTime = millis();
}

void ADProcessor::updateWAS()
{
    // Read WAS using Teensy ADC library (4 samples averaging)
    // Use ADC1 like the old firmware
    wasRaw = teensyADC->adc1->analogRead(AD_WAS_PIN);
    
    // Note: The old firmware applies 3.23x scaling, but in our architecture
    // the calibration (wasOffset and wasCountsPerDegree) handles the scaling
}

void ADProcessor::updateSwitches()
{
    // Simple digital read - just like old firmware
    int steerPinRaw = digitalRead(AD_STEER_PIN);
    
    bool workRaw;
    if (analogWorkSwitchEnabled) {
        // Read analog value
        workSwitchAnalogRaw = teensyADC->adc1->analogRead(AD_WORK_PIN);
        
        // Convert to percentage (0-100%)
        float currentPercent = getWorkSwitchAnalogPercent();
        
        // Apply hysteresis logic
        float lowerThreshold = workSwitchSetpoint - (workSwitchHysteresis * 0.5f);
        float upperThreshold = workSwitchSetpoint + (workSwitchHysteresis * 0.5f);
        
        // Determine state based on thresholds
        if (currentPercent < lowerThreshold) {
            workRaw = !invertWorkSwitch;  // Below lower threshold
        } else if (currentPercent > upperThreshold) {
            workRaw = invertWorkSwitch;    // Above upper threshold
        } else {
            // In hysteresis zone - maintain current state
            workRaw = workSwitch.debouncedState;
        }
        
        // Debug logging
        static uint32_t lastAnalogDebug = 0;
        if (millis() - lastAnalogDebug > 1000) {
            lastAnalogDebug = millis();
            LOG_DEBUG(EventSource::AUTOSTEER, "Analog work switch: raw=%d, %.1f%%, SP=%.1f%%, H=%.1f%%, state=%s",
                      workSwitchAnalogRaw, currentPercent, workSwitchSetpoint, workSwitchHysteresis,
                      workRaw ? "ON" : "OFF");
        }
    } else {
        // Digital mode
        int workPinRaw = digitalRead(AD_WORK_PIN);
        workRaw = !workPinRaw;     // Work is active LOW (pressed = 0)
    }
    
    // Convert to active states
    bool steerRaw = !steerPinRaw;   // Steer is active LOW (pressed pulls down)
    
    // Debug raw pin state changes
    static int lastSteerPinRaw = -1;
    if (steerPinRaw != lastSteerPinRaw) {
        LOG_DEBUG(EventSource::AUTOSTEER, "Steer pin %d: digital=%d, active=%d", 
                      AD_STEER_PIN, steerPinRaw, steerRaw);
        lastSteerPinRaw = steerPinRaw;
    }
    
    // Apply debouncing
    if (debounceSwitch(workSwitch, workRaw)) {
        workSwitch.hasChanged = true;
    }
    
    if (debounceSwitch(steerSwitch, steerRaw)) {
        steerSwitch.hasChanged = true;
        LOG_INFO(EventSource::AUTOSTEER, "Steer switch debounced: %s", 
                      steerSwitch.debouncedState ? "ON" : "OFF");
    }
}

bool ADProcessor::debounceSwitch(SwitchState& sw, bool rawState)
{
    bool stateChanged = false;
    
    if (rawState != sw.currentState) {
        // Raw state changed, update tracking
        sw.currentState = rawState;
        sw.lastChangeTime = millis();
    }
    else if (sw.currentState != sw.debouncedState) {
        // Check if state has been stable long enough
        if ((millis() - sw.lastChangeTime) >= debounceDelay) {
            sw.debouncedState = sw.currentState;
            stateChanged = true;
        }
    }
    
    return stateChanged;
}

float ADProcessor::getWASAngle() const
{
    // Calculate angle from raw reading
    // The WAS is expected to be centered at ~2048 (half of 12-bit range)
    // But AgOpenGPS expects values scaled by 3.23x, so center is ~6805
    
    // Use raw ADC value directly (no 3.23x scaling here)
    // The counts per degree from AgOpenGPS already accounts for the scaling
    float centeredWAS = wasRaw - 2048.0f - wasOffset;
    
    // Calculate angle
    if (wasCountsPerDegree != 0) {
        float angle = centeredWAS / wasCountsPerDegree;
        
        // Apply inversion from ConfigManager (runtime changeable)
        extern ConfigManager configManager;
        if (configManager.getInvertWAS()) {
            angle = -angle;
        }
        
        // Debug logging
        static uint32_t lastWASDebug = 0;
        if (millis() - lastWASDebug > 2000) {
            lastWASDebug = millis();
            LOG_DEBUG(EventSource::AUTOSTEER, "WAS: raw=%d, centered=%.0f, angle=%.2f°, offset=%d, CPD=%.1f, inverted=%d", 
                      wasRaw, centeredWAS, angle, wasOffset, wasCountsPerDegree, configManager.getInvertWAS());
        }
        
        return angle;
    }
    return 0.0f;
}

float ADProcessor::getWASVoltage() const
{
    // Convert 12-bit ADC reading to actual sensor voltage
    // The PCB has a 10k/10k voltage divider (R46/R48)
    // This divides by 2: 0-5V sensor -> 0-2.5V ADC
    // ADC voltage = (wasRaw * 3.3V) / 4095
    // Sensor voltage = ADC voltage * 2
    float adcVoltage = (wasRaw * 3.3f) / 4095.0f;
    return adcVoltage * 2.0f;  // Account for voltage divider
}

float ADProcessor::getJDPWMPosition() const
{
    if (!jdPWMMode || jdPWMDutyPercent <= 0) {
        return 50.0f; // Center position if no signal
    }
    
    // Scale 4-94% duty cycle to 0-99% position
    const float minDuty = 4.0f;
    const float maxDuty = 94.0f;
    
    // Clamp to valid range
    float duty = constrain(jdPWMDutyPercent, minDuty, maxDuty);
    
    // Scale to 0-99%
    float position = ((duty - minDuty) / (maxDuty - minDuty)) * 99.0f;
    
    return position;
}

void ADProcessor::printStatus() const
{
    LOG_INFO(EventSource::AUTOSTEER, "=== A/D Processor Status ===");
    
    // WAS information
    LOG_INFO(EventSource::AUTOSTEER, "WAS (Wheel Angle Sensor):");
    LOG_INFO(EventSource::AUTOSTEER, "  Raw ADC: %d", wasRaw);
    LOG_INFO(EventSource::AUTOSTEER, "  Voltage: %.3fV", getWASVoltage());
    LOG_INFO(EventSource::AUTOSTEER, "  Angle: %.2f°", getWASAngle());
    LOG_INFO(EventSource::AUTOSTEER, "  Offset: %d", wasOffset);
    LOG_INFO(EventSource::AUTOSTEER, "  Counts/Degree: %.2f", wasCountsPerDegree);
    
    // Switch states
    LOG_INFO(EventSource::AUTOSTEER, "Switches:");
    if (analogWorkSwitchEnabled) {
        LOG_INFO(EventSource::AUTOSTEER, "  Work (ANALOG): %s%s - Raw: %d (%.1f%%), SP: %.1f%%, H: %.1f%%", 
                      workSwitch.debouncedState ? "ON" : "OFF",
                      workSwitch.hasChanged ? " (changed)" : "",
                      workSwitchAnalogRaw, getWorkSwitchAnalogPercent(),
                      workSwitchSetpoint, workSwitchHysteresis);
    } else {
        LOG_INFO(EventSource::AUTOSTEER, "  Work (DIGITAL): %s%s", 
                      workSwitch.debouncedState ? "ON" : "OFF",
                      workSwitch.hasChanged ? " (changed)" : "");
    }
    LOG_INFO(EventSource::AUTOSTEER, "  Steer: %s%s", 
                  steerSwitch.debouncedState ? "ON" : "OFF",
                  steerSwitch.hasChanged ? " (changed)" : "");
    
    // Configuration
    LOG_INFO(EventSource::AUTOSTEER, "Configuration:");
    LOG_INFO(EventSource::AUTOSTEER, "  Debounce delay: %dms", debounceDelay);
    LOG_INFO(EventSource::AUTOSTEER, "  ADC resolution: 12-bit");
    LOG_INFO(EventSource::AUTOSTEER, "  ADC averaging: 16 samples");
    
    LOG_INFO(EventSource::AUTOSTEER, "=============================");
}

void ADProcessor::configureWorkPin()
{
    // Configure work pin based on mode
    if (analogWorkSwitchEnabled) {
        pinMode(AD_WORK_PIN, INPUT_DISABLE);   // Analog input (no pullup)
        LOG_INFO(EventSource::AUTOSTEER, "Work switch configured for ANALOG input");
    } else {
        pinMode(AD_WORK_PIN, INPUT_PULLUP);    // Digital with pullup
        LOG_INFO(EventSource::AUTOSTEER, "Work switch configured for DIGITAL input");
    }
}

void ADProcessor::setAnalogWorkSwitchEnabled(bool enabled)
{
    analogWorkSwitchEnabled = enabled;
    extern ConfigManager configManager;
    configManager.setAnalogWorkSwitchEnabled(enabled);
    configManager.saveAnalogWorkSwitchConfig();
    LOG_INFO(EventSource::AUTOSTEER, "Analog work switch mode saved to EEPROM: %s", 
             enabled ? "ENABLED" : "DISABLED");
}

void ADProcessor::setWorkSwitchSetpoint(float sp)
{
    workSwitchSetpoint = constrain(sp, 0.0f, 100.0f);
    extern ConfigManager configManager;
    configManager.setWorkSwitchSetpoint((uint8_t)workSwitchSetpoint);
    configManager.saveAnalogWorkSwitchConfig();
}

void ADProcessor::setWorkSwitchHysteresis(float h)
{
    workSwitchHysteresis = constrain(h, 1.0f, 25.0f);
    extern ConfigManager configManager;
    configManager.setWorkSwitchHysteresis((uint8_t)workSwitchHysteresis);
    configManager.saveAnalogWorkSwitchConfig();
    LOG_INFO(EventSource::AUTOSTEER, "Work switch hysteresis set to %.0f%%", workSwitchHysteresis);
}

void ADProcessor::setInvertWorkSwitch(bool inv)
{
    invertWorkSwitch = inv;
    extern ConfigManager configManager;
    configManager.setInvertWorkSwitch(inv);
    configManager.saveAnalogWorkSwitchConfig();
}

// JD PWM mode implementation
void ADProcessor::setJDPWMMode(bool enabled)
{
    if (jdPWMMode == enabled) return; // No change
    
    jdPWMMode = enabled;
    extern ConfigManager configManager;
    configManager.setJDPWMEnabled(enabled);
    configManager.saveTurnSensorConfig();
    
    HardwareManager* hwMgr = HardwareManager::getInstance();
    
    if (enabled) {
        // Release analog pin and acquire digital pin
        hwMgr->releasePinOwnership(AD_KICKOUT_A_PIN, HardwareManager::OWNER_ADPROCESSOR);
        
        if (hwMgr->requestPinOwnership(AD_KICKOUT_D_PIN, HardwareManager::OWNER_ADPROCESSOR, "ADProcessor-JDPWM")) {
            pinMode(AD_KICKOUT_D_PIN, INPUT_PULLUP);
            hwMgr->updatePinMode(AD_KICKOUT_D_PIN, INPUT_PULLUP);
            attachInterrupt(digitalPinToInterrupt(AD_KICKOUT_D_PIN), jdPWMRisingISR, RISING);
            LOG_INFO(EventSource::AUTOSTEER, "JD_ENC: Mode ENABLED on pin %d", AD_KICKOUT_D_PIN);
        }
    } else {
        // Release digital pin and acquire analog pin
        detachInterrupt(digitalPinToInterrupt(AD_KICKOUT_D_PIN));
        hwMgr->releasePinOwnership(AD_KICKOUT_D_PIN, HardwareManager::OWNER_ADPROCESSOR);
        
        if (hwMgr->requestPinOwnership(AD_KICKOUT_A_PIN, HardwareManager::OWNER_ADPROCESSOR, "ADProcessor")) {
            pinMode(AD_KICKOUT_A_PIN, INPUT_DISABLE);
            hwMgr->updatePinMode(AD_KICKOUT_A_PIN, INPUT_DISABLE);
            LOG_INFO(EventSource::AUTOSTEER, "JD_ENC: Mode DISABLED - analog pressure mode restored");
        }
    }
}

// JD PWM interrupt handlers
void ADProcessor::jdPWMRisingISR()
{
    if (instance) {
        uint32_t nowMicros = micros();
        
        // Calculate period from previous rising edge
        if (instance->jdPWMRiseTime != 0) {
            instance->jdPWMPeriod = nowMicros - instance->jdPWMRiseTime;
            
            // Calculate duty cycle percentage
            if (instance->jdPWMPeriod > 0 && instance->jdPWMDutyTime > 0) {
                instance->jdPWMDutyPercent = (instance->jdPWMDutyTime * 100.0f) / instance->jdPWMPeriod;
            }
        }
        
        instance->jdPWMPrevRiseTime = instance->jdPWMRiseTime;
        instance->jdPWMRiseTime = nowMicros;
        attachInterrupt(digitalPinToInterrupt(AD_KICKOUT_D_PIN), jdPWMFallingISR, FALLING);
        
        // Track interrupt rate for diagnostics
        static uint32_t riseCount = 0;
        static uint32_t lastRateCheck = 0;
        riseCount++;
        
        uint32_t now = millis();
        if (now - lastRateCheck > 10000) { // Every 10 seconds
            uint32_t rate = riseCount * 100 / ((now - lastRateCheck) / 10); // Rate per second
            LOG_DEBUG(EventSource::AUTOSTEER, "JD_ENC Signal: %lu Hz, period=%luus, duty=%.1f%%", 
                      rate, instance->jdPWMPeriod, instance->jdPWMDutyPercent);
            riseCount = 0;
            lastRateCheck = now;
        }
    }
}

void ADProcessor::jdPWMFallingISR()
{
    if (instance) {
        uint32_t fallTime = micros();
        instance->jdPWMDutyTime = fallTime - instance->jdPWMRiseTime;
        attachInterrupt(digitalPinToInterrupt(AD_KICKOUT_D_PIN), jdPWMRisingISR, RISING);
    }
}