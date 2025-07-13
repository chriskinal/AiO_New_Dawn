#include "ADProcessor.h"
#include "EventLogger.h"

// Static instance
ADProcessor* ADProcessor::instance = nullptr;

ADProcessor::ADProcessor() : 
    wasRaw(0),
    wasOffset(0),
    wasCountsPerDegree(1.0f),
    kickoutAnalogRaw(0),
    pressureReading(0.0f),
    motorCurrentRaw(0),
    debounceDelay(50),  // 50ms default debounce
    lastProcessTime(0),
    teensyADC(nullptr)
{
    // Initialize switch states
    workSwitch = {false, false, 0, false};
    steerSwitch = {false, false, 0, false};
    
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
    
    // Configure pins
    pinMode(AD_STEER_PIN, INPUT_PULLUP);      // Steer switch with internal pullup
    pinMode(AD_WORK_PIN, INPUT_PULLUP);       // Work switch with pullup
    pinMode(AD_WAS_PIN, INPUT_DISABLE);       // WAS analog input (no pullup)
    pinMode(AD_KICKOUT_A_PIN, INPUT_DISABLE); // Pressure sensor analog input
    pinMode(AD_CURRENT_PIN, INPUT_DISABLE);   // Current sensor analog input
    
    // Test immediately after setting
    LOG_DEBUG(EventSource::AUTOSTEER, "After pinMode: Pin %d digital=%d", AD_STEER_PIN, digitalRead(AD_STEER_PIN));
    
    // Initialize Teensy ADC library
    teensyADC = new ADC();
    
    // Configure ADC0 for WAS reading - optimized settings
    teensyADC->adc0->setAveraging(4);                                      // Reduced from 16 for faster reads
    teensyADC->adc0->setResolution(12);                                    // 12-bit resolution
    teensyADC->adc0->setConversionSpeed(ADC_CONVERSION_SPEED::HIGH_SPEED); // Faster than MED_SPEED
    teensyADC->adc0->setSamplingSpeed(ADC_SAMPLING_SPEED::HIGH_SPEED);     // Faster than MED_SPEED
    
    // Configure ADC1 for other sensors if needed
    teensyADC->adc1->setAveraging(4);
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
    
    // Read other sensors at reduced rate (every 10ms = 100Hz)
    static uint32_t lastSlowRead = 0;
    uint32_t now = millis();
    
    if (now - lastSlowRead >= 10) {
        lastSlowRead = now;
        
        // Update switches
        updateSwitches();
        
        // Read kickout sensors using ADC1 for parallel operation
        kickoutAnalogRaw = teensyADC->adc1->analogRead(AD_KICKOUT_A_PIN);
        motorCurrentRaw = teensyADC->adc1->analogRead(AD_CURRENT_PIN);
        
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
    // Angle = (raw - offset) / countsPerDegree
    if (wasCountsPerDegree != 0) {
        return (float)(wasRaw - wasOffset) / wasCountsPerDegree;
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
    float adcVoltage = (wasRaw * 4.9f) / 4095.0f;
    return adcVoltage; // * 2.0f;  // Account for voltage divider
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
