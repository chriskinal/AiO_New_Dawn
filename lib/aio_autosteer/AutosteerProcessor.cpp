#include "AutosteerProcessor.h"
#include "PGNProcessor.h"
#include "ADProcessor.h"
#include "EncoderProcessor.h"
#include "MotorDriverInterface.h"
#include "KeyaCANDriver.h"
#include "TractorCANDriver.h"
#include "ConfigManager.h"
#include "LEDManagerFSM.h"
#include "EventLogger.h"
#include "QNetworkBase.h"
#include "HardwareManager.h"
#include "WheelAngleFusion.h"
#include "MotorDriverManager.h"
#include "KickoutMonitor.h"
#include <cmath>  // For sin() function

// External network function
extern void sendUDPbytes(uint8_t* data, int len);

// External objects and pointers
extern ConfigManager configManager;
// extern LEDManager ledManager; // Using FSM version now
extern ADProcessor adProcessor;
extern MotorDriverInterface* motorPTR;
extern WheelAngleFusion* wheelAngleFusionPtr;

// No longer need external network config - using ConfigManager
// extern struct NetworkConfig netConfig;

// Global pointer definition
AutosteerProcessor* autosteerPTR = nullptr;

// Singleton instance
AutosteerProcessor* AutosteerProcessor::instance = nullptr;

AutosteerProcessor::AutosteerProcessor() {
}

AutosteerProcessor* AutosteerProcessor::getInstance() {
    if (instance == nullptr) {
        instance = new AutosteerProcessor();
    }
    return instance;
}

bool AutosteerProcessor::init() {
    // Check if already initialized to prevent duplicate PGN registrations
    static bool initialized = false;
    if (initialized) {
        LOG_DEBUG(EventSource::AUTOSTEER, "AutosteerProcessor already initialized, updating VWAS only");
        
        // Just update VWAS if needed
        if (configManager.getINSUseFusion() && !wheelAngleFusionPtr) {
            initializeFusion();
        }
        return true;
    }
    
    LOG_INFO(EventSource::AUTOSTEER, "Initializing AutosteerProcessor");
    
    // Make sure instance is set
    instance = this;
    autosteerPTR = this;  // Also set global pointer
    
    // Initialize motor config tracking from EEPROM values
    previousMotorConfig = configManager.getMotorDriverConfig();
    previousCytronDriver = configManager.getCytronDriver() ? 1 : 0;
    motorConfigInitialized = true;
    LOG_INFO(EventSource::AUTOSTEER, "Motor config tracking initialized: Config=0x%02X, Cytron=%d", 
             previousMotorConfig, previousCytronDriver);
    
    // Initialize Virtual WAS if enabled
    if (configManager.getINSUseFusion()) {
        initializeFusion();
    }
    
    // Initialize button pin
    pinMode(2, INPUT_PULLUP);
    LOG_DEBUG(EventSource::AUTOSTEER, "Button pin 2 configured as INPUT_PULLUP");
    
    // Initialize LOCK output pin (SLEEP_PIN = 4)
    pinMode(4, OUTPUT);
    digitalWrite(4, LOW);  // Start with LOCK OFF (like NG-V6)
    LOG_DEBUG(EventSource::AUTOSTEER, "LOCK output pin 4 configured as OUTPUT, initially LOW");
    
    // Load steer settings from EEPROM
    configManager.loadSteerSettings();
    
    // Update ADProcessor with loaded values
    adProcessor.setWASOffset(configManager.getWasOffset());
    adProcessor.setWASCountsPerDegree(configManager.getSteerSensorCounts());
    
    LOG_INFO(EventSource::AUTOSTEER, "Loaded steer settings from EEPROM: offset=%d, CPD=%d, highPWM=%d", 
             configManager.getWasOffset(), configManager.getSteerSensorCounts(), configManager.getHighPWM());
    
    // PID functionality is now integrated directly in updateMotorControl()
    
    // Register PGN handlers with PGNProcessor
    if (PGNProcessor::instance) {
        LOG_DEBUG(EventSource::AUTOSTEER, "Registering PGN callbacks...");
        
        // Register for broadcast messages (PGN 200, 202)
        bool regBroadcast = PGNProcessor::instance->registerBroadcastCallback(handlePGNStatic, "AutosteerHandler");
        
        // Register for PGN 251 (Steer Config)
        bool reg251 = PGNProcessor::instance->registerCallback(251, handlePGNStatic, "AutosteerHandler");
        
        // Register for PGN 252 (Steer Settings)
        bool reg252 = PGNProcessor::instance->registerCallback(252, handlePGNStatic, "AutosteerHandler");
        
        // Register for PGN 254 (Steer Data with button)
        bool reg254 = PGNProcessor::instance->registerCallback(254, handlePGNStatic, "AutosteerHandler");
            
        LOG_DEBUG(EventSource::AUTOSTEER, "PGN registrations: Broadcast=%d, 251=%d, 252=%d, 254=%d", regBroadcast, reg251, reg252, reg254);
    } else {
        LOG_ERROR(EventSource::AUTOSTEER, "PGNProcessor not initialized!");
        return false;
    }
    
    // Initialize KickoutMonitor
    kickoutMonitor = KickoutMonitor::getInstance();
    if (kickoutMonitor) {
        kickoutMonitor->init(motorPTR);
        LOG_INFO(EventSource::AUTOSTEER, "KickoutMonitor initialized");
    } else {
        LOG_ERROR(EventSource::AUTOSTEER, "Failed to initialize KickoutMonitor");
    }
    
    LOG_INFO(EventSource::AUTOSTEER, "AutosteerProcessor initialized successfully");
    initialized = true;  // Mark as initialized to prevent duplicate PGN registrations
    return true;
}

void AutosteerProcessor::initializeFusion() {
    LOG_INFO(EventSource::AUTOSTEER, "Virtual WAS enabled - initializing VWAS system");
    
    // Create fusion instance if not already created
    if (!wheelAngleFusionPtr) {
        wheelAngleFusionPtr = new WheelAngleFusion();
    }
    
    // Initialize with sensor interfaces
    // Note: We need the Keya driver, GNSS, and IMU processors
    KeyaCANDriver* keyaDriver = nullptr;
    if (motorPTR && motorPTR->getType() == MotorDriverType::KEYA_CAN) {
        keyaDriver = static_cast<KeyaCANDriver*>(motorPTR);
    }
    
    extern GNSSProcessor* gnssProcessorPtr;
    extern IMUProcessor imuProcessor;  // Global instance, not pointer
    
    if (wheelAngleFusionPtr->init(keyaDriver, gnssProcessorPtr, &imuProcessor)) {
        LOG_INFO(EventSource::AUTOSTEER, "Virtual WAS (VWAS) initialized successfully");
        
        // Load fusion config from saved values
        WheelAngleFusion::Config fusionConfig = wheelAngleFusionPtr->getConfig();
        fusionConfig.wheelbase = 2.5f;  // Default wheelbase - not yet configurable
        fusionConfig.countsPerDegree = 100.0f;  // Default counts - not yet configurable
        wheelAngleFusionPtr->setConfig(fusionConfig);
    } else {
        LOG_ERROR(EventSource::AUTOSTEER, "Failed to initialize Virtual WAS");
        configManager.setINSUseFusion(false);  // Disable VWAS
        delete wheelAngleFusionPtr;
        wheelAngleFusionPtr = nullptr;
    }
}

