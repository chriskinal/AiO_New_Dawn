#ifndef PWMPROCESSOR_H
#define PWMPROCESSOR_H

#include <Arduino.h>

/**
 * PWMProcessor - PWM Output Processor for Autosteer
 * 
 * Handles:
 * - Speed pulse output for odometry/speed sensing
 *   Pin D36 with open-collector output (Q5 transistor)
 *   Output is inverted: HIGH from Teensy = LOW output
 * - Configurable frequency and duty cycle
 * - Can generate pulses based on speed input
 */
class PWMProcessor {
public:
    PWMProcessor();
    ~PWMProcessor() = default;
    
    // Initialization
    bool init();
    
    // Speed pulse control
    void setSpeedPulseHz(float hz);        // Set pulse frequency in Hz
    void setSpeedPulseDuty(float duty);    // Set duty cycle (0.0-1.0)
    void enableSpeedPulse(bool enable);    // Enable/disable output
    
    // Speed-based pulse generation
    void setSpeedKmh(float speedKmh);      // Set speed in km/h
    void setPulsesPerMeter(float ppm);     // Set pulses per meter calibration
    
    // Get current settings
    float getSpeedPulseHz() const { return pulseFrequency; }
    float getSpeedPulseDuty() const { return pulseDuty; }
    bool isSpeedPulseEnabled() const { return pulseEnabled; }
    float getSpeedKmh() const { return currentSpeedKmh; }
    float getPulsesPerMeter() const { return pulsesPerMeter; }
    
    // Diagnostics
    void printStatus() const;
    
    // Static instance for singleton pattern
    static PWMProcessor* instance;
    static PWMProcessor* getInstance();

private:
    // Pin assignment from pcb.h
    static constexpr uint8_t SPEED_PULSE_PIN = 33;      // D33 - actual speed pulse output
    static constexpr uint8_t SPEED_PULSE_LED_PIN = 37;  // D37 - output for LED at 1/10 speed pulse frequency
    
    // PWM parameters
    float pulseFrequency;      // Hz
    float pulseDuty;           // 0.0-1.0
    bool pulseEnabled;
    
    // Speed-based generation
    float currentSpeedKmh;
    float pulsesPerMeter;
    
    // Helper methods
    void updatePWM();
    float speedToFrequency(float speedKmh) const;
};

#endif // PWMPROCESSOR_H