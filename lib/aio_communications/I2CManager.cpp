// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

// I2CManager.cpp - Implementation of I2C bus management
#include "I2CManager.h"
#include "EventLogger.h"
#include "HardwareManager.h"

I2CManager::I2CManager() {
    // Initialize bus info structures
    wire0Info = {false, 0, 0, {0}};
    wire1Info = {false, 0, 0, {0}};
    wire2Info = {false, 0, 0, {0}};
}

bool I2CManager::initializeI2C() {
    LOG_INFO(EventSource::SYSTEM, "I2C Manager Initialization starting");
    
    bool success = true;
    
    // Initialize Wire (I2C0) - Primary I2C bus on pins 18/19
    LOG_DEBUG(EventSource::SYSTEM, "Initializing Wire (I2C0)...");
    if (initializeBus(Wire, I2C_SPEED_FAST)) {
        LOG_DEBUG(EventSource::SYSTEM, "Wire (I2C0) initialized successfully");
        wire0Info.initialized = true;
        wire0Info.speed = I2C_SPEED_FAST;
    } else {
        LOG_ERROR(EventSource::SYSTEM, "Wire (I2C0) initialization FAILED");
        success = false;
    }
    
    // Initialize Wire1 (I2C1) - Secondary I2C bus on pins 16/17
    LOG_DEBUG(EventSource::SYSTEM, "Initializing Wire1 (I2C1)...");
    if (initializeBus(Wire1, I2C_SPEED_FAST)) {
        LOG_DEBUG(EventSource::SYSTEM, "Wire1 (I2C1) initialized successfully");
        wire1Info.initialized = true;
        wire1Info.speed = I2C_SPEED_FAST;
    } else {
        LOG_ERROR(EventSource::SYSTEM, "Wire1 (I2C1) initialization FAILED");
        success = false;
    }
    
    // Initialize Wire2 (I2C2) - Third I2C bus on pins 24/25
    LOG_DEBUG(EventSource::SYSTEM, "Initializing Wire2 (I2C2)...");
    if (initializeBus(Wire2, I2C_SPEED_FAST)) {
        LOG_DEBUG(EventSource::SYSTEM, "Wire2 (I2C2) initialized successfully");
        wire2Info.initialized = true;
        wire2Info.speed = I2C_SPEED_FAST;
    } else {
        LOG_ERROR(EventSource::SYSTEM, "Wire2 (I2C2) initialization FAILED");
        success = false;
    }
    
    // Device detection disabled - can hang with misbehaving devices
    // System works fine with known device addresses
    if (success) {
        LOG_INFO(EventSource::SYSTEM, "I2C buses initialized");
    }
    
    return success;
}

bool I2CManager::initializeBus(TwoWire& wire, uint32_t speed) {
    // Determine which bus this is
    HardwareManager::I2CBus busId;
    const char* busName;
    if (&wire == &Wire) {
        busId = HardwareManager::I2C_BUS_0;
        busName = "Wire";
    } else if (&wire == &Wire1) {
        busId = HardwareManager::I2C_BUS_1;
        busName = "Wire1";
    } else if (&wire == &Wire2) {
        busId = HardwareManager::I2C_BUS_2;
        busName = "Wire2";
    } else {
        LOG_ERROR(EventSource::SYSTEM, "Unknown I2C bus");
        return false;
    }
    
    // Register I2C speed with HardwareManager
    HardwareManager* hwMgr = HardwareManager::getInstance();
    if (!hwMgr->requestI2CSpeed(busId, speed, "I2CManager")) {
        LOG_WARNING(EventSource::SYSTEM, "Failed to register I2C speed for %s", busName);
    }
    
    // Begin I2C bus
    wire.begin();
    
    // Set clock speed
    wire.setClock(speed);
    
    // Small delay to let bus stabilize
    delay(10);
    
    // Test if bus is working by trying to scan for devices
    wire.beginTransmission(0x00);
    uint8_t error = wire.endTransmission();
    
    // Error 2 means no device at address 0x00, which is expected
    // Any other error indicates a bus problem
    return (error == 2 || error == 0);
}

