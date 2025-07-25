#include "ADProcessor.h"
#include "EventLogger.h"
#include "HardwareManager.h"

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
    
    // Configure pins with ownership tracking
    pinMode(AD_STEER_PIN, INPUT_PULLUP);      // Steer switch with internal pullup
    pinMode(AD_WORK_PIN, INPUT_PULLUP);       // Work switch with pullup
    pinMode(AD_WAS_PIN, INPUT_DISABLE);       // WAS analog input (no pullup)
    
    // Request ownership of KICKOUT_A for pressure sensor mode
    HardwareManager* hwMgr = HardwareManager::getInstance();
    if (hwMgr->requestPinOwnership(AD_KICKOUT_A_PIN, HardwareManager::OWNER_ADPROCESSOR, "ADProcessor")) {
        pinMode(AD_KICKOUT_A_PIN, INPUT_DISABLE); // Pressure sensor analog input
        hwMgr->updatePinMode(AD_KICKOUT_A_PIN, INPUT_DISABLE);
    } else {
        LOG_WARNING(EventSource::AUTOSTEER, "Failed to get ownership of KICKOUT_A pin");
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
    // Always read WAS - critical for steering
    updateWAS();
    
    // Fast current sensor sampling (every 1ms like test sketch)
    static uint32_t lastCurrentSample = 0;
    uint32_t now = millis();
    
    if (now - lastCurrentSample >= 1) {
        lastCurrentSample = now;
        
        // Read current sensor and store in buffer
        uint16_t reading = teensyADC->adc1->analogRead(AD_CURRENT_PIN);
        
        // Simple approach from test sketch - subtract baseline offset
        float adjusted = (float)(reading - 77);  // 77 is our baseline
        currentBuffer[currentBufferIndex] = (adjusted > 0) ? adjusted : 0.0f;
        currentBufferIndex = (currentBufferIndex + 1) % CURRENT_BUFFER_SIZE;
        
        // Calculate average of buffer
        float sum = 0;
        for (int i = 0; i < CURRENT_BUFFER_SIZE; i++) {
            sum += currentBuffer[i];
        }
        currentReading = sum / CURRENT_BUFFER_SIZE;
    }
    
    // Read other sensors at reduced rate (every 10ms = 100Hz)
    static uint32_t lastSlowRead = 0;
    
    if (now - lastSlowRead >= 10) {
        lastSlowRead = now;
        
        // Update switches
        updateSwitches();
        
        // Read kickout sensors
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
    int workPinRaw = digitalRead(AD_WORK_PIN);
    
    // Convert to active states
    bool workRaw = !workPinRaw;     // Work is active LOW (pressed = 0)
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
        
        // Debug logging
        static uint32_t lastWASDebug = 0;
        if (millis() - lastWASDebug > 2000) {
            lastWASDebug = millis();
            LOG_DEBUG(EventSource::AUTOSTEER, "WAS: raw=%d, centered=%.0f, angle=%.2f°, offset=%d, CPD=%.1f", 
                      wasRaw, centeredWAS, angle, wasOffset, wasCountsPerDegree);
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
    LOG_INFO(EventSource::AUTOSTEER, "  Work: %s%s", 
                  workSwitch.debouncedState ? "ON" : "OFF",
                  workSwitch.hasChanged ? " (changed)" : "");
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