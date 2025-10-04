// MotorDriverManager.h - Unified motor driver detection and creation
#ifndef MOTOR_DRIVER_MANAGER_H
#define MOTOR_DRIVER_MANAGER_H

#include <Arduino.h>
#include "MotorDriverInterface.h"
#include "PWMMotorDriver.h"
#include "TractorCANDriver.h"
// #include "KeyaCANDriver.h"  // Now handled by TractorCANDriver
#include "KeyaSerialDriver.h"
#include "DanfossMotorDriver.h"
#include "HardwareManager.h"
#include "CANManager.h"
#include "EventLogger.h"
#include "ConfigManager.h"

// Motor driver configuration values from PGN251 Byte 8
enum class MotorDriverConfig : uint8_t {
    DRV8701_WHEEL_ENCODER = 0x00,
    DANFOSS_WHEEL_ENCODER = 0x01,
    DRV8701_PRESSURE_SENSOR = 0x02,
    DANFOSS_PRESSURE_SENSOR = 0x03,
    DRV8701_CURRENT_SENSOR = 0x04
};

class MotorDriverManager {
private:
    static MotorDriverManager* instance;
    
    // Detection state
    MotorDriverType detectedType = MotorDriverType::NONE;
    KickoutType kickoutType = KickoutType::NONE;
    bool detectionComplete = false;
    uint32_t detectionStartTime = 0;
    bool keyaSerialChecked = false;
    
    // Configuration from EEPROM
    uint8_t motorConfigByte = 0x00;  // From PGN251 Byte 8
    
    // Private constructor for singleton
    MotorDriverManager() {}
    
    // Internal detection methods
    void readMotorConfig();
    bool performDetection(bool keyaHeartbeatDetected);
    bool probeKeyaSerial();
    
public:
    static MotorDriverManager* getInstance() {
        if (instance == nullptr) {
            instance = new MotorDriverManager();
        }
        return instance;
    }
    
    // Initialize detection process
    void init();
    
    // Detect and create motor driver
    MotorDriverInterface* detectAndCreateMotorDriver(HardwareManager* hwMgr, CANManager* canMgr);
    
    // Create motor driver based on specific type
    static MotorDriverInterface* createMotorDriver(MotorDriverType type, 
                                                   HardwareManager* hwMgr,
                                                   CANManager* canMgr);
    
    // Update motor configuration from PGN251
    void updateMotorConfig(uint8_t configByte);
    
    // Get detection results
    MotorDriverType getDetectedType() const { return detectedType; }
    KickoutType getKickoutType() const { return kickoutType; }
    bool isDetectionComplete() const { return detectionComplete; }
};

#endif // MOTOR_DRIVER_MANAGER_H