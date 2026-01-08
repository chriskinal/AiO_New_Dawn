// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

// MotorDriverManager.cpp - Implementation
#include "MotorDriverManager.h"
#include "SerialManager.h"

// Define the static instance
MotorDriverManager* MotorDriverManager::instance = nullptr;

void MotorDriverManager::init() {
    LOG_INFO(EventSource::AUTOSTEER, "Initializing motor driver manager");
    detectionStartTime = millis();
    
    // Read motor configuration from EEPROM
    readMotorConfig();
}

MotorDriverInterface* MotorDriverManager::detectAndCreateMotorDriver(HardwareManager* hwMgr, CANManager* canMgr) {
    LOG_INFO(EventSource::AUTOSTEER, "Starting motor driver detection...");

    // Initialize detection
    init();

    // First check if TractorCANDriver is configured
    extern ConfigManager configManager;
    CANSteerConfig canConfig = configManager.getCANSteerConfig();

    if (canConfig.brand != 0) {  // 0 = DISABLED
        // Use TractorCANDriver for all CAN-based steering
        detectedType = MotorDriverType::TRACTOR_CAN;
        detectionComplete = true;
        LOG_INFO(EventSource::AUTOSTEER, "Using TractorCANDriver - Brand: %d", canConfig.brand);
    } else {
        // Original detection logic for non-CAN motors
        // Wait for detection to complete (reduced to 1 second since no Keya auto-detect)
        uint32_t startTime = millis();

        while (!detectionComplete && (millis() - startTime) < 1100) {
            performDetection(false);  // No more Keya auto-detection
            delay(10);
        }

        // Force detection completion if timeout
        if (!detectionComplete) {
            LOG_DEBUG(EventSource::AUTOSTEER, "Detection timeout - using configured/default driver");
            performDetection(false);
        }
    }
    
    const char* driverName = "Unknown";
    switch (detectedType) {
        case MotorDriverType::TRACTOR_CAN:
            driverName = "Tractor CAN Driver";
            break;
        case MotorDriverType::KEYA_SERIAL:
            driverName = "Keya Serial Motor";
            break;
        case MotorDriverType::DANFOSS:
            driverName = "Danfoss Valve";
            break;
        case MotorDriverType::DRV8701:
            driverName = "DRV8701 PWM";
            break;
        default:
            break;
    }
    
    LOG_INFO(EventSource::AUTOSTEER, "Motor driver detected: %s", driverName);
    
    // Create the motor driver
    return createMotorDriver(detectedType, hwMgr, canMgr);
}

MotorDriverInterface* MotorDriverManager::createMotorDriver(MotorDriverType type, 
                                                           HardwareManager* hwMgr,
                                                           CANManager* canMgr) {
    switch (type) {
        case MotorDriverType::DRV8701:
            return new PWMMotorDriver(
                MotorDriverType::DRV8701,
                hwMgr->getPWM1Pin(),       // PWM1 for LEFT
                hwMgr->getPWM2Pin(),       // PWM2 for RIGHT  
                hwMgr->getSleepPin(),      // Enable pin (SLEEP_PIN on DRV8701, also LOCK output)
                hwMgr->getCurrentPin()     // Current sense pin
            );
            
        case MotorDriverType::TRACTOR_CAN:
            return new TractorCANDriver();
            
        case MotorDriverType::KEYA_SERIAL:
            return new KeyaSerialDriver();
            
        case MotorDriverType::DANFOSS:
            LOG_INFO(EventSource::AUTOSTEER, "Creating Danfoss valve driver");
            return new DanfossMotorDriver(hwMgr);
            
        default:
            LOG_WARNING(EventSource::AUTOSTEER, "Unknown motor type");
            return nullptr;
    }
}

