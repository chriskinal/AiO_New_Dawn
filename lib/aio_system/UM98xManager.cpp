// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

// UM98xManager.cpp - Configuration manager for UM981/UM982 GPS receivers
#include "UM98xManager.h"
#include "GNSSProcessor.h"

// External instance
extern GNSSProcessor gnssProcessor;

UM98xManager::UM98xManager() : gpsSerial(nullptr) {
}

bool UM98xManager::init(HardwareSerial* serial) {
    if (!serial) {
        LOG_ERROR(EventSource::SYSTEM, "UM98xManager: Invalid serial port");
        return false;
    }
    
    gpsSerial = serial;
    LOG_INFO(EventSource::SYSTEM, "UM98xManager initialized");
    return true;
}

bool UM98xManager::readConfiguration(UM98xConfig& config) {
    if (!gpsSerial) {
        LOG_ERROR(EventSource::SYSTEM, "UM98xManager: Not initialized");
        return false;
    }
    
    // Pause GNSSProcessor to get exclusive serial access
    LOG_INFO(EventSource::SYSTEM, "Pausing GNSSProcessor for configuration read");
    gnssProcessor.pauseProcessing();
    
    // Clear any existing data
    config.configCommands = "";
    config.modeSettings = "";
    config.messageSettings = "";
    
    bool success = true;
    String response;
    String savedMessageSettings;
    
    // First, read current message settings before clearing
    LOG_INFO(EventSource::SYSTEM, "Reading current message outputs before clearing...");
    if (sendCommandAndWaitForResponse("UNILOGLIST", response)) {
        parseLogListResponse(response, savedMessageSettings);
        if (savedMessageSettings.length() > 0) {
            // Log each message on its own line to avoid formatting issues
            LOG_INFO(EventSource::SYSTEM, "Found active messages:");
            int start = 0;
            int end = savedMessageSettings.indexOf('\n');
            while (end != -1 || start < (int)savedMessageSettings.length()) {
                String msg;
                if (end != -1) {
                    msg = savedMessageSettings.substring(start, end);
                    start = end + 1;
                    end = savedMessageSettings.indexOf('\n', start);
                } else {
                    msg = savedMessageSettings.substring(start);
                    start = savedMessageSettings.length();
                }
                msg.trim();
                if (msg.length() > 0) {
                    LOG_INFO(EventSource::SYSTEM, "  %s", msg.c_str());
                }
            }
        }
    }
    
    // Now clear all message outputs to avoid interference during read
    LOG_INFO(EventSource::SYSTEM, "Clearing all COM port logs before reading configuration...");
    
    // Clear all three COM ports
    sendCommandAndWaitForResponse("UNLOGALL COM1", response);
    sendCommandAndWaitForResponse("UNLOGALL COM2", response);
    sendCommandAndWaitForResponse("UNLOGALL COM3", response);
    
    // Give GPS a moment to stop sending messages
    delay(100);
    
    // Read CONFIG
    LOG_INFO(EventSource::SYSTEM, "Reading CONFIG from UM98x...");
    if (sendCommandAndWaitForResponse("CONFIG", response)) {
        if (!parseConfigResponse(response, config.configCommands)) {
            LOG_ERROR(EventSource::SYSTEM, "Failed to parse CONFIG response");
            success = false;
        }
    } else {
        LOG_ERROR(EventSource::SYSTEM, "CONFIG command failed");
        success = false;
    }
    
    // Read MODE
    if (success) {
        LOG_INFO(EventSource::SYSTEM, "Reading MODE from UM98x...");
        bool modeSuccess = false;
        
        // Try MODE command up to 3 times
        for (int attempt = 0; attempt < 3 && !modeSuccess; attempt++) {
            if (attempt > 0) {
                LOG_WARNING(EventSource::SYSTEM, "Retrying MODE command (attempt %d)", attempt + 1);
                delay(100);  // Brief delay before retry
            }
            
            if (sendCommandAndWaitForResponse("MODE", response)) {
                if (parseModeResponse(response, config.modeSettings)) {
                    modeSuccess = true;
                } else {
                    LOG_ERROR(EventSource::SYSTEM, "Failed to parse MODE response");
                }
            } else {
                LOG_ERROR(EventSource::SYSTEM, "MODE command failed");
            }
        }
        
        if (!modeSuccess) {
            LOG_ERROR(EventSource::SYSTEM, "MODE command failed after 3 attempts");
            success = false;
        }
    }
    
    // Read UNILOGLIST
    if (success) {
        LOG_INFO(EventSource::SYSTEM, "Reading UNILOGLIST from UM98x...");
        if (sendCommandAndWaitForResponse("UNILOGLIST", response)) {
            if (!parseLogListResponse(response, config.messageSettings)) {
                LOG_ERROR(EventSource::SYSTEM, "Failed to parse UNILOGLIST response");
                success = false;
            }
            // If UNILOGLIST is empty but we saved messages earlier, use those
            if (config.messageSettings.length() == 0 && savedMessageSettings.length() > 0) {
                LOG_INFO(EventSource::SYSTEM, "Using saved message settings for display");
                config.messageSettings = savedMessageSettings;
            }
        } else {
            LOG_ERROR(EventSource::SYSTEM, "UNILOGLIST command failed");
            success = false;
        }
    }
    
    if (success) {
        LOG_INFO(EventSource::SYSTEM, "UM98x configuration read successfully");
        
        // Restore the message outputs that we saved earlier
        if (savedMessageSettings.length() > 0) {
            LOG_INFO(EventSource::SYSTEM, "Restoring message outputs...");
            
            // Split and send each message setting
            int start = 0;
            int end = savedMessageSettings.indexOf('\n');
            
            while (end != -1 || start < (int)savedMessageSettings.length()) {
                String line;
                if (end != -1) {
                    line = savedMessageSettings.substring(start, end);
                    start = end + 1;
                    end = savedMessageSettings.indexOf('\n', start);
                } else {
                    line = savedMessageSettings.substring(start);
                    start = savedMessageSettings.length();
                }
                
                line.trim();
                if (line.length() > 0) {
                    LOG_DEBUG(EventSource::SYSTEM, "Restoring: %s", line.c_str());
                    String dummyResponse;
                    if (!sendCommandAndWaitForResponse(line, dummyResponse)) {
                        LOG_WARNING(EventSource::SYSTEM, "Failed to restore: %s", line.c_str());
                    }
                }
            }
        }
    }
    
    // Resume GNSSProcessor after all operations are complete
    gnssProcessor.resumeProcessing();
    
    return success;
}