bool I2CManager::detectDevices() {
    bool foundAny = false;
    
    if (wire0Info.initialized) {
        LOG_DEBUG(EventSource::SYSTEM, "Scanning Wire (I2C0)...");
        if (scanBus(Wire, wire0Info)) {
            foundAny = true;
        }
    }
    
    if (wire1Info.initialized) {
        LOG_DEBUG(EventSource::SYSTEM, "Scanning Wire1 (I2C1)...");
        if (scanBus(Wire1, wire1Info)) {
            foundAny = true;
        }
    }
    
    if (wire2Info.initialized) {
        LOG_DEBUG(EventSource::SYSTEM, "Scanning Wire2 (I2C2)...");
        if (scanBus(Wire2, wire2Info)) {
            foundAny = true;
        }
    }
    
    return foundAny;
}

bool I2CManager::scanBus(TwoWire& wire, I2CBusInfo& busInfo) {
    busInfo.deviceCount = 0;
    memset(busInfo.deviceAddresses, 0, sizeof(busInfo.deviceAddresses));
    
    bool foundAny = false;
    uint32_t scanStart = millis();
    
    // Scan all valid I2C addresses (0x08-0x77)
    for (uint8_t address = 0x08; address <= 0x77; address++) {
        // Check for timeout - max 2 seconds per bus
        if (millis() - scanStart > 2000) {
            LOG_WARNING(EventSource::SYSTEM, "  I2C scan timeout at address 0x%02X", address);
            break;
        }
        
        wire.beginTransmission(address);
        uint8_t error = wire.endTransmission();
        
        if (error == 0) {
            // Device found
            busInfo.deviceAddresses[address] = 1;
            busInfo.deviceCount++;
            foundAny = true;
            
            // Identify the device
            I2CDeviceType deviceType = identifyDevice(wire, address);
            
            LOG_INFO(EventSource::SYSTEM, "  Found device at 0x%02X: %s", 
                         address, getDeviceTypeName(deviceType));
        } else if (error == 2) {
            // NACK on address - no device present (normal)
        } else if (error == 3) {
            // NACK on data - device present but not responding properly
            LOG_DEBUG(EventSource::SYSTEM, "  Device at 0x%02X not responding properly", address);
        } else if (error == 4) {
            // Other error - could indicate bus problem
            LOG_WARNING(EventSource::SYSTEM, "  I2C error %d at address 0x%02X", error, address);
            // Don't continue scanning if we get unknown errors
            break;
        }
        
        delay(1); // Small delay between scans
    }
    
    if (!foundAny) {
        LOG_DEBUG(EventSource::SYSTEM, "  No devices found");
    } else {
        LOG_DEBUG(EventSource::SYSTEM, "  Total devices: %d", busInfo.deviceCount);
    }
    
    return foundAny;
}

bool I2CManager::isDevicePresent(TwoWire& wire, uint8_t address) {
    wire.beginTransmission(address);
    return (wire.endTransmission() == 0);
}

I2CDeviceType I2CManager::identifyDevice(TwoWire& wire, uint8_t address) {
    // Identify common devices by their I2C address
    switch (address) {
        case BNO08X_DEFAULT_ADDRESS:
        case BNO08X_ALT_ADDRESS:
            // Could be BNO08x or ADS1115, need further detection
            // For now, assume BNO08x as it's more common in this application
            return I2CDeviceType::BNO08X;
            
        case CMPS14_ADDRESS:
            return I2CDeviceType::CMPS14;
            
        case ADS1115_ADDRESS_GND:
        case ADS1115_ADDRESS_VDD:
            // These addresses are unique to ADS1115
            return I2CDeviceType::ADS1115;
            
        case MCP23017_ADDRESS:
        case MCP23017_ADDRESS + 1:
        case MCP23017_ADDRESS + 2:
        case MCP23017_ADDRESS + 3:
        case MCP23017_ADDRESS + 4:
        case MCP23017_ADDRESS + 5:
        case MCP23017_ADDRESS + 6:
        case MCP23017_ADDRESS + 7:
            return I2CDeviceType::MCP23017;
            
        case PCA9685_ADDRESS:
            return I2CDeviceType::PCA9685;
            
        default:
            return I2CDeviceType::GENERIC;
    }
}

I2CDeviceType I2CManager::getDeviceType(TwoWire& wire, uint8_t address) {
    if (!isDevicePresent(wire, address)) {
        return I2CDeviceType::UNKNOWN;
    }
    
    return identifyDevice(wire, address);
}

