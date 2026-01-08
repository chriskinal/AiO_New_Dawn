// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

// SimpleOTAHandler.cpp
// Simple OTA firmware update handler implementation

#include "SimpleOTAHandler.h"
#include "EventLogger.h"
#include <EEPROM.h>

// Static member definitions
bool SimpleOTAHandler::otaInProgress = false;
bool SimpleOTAHandler::otaComplete = false;
uint32_t SimpleOTAHandler::totalBytes = 0;
uint32_t SimpleOTAHandler::processedBytes = 0;
uint8_t SimpleOTAHandler::progress = 0;
const char* SimpleOTAHandler::errorMsg = nullptr;
String SimpleOTAHandler::hexBuffer;
uint32_t SimpleOTAHandler::bufferAddr = 0;
uint32_t SimpleOTAHandler::bufferSize = 0;
uint32_t SimpleOTAHandler::baseAddress = 0;
uint32_t SimpleOTAHandler::minAddress = 0xFFFFFFFF;
uint32_t SimpleOTAHandler::maxAddress = 0;

bool SimpleOTAHandler::init() {
    // Use FlasherX's firmware buffer initialization
    int bufferType = firmware_buffer_init(&bufferAddr, &bufferSize);
    
    if (bufferType == NO_BUFFER_TYPE) {
        LOG_ERROR(EventSource::SYSTEM, "Failed to allocate OTA buffer");
        return false;
    }
    
    const char* bufferTypeStr = (bufferType == RAM_BUFFER_TYPE) ? "RAM" : "FLASH";
    // OTA handler initialized
    return true;
}

void SimpleOTAHandler::reset() {
    otaInProgress = true;
    otaComplete = false;
    totalBytes = 0;
    processedBytes = 0;
    progress = 0;
    errorMsg = nullptr;
    hexBuffer = "";
    baseAddress = 0;
    minAddress = 0xFFFFFFFF;
    maxAddress = 0;
    
    // Clear the firmware buffer
    // Note: For flash buffers, we don't need to clear as we'll erase before writing
    if (bufferAddr != 0 && !IN_FLASH(bufferAddr)) {
        memset((void*)bufferAddr, 0xFF, bufferSize);
    }
    
    // OTA upload started
}

bool SimpleOTAHandler::processChunk(const uint8_t* data, size_t len) {
    if (!otaInProgress || bufferAddr == 0) {
        errorMsg = "OTA not initialized";
        return false;
    }
    
    // Add data to hex buffer
    for (size_t i = 0; i < len; i++) {
        char c = (char)data[i];
        
        if (c == '\r') continue; // Skip carriage returns
        
        if (c == '\n') {
            // Process complete line
            if (hexBuffer.length() > 0) {
                if (!processHexLine(hexBuffer)) {
                    otaInProgress = false;
                    return false;
                }
                hexBuffer = "";
            }
        } else {
            hexBuffer += c;
        }
    }
    
    processedBytes += len;
    
    // Update progress (we don't know total size, so estimate based on typical firmware size)
    // Typical Teensy 4.1 firmware is 100-400KB
    uint32_t estimatedSize = 250 * 1024; // 250KB estimate
    progress = min(99, (processedBytes * 100) / estimatedSize);
    
    return true;
}

bool SimpleOTAHandler::processHexLine(const String& line) {
    if (line.length() < 11) {
        return true; // Skip empty or too short lines
    }
    
    if (line[0] != ':') {
        errorMsg = "Invalid hex line format";
        return false;
    }
    
    uint32_t addr;
    uint8_t data[32];
    uint8_t len;
    uint8_t type;
    
    if (!parseIntelHex(line, addr, data, len, type)) {
        return false;
    }
    
    switch (type) {
        case 0: // Data record
            {
                uint32_t fullAddr = baseAddress + addr;
                
                // Check if address is in valid range for Teensy 4.1
                if (fullAddr < 0x60000000 || fullAddr >= 0x60800000) {
                    errorMsg = "Address out of range";
                    return false;
                }
                
                // Update min/max addresses
                if (fullAddr < minAddress) minAddress = fullAddr;
                if (fullAddr + len > maxAddress) maxAddress = fullAddr + len;
                
                // Calculate address in buffer
                uint32_t destAddr = bufferAddr + fullAddr - FLASH_BASE_ADDR;
                
                // Check bounds
                if (fullAddr - FLASH_BASE_ADDR + len > bufferSize) {
                    errorMsg = "Firmware too large";
                    return false;
                }
                
                // Write to buffer (RAM or FLASH)
                if (!IN_FLASH(bufferAddr)) {
                    // RAM buffer - just copy
                    memcpy((void*)destAddr, data, len);
                } else {
                    // FLASH buffer - write directly
                    int error = flash_write_block(destAddr, (char*)data, len);
                    if (error) {
                        errorMsg = "Flash write failed";
                        LOG_ERROR(EventSource::SYSTEM, "Flash write error %02X at 0x%08X", error, destAddr);
                        return false;
                    }
                }
            }
            break;
            
        case 1: // End of file
            otaComplete = true;
            progress = 100;
            // OTA upload complete
            break;
            
        case 4: // Extended linear address
            if (len == 2) {
                baseAddress = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16);
            }
            break;
            
        case 5: // Start linear address (ignore)
            break;
            
        default:
            LOG_WARNING(EventSource::SYSTEM, "Unknown hex record type: %d", type);
    }
    
    return true;
}