bool MotorDriverManager::performDetection(bool keyaHeartbeatDetected) {
    if (detectionComplete) {
        return true;
    }
    
    // Priority 1: Check for Keya CAN heartbeat - DISABLED, now using TractorCANDriver
    // if (keyaHeartbeatDetected) {
    //     detectedType = MotorDriverType::KEYA_CAN;
    //     kickoutType = KickoutType::NONE;  // Keya uses motor slip detection
    //     LOG_INFO(EventSource::AUTOSTEER, "Detected Keya CAN motor via heartbeat");
    //     detectionComplete = true;
    //     return true;
    // }
    
    // Priority 2: Check for Keya Serial (after 1 second, if no CAN)
    if (millis() - detectionStartTime > 1000 && !keyaSerialChecked) {
        keyaSerialChecked = true;
        if (probeKeyaSerial()) {
            detectedType = MotorDriverType::KEYA_SERIAL;
            kickoutType = KickoutType::NONE;  // Keya uses motor slip detection
            LOG_INFO(EventSource::AUTOSTEER, "Detected Keya Serial motor via RS232");
            detectionComplete = true;
            return true;
        }
    }
    
    // Reduced wait time since Keya is now configured via web
    if (millis() - detectionStartTime < 1000) {
        return false;  // Still waiting
    }
    
    // Priority 2: Check EEPROM configuration
    switch (static_cast<MotorDriverConfig>(motorConfigByte)) {
        case MotorDriverConfig::DANFOSS_WHEEL_ENCODER:
            detectedType = MotorDriverType::DANFOSS;
            kickoutType = KickoutType::WHEEL_ENCODER;
            LOG_INFO(EventSource::AUTOSTEER, "Detected Danfoss valve with wheel encoder (config 0x%02X)", motorConfigByte);
            break;
            
        case MotorDriverConfig::DANFOSS_PRESSURE_SENSOR:
            detectedType = MotorDriverType::DANFOSS;
            kickoutType = KickoutType::PRESSURE_SENSOR;
            LOG_INFO(EventSource::AUTOSTEER, "Detected Danfoss valve with pressure sensor (config 0x%02X)", motorConfigByte);
            break;
            
        case MotorDriverConfig::DRV8701_WHEEL_ENCODER:
            detectedType = MotorDriverType::DRV8701;
            kickoutType = KickoutType::WHEEL_ENCODER;
            LOG_INFO(EventSource::AUTOSTEER, "Detected DRV8701 with wheel encoder (config 0x%02X)", motorConfigByte);
            break;
            
        case MotorDriverConfig::DRV8701_PRESSURE_SENSOR:
            detectedType = MotorDriverType::DRV8701;
            kickoutType = KickoutType::PRESSURE_SENSOR;
            LOG_INFO(EventSource::AUTOSTEER, "Detected DRV8701 with pressure sensor (config 0x%02X)", motorConfigByte);
            break;
            
        case MotorDriverConfig::DRV8701_CURRENT_SENSOR:
            detectedType = MotorDriverType::DRV8701;
            kickoutType = KickoutType::CURRENT_SENSOR;
            LOG_INFO(EventSource::AUTOSTEER, "Detected DRV8701 with current sensor (config 0x%02X)", motorConfigByte);
            break;
            
        default:
            // Priority 3: Default to DRV8701 with wheel encoder
            detectedType = MotorDriverType::DRV8701;
            kickoutType = KickoutType::WHEEL_ENCODER;
            LOG_WARNING(EventSource::AUTOSTEER, "Unknown motor config 0x%02X, defaulting to DRV8701 with wheel encoder", motorConfigByte);
            break;
    }
    
    detectionComplete = true;
    return true;
}

void MotorDriverManager::updateMotorConfig(uint8_t configByte) {
    if (motorConfigByte != configByte) {
        motorConfigByte = configByte;
        // AutosteerProcessor handles the logging and EEPROM save
    }
}

bool MotorDriverManager::probeKeyaSerial() {
    // Send 0xE2 query command and check for echo response
    LOG_INFO(EventSource::AUTOSTEER, "Probing for Keya Serial motor on RS232...");
    
    // Build query command (0xE2 = query speed)
    uint8_t queryCmd[4];
    queryCmd[0] = 0xE2;
    queryCmd[1] = 0x00;
    queryCmd[2] = 0x00;
    // Calculate checksum (sum of all bytes & 0xFF)
    queryCmd[3] = (queryCmd[0] + queryCmd[1] + queryCmd[2]) & 0xFF;
    
    // Clear any pending data
    while (SerialRS232.available()) {
        SerialRS232.read();
    }
    
    // Send query
    SerialRS232.write(queryCmd, 4);
    
    // Wait for response (up to 100ms)
    uint32_t probeStart = millis();
    uint8_t response[5];  // Motor returns 5 bytes for query
    uint8_t responseIndex = 0;
    
    while (millis() - probeStart < 100 && responseIndex < 5) {
        if (SerialRS232.available()) {
            response[responseIndex++] = SerialRS232.read();
        }
    }
    
    // Check if we got a valid echo response (at least 4 bytes starting with 0xE2)
    if (responseIndex >= 4 && response[0] == 0xE2) {
        LOG_INFO(EventSource::AUTOSTEER, "Keya Serial probe successful - got %d byte response", responseIndex);
        return true;
    }
    
    LOG_DEBUG(EventSource::AUTOSTEER, "Keya Serial probe failed - got %d bytes", responseIndex);
    return false;
}

void MotorDriverManager::readMotorConfig() {
    // Read from ConfigManager (EEPROM)
    extern ConfigManager configManager;
    motorConfigByte = configManager.getMotorDriverConfig();
    
    const char* configDesc = "Unknown";
    switch (motorConfigByte) {
        case 0x00: configDesc = "DRV8701 + Wheel Encoder"; break;
        case 0x01: configDesc = "Danfoss + Wheel Encoder"; break;
        case 0x02: configDesc = "DRV8701 + Pressure Sensor"; break;
        case 0x03: configDesc = "Danfoss + Pressure Sensor"; break;
        case 0x04: configDesc = "DRV8701 + Current Sensor"; break;
    }
    
    LOG_INFO(EventSource::AUTOSTEER, "Motor config from EEPROM: 0x%02X (%s)", motorConfigByte, configDesc);
}