bool UM98xManager::writeConfiguration(const UM98xConfig& config) {
    if (!gpsSerial) {
        LOG_ERROR(EventSource::SYSTEM, "UM98xManager: Not initialized");
        return false;
    }
    
    // Pause GNSSProcessor
    gnssProcessor.pauseProcessing();
    
    bool success = true;
    String response;
    
    // First, stop all message outputs to avoid interference
    LOG_INFO(EventSource::SYSTEM, "Clearing all COM port logs before configuration...");
    
    // Clear all three COM ports
    if (!sendCommandAndWaitForResponse("UNLOGALL COM1", response)) {
        LOG_WARNING(EventSource::SYSTEM, "UNLOGALL COM1 failed - continuing");
    }
    if (!sendCommandAndWaitForResponse("UNLOGALL COM2", response)) {
        LOG_WARNING(EventSource::SYSTEM, "UNLOGALL COM2 failed - continuing");
    }
    if (!sendCommandAndWaitForResponse("UNLOGALL COM3", response)) {
        LOG_WARNING(EventSource::SYSTEM, "UNLOGALL COM3 failed - continuing");
    }
    
    // Give GPS a moment to stop sending messages
    delay(100);
    
    // 1. Send each CONFIG command line
    if (config.configCommands.length() > 0) {
        LOG_INFO(EventSource::SYSTEM, "Writing CONFIG commands...");
        
        // Split config commands by newline
        int start = 0;
        int end = config.configCommands.indexOf('\n');
        
        while (end != -1 || start < (int)config.configCommands.length()) {
            String line;
            if (end != -1) {
                line = config.configCommands.substring(start, end);
                start = end + 1;
                end = config.configCommands.indexOf('\n', start);
            } else {
                line = config.configCommands.substring(start);
                start = config.configCommands.length();
            }
            
            line.trim();
            if (line.length() > 0) {
                LOG_DEBUG(EventSource::SYSTEM, "Sending: %s", line.c_str());
                if (!sendCommandAndWaitForResponse(line, response)) {
                    LOG_ERROR(EventSource::SYSTEM, "Failed to send CONFIG: %s", line.c_str());
                    success = false;
                    break;
                }
            }
        }
    }
    
    // 2. Set MODE
    if (success && config.modeSettings.length() > 0) {
        LOG_INFO(EventSource::SYSTEM, "Setting MODE...");
        String modeCmd = config.modeSettings;
        modeCmd.trim();
        
        if (!sendCommandAndWaitForResponse(modeCmd, response)) {
            LOG_ERROR(EventSource::SYSTEM, "Failed to set MODE: %s", modeCmd.c_str());
            success = false;
        }
    }
    
    // 3. Clear existing logs and set new ones
    if (success) {
        LOG_INFO(EventSource::SYSTEM, "Clearing logs before setting new ones...");
        // Clear all COM ports again before setting new logs
        sendCommandAndWaitForResponse("UNLOGALL COM1", response);
        sendCommandAndWaitForResponse("UNLOGALL COM2", response);
        sendCommandAndWaitForResponse("UNLOGALL COM3", response);
        
        if (config.messageSettings.length() > 0) {
            LOG_INFO(EventSource::SYSTEM, "Setting message outputs...");
            
            // Split message settings by newline
            int start = 0;
            int end = config.messageSettings.indexOf('\n');
            
            while (end != -1 || start < (int)config.messageSettings.length()) {
                String line;
                if (end != -1) {
                    line = config.messageSettings.substring(start, end);
                    start = end + 1;
                    end = config.messageSettings.indexOf('\n', start);
                } else {
                    line = config.messageSettings.substring(start);
                    start = config.messageSettings.length();
                }
                
                line.trim();
                if (line.length() > 0) {
                    LOG_DEBUG(EventSource::SYSTEM, "Sending: %s", line.c_str());
                    if (!sendCommandAndWaitForResponse(line, response)) {
                        LOG_ERROR(EventSource::SYSTEM, "Failed to set log: %s", line.c_str());
                        success = false;
                        break;
                    }
                }
            }
        }
    }
    
    // 4. CRITICAL: Save configuration to EEPROM
    if (success) {
        LOG_INFO(EventSource::SYSTEM, "Saving configuration to UM98x EEPROM...");
        if (!sendCommandAndWaitForResponse("SAVECONFIG", response, SAVECONFIG_TIMEOUT)) {
            LOG_ERROR(EventSource::SYSTEM, "SAVECONFIG failed - configuration not saved!");
            success = false;
        } else {
            LOG_INFO(EventSource::SYSTEM, "Configuration saved to UM98x EEPROM successfully");
        }
    }
    
    // Resume GNSSProcessor
    gnssProcessor.resumeProcessing();
    
    return success;
}