void AutosteerProcessor::process() {
    // === 100Hz AUTOSTEER LOOP (called by SimpleScheduler) ===
    
    // Track link state for down detection
    static bool previousLinkState = true;
    bool currentLinkState = QNetworkBase::isConnected();
    
    if (previousLinkState && !currentLinkState) {
        // Link just went DOWN
        LOG_WARNING(EventSource::AUTOSTEER, "Motor disabled - ethernet link down");
        linkWasDown = true;  // Set flag for handleSteerData
    }
    previousLinkState = currentLinkState;
    
    // Update Virtual WAS if enabled
    if (configManager.getINSUseFusion()) {
        // Initialize fusion if enabled but not yet initialized
        if (!wheelAngleFusionPtr) {
            initializeFusion();
        }

        // Update fusion if initialized successfully
        if (wheelAngleFusionPtr) {
            float dt = 10.0f / 1000.0f;  // 10ms = 0.01 seconds (100Hz from SimpleScheduler)
            wheelAngleFusionPtr->update(dt);

            // Periodic sensor fusion mode logging (every 60 seconds)
            static uint32_t lastFusionLog = 0;
            if (millis() - lastFusionLog > 60000) {
                lastFusionLog = millis();
                LOG_INFO(EventSource::AUTOSTEER, "Sensor Fusion Mode: %s",
                         wheelAngleFusionPtr->getFusionMode());
            }
        }
    }

    // === BUTTON/SWITCH LOGIC ===
    // Static variable for Massey/Fendt/CaseIH button state tracking (needs to persist across cycles)
    static bool lastMasseyEngageState = false;
    static bool lastFendtButtonState = false;
    static bool lastCaseIHEngageState = false;
    static bool lastCATMTEngageState = false;
    static bool lastClaasEngageState = false;
    static bool lastJcbEngageState = false;
    static bool lastLindnerEngageState = false;

    // Debug: log button/switch config periodically
    static uint32_t lastConfigLog = 0;
    if (millis() - lastConfigLog > 5000) {
        lastConfigLog = millis();
        LOG_DEBUG(EventSource::AUTOSTEER, "Button config: button=%d, switch=%d",
                  configManager.getSteerButton(), configManager.getSteerSwitch());
    }

    if (configManager.getSteerButton() || configManager.getSteerSwitch()) {
        if (configManager.getSteerButton()) {
            // BUTTON MODE - Toggle on press
            static bool lastButtonReading = HIGH;
            bool buttonReading = adProcessor.isSteerSwitchOn() ? LOW : HIGH;  // Convert to active low

            // Also check tractor-specific buttons if using TractorCANDriver
            bool masseyEngagePressed = false;
            bool fendtButtonPressed = false;
            bool caseIHEngagePressed = false;
            bool catMTEngagePressed = false;
            bool claasEngagePressed = false;
            bool jcbEngagePressed = false;
            bool lindnerEngagePressed = false;

            if (motorPTR && motorPTR->getType() == MotorDriverType::TRACTOR_CAN) {
                TractorCANDriver* tractorCAN = static_cast<TractorCANDriver*>(motorPTR);

                // Check Massey button
                bool currentMasseyEngage = tractorCAN->isEngageButtonPressed();
                // Detect falling edge of Massey engage button (release)
                if (!currentMasseyEngage && lastMasseyEngageState) {
                    masseyEngagePressed = true;
                }
                lastMasseyEngageState = currentMasseyEngage;

                // Check Fendt button
                bool currentFendtButton = tractorCAN->isFendtButtonPressed();
                // Detect falling edge of Fendt button (release)
                if (!currentFendtButton && lastFendtButtonState) {
                    fendtButtonPressed = true;
                }
                lastFendtButtonState = currentFendtButton;

                // Check Case IH engage state
                bool currentCaseIHEngage = tractorCAN->isCaseIHEngaged();
                // Detect rising edge of Case IH engage (OFF to ON transition)
                if (currentCaseIHEngage && !lastCaseIHEngageState) {
                    caseIHEngagePressed = true;
                }
                lastCaseIHEngageState = currentCaseIHEngage;

                // Check CAT MT engage state
                bool currentCATMTEngage = tractorCAN->isCATMTEngaged();
                // Detect rising edge of CAT MT engage (OFF to ON transition)
                if (currentCATMTEngage && !lastCATMTEngageState) {
                    catMTEngagePressed = true;
                }
                lastCATMTEngageState = currentCATMTEngage;

                // Check CLAAS engage state
                bool currentClaasEngage = tractorCAN->isClaasEngaged();
                // Detect rising edge of CLAAS engage (OFF to ON transition)
                if (currentClaasEngage && !lastClaasEngageState) {
                    claasEngagePressed = true;
                }
                lastClaasEngageState = currentClaasEngage;

                // Check JCB engage state
                bool currentJcbEngage = tractorCAN->isJcbEngaged();
                // Detect rising edge of JCB engage (OFF to ON transition)
                if (currentJcbEngage && !lastJcbEngageState) {
                    jcbEngagePressed = true;
                }
                lastJcbEngageState = currentJcbEngage;

                // Check Lindner engage state
                bool currentLindnerEngage = tractorCAN->isLindnerEngaged();
                // Detect rising edge of Lindner engage (OFF to ON transition)
                if (currentLindnerEngage && !lastLindnerEngageState) {
                    lindnerEngagePressed = true;
                }
                lastLindnerEngageState = currentLindnerEngage;
            }

            // Check if any button was pressed
            if ((buttonReading == LOW && lastButtonReading == HIGH) || masseyEngagePressed ||
                fendtButtonPressed || caseIHEngagePressed || catMTEngagePressed || claasEngagePressed || jcbEngagePressed || lindnerEngagePressed) {
                // Button was just pressed - toggle state
                steerState = !steerState;
                const char* buttonType = masseyEngagePressed ? "Massey K_Bus button" :
                                        fendtButtonPressed ? "Fendt armrest button" :
                                        caseIHEngagePressed ? "Case IH engage" :
                                        catMTEngagePressed ? "CAT MT engage" :
                                        claasEngagePressed ? "CLAAS engage" :
                                        jcbEngagePressed ? "JCB engage" :
                                        lindnerEngagePressed ? "Lindner engage" : "button";
                LOG_INFO(EventSource::AUTOSTEER, "Autosteer %s via %s press",
                         steerState == 0 ? "ARMED" : "DISARMED",
                         buttonType);

                // Reset encoder count when autosteer is armed
                if (steerState == 0 && EncoderProcessor::getInstance() && EncoderProcessor::getInstance()->isEnabled()) {
                    EncoderProcessor::getInstance()->resetPulseCount();
                    LOG_INFO(EventSource::AUTOSTEER, "Encoder count reset for new engagement");
                }

                // Pulse blue LED for button press
                ledManagerFSM.pulseButton();
            }
            lastButtonReading = buttonReading;
        } else {
            // SWITCH MODE - Follow switch position
            bool switchOn = adProcessor.isSteerSwitchOn();
            static bool lastSwitchState = false;
            
            if (switchOn != lastSwitchState) {
                // Switch state changed
                steerState = switchOn ? 0 : 1;  // 0 = armed, 1 = disarmed
                LOG_INFO(EventSource::AUTOSTEER, "Autosteer %s via switch", 
                         steerState == 0 ? "ARMED" : "DISARMED");
                
                // Reset encoder count when autosteer is armed
                if (steerState == 0 && EncoderProcessor::getInstance() && EncoderProcessor::getInstance()->isEnabled()) {
                    EncoderProcessor::getInstance()->resetPulseCount();
                    LOG_INFO(EventSource::AUTOSTEER, "Encoder count reset for new engagement");
                }
                
                lastSwitchState = switchOn;
            }
        }
    }
    
    // Check if guidance status changed from AgOpenGPS
    if (guidanceStatusChanged) {
        LOG_INFO(EventSource::AUTOSTEER, "Guidance status changed: %s (steerState=%d, hasKickout=%d)",
                 guidanceActive ? "ACTIVE" : "INACTIVE", steerState,
                 kickoutMonitor ? kickoutMonitor->hasKickout() : 0);

        if (guidanceActive) {
            // Guidance turned ON in AgOpenGPS
            steerState = 0;  // Activate steering
            LOG_INFO(EventSource::AUTOSTEER, "Autosteer ARMED via AgOpenGPS (OSB)");

            // If there's a kickout active, clear it
            if (kickoutMonitor && kickoutMonitor->hasKickout()) {
                kickoutMonitor->clearKickout();
                LOG_INFO(EventSource::AUTOSTEER, "KICKOUT: Cleared via AgOpenGPS (OSB)");
            }
            
            // Reset encoder count when autosteer engages
            if (EncoderProcessor::getInstance() && EncoderProcessor::getInstance()->isEnabled()) {
                EncoderProcessor::getInstance()->resetPulseCount();
                LOG_INFO(EventSource::AUTOSTEER, "Encoder count reset for new engagement");
            }
        }
        guidanceStatusChanged = false;  // Clear flag
    }
    
    // If AgOpenGPS has stopped steering, turn off after delay
    // BUT only if not using a physical switch in switch mode OR button mode
    static int switchCounter = 0;
    bool physicalSwitchActive = configManager.getSteerSwitch() && adProcessor.isSteerSwitchOn();
    bool buttonModeActive = configManager.getSteerButton();

    if (steerState == 0 && !guidanceActive && !physicalSwitchActive && !buttonModeActive) {
        if (switchCounter++ > 30) {  // 30 * 10ms = 300ms delay
            steerState = 1;
            switchCounter = 0;
            LOG_INFO(EventSource::AUTOSTEER, "Autosteer DISARMED - guidance inactive");
        }
    } else {
        switchCounter = 0;
    }
    
    // Check for work switch changes and log them
    static bool lastWorkState = false;
    bool currentWorkState = adProcessor.isWorkSwitchOn();
    if (currentWorkState != lastWorkState) {
        LOG_INFO(EventSource::AUTOSTEER, "Work switch %s", 
                 currentWorkState ? "ON (sections active)" : "OFF (sections inactive)");
        lastWorkState = currentWorkState;
    }
    
    // Pressure sensor kickout is now handled by KickoutMonitor
    static bool lastPressureSensorState = false;
    bool currentPressureSensorState = configManager.getPressureSensor();
    if (currentPressureSensorState != lastPressureSensorState) {
        LOG_INFO(EventSource::AUTOSTEER, "Pressure sensor kickout %s", 
                 currentPressureSensorState ? "ENABLED" : "DISABLED");
        lastPressureSensorState = currentPressureSensorState;
    }
    
    // Check motor status for errors (including CAN connection loss)
    if (motorPTR) {
        MotorStatus motorStatus = motorPTR->getStatus();
        if (motorStatus.hasError && steerState == 0 && guidanceActive) {
            LOG_WARNING(EventSource::AUTOSTEER, "KICKOUT: Motor error detected");
            emergencyStop();
            return;  // Skip the rest of this cycle
        }
        
        // Check Keya-specific motor slip
        if (motorPTR->getType() == MotorDriverType::KEYA_CAN && 
            steerState == 0 && guidanceActive) {
            KeyaCANDriver* keya = static_cast<KeyaCANDriver*>(motorPTR);
            if (keya->checkMotorSlip()) {
                LOG_WARNING(EventSource::AUTOSTEER, "KICKOUT: Keya motor slip detected");
                emergencyStop();
                return;  // Skip the rest of this cycle
            }
        }
    }
    
    // Track when button/switch is pressed while in kickout
    static uint32_t kickoutButtonPressTime = 0;
    static bool kickoutButtonPressed = false;
    
    // Process motor driver (for serial communication)
    motorDriver.process();
    
    // Process kickout monitoring
    if (kickoutMonitor) {
        kickoutMonitor->process();
        
        // Check if kickout is active while steering is armed
        if (kickoutMonitor->hasKickout() && steerState == 0) {
            // Disarm steering - this will stop the motor
            steerState = 1;  // Disarmed
            emergencyStop();
            LOG_WARNING(EventSource::AUTOSTEER, "KICKOUT: %s - steering disarmed", kickoutMonitor->getReasonString());
            kickoutButtonPressTime = millis();  // Start grace period for button clear
            kickoutButtonPressed = false;
            // Don't clear kickout here - let KickoutMonitor auto-clear when conditions return to normal
        }
        
        // Grace period after kickout - allow button/switch to clear it
        if (kickoutMonitor->hasKickout() && steerState == 0 && !kickoutButtonPressed &&
            millis() - kickoutButtonPressTime < 5000) {  // 5 second grace period
            // User pressed button/switch to re-arm during grace period - clear kickout
            kickoutMonitor->clearKickout();
            kickoutButtonPressed = true;
            LOG_INFO(EventSource::AUTOSTEER, "KICKOUT: Cleared via button/switch during grace period");
            
            // Reset encoder count
            if (EncoderProcessor::getInstance() && EncoderProcessor::getInstance()->isEnabled()) {
                EncoderProcessor::getInstance()->resetPulseCount();
                LOG_INFO(EventSource::AUTOSTEER, "Encoder count reset for new engagement");
            }
        }
        
            // Check for OSB re-engagement during kickout
        // When in kickout (steerState=1) and guidance is active, check if user is trying to re-engage
        // The OSB doesn't change any bits, but we can detect repeated clicks by watching for
        // guidance going off then on again quickly
        static uint32_t lastGuidanceOffTime = 0;
        static bool waitingForGuidanceOn = false;
        
        if (kickoutMonitor->hasKickout() && steerState == 1) {
            if (!guidanceActive && prevGuidanceStatus) {
                // Guidance just went OFF - user might have clicked OSB
                lastGuidanceOffTime = millis();
                waitingForGuidanceOn = true;
            }
            else if (guidanceActive && !prevGuidanceStatus && waitingForGuidanceOn &&
                     (millis() - lastGuidanceOffTime < 1000)) {
                // Guidance went back ON within 1 second - this is an OSB toggle
                waitingForGuidanceOn = false;

                LOG_INFO(EventSource::AUTOSTEER, "OSB toggle detected during kickout - clearing kickout");

                // Clear kickout and re-arm
                kickoutMonitor->clearKickout();
                steerState = 0;  // Re-arm
                LOG_INFO(EventSource::AUTOSTEER, "KICKOUT: Cleared via OSB toggle");
                
                // Reset encoder count
                if (EncoderProcessor::getInstance() && EncoderProcessor::getInstance()->isEnabled()) {
                    EncoderProcessor::getInstance()->resetPulseCount();
                    LOG_INFO(EventSource::AUTOSTEER, "Encoder count reset for new engagement");
                }
            }
        }
    }
    
    // Always update current angle reading (needed for PGN253 even when autosteer is off)
    // Get current steering angle - use VWAS if enabled and available
    if (configManager.getINSUseFusion() && wheelAngleFusionPtr && wheelAngleFusionPtr->isHealthy()) {
        currentAngle = wheelAngleFusionPtr->getFusedAngle();
    } else {
        // Fall back to physical WAS
        currentAngle = adProcessor.getWASAngle();
    }
    
    // Apply Ackerman fix to current angle if it's negative (left turn)
    actualAngle = currentAngle;
    if (actualAngle < 0) {
        float ackermanFix = configManager.getAckermanFix();
        actualAngle = actualAngle * ackermanFix;
        
        // Log Ackerman fix application periodically
        static uint32_t lastAckermanLog = 0;
        if (millis() - lastAckermanLog > 5000 && abs(actualAngle) > 1.0f) {
            lastAckermanLog = millis();
            LOG_DEBUG(EventSource::AUTOSTEER, "Ackerman fix applied: %.2f° * %.2f = %.2f°", 
                     currentAngle, ackermanFix, actualAngle);
        }
    }
    
    // Update motor control
    updateMotorControl();
    
    // Note: LOCK output is handled by motor driver enable pin (dual-purpose)
    
    // Send PGN 253 status to AgOpenGPS
    sendPGN253();
    
    // Update LED status - simple motor state tracking
    bool motorActive = (motorState != MotorState::DISABLED);  // Check actual motor state

    // Map motor state directly to LED state
    LEDManagerFSM::SteerState ledState;
    if (motorActive) {
        ledState = LEDManagerFSM::STEER_ENGAGED;     // Green - motor running
    } else {
        ledState = LEDManagerFSM::STEER_READY;       // Amber - motor not running
    }

    // Debug logging for LED state
    static LEDManagerFSM::SteerState lastLedState = LEDManagerFSM::STEER_READY;
    if (ledState != lastLedState) {
        LOG_INFO(EventSource::AUTOSTEER, "LED state change: motor=%s -> %s",
                 motorActive ? "ACTIVE" : "DISABLED",
                 ledState == LEDManagerFSM::STEER_READY ? "AMBER" : "GREEN");
        lastLedState = ledState;
    }
    ledManagerFSM.transitionSteerState(ledState);
}

