// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

#ifndef PWMPROCESSOR_H
#define PWMPROCESSOR_H

#include <Arduino.h>

// Forward declaration
class HardwareManager;

/**
 * PWMProcessor - PWM Output Processor for Autosteer
 *
 * Handles:
 * - Speed pulse output for odometry/speed sensing
 *   With open-collector output (Q11 fet)
 *   Output is inverted: HIGH from Teensy = LOW output from Q11 which activates an optocoupler to connect the Speed+ & Speed- pins to each other
 * - Configurable frequency and duty cycle
 * - Can generate pulses based on speed input
 *
 * Pin assignments are read from HardwareManager during init()
 */
class PWMProcessor {
public:
    PWMProcessor();
    ~PWMProcessor() = default;
    
    // Initialization
    bool init();
    
    // Main processing - call from loop()
    void process();
    
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
    // HardwareManager reference for pin access
    HardwareManager* hwManager;

    // Pin assignments - cached from HardwareManager during init()
    uint8_t speedPulsePin;      // Actual speed pulse output
    uint8_t speedPulseLEDPin;   // LED output at 1/10 speed pulse frequency

    // PWM parameters
    float pulseFrequency;      // Hz
    float pulseDuty;           // 0.0-1.0
    bool pulseEnabled;
    
    // Speed-based generation
    float currentSpeedKmh;
    float pulsesPerMeter;
    
    // Timing
    uint32_t lastSpeedUpdate;
    
    // Helper methods
    void updatePWM();
    float speedToFrequency(float speedKmh) const;
};

#endif // PWMPROCESSOR_H