bool UM98xManager::sendCommandAndWaitForResponse(const String& cmd, String& response, uint32_t timeout) {
    // Clear serial buffer first
    flushSerialBuffer();
    
    // Send command with CRLF
    gpsSerial->print(cmd);
    gpsSerial->print("\r\n");
    
    // Clear response
    response = "";
    
    // Wait for acknowledgment or response
    uint32_t startTime = millis();
    String line;
    bool gotAck = false;
    int configLinesReceived = 0;
    uint32_t lastConfigTime = 0;
    
    while (millis() - startTime < timeout) {
        if (readLineWithTimeout(line, 100)) {  // 100ms timeout per line
            
            // Check if this is our command acknowledgment
            if (line.startsWith("$command,") && line.indexOf("response: OK") > 0) {
                gotAck = true;
                response += line + "\n";
                // Don't return yet for query commands - keep reading
                if (cmd != "CONFIG" && cmd != "MODE" && cmd != "UNILOGLIST") {
                    return true;  // Non-query commands can return after OK
                }
                lastConfigTime = millis();  // Set initial time when we get ACK
            }
            // Check if this is CONFIG data
            else if (cmd == "CONFIG" && line.startsWith("$CONFIG,")) {
                response += line + "\n";
                configLinesReceived++;
                lastConfigTime = millis();
                
                // Check if this is a COM port config (last items)
                if (line.startsWith("$CONFIG,COM")) {
                    // COM3 is typically the last config item
                    if (line.startsWith("$CONFIG,COM3,")) {
                        // Wait a bit to ensure no more CONFIG lines
                        delay(100);
                        if (!gpsSerial->available()) {
                            return true;
                        }
                    }
                }
            }
            // Check if this is MODE data
            else if (cmd == "MODE" && line.startsWith("#MODE,")) {
                response += line + "\n";
                return true;  // MODE is a single line response
            }
            // Check if this is UNILOGLIST data
            else if (cmd == "UNILOGLIST" && line.startsWith("<")) {
                response += line + "\n";
                
                // Check if this is the count line (e.g., "<\t3" or "<\t0")
                String countStr = line.substring(1);  // Skip the '<'
                countStr.trim();  // This will remove tabs, spaces, etc.
                
                int logCount = countStr.toInt();
                // Check if this is a valid count (including 0)
                if (countStr.length() <= 3 && countStr == String(logCount)) {
                    if (logCount == 0) {
                        // No logs configured, we're done
                        return true;
                    }
                    
                    // Read the specified number of log entries
                    int logsRead = 0;
                    while (logsRead < logCount && millis() - startTime < timeout) {
                        if (readLineWithTimeout(line, 100)) {
                            response += line + "\n";
                            logsRead++;
                        }
                    }
                    
                    if (logsRead == logCount) {
                        return true;
                    }
                } else {
                    lastConfigTime = millis();
                }
            }
            // For query commands, if we got data and haven't seen more for 300ms, we're done
            else if (gotAck && (cmd == "CONFIG" || cmd == "UNILOGLIST")) {
                if (lastConfigTime > 0 && (millis() - lastConfigTime > 300)) {
                    return true;
                }
            }
        }
    }
    
    // If we got here, check if we received data for query commands
    if ((cmd == "CONFIG" || cmd == "MODE" || cmd == "UNILOGLIST") && gotAck) {
        return true;  // We got the OK and whatever data followed
    }
    
    LOG_ERROR(EventSource::SYSTEM, "Command timeout: %s", cmd.c_str());
    return false;
}