void AutosteerProcessor::handleBroadcastPGN(uint8_t pgn, const uint8_t* data, size_t len) {
    // Check if this is a Hello PGN
    if (pgn == 200) {
        // PGN 200 - Hello from AgIO, send reply
        sendHelloReply();
    }
    // Check if this is a Scan Request PGN
    else if (pgn == 202) {
        // PGN 202 - Scan request from AgIO
        sendScanReply();
    }
}

void AutosteerProcessor::sendHelloReply() {
    // Hello from AutoSteer - PGN 126 (0x7E)
    // Format: {header, source, pgn, length, angleLo, angleHi, countsLo, countsHi, switches, checksum}
    
    uint8_t helloFromSteer[] = {
        128, 129,       // Header (0x80, 0x81)
        126,            // Source: Steer module (0x7E)
        126,            // PGN: Steer reply (0x7E)
        5,              // Length
        0, 0,           // Angle (0 for now)
        0, 0,           // Counts (0 for now)
        0,              // Switch byte (0 for now)
        71              // Checksum (hardcoded like NG-V6)
    };
    
    // Send via UDP
    sendUDPbytes(helloFromSteer, sizeof(helloFromSteer));
}

void AutosteerProcessor::sendScanReply() {
    // Scan reply from AutoSteer - PGN 203 (0xCB)
    // Format: {header, source, pgn, length, ip1, ip2, ip3, subnet1, subnet2, subnet3, checksum}
    uint8_t ip[4];
    configManager.getIPAddress(ip);
    
    uint8_t scanReply[] = {
        0x80, 0x81,                    // Header
        0x7E,                          // Source: Steer module
        0xCB,                          // PGN: 203 Scan reply
        0x07,                          // Length (data only)
        ip[0],        // IP octet 1
        ip[1],        // IP octet 2
        ip[2],        // IP octet 3
        ip[3],        // IP octet 4
        ip[0],        // Subnet octet 1 (repeat IP)
        ip[1],        // Subnet octet 2 (repeat IP)
        ip[2],        // Subnet octet 3 (repeat IP)
        0                              // CRC placeholder
    };
    
    // Calculate CRC
    uint8_t crc = 0;
    for (int i = 2; i < sizeof(scanReply) - 1; i++) {
        crc += scanReply[i];
    }
    scanReply[sizeof(scanReply) - 1] = crc;
    
    // Send via UDP
    sendUDPbytes(scanReply, sizeof(scanReply));
}

