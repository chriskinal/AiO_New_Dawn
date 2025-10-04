#include "KickoutMonitor.h"
#include "ConfigManager.h"
#include "ADProcessor.h"
#include "EncoderProcessor.h"
#include "HardwareManager.h"  // For KICKOUT_D_PIN and KICKOUT_A_PIN
#include "EventLogger.h"
#include "TurnSensorTypes.h"
#include "KeyaCANDriver.h"

// External global objects
extern ConfigManager configManager;
extern ADProcessor adProcessor;
extern EncoderProcessor* encoderProcessor;

// External network function
extern void sendUDPbytes(uint8_t* data, int len);

// Singleton instance
KickoutMonitor* KickoutMonitor::instance = nullptr;

// No ISR needed - we'll poll the encoder pin

KickoutMonitor::KickoutMonitor() : 
    configMgr(nullptr),
    adProcessor(nullptr),
    motorDriver(nullptr),
    encoderProc(nullptr),
    encoderPulseCount(0),
    lastPulseCheck(0),
    lastPulseCount(0),
    lastEncoderState(false),
    lastPressureReading(0),
    lastCurrentReading(0),
    currentHighStartTime(0),
    kickoutActive(false),
    kickoutReason(NONE),
    kickoutTime(0) {
}

KickoutMonitor::~KickoutMonitor() {
    instance = nullptr;
}

KickoutMonitor* KickoutMonitor::getInstance() {
    if (instance == nullptr) {
        instance = new KickoutMonitor();
    }
    return instance;
}

bool KickoutMonitor::init(MotorDriverInterface* driver) {
    LOG_INFO(EventSource::AUTOSTEER, "Initializing KickoutMonitor");
    
    // Get external dependencies
    configMgr = &configManager;
    this->adProcessor = &::adProcessor;  // Use global scope
    motorDriver = driver;
    encoderProc = encoderProcessor;
    
    if (!configMgr || !adProcessor) {
        LOG_ERROR(EventSource::AUTOSTEER, "KickoutMonitor - Missing dependencies");
        return false;
    }
    
    // Log which sensors are relevant for this motor type
    if (driver) {
        if (driver->getType() == MotorDriverType::KEYA_CAN) {
            LOG_INFO(EventSource::AUTOSTEER, "Keya motor detected - external sensors (encoder/pressure/current) will be ignored");
            LOG_INFO(EventSource::AUTOSTEER, "Keya uses internal slip detection via CAN");
        } else {
            LOG_INFO(EventSource::AUTOSTEER, "PWM/Hydraulic motor - external sensors active if configured");
        }
    }
    
    // Encoder pin initialization is handled by EncoderProcessor
    // KickoutMonitor just reads the processed encoder data
    
    LOG_INFO(EventSource::AUTOSTEER, "KickoutMonitor initialized successfully");
    return true;
}

