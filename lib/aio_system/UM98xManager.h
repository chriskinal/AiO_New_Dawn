// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

// UM98xManager.h - Configuration manager for UM981/UM982 GPS receivers
#ifndef UM98X_MANAGER_H
#define UM98X_MANAGER_H

#include "Arduino.h"
#include "EventLogger.h"

class UM98xManager {
public:
    // Configuration data structure
    struct UM98xConfig {
        String configCommands;   // Multi-line CONFIG commands
        String modeSettings;     // MODE command
        String messageSettings;  // Log output commands
    };
    
    // Constructor
    UM98xManager();
    
    // Initialize with serial port
    bool init(HardwareSerial* serial);
    
    // Read current configuration from GPS
    bool readConfiguration(UM98xConfig& config);
    
    // Write configuration to GPS and save to EEPROM
    bool writeConfiguration(const UM98xConfig& config);
    
private:
    HardwareSerial* gpsSerial;
    static constexpr uint32_t COMMAND_TIMEOUT = 5000;  // 5 second timeout
    static constexpr uint32_t SAVECONFIG_TIMEOUT = 10000; // 10 seconds for SAVECONFIG
    static constexpr size_t BUFFER_SIZE = 512;
    
    // Send command and collect response
    bool sendCommandAndWaitForResponse(const String& cmd, String& response, uint32_t timeout = COMMAND_TIMEOUT);
    
    // Clear serial buffer before sending commands
    void flushSerialBuffer();
    
    // Check if a line is a command response (not NMEA)
    bool isCommandResponse(const String& line);
    
    // Response parsers for each command type
    bool parseConfigResponse(const String& response, String& configOut);
    bool parseModeResponse(const String& response, String& modeOut);
    bool parseLogListResponse(const String& response, String& messagesOut);
    
    // Extract line from serial with timeout
    bool readLineWithTimeout(String& line, uint32_t timeout);
};

#endif // UM98X_MANAGER_H