void AutosteerProcessor::handleSteerConfig(uint8_t pgn, const uint8_t* data, size_t len) {
    // PGN 251 - Steer Config
    // Expected length is 14 bytes total, but we get data after header
    
    LOG_DEBUG(EventSource::AUTOSTEER, "PGN 251 (Steer Config) received, %d bytes", len);
    
    // Clear any active kickout when steer config is received
    // This happens when user clicks the test button in AgOpenGPS
    if (kickoutMonitor && kickoutMonitor->hasKickout()) {
        kickoutMonitor->clearKickout();
        LOG_INFO(EventSource::AUTOSTEER, "KICKOUT: Cleared via steer config update");
        
        // Reset encoder count
        if (EncoderProcessor::getInstance() && EncoderProcessor::getInstance()->isEnabled()) {
            EncoderProcessor::getInstance()->resetPulseCount();
            LOG_INFO(EventSource::AUTOSTEER, "Encoder count reset via steer config");
        }
    }
    
    // Always show debug info first, regardless of packet validity
    char debugMsg[256];
    snprintf(debugMsg, sizeof(debugMsg), "Raw PGN 251 data:");
    for (int i = 0; i < len && strlen(debugMsg) < 200; i++) {
        char buf[20];
        snprintf(buf, sizeof(buf), " [%d]=0x%02X(%d)", i, data[i], data[i]);
        strncat(debugMsg, buf, sizeof(debugMsg) - strlen(debugMsg) - 1);
    }
    LOG_DEBUG(EventSource::AUTOSTEER, "%s", debugMsg);
    
    if (len < 4) {  // Minimum needed for basic config
        LOG_ERROR(EventSource::AUTOSTEER, "PGN 251 too short! Got %d bytes", len);
        return;
    }
    
    // Data array indices (after header/length removal):
    // Per PGN.md: set0, pulseCount, minSpeed, sett1, ***, ***, ***, ***
    // [0] = setting0 byte
    // [1] = pulseCountMax
    // [2] = minSpeed
    // [3] = setting1 byte
    // [4-7] = reserved/unused
    
    uint8_t sett0 = data[0];
    bool invertWAS = bitRead(sett0, 0);
    bool isRelayActiveHigh = bitRead(sett0, 1);
    bool motorDriveDirection = bitRead(sett0, 2);
    bool singleInputWAS = bitRead(sett0, 3);
    bool cytronDriver = bitRead(sett0, 4);
    bool steerSwitch = bitRead(sett0, 5);
    bool steerButton = bitRead(sett0, 6);
    bool shaftEncoder = bitRead(sett0, 7);
    
    uint8_t pulseCountMax = data[1];  // Fixed: was data[2]
    uint8_t minSpeed = data[2];       // Fixed: was data[3]
    
    uint8_t sett1 = data[3];              // Fixed: was data[4]
    bool isDanfoss = bitRead(sett1, 0);
    bool pressureSensor = bitRead(sett1, 1);
    bool currentSensor = bitRead(sett1, 2);
    bool isUseYAxis = bitRead(sett1, 3);
    
    // When current sensor is enabled, data[1] (pulseCountMax) is repurposed as current threshold
    if (currentSensor) {
        uint8_t currentThreshold = data[1];
        LOG_INFO(EventSource::AUTOSTEER, "Current sensor enabled - threshold=%d (%.1f%%)", 
                  currentThreshold, (currentThreshold * 100.0f) / 255.0f);
        configManager.setCurrentThreshold(currentThreshold);
    } else if (pressureSensor) {
        // When pressure sensor is enabled, data[1] might be pressure threshold
        uint8_t pressureThreshold = data[1];
        LOG_INFO(EventSource::AUTOSTEER, "Pressure sensor enabled - threshold=%d (%.1f%%)", 
                  pressureThreshold, (pressureThreshold * 100.0f) / 255.0f);
        configManager.setPressureThreshold(pressureThreshold);
    } else if (shaftEncoder) {
        // When encoder is enabled, data[1] is pulse count max
        LOG_INFO(EventSource::AUTOSTEER, "Shaft encoder enabled - pulseCountMax=%d", 
                  pulseCountMax);
    }
    
    // Read motor driver configuration from byte 8 of the message (data array index 3)
    // Message structure: Header(5) + Data(8) + CRC(1) = 14 bytes total
    // Byte 8 of the message = data[3] (since data array starts after 5-byte header)
    uint8_t motorDriverConfig = data[3];
    
    // Workaround: Clear Cytron bit when Danfoss is selected
    // AgOpenGPS doesn't always clear this bit when switching to Danfoss
    if (isDanfoss || (motorDriverConfig & 0x01)) {
        cytronDriver = false;
    }
    
    // Update motor driver manager with new configuration
    MotorDriverManager::getInstance()->updateMotorConfig(motorDriverConfig);
    
    LOG_DEBUG(EventSource::AUTOSTEER, "InvertWAS: %d", invertWAS);
    LOG_DEBUG(EventSource::AUTOSTEER, "MotorDriveDirection: %d", motorDriveDirection);
    LOG_DEBUG(EventSource::AUTOSTEER, "SteerSwitch: %d", steerSwitch);
    LOG_DEBUG(EventSource::AUTOSTEER, "SteerButton: %d", steerButton);
    LOG_DEBUG(EventSource::AUTOSTEER, "PulseCountMax: %d", pulseCountMax);
    LOG_DEBUG(EventSource::AUTOSTEER, "MinSpeed: %d", minSpeed);
    
    // Determine motor type from config
    const char* motorType = "Unknown";
    bool isDanfossConfig = false;
    switch (motorDriverConfig) {
        case 0x00: motorType = cytronDriver ? "Cytron IBT2" : "DRV8701"; break;
        case 0x01: motorType = "Danfoss"; isDanfossConfig = true; break;
        case 0x02: motorType = cytronDriver ? "Cytron IBT2" : "DRV8701"; break;
        case 0x03: motorType = "Danfoss"; isDanfossConfig = true; break;
        case 0x04: motorType = cytronDriver ? "Cytron IBT2" : "DRV8701"; break;
        default: motorType = "Unknown"; break;
    }
    
    // Determine steer enable type
    const char* steerType = "None";
    if (steerButton) {
        steerType = "Button";
    } else if (steerSwitch) {
        steerType = "Switch";
    }
    
    // Log all settings at INFO level in a single message so users see everything
    LOG_INFO(EventSource::AUTOSTEER, "Steer config: WAS=%s Motor=%s MinSpeed=%d Steer=%s Encoder=%s Pressure=%s Current=%s (max=%d) MotorType=%s", 
             invertWAS ? "Inv" : "Norm",
             motorDriveDirection ? "Rev" : "Norm",
             minSpeed,
             steerType,
             shaftEncoder ? "Yes" : "No",
             pressureSensor ? "Yes" : "No",
             currentSensor ? "Yes" : "No",
             pulseCountMax,
             motorType);
             
    // Additional debug for encoder configuration
    LOG_DEBUG(EventSource::AUTOSTEER, "Encoder Debug: ShaftEncoder=%d, IsDanfoss=%d, MotorConfig=0x%02X, MotorType=%s",
             shaftEncoder, isDanfoss, motorDriverConfig, motorType);
    
    
    // Save config to EEPROM
    configManager.setInvertWAS(invertWAS);
    configManager.setIsRelayActiveHigh(isRelayActiveHigh);
    configManager.setMotorDriveDirection(motorDriveDirection);
    configManager.setCytronDriver(cytronDriver);
    configManager.setSteerSwitch(steerSwitch);
    configManager.setSteerButton(steerButton);
    configManager.setShaftEncoder(shaftEncoder);
    configManager.setPressureSensor(pressureSensor);
    configManager.setCurrentSensor(currentSensor);
    configManager.setPulseCountMax(pulseCountMax);
    configManager.setMinSpeed(minSpeed);
    configManager.setMotorDriverConfig(motorDriverConfig);
    configManager.setIsUseYAxis(isUseYAxis);  // Save Y-axis swap setting for IMU
    
    // Note: Sensor configuration updates would go here if we want dynamic changes
    // For now, sensor changes require reboot to ensure clean state
    
    // Check for motor type changes
    bool motorTypeChanged = false;
    int8_t currentCytronDriver = cytronDriver ? 1 : 0;
    
    // Only check for changes if we've initialized from EEPROM
    if (motorConfigInitialized) {
        LOG_DEBUG(EventSource::AUTOSTEER, "Current motor state: Config=0x%02X, Cytron=%d (previous: Config=0x%02X, Cytron=%d)",
                  motorDriverConfig, currentCytronDriver,
                  previousMotorConfig, previousCytronDriver);
        
        // Only check motor-relevant bits (bit 0 = Danfoss, CytronDriver is separate)
        // Ignore sensor bits (bits 1-2) when checking for motor changes
        uint8_t previousMotorBits = previousMotorConfig & 0x01;  // Danfoss bit only
        uint8_t currentMotorBits = motorDriverConfig & 0x01;
        
        if (previousMotorBits != currentMotorBits ||
            previousCytronDriver != currentCytronDriver) {
            LOG_INFO(EventSource::AUTOSTEER, "Motor change detected: Danfoss %d->%d, Cytron %d->%d",
                     previousMotorBits, currentMotorBits,
                     previousCytronDriver, currentCytronDriver);
            motorTypeChanged = true;
        }
    } else {
        // First PGN 251 received, but we should already be initialized from init()
        LOG_WARNING(EventSource::AUTOSTEER, "Motor config not initialized - this shouldn't happen!");
    }
    
    // Update tracked values
    previousMotorConfig = motorDriverConfig;
    previousCytronDriver = currentCytronDriver;
    
    configManager.saveSteerConfig();
    configManager.saveTurnSensorConfig();  // Also save turn sensor config (includes current threshold)
    LOG_INFO(EventSource::AUTOSTEER, "Steer config saved to EEPROM");

    // Apply encoder settings if changed
    if (EncoderProcessor::getInstance()) {
        bool currentEncoderEnabled = EncoderProcessor::getInstance()->isEnabled();

        // Update encoder enable state if changed
        if (currentEncoderEnabled != shaftEncoder) {
            EncoderProcessor::getInstance()->updateConfig(
                (EncoderType)configManager.getEncoderType(), shaftEncoder);
            LOG_INFO(EventSource::AUTOSTEER, "Encoder %s via PGN 251", shaftEncoder ? "enabled" : "disabled");
        }
    }


    if (motorTypeChanged) {
        LOG_WARNING(EventSource::AUTOSTEER, "Motor type changed - rebooting in 2 seconds...");
        delay(2000);
        SCB_AIRCR = 0x05FA0004; // Teensy Reset
    }
}