bool SimpleOTAHandler::parseIntelHex(const String& line, uint32_t& addr, uint8_t* data, uint8_t& len, uint8_t& type) {
    if (line.length() < 11 || line[0] != ':') {
        errorMsg = "Invalid hex line";
        return false;
    }
    
    // Parse length
    len = (hexToByte(line[1]) << 4) | hexToByte(line[2]);
    
    // Parse address
    addr = (hexToByte(line[3]) << 12) | (hexToByte(line[4]) << 8) | 
           (hexToByte(line[5]) << 4) | hexToByte(line[6]);
    
    // Parse type
    type = (hexToByte(line[7]) << 4) | hexToByte(line[8]);
    
    // Check line length
    if (line.length() < (11 + len * 2)) {
        errorMsg = "Hex line too short";
        return false;
    }
    
    // Parse data
    for (uint8_t i = 0; i < len; i++) {
        data[i] = (hexToByte(line[9 + i*2]) << 4) | hexToByte(line[10 + i*2]);
    }
    
    // Verify checksum
    uint8_t sum = len + (addr >> 8) + (addr & 0xFF) + type;
    for (uint8_t i = 0; i < len; i++) {
        sum += data[i];
    }
    uint8_t checksum = (hexToByte(line[9 + len*2]) << 4) | hexToByte(line[10 + len*2]);
    
    if (((sum + checksum) & 0xFF) != 0) {
        errorMsg = "Checksum error";
        return false;
    }
    
    return true;
}

uint8_t SimpleOTAHandler::hexToByte(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0;
}

bool SimpleOTAHandler::finalize() {
    if (!otaComplete) {
        // Process any remaining data in buffer
        if (hexBuffer.length() > 0) {
            processHexLine(hexBuffer);
        }
    }
    
    otaInProgress = false;
    
    if (!otaComplete) {
        errorMsg = "Incomplete firmware file";
        return false;
    }
    
    // Verify we got reasonable firmware
    uint32_t firmwareSize = maxAddress - minAddress;
    if (firmwareSize < 1024) {
        errorMsg = "Firmware too small";
        return false;
    }
    
    LOG_INFO(EventSource::SYSTEM, "Firmware validated: %d bytes at 0x%08X", 
             firmwareSize, minAddress);
    
    return true;
}

bool SimpleOTAHandler::applyUpdate() {
    if (!otaComplete || bufferAddr == 0) {
        errorMsg = "No valid firmware to apply";
        return false;
    }
    
    // Calculate firmware size
    uint32_t firmwareSize = maxAddress - minAddress;
    
    LOG_WARNING(EventSource::SYSTEM, "Applying firmware update...");
    
    // If buffer is in flash, data is already written
    if (IN_FLASH(bufferAddr)) {
        // Use flash_move to copy from buffer to program area
        LOG_INFO(EventSource::SYSTEM, "Moving firmware from buffer to program flash...");
        flash_move(FLASH_BASE_ADDR, bufferAddr, firmwareSize);
    } else {
        // RAM buffer - need to write to flash
        LOG_INFO(EventSource::SYSTEM, "Writing firmware from RAM to flash...");
        flash_move(FLASH_BASE_ADDR, bufferAddr, firmwareSize);
    }
    
    // Free the buffer
    firmware_buffer_free(bufferAddr, bufferSize);
    
    LOG_INFO(EventSource::SYSTEM, "Firmware update complete, rebooting...");
    
    // Delay to allow log message to be sent
    delay(100);
    
    // Reboot
    SCB_AIRCR = 0x05FA0004;
    
    return true;
}