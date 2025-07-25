// MotorDriverManager.cpp - Implementation
#include "MotorDriverManager.h"

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
    
    // Wait for detection to complete (up to 2 seconds for Keya)
    uint32_t startTime = millis();
    bool keyaChecked = false;
    
    while (!detectionComplete && (millis() - startTime) < 2100) {
        // Check for Keya heartbeat
        bool keyaDetected = canMgr && canMgr->isKeyaDetected();
        if (keyaDetected && !keyaChecked) {
            LOG_INFO(EventSource::AUTOSTEER, "Keya CAN heartbeat detected");
            keyaChecked = true;
        }
        performDetection(keyaDetected);
        delay(10);
    }
    
    // Force detection completion if timeout
    if (!detectionComplete) {
        LOG_DEBUG(EventSource::AUTOSTEER, "Detection timeout - using configured/default driver");
        performDetection(false);
    }
    
    const char* driverName = "Unknown";
    switch (detectedType) {
        case MotorDriverType::KEYA_CAN:
            driverName = "Keya CAN Motor";
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
            
        case MotorDriverType::KEYA_CAN:
            return new KeyaCANDriver();
            
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
    
    // Priority 1: Check for Keya CAN heartbeat
    if (keyaHeartbeatDetected) {
        detectedType = MotorDriverType::KEYA_CAN;
        kickoutType = KickoutType::NONE;  // Keya uses motor slip detection
        LOG_INFO(EventSource::AUTOSTEER, "Detected Keya CAN motor via heartbeat");
        detectionComplete = true;
        return true;
    }
    
    // Wait up to 2 seconds for Keya heartbeat
    if (millis() - detectionStartTime < 2000) {
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