void KickoutMonitor::process() {
    // Get encoder processor if we don't have it yet
    if (!encoderProc) {
        encoderProc = encoderProcessor;  // Try to get the global instance
    }
    
    // Determine motor type for sensor relevance
    MotorDriverType motorType = motorDriver ? motorDriver->getType() : MotorDriverType::NONE;
    bool isKeyaMotor = (motorType == MotorDriverType::KEYA_CAN ||
                       motorType == MotorDriverType::KEYA_SERIAL ||
                       motorType == MotorDriverType::TRACTOR_CAN);  // TRACTOR_CAN handles its own kickout
    
    // Debug motor type and sensor configuration
    static uint32_t lastDebugTime = 0;
    if (millis() - lastDebugTime > 10000) {  // Every 10 seconds
        lastDebugTime = millis();
        if (motorDriver) {
            const char* motorTypeName = "Unknown";
            switch (motorDriver->getType()) {
                case MotorDriverType::KEYA_CAN: motorTypeName = "KEYA_CAN"; break;
                case MotorDriverType::KEYA_SERIAL: motorTypeName = "KEYA_SERIAL"; break;
                case MotorDriverType::TRACTOR_CAN: motorTypeName = "TRACTOR_CAN"; break;
                case MotorDriverType::DANFOSS: motorTypeName = "DANFOSS"; break;
                case MotorDriverType::DRV8701: motorTypeName = "DRV8701"; break;
                case MotorDriverType::CYTRON_MD30C: motorTypeName = "CYTRON_MD30C"; break;
                case MotorDriverType::IBT2: motorTypeName = "IBT2"; break;
                case MotorDriverType::GENERIC_PWM: motorTypeName = "GENERIC_PWM"; break;
                default: break;
            }
            LOG_DEBUG(EventSource::AUTOSTEER, "KickoutMonitor: Motor=%s, isKeya=%d, Encoder=%d, Pressure=%d, Current=%d",
                     motorTypeName, isKeyaMotor,
                     configMgr->getShaftEncoder(),
                     configMgr->getPressureSensor(),
                     configMgr->getCurrentSensor());
        }
    }
    
    // Only read sensors relevant to the motor type
    if (!isKeyaMotor) {
        // Get encoder pulse count from EncoderProcessor (not relevant for Keya)
        if (encoderProc && encoderProc->isEnabled()) {
            int32_t newCount = encoderProc->getPulseCount();
            
            // Log significant changes in encoder count  
            static int32_t lastLoggedCount = 0;
            
            if (abs(newCount - lastLoggedCount) >= 10) {  // Log every 10 counts
                uint16_t maxPulses = configMgr->getPulseCountMax();
                LOG_DEBUG(EventSource::AUTOSTEER, "Encoder count: %d (max: %u)", newCount, maxPulses);
                lastLoggedCount = newCount;
            }
            
            encoderPulseCount = newCount;
        }
        
        // Get pressure and current readings from ADProcessor (not relevant for Keya)
        lastPressureReading = (uint16_t)adProcessor->getPressureReading();  // Filtered value (0-255)
        lastCurrentReading = adProcessor->getMotorCurrent();
    }
    
    // PGN250 is now sent by SimpleScheduler at 10Hz via sendPGN250()
    
    // Check kickout conditions based on motor type
    if (!kickoutActive) {
        // Not currently in kickout - check if we should trigger one
        
        // Debug JD PWM status periodically
        static uint32_t lastJDDebug = 0;
        if (configMgr->getJDPWMEnabled() && (millis() - lastJDDebug > 2000)) {
            LOG_DEBUG(EventSource::AUTOSTEER, "JD_PWM_KICKOUT: enabled=%d, motion_as_pressure=%u (AOG handles threshold), isKeyaMotor=%d",
                      configMgr->getJDPWMEnabled(), lastPressureReading, isKeyaMotor);
            lastJDDebug = millis();
        }
        
        // External sensor checks - NOT for Keya motors
        if (!isKeyaMotor && configMgr->getShaftEncoder()) {
            // Encoder is enabled for non-Keya motors
            if (checkEncoderKickout()) {
            kickoutActive = true;
            kickoutReason = ENCODER_OVERSPEED;
            kickoutTime = millis();
            
            LOG_WARNING(EventSource::AUTOSTEER, "KICKOUT: %s", getReasonString());
            
                // Notify motor driver
                if (motorDriver) {
                    motorDriver->handleKickout(KickoutType::WHEEL_ENCODER, encoderPulseCount);
                }
            }
        }
        else if (!isKeyaMotor && configMgr->getJDPWMEnabled() && checkPressureKickout()) {
            // JD PWM mode uses pressure kickout mechanism since motion is sent as pressure
            kickoutActive = true;
            kickoutReason = JD_PWM_MOTION;
            kickoutTime = millis();
            
            LOG_WARNING(EventSource::AUTOSTEER, "JD_PWM_KICKOUT: *** KICKOUT ACTIVATED ***");
            LOG_WARNING(EventSource::AUTOSTEER, "KICKOUT: %s", getReasonString());
            
            // Notify motor driver using pressure sensor kickout type for compatibility
            if (motorDriver) {
                motorDriver->handleKickout(KickoutType::PRESSURE_SENSOR, lastPressureReading);
            }
        }
        else if (!isKeyaMotor && configMgr->getPressureSensor() && !configMgr->getJDPWMEnabled() && checkPressureKickout()) {
            LOG_DEBUG(EventSource::AUTOSTEER, "PRESSURE_KICKOUT: Regular pressure mode (JD PWM disabled)");
            kickoutActive = true;
            kickoutReason = PRESSURE_HIGH;
            kickoutTime = millis();
            
            LOG_WARNING(EventSource::AUTOSTEER, "KICKOUT: %s", getReasonString());
            
            // Notify motor driver
            if (motorDriver) {
                float pressureVolts = (lastPressureReading * 3.3f) / 4095.0f;
                motorDriver->handleKickout(KickoutType::PRESSURE_SENSOR, pressureVolts);
            }
        }
        else if (!isKeyaMotor && configMgr->getCurrentSensor() && checkCurrentKickout()) {
            kickoutActive = true;
            kickoutReason = CURRENT_HIGH;
            kickoutTime = millis();
            
            LOG_WARNING(EventSource::AUTOSTEER, "KICKOUT: %s", getReasonString());
            
            // Notify motor driver with current in amps
            if (motorDriver) {
                float currentAmps = motorDriver->getCurrentDraw();
                motorDriver->handleKickout(KickoutType::CURRENT_SENSOR, currentAmps);
            }
        }
        else if (isKeyaMotor && checkMotorSlipOverCurrentKickout()) {
            kickoutActive = true;
            // Determine specific reason based on motor type
            if (motorDriver->getType() == MotorDriverType::KEYA_CAN) {
                kickoutReason = KEYA_SLIP;  // checkMotorSlip handles both slip and errors
            } else {
                kickoutReason = MOTOR_SLIP;
            }
            kickoutTime = millis();
            
            LOG_WARNING(EventSource::AUTOSTEER, "KICKOUT: %s", getReasonString());
            
            // Motor already knows about its own slip condition
        }
    } else {
        // Currently in kickout - check if conditions have returned to normal
        bool conditionsNormal = true;
        
        // Check the condition that caused the kickout
        switch (kickoutReason) {
            case ENCODER_OVERSPEED:
                if (!isKeyaMotor && configMgr->getShaftEncoder() && checkEncoderKickout()) {
                    conditionsNormal = false;
                }
                break;
                
            case JD_PWM_MOTION:
                if (!isKeyaMotor && configMgr->getJDPWMEnabled() && checkJDPWMKickout()) {
                    conditionsNormal = false;
                }
                break;
                
            case PRESSURE_HIGH:
                if (!isKeyaMotor && configMgr->getPressureSensor() && !configMgr->getJDPWMEnabled() && checkPressureKickout()) {
                    conditionsNormal = false;
                }
                break;
                
            case CURRENT_HIGH:
                if (!isKeyaMotor && configMgr->getCurrentSensor() && checkCurrentKickout()) {
                    conditionsNormal = false;
                }
                break;
                
            case MOTOR_SLIP:
            case KEYA_SLIP:
            case KEYA_ERROR:
                if (isKeyaMotor && checkMotorSlipOverCurrentKickout()) {
                    conditionsNormal = false;
                }
                break;
                
            default:
                break;
        }
        
        // Auto-clear if conditions are back to normal
        if (conditionsNormal) {
            clearKickout();
        }
    }
}