const char* I2CManager::getDeviceTypeName(I2CDeviceType type) {
    switch (type) {
        case I2CDeviceType::BNO08X:
            return "BNO08x IMU";
        case I2CDeviceType::CMPS14:
            return "CMPS14 Compass";
        case I2CDeviceType::ADS1115:
            return "ADS1115 ADC";
        case I2CDeviceType::MCP23017:
            return "MCP23017 I/O Expander";
        case I2CDeviceType::PCA9685:
            return "PCA9685 LED Driver";
        case I2CDeviceType::GENERIC:
            return "Generic I2C Device";
        case I2CDeviceType::UNKNOWN:
        default:
            return "Unknown Device";
    }
}

bool I2CManager::setBusSpeed(TwoWire& wire, uint32_t speed) {
    wire.setClock(speed);
    
    // Update our tracking
    if (&wire == &Wire) {
        wire0Info.speed = speed;
    } else if (&wire == &Wire1) {
        wire1Info.speed = speed;
    } else if (&wire == &Wire2) {
        wire2Info.speed = speed;
    }
    
    return true;
}

bool I2CManager::resetBus(TwoWire& wire) {
    // End and restart the bus
    wire.end();
    delay(10);
    
    // Reinitialize at the current speed
    uint32_t speed = I2C_SPEED_FAST;
    if (&wire == &Wire && wire0Info.initialized) {
        speed = wire0Info.speed;
    } else if (&wire == &Wire1 && wire1Info.initialized) {
        speed = wire1Info.speed;
    } else if (&wire == &Wire2 && wire2Info.initialized) {
        speed = wire2Info.speed;
    }
    
    return initializeBus(wire, speed);
}

uint8_t I2CManager::getDeviceCount(TwoWire& wire) {
    if (&wire == &Wire) {
        return wire0Info.deviceCount;
    } else if (&wire == &Wire1) {
        return wire1Info.deviceCount;
    } else if (&wire == &Wire2) {
        return wire2Info.deviceCount;
    }
    return 0;
}

void I2CManager::printI2CStatus() {
    LOG_INFO(EventSource::SYSTEM, "\n=== I2C Manager Status ===");
    
    LOG_INFO(EventSource::SYSTEM, "Initialized buses:");
    int busCount = 0;
    if (wire0Info.initialized) {
        LOG_INFO(EventSource::SYSTEM, " Wire");
        busCount++;
    }
    if (wire1Info.initialized) {
        LOG_INFO(EventSource::SYSTEM, " Wire1");
        busCount++;
    }
    if (wire2Info.initialized) {
        LOG_INFO(EventSource::SYSTEM, " Wire2");
        busCount++;
    }
    if (busCount == 0) {
        LOG_INFO(EventSource::SYSTEM, " NONE");
    }
    
    // Print status for each initialized bus
    if (wire0Info.initialized) {
        printBusStatus(Wire, "Wire (I2C0)");
    }
    if (wire1Info.initialized) {
        printBusStatus(Wire1, "Wire1 (I2C1)");
    }
    if (wire2Info.initialized) {
        printBusStatus(Wire2, "Wire2 (I2C2)");
    }
    
    LOG_INFO(EventSource::SYSTEM, "=============================\n");
}

void I2CManager::printBusStatus(TwoWire& wire, const char* busName) {
    I2CBusInfo* info = nullptr;
    
    if (&wire == &Wire) {
        info = &wire0Info;
    } else if (&wire == &Wire1) {
        info = &wire1Info;
    } else if (&wire == &Wire2) {
        info = &wire2Info;
    }
    
    if (!info || !info->initialized) {
        return;
    }
    
    LOG_INFO(EventSource::SYSTEM, "\n--- %s ---", busName);
    LOG_INFO(EventSource::SYSTEM, "Speed: %d Hz", info->speed);
    LOG_INFO(EventSource::SYSTEM, "Devices: %d", info->deviceCount);
    
    if (info->deviceCount > 0) {
        LOG_INFO(EventSource::SYSTEM, "Addresses:");
        for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
            if (info->deviceAddresses[addr]) {
                I2CDeviceType type = identifyDevice(wire, addr);
                LOG_INFO(EventSource::SYSTEM, "  0x%02X - %s", addr, getDeviceTypeName(type));
            }
        }
    }
}