void AutosteerProcessor::handleSteerSettings(uint8_t pgn, const uint8_t* data, size_t len) {
    // PGN 252 - Steer Settings
    // Expected length is 14 bytes total, but we get data after header
    
    LOG_DEBUG(EventSource::AUTOSTEER, "PGN 252 (Steer Settings) received, %d bytes", len);
    
    if (len < 8) {
        LOG_ERROR(EventSource::AUTOSTEER, "PGN 252 too short!");
        return;
    }
    
    // Parse PGN 252 data directly to local variables
    uint8_t kp = data[0];  // Raw byte value from AgOpenGPS
    uint8_t highPWM = data[1];
    uint8_t lowPWM = data[2];
    uint8_t minPWM = data[3];
    
    // V6-NG adjusts lowPWM
    float temp = (float)minPWM * 1.2;
    lowPWM = (uint8_t)temp;
    
    uint8_t steerSensorCounts = data[4];
    
    // WAS offset is int16
    int16_t wasOffset = data[5] | (data[6] << 8);
    
    float ackermanFix = (float)data[7] * 0.01f;
    
    // Log all settings at INFO level
    LOG_INFO(EventSource::AUTOSTEER, "Steer settings: Kp=%d PWM=%d-%d-%d WAS_offset=%d counts=%d Ackerman=%.2f", 
             kp, minPWM, lowPWM, highPWM, wasOffset, steerSensorCounts, ackermanFix);
    
    // Update ADProcessor with WAS calibration values
    adProcessor.setWASOffset(wasOffset);
    adProcessor.setWASCountsPerDegree(steerSensorCounts);
    LOG_INFO(EventSource::AUTOSTEER, "Updated ADProcessor with offset=%d, CPD=%d", 
             wasOffset, steerSensorCounts);
    
    // Save steer settings to ConfigManager
    configManager.setKp(kp);
    configManager.setHighPWM(highPWM);
    configManager.setLowPWM(lowPWM);
    configManager.setMinPWM(minPWM);
    configManager.setSteerSensorCounts(steerSensorCounts);
    configManager.setWasOffset(wasOffset);
    configManager.setAckermanFix(ackermanFix);
    configManager.saveSteerSettings();
    LOG_INFO(EventSource::AUTOSTEER, "Steer settings saved to EEPROM");

    // Log confirmation that settings are now active
    LOG_INFO(EventSource::AUTOSTEER, "Settings now active - no reboot required. Motor will use new PWM values immediately.");
}