bool KickoutMonitor::checkEncoderKickout() {
    // Get max pulse count from config - this is the absolute count threshold
    uint16_t maxPulses = configMgr->getPulseCountMax();
    
    // Get absolute value of encoder count (handles both directions)
    // encoderPulseCount is int32_t from EncoderProcessor
    uint32_t absoluteCount = abs((int32_t)encoderPulseCount);
    
    // Check if we exceeded the threshold
    if (absoluteCount > maxPulses) {
        // Only log when first detecting kickout (not already active) or every 1 second
        static uint32_t lastLogTime = 0;
        uint32_t now = millis();
        
        if (!kickoutActive || (now - lastLogTime >= 1000)) {
            LOG_DEBUG(EventSource::AUTOSTEER, "Encoder kickout: count=%d (max %u)", 
                          encoderPulseCount, maxPulses);
            lastLogTime = now;
        }
        return true;
    }
    
    return false;
}

bool KickoutMonitor::checkPressureKickout() {
    // Use the already-read pressure value from process()
    // This is the filtered/scaled value (0-255 range)
    
    // Get pressure threshold from config
    uint8_t threshold = configMgr->getPulseCountMax();
    
    if (lastPressureReading > threshold) {
        // Only log when first detecting kickout (not already active) or every 1 second
        static uint32_t lastLogTime = 0;
        uint32_t now = millis();
        
        if (!kickoutActive || (now - lastLogTime >= 1000)) {
            LOG_DEBUG(EventSource::AUTOSTEER, "Pressure high: %u (threshold %u)", 
                          lastPressureReading, threshold);
            lastLogTime = now;
        }
        return true;
    }
    
    return false;
}

