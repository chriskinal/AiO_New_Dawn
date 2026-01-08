// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

#ifndef ENCODER_PROCESSOR_H
#define ENCODER_PROCESSOR_H

#include <Arduino.h>
#include "TurnSensorTypes.h"
#include "Encoder.h"

/**
 * EncoderProcessor - Handles digital rotary encoders for kickout detection
 * 
 * Supports:
 * - Single channel encoders (pulse counting)
 * - Quadrature encoders (position tracking)
 * 
 * Feeds pulse count data to KickoutMonitor for threshold checking
 */
class EncoderProcessor {
private:
    static EncoderProcessor* instance;
    
    // Configuration
    EncoderType encoderType = EncoderType::SINGLE;
    bool encoderEnabled = false;
    
    // Encoder object (created dynamically based on config)
    Encoder* encoder = nullptr;
    
    // Encoder readings
    int32_t pulseCount = 0;
    int32_t lastEncoderValue = 0;
    
    // Private constructor for singleton
    EncoderProcessor() = default;
    
public:
    // Singleton access
    static EncoderProcessor* getInstance();
    
    // Initialization
    bool init();
    void updateConfig(EncoderType type, bool enabled);
    
    // Process encoder readings
    void process();
    
    // Reset pulse count
    void resetPulseCount();
    
    // Getters
    int32_t getPulseCount() const { return pulseCount; }
    bool isEnabled() const { return encoderEnabled; }
    EncoderType getEncoderType() const { return encoderType; }
    
private:
    // Initialize/deinitialize encoder based on configuration
    void initEncoder();
    void deinitEncoder();
};

// Global instance
extern EncoderProcessor* encoderProcessor;

#endif // ENCODER_PROCESSOR_H