void AutosteerProcessor::handleSteerData(uint8_t pgn, const uint8_t* data, size_t len) {
    // PGN 254 - Steer Data (comes at 10Hz from AgOpenGPS)
    // For now, we only care about the autosteer enable bit
    
    if (len < 3) {
        LOG_DEBUG(EventSource::AUTOSTEER, "PGN 254 too short, ignoring");
        return;  // Too short, ignore
    }
    
    // Check if we're recovering from a link down event
    static uint32_t linkUpTime = 0;
    static bool waitingForStableLink = false;
    
    if (linkWasDown) {
        // Link was down, now we're receiving PGN 254 again
        linkUpTime = millis();
        waitingForStableLink = true;
        linkWasDown = false;  // Clear the flag
    }
    
    // Wait 3 seconds for network negotiation after link restoration
    if (waitingForStableLink && (millis() - linkUpTime > 3000)) {
        LOG_INFO(EventSource::AUTOSTEER, "Communication restored - motor under AOG control");
        waitingForStableLink = false;
    }
    
    lastPGN254Time = millis();
    lastCommandTime = millis();  // Update watchdog timer
    
    // Debug: Log raw PGN 254 data
    // Data format (from PGNProcessor we get data starting at speed):
    // [0-1] = Speed (uint16, 0.1 km/h resolution)
    // [2] = Status byte
    //       Bit 0: Guidance active
    //       Bit 6: Autosteer enable (this is what we need for button)
    // [3-4] = Steer angle setpoint (int16, divide by 100 for degrees)
    // [5] = XTE (cross track error)
    // [6] = Machine sections 1-8
    // [7] = Machine sections 9-16
    
    // Extract speed
    vehicleSpeed = (uint16_t)(data[1] << 8 | data[0]) * 0.1f; // Convert to km/h
    
    // Extract status
    uint8_t status = data[2];
    bool newAutosteerState = (status & 0x40) != 0;  // Bit 6 is autosteer enable
    
    // Debug OSB behavior - log every second during kickout
    static uint32_t lastStatusLog = 0;
    if (kickoutMonitor && kickoutMonitor->hasKickout() && millis() - lastStatusLog > 1000) {
        lastStatusLog = millis();
        LOG_DEBUG(EventSource::AUTOSTEER, "During kickout - PGN254 status: 0x%02X (guidance=%d, autosteer=%d), steerState=%d",
                 status, (status & 0x01) != 0, (status & 0x40) != 0, steerState);
    }
    
    // Also log any status changes
    static uint8_t lastStatus = 0;
    if (status != lastStatus) {
        LOG_DEBUG(EventSource::AUTOSTEER, "PGN254 status changed: 0x%02X -> 0x%02X (guidance=%d, autosteer=%d)",
                 lastStatus, status, (status & 0x01) != 0, (status & 0x40) != 0);
        lastStatus = status;
    }
    
    
    // Track guidance status changes
    static bool firstBroadcast = true;
    bool newGuidanceActive = (status & 0x01) != 0;   // Bit 0 is guidance active
    
    if (firstBroadcast) {
        // On first broadcast, just set the status without triggering change
        guidanceActive = newGuidanceActive;
        prevGuidanceStatus = newGuidanceActive;
        guidanceStatusChanged = false;
        firstBroadcast = false;
    } else {
        // Normal operation - detect changes
        prevGuidanceStatus = guidanceActive;
        guidanceActive = newGuidanceActive;
        guidanceStatusChanged = (guidanceActive != prevGuidanceStatus);
    }
    // Extract steer angle
    int16_t angleRaw = (int16_t)(data[4] << 8 | data[3]);
    targetAngle = angleRaw / 100.0f;
    
    // Debug log for AgIO test mode
    if (targetAngle != 0.0f || autosteerEnabled) {
        LOG_DEBUG(EventSource::AUTOSTEER, "PGN254: speed=%.1f km/h, target=%.1f°, enabled=%d, guidance=%d", 
                  vehicleSpeed, targetAngle, autosteerEnabled, guidanceActive);
    }
    
    // Extract XTE
    crossTrackError = (int8_t)data[5];
    
    // Extract sections
    uint8_t sections1_8 = data[6];
    uint8_t sections9_16 = data[7];
    machineSections = (uint16_t)(sections9_16 << 8 | sections1_8);
    
    // Track autosteer enable bit changes for OSB handling
    static bool prevAutosteerEnabled = false;
    if (newAutosteerState != prevAutosteerEnabled) {
        LOG_INFO(EventSource::AUTOSTEER, "AgOpenGPS autosteer bit changed: %s",
                      newAutosteerState ? "ENABLED" : "DISABLED");

        // OSB button was pressed - handle it even when button mode is configured
        if (newAutosteerState && !prevAutosteerEnabled) {
            // OSB turned ON - arm autosteer
            steerState = 0;
            LOG_INFO(EventSource::AUTOSTEER, "Autosteer ARMED via AgOpenGPS (OSB bit 6)");

            // If there's a kickout active, clear it
            if (kickoutMonitor && kickoutMonitor->hasKickout()) {
                kickoutMonitor->clearKickout();
                LOG_INFO(EventSource::AUTOSTEER, "KICKOUT: Cleared via OSB");
            }

            // Reset encoder count
            if (EncoderProcessor::getInstance() && EncoderProcessor::getInstance()->isEnabled()) {
                EncoderProcessor::getInstance()->resetPulseCount();
                LOG_INFO(EventSource::AUTOSTEER, "Encoder count reset for new engagement");
            }
        } else if (!newAutosteerState && prevAutosteerEnabled) {
            // OSB turned OFF - disarm autosteer
            steerState = 1;
            LOG_INFO(EventSource::AUTOSTEER, "Autosteer DISARMED via AgOpenGPS (OSB bit 6)");
        }
        prevAutosteerEnabled = newAutosteerState;
    }
    autosteerEnabled = newAutosteerState;
}