bool KickoutMonitor::checkCurrentKickout() {
    // Read current sensor
    lastCurrentReading = adProcessor->getMotorCurrent();
    
    // Get threshold from config (0-255, same scale as PGN250)
    uint8_t thresholdPercent = configMgr->getCurrentThreshold();
    
    
    // Convert threshold to raw ADC counts to match what ADProcessor returns
    // ADProcessor returns counts after baseline subtraction
    // Use same scale as PGN250 for consistency: 1680 counts = 255 (100%)
    uint16_t thresholdCounts = (thresholdPercent * 1680) / 255;
    
    // Add 10% hysteresis to prevent triggering right at threshold
    // This helps with direction changes where current briefly spikes
    uint16_t thresholdWithHysteresis = thresholdCounts + (thresholdCounts / 10);
    
    if (lastCurrentReading > thresholdWithHysteresis) {
        // Current is above threshold - check if it's been high long enough
        uint32_t now = millis();
        
        if (currentHighStartTime == 0) {
            // First time seeing high current - start timing
            currentHighStartTime = now;
            LOG_INFO(EventSource::AUTOSTEER, "Current high detected: %u counts (%.1f%%) - monitoring for %.1f seconds", 
                      lastCurrentReading, (lastCurrentReading * 100.0f) / 1680.0f, CURRENT_SPIKE_FILTER_MS / 1000.0f);
            return false; // Don't kickout yet
        }
        
        // Check if current has been high long enough
        uint32_t highDuration = now - currentHighStartTime;
        if (highDuration >= CURRENT_SPIKE_FILTER_MS) {
            // Current has been high for long enough - trigger kickout
            if (!kickoutActive) {
                LOG_INFO(EventSource::AUTOSTEER, "Current kickout after %ums: reading=%u counts > threshold=%u counts (+10%% = %u) (config=%.1f%%)", 
                              highDuration, lastCurrentReading, thresholdCounts, thresholdWithHysteresis, 
                              (thresholdPercent * 100.0f) / 255.0f);
            }
            currentHighStartTime = 0; // Reset for next time
            return true;
        }
        
        // Still waiting for duration to elapse
        return false;
    } else {
        // Current is below threshold - reset timer if it was running
        if (currentHighStartTime != 0) {
            uint32_t duration = millis() - currentHighStartTime;
            LOG_INFO(EventSource::AUTOSTEER, "Current returned to normal after %ums - no kickout", duration);
            currentHighStartTime = 0;
        }
        return false;
    }
}

bool KickoutMonitor::checkMotorSlipOverCurrentKickout() {
    // Check if motor driver reports slip condition
    if (!motorDriver) {
        return false;
    }

    // Check motor type
    MotorDriverType motorType = motorDriver->getType();

    if (motorType == MotorDriverType::KEYA_CAN) {
        // Cast to KeyaCANDriver to access slip detection
        KeyaCANDriver* keyaDriver = static_cast<KeyaCANDriver*>(motorDriver);
        if (keyaDriver->checkMotorSlip()) {
            LOG_WARNING(EventSource::AUTOSTEER, "KICKOUT: Keya motor slip detected");
            return true;
        }
        float current = keyaDriver->getKeyaCurrentX32();      // Get current from Keya x32, as only 1A resolution
        uint8_t threshold = configMgr->getCurrentThreshold(); // Get threshold from config (0-255, same scale as PGN250)
        //Serial.print("Keya current: "); Serial.print(current); Serial.print(" Threshold: "); Serial.println(threshold);
        if (current > threshold) { 
            LOG_WARNING(EventSource::AUTOSTEER, "KICKOUT: Keya motor current (A) %.1f value (Ax32): %.f over threshold %u",
                        current/32, current, threshold);
            return true;
        }
    }
    else if (motorType == MotorDriverType::TRACTOR_CAN) {
        // TRACTOR_CAN handles its own internal kickout
        // No slip detection needed here
        return false;
    }

    // For other motor types, could check position feedback vs commanded
    // This would require additional implementation

    return false;
}

bool KickoutMonitor::checkJDPWMKickout() {
    // In JD PWM mode, the motion value is already sent as pressure data
    // AgOpenGPS will handle the kickout through its pressure threshold
    // This function now only exists for logging purposes
    
    if (configMgr->getJDPWMEnabled()) {
        // Debug output
        static uint32_t lastDebugTime = 0;
        uint32_t now = millis();
        if (now - lastDebugTime > 1000) { // Debug every second
            LOG_DEBUG(EventSource::AUTOSTEER, "JD_PWM_CHECK: motion_as_pressure=%u (AOG handles threshold)",
                      lastPressureReading);
            lastDebugTime = now;
        }
    }
    
    // Always return false - let pressure kickout handle it
    return false;
}