void UM98xManager::flushSerialBuffer() {
    while (gpsSerial->available()) {
        gpsSerial->read();
    }
}

bool UM98xManager::isCommandResponse(const String& line) {
    // Command responses start with $ or # and contain specific patterns
    return line.startsWith("$command,") || 
           line.startsWith("$CONFIG,") ||
           line.startsWith("#MODE,") ||
           line.startsWith("<.");
}

bool UM98xManager::parseConfigResponse(const String& response, String& configOut) {
    configOut = "";
    
    // Split response into lines
    int start = 0;
    int end = response.indexOf('\n');
    
    while (end != -1 || start < response.length()) {
        String line;
        if (end != -1) {
            line = response.substring(start, end);
            start = end + 1;
            end = response.indexOf('\n', start);
        } else {
            line = response.substring(start);
            start = response.length();
        }
        
        line.trim();
        
        // Parse lines starting with $CONFIG,
        if (line.startsWith("$CONFIG,")) {
            // Find the second comma
            int firstComma = line.indexOf(',');
            if (firstComma > 0) {
                int secondComma = line.indexOf(',', firstComma + 1);
                if (secondComma > 0) {
                    // Extract config command (ignore checksum)
                    int asterisk = line.indexOf('*');
                    String configCmd;
                    if (asterisk > secondComma) {
                        configCmd = line.substring(secondComma + 1, asterisk);
                    } else {
                        configCmd = line.substring(secondComma + 1);
                    }
                    
                    configCmd.trim();
                    if (configCmd.length() > 0) {
                        if (configOut.length() > 0) {
                            configOut += "\n";
                        }
                        configOut += configCmd;
                    }
                }
            }
        }
    }
    
    return configOut.length() > 0;
}