// Static callback wrapper
void AutosteerProcessor::handlePGNStatic(uint8_t pgn, const uint8_t* data, size_t len) {
    if (instance) {
        // Don't print for PGN 254, 200, or 202 since they come frequently
        if (pgn != 254 && pgn != 200 && pgn != 202) {
            LOG_DEBUG(EventSource::AUTOSTEER, "Received PGN %d", pgn);
        }
        
        // Handle broadcast PGNs
        if (pgn == 200 || pgn == 202) {
            instance->handleBroadcastPGN(pgn, data, len);
        }
        else if (pgn == 251) {
            instance->handleSteerConfig(pgn, data, len);
        }
        else if (pgn == 252) {
            instance->handleSteerSettings(pgn, data, len);
        }
        else if (pgn == 254) {
            instance->handleSteerData(pgn, data, len);
        }
        // We'll add other PGNs one at a time as needed
    } else {
        LOG_ERROR(EventSource::AUTOSTEER, "No instance!");
    }
}

void AutosteerProcessor::sendPGN253() {
    // PGN 253 - Status data TO AgOpenGPS
    // Format: {header, source, pgn, length, 
    //          steerAngleLo, steerAngleHi,    // bytes 5-6: actual steer angle
    //          headingLo, headingHi,           // bytes 7-8: IMU heading (deprecated)
    //          rollLo, rollHi,                 // bytes 9-10: roll (deprecated)
    //          switchByte,                     // byte 11: switch states
    //          pwmDisplay,                     // byte 12: PWM value
    //          checksum}
    
    // Get actual values - use actualAngle which includes Ackerman correction
    int16_t actualSteerAngle = (int16_t)(actualAngle * 100.0f);  // Actual angle * 100
    int16_t heading = 0;            // Deprecated - sent by GNSS
    int16_t roll = 0;               // Deprecated - sent by GNSS  
    uint8_t pwmDisplay = (uint8_t)abs(motorPWM);  // Already in 0-255 range
    
    // Build switch byte
    // Bit 0: work switch (inverted)
    // Bit 1: steer switch (inverted steerState)
    // Bit 2: remote/kickout input
    // Bit 3: unused
    // Bit 4: unused
    // Bit 5: fusion active (1 = fusion, 0 = WAS)
    uint8_t switchByte = 0;
    switchByte |= (0 << 2);        // No remote/kickout for now
    switchByte |= (steerState << 1);  // Steer state in bit 1
    switchByte |= !adProcessor.isWorkSwitchOn();  // Work switch state (inverted) in bit 0
    
    uint8_t pgn253[] = {
        0x80, 0x81,                    // Header
        0x7E,                          // Source: Steer module (126)
        0xFD,                          // PGN: 253
        8,                             // Length
        (uint8_t)(actualSteerAngle & 0xFF),      // Steer angle low
        (uint8_t)(actualSteerAngle >> 8),        // Steer angle high
        (uint8_t)(heading & 0xFF),                // Heading low
        (uint8_t)(heading >> 8),                  // Heading high
        (uint8_t)(roll & 0xFF),                   // Roll low
        (uint8_t)(roll >> 8),                     // Roll high
        switchByte,                               // Switch byte
        pwmDisplay,                               // PWM display
        0                                         // CRC placeholder
    };
    
    // Calculate CRC
    uint8_t crc = 0;
    for (int i = 2; i < sizeof(pgn253) - 1; i++) {
        crc += pgn253[i];
    }
    pgn253[sizeof(pgn253) - 1] = crc;
    
    // Send via UDP
    sendUDPbytes(pgn253, sizeof(pgn253));
}