void KickoutMonitor::clearKickout() {
    if (kickoutActive) {
        LOG_INFO(EventSource::AUTOSTEER, "KICKOUT: Cleared after %lu ms", 
                      millis() - kickoutTime);
    }
    kickoutActive = false;
    kickoutReason = NONE;
    kickoutTime = 0;
    
    // Reset current spike timer
    currentHighStartTime = 0;
    
    // Reset encoder count via EncoderProcessor
    if (encoderProc) {
        encoderProc->resetPulseCount();
    }
    encoderPulseCount = 0;
    lastPulseCount = 0;
}


const char* KickoutMonitor::getReasonString() const {
    switch (kickoutReason) {
        case NONE: return "None";
        case ENCODER_OVERSPEED: return "Encoder Overspeed";
        case PRESSURE_HIGH: return "Pressure High";
        case CURRENT_HIGH: return "Current High";
        case MOTOR_SLIP: return "Motor Slip";
        case KEYA_SLIP: return "Keya Motor Slip";
        case KEYA_ERROR: return "Keya Motor Error";
        case JD_PWM_MOTION: return "JD PWM Motion Detected";
        default: return "Unknown";
    }
}

uint8_t KickoutMonitor::getTurnSensorReading() const {
    // Determine which turn sensor type is active
    TurnSensorType sensorType = TurnSensorType::NONE;
    
    bool hasEncoder = configMgr->getShaftEncoder();
    bool hasPressure = configMgr->getPressureSensor();
    bool hasCurrent = configMgr->getCurrentSensor();
    bool hasJDPWM = configMgr->getJDPWMEnabled();
    
    if (hasEncoder) {
        sensorType = TurnSensorType::ENCODER;
    } else if (hasJDPWM) {
        sensorType = TurnSensorType::JD_PWM;
    } else if (hasPressure) {
        sensorType = TurnSensorType::PRESSURE;
    } else if (hasCurrent) {
        sensorType = TurnSensorType::CURRENT;
    }
    
    
    // Return the appropriate sensor value
    switch (sensorType) {
        case TurnSensorType::ENCODER:
            return (uint8_t)min(encoderPulseCount, 255);
            
        case TurnSensorType::JD_PWM:
            return (uint8_t)lastPressureReading;  // JD PWM motion value is stored in pressure reading
            
        case TurnSensorType::PRESSURE:
            return (uint8_t)lastPressureReading;
            
        case TurnSensorType::CURRENT: {
            // Scale current reading to percentage of 8.4A max
            // ~150-260 counts per amp, so 8.4A would be ~1260-2184 counts
            // Scale to 0-255 where 255 = 8.4A (100%)
            // Using conservative estimate: 200 counts/amp * 8.4A = 1680 counts for full scale
            float scaledCurrent = (lastCurrentReading * 255.0f) / 1680.0f;
            uint8_t result = (uint8_t)min(scaledCurrent, 255.0f);
            
            
            return result;
        }
            
        case TurnSensorType::NONE:
        default:
            return 0;
    }
}

void KickoutMonitor::sendPGN250() {
    // Update sensor readings right before sending to ensure fresh data
    bool isKeyaMotor = (motorDriver && motorDriver->getType() == MotorDriverType::KEYA_CAN);
    if (!isKeyaMotor && adProcessor) {
        lastPressureReading = (uint16_t)adProcessor->getPressureReading();
        lastCurrentReading = adProcessor->getMotorCurrent();
    }

    // PGN 250 - Turn Sensor Data to AgOpenGPS
    // Format per NG-V6: {header, source, pgn, length, sensorValue, 0, 0, 0, 0, 0, 0, 0, checksum}
    uint8_t pgn250[] = {
        0x80, 0x81,      // Header
        126,             // Source: Steer module (0x7E)
        0xFA,            // PGN: 250
        8,               // Length
        0,               // Sensor value (byte 5)
        0,               // Reserved
        0,               // Reserved
        0,               // Reserved
        0,               // Reserved
        0,               // Reserved
        0,               // Reserved
        0,               // Reserved
        0                // Checksum
    };
    
    // Get the sensor reading based on active turn sensor type
    pgn250[5] = getTurnSensorReading();
    
    // Calculate checksum
    uint8_t checksum = 0;
    for (int i = 2; i < sizeof(pgn250) - 1; i++) {
        checksum += pgn250[i];
    }
    pgn250[sizeof(pgn250) - 1] = checksum;
    
    // Send via UDP
    sendUDPbytes(pgn250, sizeof(pgn250));
    
}