bool UM98xManager::parseModeResponse(const String& response, String& modeOut) {
    modeOut = "";
    
    // Look for line starting with #MODE,
    // Format: #MODE,76,GPS,FINE,2382,246983200,0,0,18,47;MODE ROVER SURVEY,*2A
    int modeStart = response.indexOf("#MODE,");
    if (modeStart >= 0) {
        int lineEnd = response.indexOf('\n', modeStart);
        String modeLine;
        if (lineEnd > modeStart) {
            modeLine = response.substring(modeStart, lineEnd);
        } else {
            modeLine = response.substring(modeStart);
        }
        
        // Find semicolon and extract mode setting
        int semicolon = modeLine.indexOf(';');
        if (semicolon > 0) {
            // Remove the trailing comma before asterisk if present
            int asterisk = modeLine.indexOf("*", semicolon);
            if (asterisk > semicolon) {
                modeOut = modeLine.substring(semicolon + 1, asterisk);
                // Remove trailing comma if present
                if (modeOut.endsWith(",")) {
                    modeOut = modeOut.substring(0, modeOut.length() - 1);
                }
            } else {
                modeOut = modeLine.substring(semicolon + 1);
            }
            modeOut.trim();
            return true;
        }
    }
    
    return false;
}

bool UM98xManager::parseLogListResponse(const String& response, String& messagesOut) {
    messagesOut = "";
    
    // Split response into lines
    int start = 0;
    int end = response.indexOf('\n');
    
    while (end != -1 || start < response.length()) {
        String line;
        if (end != -1) {
            line = response.substring(start, end);
            start = end + 1;
            end = response.indexOf('\n', start);
        } else {
            line = response.substring(start);
            start = response.length();
        }
        
        line.trim();
        
        // Parse lines starting with < but skip the count line
        if (line.startsWith("<")) {
            String content = line.substring(1);  // Skip "<"
            content.trim();  // This removes tabs, spaces, etc.
            
            // Check if this is the count line (just a number)
            int count = content.toInt();
            if (content.length() <= 3 && (count >= 0 && content == String(count))) {
                // This is the count line, skip it
                continue;
            }
            
            // This is a log command
            if (content.length() > 0) {
                if (messagesOut.length() > 0) {
                    messagesOut += "\n";
                }
                messagesOut += content;
            }
        }
    }
    
    return true;  // Can be empty if no logs configured
}

bool UM98xManager::readLineWithTimeout(String& line, uint32_t timeout) {
    line = "";
    uint32_t startTime = millis();
    
    while (millis() - startTime < timeout) {
        if (gpsSerial->available()) {
            char c = gpsSerial->read();
            if (c == '\r') {
                continue;  // Skip CR
            } else if (c == '\n') {
                if (line.length() > 0) {
                    return true;  // Got complete line
                }
            } else {
                line += c;
                if (line.length() > BUFFER_SIZE) {
                    // Prevent buffer overflow
                    return true;
                }
            }
        }
    }
    
    return false;  // Timeout
}