void AutosteerProcessor::updateMotorControl() {
    // Current angle is now updated in process() before this function is called
    
    // Check if steering should be active
    bool shouldBeActive = shouldSteerBeActive();
    
    // Handle state transitions
    if (shouldBeActive && motorState == MotorState::DISABLED) {
        // Transition: Start soft-start sequence
        motorState = MotorState::SOFT_START;
        softStartBeginTime = millis();
        softStartRampValue = 0.0f;
        LOG_INFO(EventSource::AUTOSTEER, "Motor STARTING - soft-start sequence (%dms)",
                 softStartDurationMs);
        // Update LED immediately
        ledManagerFSM.transitionSteerState(LEDManagerFSM::STEER_ENGAGED);
        LOG_INFO(EventSource::AUTOSTEER, "LED -> GREEN (motor starting)");
    } 
    else if (!shouldBeActive && motorState != MotorState::DISABLED) {
        // Transition: Disable motor
        motorState = MotorState::DISABLED;
        motorPWM = 0;
        if (motorPTR) {
            motorPTR->enable(false);
            motorPTR->setPWM(0);

            // LOCK output control
            if (motorPTR->getType() == MotorDriverType::KEYA_CAN) {
                // Directly control LOCK output for Keya motor
                digitalWrite(4, LOW);  // SLEEP_PIN = 4, LOW = LOCK OFF
                LOG_INFO(EventSource::AUTOSTEER, "LOCK output: INACTIVE (pin 4 LOW for Keya motor)");
            } else {
                // For PWM motors, pin 4 is controlled by the motor driver
                LOG_INFO(EventSource::AUTOSTEER, "LOCK output: INACTIVE (motor disabled)");
            }
        }
        // Update LED immediately when motor disabled
        ledManagerFSM.transitionSteerState(LEDManagerFSM::STEER_READY);
        LOG_INFO(EventSource::AUTOSTEER, "LED -> AMBER (motor disabled)");

        // Give more specific disable reason
        if (!QNetworkBase::isConnected()) {
            // Already logged in link state change detection above
        } else if (vehicleSpeed <= 0.1f) {
            LOG_INFO(EventSource::AUTOSTEER, "Motor disabled - speed too low (%.1f km/h)", vehicleSpeed);
        } else if (!guidanceActive) {
            LOG_INFO(EventSource::AUTOSTEER, "Motor disabled - guidance inactive");
        } else if (steerState != 0) {
            LOG_INFO(EventSource::AUTOSTEER, "Motor disabled - steer switch off");
        } else if (millis() - lastCommandTime > WATCHDOG_TIMEOUT) {
            LOG_INFO(EventSource::AUTOSTEER, "Motor disabled - communication timeout");
        } else {
            LOG_INFO(EventSource::AUTOSTEER, "Motor disabled");
        }
        return;
    }
    else if (!shouldBeActive) {
        // Already disabled, nothing to do
        return;
    }
    
    // Ackerman fix is now applied in process() before this function is called
    
    // Calculate angle error
    float angleError = actualAngle - targetAngle;
    float errorAbs = abs(angleError);
    
    // Get PWM settings from ConfigManager (cached for performance)
    uint8_t kp = configManager.getKp();
    uint8_t highPWM = configManager.getHighPWM();
    uint8_t minPWM = configManager.getMinPWM();

    // Debug log to verify settings are being read
    static uint32_t lastSettingsVerifyLog = 0;
    static uint8_t lastKp = 0;
    static uint8_t lastHighPWM = 0;
    if (millis() - lastSettingsVerifyLog > 5000 || kp != lastKp || highPWM != lastHighPWM) {
        lastSettingsVerifyLog = millis();
        lastKp = kp;
        lastHighPWM = highPWM;
        LOG_INFO(EventSource::AUTOSTEER, "Active PWM settings: Kp=%d, highPWM=%d, minPWM=%d", kp, highPWM, minPWM);
    }
    
    // Calculate base PWM output (error * Kp)
    int16_t pValue = kp * angleError;
    
    // Apply PWM calculation similar to V6
    if (highPWM > 0) {  // Check if we have valid settings
        // Start with base P value
        int16_t pwmDrive = pValue;
        
        // Add min throttle factor so no delay from motor resistance
        if (pwmDrive < 0) {
            pwmDrive -= minPWM;
        } else if (pwmDrive > 0) {
            pwmDrive += minPWM;
        } else {
            // Dead zone - set pwmDrive to 0
            pwmDrive = 0;
        }
        
        // Limit the PWM drive to highPWM setting
        if (pwmDrive > highPWM) {
            pwmDrive = highPWM;
        } else if (pwmDrive < -highPWM) {
            pwmDrive = -highPWM;
        }
        
        // Store final PWM value
        motorPWM = pwmDrive;
        
        // Log the PWM calculation periodically
        static uint32_t lastPWMCalcLog = 0;
        if (millis() - lastPWMCalcLog > 5000) {  // Every 5 seconds
            lastPWMCalcLog = millis();
            LOG_DEBUG(EventSource::AUTOSTEER, "PWM calc: actual=%.1f° - target=%.1f° = error=%.1f° * Kp=%d = %d, +minPWM=%d, limit=%d, final=%d", 
                     actualAngle, targetAngle, angleError, kp, pValue, minPWM, highPWM, pwmDrive);
        }
        
        // Debug log final motor PWM periodically
        static uint32_t lastMotorPWMLog = 0;
        if (millis() - lastMotorPWMLog > 1000 && abs(motorPWM) > 10) {
            lastMotorPWMLog = millis();
            LOG_DEBUG(EventSource::AUTOSTEER, "Motor PWM: %d (highPWM=%d)", 
                      motorPWM, highPWM);
        }
        
        // Apply soft-start if active
        if (motorState == MotorState::SOFT_START) {
                uint32_t elapsed = millis() - softStartBeginTime;
                
                if (elapsed >= softStartDurationMs) {
                    // Soft-start complete, transition to normal
                    motorState = MotorState::NORMAL_CONTROL;
                    LOG_INFO(EventSource::AUTOSTEER, "Motor ACTIVE - normal steering control");
                } else {
                    // Calculate ramp progress (0.0 to 1.0)
                    float rampProgress = (float)elapsed / softStartDurationMs;
                    
                    // Use sine curve for smooth acceleration (slow-fast-slow)
                    float sineRamp = sin(rampProgress * PI / 2.0f);
                    
                    // Calculate soft-start limit based on lowPWM
                    uint8_t lowPWM = configManager.getLowPWM();
                    int16_t softStartLimit = (int16_t)(lowPWM * softStartMaxPWM * sineRamp);
                    
                    // Apply limit in direction of motor PWM
                    if (motorPWM > 0) {
                        motorPWM = min(motorPWM, softStartLimit);
                    } else if (motorPWM < 0) {
                        motorPWM = max(motorPWM, -softStartLimit);
                    }
                    
                    softStartRampValue = (float)softStartLimit;
                    
                    // Debug logging every 50ms during soft-start
                    static uint32_t lastSoftStartDebug = 0;
                    if (millis() - lastSoftStartDebug > 50) {
                        lastSoftStartDebug = millis();
                        LOG_DEBUG(EventSource::AUTOSTEER, "Soft-start: elapsed=%dms, progress=%.2f, limit=%d, pwm=%d", 
                                  elapsed, rampProgress, softStartLimit, motorPWM);
                    }
                }
            }
    } else {
        // No valid PWM config
        motorPWM = 0;
        LOG_ERROR(EventSource::AUTOSTEER, "Invalid PWM configuration");
    }
    
    // Apply motor direction from config
    if (configManager.getMotorDriveDirection()) {
        motorPWM = -motorPWM;  // Invert if configured
    }
    
    // Final PWM limit check - ensure we never exceed highPWM setting
    // Log current settings for debugging (DEBUG level to avoid spam)
    static uint32_t lastPWMSettingsLog = 0;
    if (millis() - lastPWMSettingsLog > 30000) {  // Log every 30 seconds
        lastPWMSettingsLog = millis();
        // Note: PWM settings now come from ConfigManager
        LOG_DEBUG(EventSource::AUTOSTEER, "PWM Settings: highPWM=%d, lowPWM=%d, minPWM=%d", 
                  configManager.getHighPWM(), configManager.getLowPWM(), configManager.getMinPWM());
    }
    
    // Motor speed is now properly scaled to respect highPWM limit
    // No additional limiting needed here since we scale by newHighPWM above
    
    
    // Send to motor
    if (motorPTR) {
        // Only enable motor if not in disabled state
        if (motorState != MotorState::DISABLED) {
            motorPTR->enable(true);
            motorPTR->setPWM(motorPWM);
            
            // Debug log to confirm PWM is being sent
            static uint32_t lastMotorCmdLog = 0;
            if (millis() - lastMotorCmdLog > 1000) {
                lastMotorCmdLog = millis();
                LOG_DEBUG(EventSource::AUTOSTEER, "Sending to motor: PWM=%d, State=%d", 
                          motorPWM, (int)motorState);
            }
        }
        
        // LOCK output control
        // For PWM motors, pin 4 is controlled by the motor driver
        // For Keya/CAN motors, we need to control pin 4 directly
        if (motorPTR->getType() == MotorDriverType::KEYA_CAN) {
            // Directly control LOCK output for Keya motor
            digitalWrite(4, HIGH);  // SLEEP_PIN = 4, HIGH = LOCK ON
            
            static bool lockLogged = false;
            if (motorState == MotorState::NORMAL_CONTROL && !lockLogged) {
                LOG_INFO(EventSource::AUTOSTEER, "LOCK output: ACTIVE (pin 4 HIGH for Keya motor)");
                lockLogged = true;
            } else if (motorState == MotorState::DISABLED) {
                lockLogged = false;
            }
        }
    }
    
}

bool AutosteerProcessor::shouldSteerBeActive() const {
    // Check kickout cooldown
    if (kickoutTime > 0 && (millis() - kickoutTime < KICKOUT_COOLDOWN_MS)) {
        return false;  // Still in cooldown
    }
    
    // Check ethernet link
    if (!QNetworkBase::isConnected()) {
        return false;  // No ethernet link
    }
    
    // Check watchdog timeout
    if (millis() - lastCommandTime > WATCHDOG_TIMEOUT) {
        return false;  // No recent commands
    }
    
    // Check all enable conditions
    // Note: We use guidanceActive (bit 0) and steerState instead of autosteerEnabled (bit 6)
    // because AgOpenGPS may not set bit 6 until it receives confirmation from us
    bool active = guidanceActive &&           // Guidance line active (bit 0 from PGN 254)
                  (steerState == 0) &&        // Our button/OSB state (0=active)
                  (vehicleSpeed > (configManager.getMinSpeed() / 10.0f));  // Moving (MinSpeed is in 0.1 km/h units)
    
    // Debug logging for test mode
    static uint32_t lastDebugTime = 0;
    if (millis() - lastDebugTime > 1000) {
        lastDebugTime = millis();
        LOG_DEBUG(EventSource::AUTOSTEER, "shouldSteerBeActive: guidance=%d, steerState=%d, speed=%.1f -> %s",
                  guidanceActive, steerState, vehicleSpeed, active ? "YES" : "NO");
    }
    
    return active;
}

void AutosteerProcessor::emergencyStop() {
    LOG_WARNING(EventSource::AUTOSTEER, "EMERGENCY STOP");
    
    // Reset motor state
    motorState = MotorState::DISABLED;
    
    // Disable motor immediately
    motorPWM = 0;
    if (motorPTR) {
        motorPTR->setPWM(0);
        motorPTR->enable(false);
        
        // Ensure LOCK is OFF on kickout
        if (motorPTR->getType() == MotorDriverType::KEYA_CAN) {
            digitalWrite(4, LOW);  // SLEEP_PIN = 4, LOW = LOCK OFF
        }
    }
    
    // Set inactive state
    steerState = 1;
    
    // Start kickout cooldown
    kickoutTime = millis();
}

