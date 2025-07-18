#include "AutosteerProcessor.h"
#include "PGNProcessor.h"
#include "ADProcessor.h"
#include "MotorDriverInterface.h"
#include "KeyaCANDriver.h"
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

// External network config
extern struct NetworkConfig netConfig;

// Global pointer definition
AutosteerProcessor* autosteerPTR = nullptr;

// Singleton instance
AutosteerProcessor* AutosteerProcessor::instance = nullptr;

AutosteerProcessor::AutosteerProcessor() {
    // Initialize config with defaults
    memset(&steerConfig, 0, sizeof(SteerConfig));
    memset(&steerSettings, 0, sizeof(SteerSettings));
    
    // Set some sensible defaults
    steerSettings.Kp = 10;  // 1.0 * 10 (AgOpenGPS format)
    steerSettings.highPWM = 255;
    steerSettings.lowPWM = 10;
    steerSettings.minPWM = 5;
    steerSettings.steerSensorCounts = 1;  // Avoid divide by zero
    steerSettings.wasOffset = 0;
    steerSettings.AckermanFix = 1.0f;
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
    
    // Load steer config from EEPROM
    steerConfig.InvertWAS = configManager.getInvertWAS();
    steerConfig.IsRelayActiveHigh = configManager.getIsRelayActiveHigh();
    steerConfig.MotorDriveDirection = configManager.getMotorDriveDirection();
    steerConfig.CytronDriver = configManager.getCytronDriver();
    steerConfig.SteerSwitch = configManager.getSteerSwitch();
    steerConfig.SteerButton = configManager.getSteerButton();
    steerConfig.PressureSensor = configManager.getPressureSensor();
    steerConfig.PulseCountMax = configManager.getPulseCountMax();
    steerConfig.MinSpeed = configManager.getMinSpeed();
    steerConfig.MotorDriverConfig = configManager.getMotorDriverConfig();
    LOG_DEBUG(EventSource::AUTOSTEER, "Loaded steer config from EEPROM: Pressure=%s, PulseMax=%d, RelayActive=%s", 
              steerConfig.PressureSensor ? "Yes" : "No", steerConfig.PulseCountMax,
              steerConfig.IsRelayActiveHigh ? "HIGH" : "LOW");
    
    // Initialize motor config tracking from EEPROM values
    previousMotorConfig = steerConfig.MotorDriverConfig;
    previousCytronDriver = steerConfig.CytronDriver ? 1 : 0;
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
    steerSettings.Kp = configManager.getKp();
    steerSettings.highPWM = configManager.getHighPWM();
    steerSettings.lowPWM = configManager.getLowPWM();
    steerSettings.minPWM = configManager.getMinPWM();
    steerSettings.steerSensorCounts = configManager.getSteerSensorCounts();
    steerSettings.wasOffset = configManager.getWasOffset();
    steerSettings.AckermanFix = configManager.getAckermanFix();
    
    LOG_INFO(EventSource::AUTOSTEER, "Loaded steer settings from EEPROM: offset=%d, CPD=%d, highPWM=%d", 
             steerSettings.wasOffset, steerSettings.steerSensorCounts, steerSettings.highPWM);
    
    // Update ADProcessor with loaded values
    adProcessor.setWASOffset(steerSettings.wasOffset);
    adProcessor.setWASCountsPerDegree(steerSettings.steerSensorCounts);
    
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
        fusionConfig.wheelbase = 2.5f;  // TODO: Load from config
        fusionConfig.countsPerDegree = 100.0f;  // TODO: Load from calibration
        wheelAngleFusionPtr->setConfig(fusionConfig);
    } else {
        LOG_ERROR(EventSource::AUTOSTEER, "Failed to initialize Virtual WAS");
        configManager.setINSUseFusion(false);  // Disable VWAS
        delete wheelAngleFusionPtr;
        wheelAngleFusionPtr = nullptr;
    }
}

void AutosteerProcessor::process() {
    // Run autosteer loop at 100Hz
    uint32_t currentTime = millis();
    if (currentTime - lastLoopTime < LOOP_TIME) {
        return;  // Not time yet
    }
    lastLoopTime = currentTime;
    
    // === 100Hz AUTOSTEER LOOP STARTS HERE ===
    
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
    if (wheelAngleFusionPtr && configManager.getINSUseFusion()) {
        float dt = LOOP_TIME / 1000.0f;  // Convert to seconds
        wheelAngleFusionPtr->update(dt);
    }
    
    // === BUTTON/SWITCH LOGIC ===
    if (steerConfig.SteerButton || steerConfig.SteerSwitch) {
        if (steerConfig.SteerButton) {
            // BUTTON MODE - Toggle on press
            static bool lastButtonReading = HIGH;
            bool buttonReading = adProcessor.isSteerSwitchOn() ? LOW : HIGH;  // Convert to active low
            
            if (buttonReading == LOW && lastButtonReading == HIGH) {
                // Button was just pressed - toggle state
                steerState = !steerState;
                LOG_INFO(EventSource::AUTOSTEER, "Autosteer %s via button press", 
                         steerState == 0 ? "ARMED" : "DISARMED");
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
                lastSwitchState = switchOn;
            }
        }
    }
    
    // Check if guidance status changed from AgOpenGPS
    if (guidanceStatusChanged) {
        if (guidanceActive) {
            // Guidance turned ON in AgOpenGPS
            steerState = 0;  // Activate steering
            LOG_INFO(EventSource::AUTOSTEER, "Autosteer ARMED via AgOpenGPS (OSB)");
        }
        guidanceStatusChanged = false;  // Clear flag
    }
    
    // If AgOpenGPS has stopped steering, turn off after delay
    // BUT only if not using a physical switch in switch mode
    static int switchCounter = 0;
    bool physicalSwitchActive = steerConfig.SteerSwitch && adProcessor.isSteerSwitchOn();
    
    if (steerState == 0 && !guidanceActive && !physicalSwitchActive) {
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
    
    // Check pressure sensor kickout if enabled
    static bool lastPressureSensorState = false;
    if (steerConfig.PressureSensor != lastPressureSensorState) {
        LOG_INFO(EventSource::AUTOSTEER, "Pressure sensor kickout %s", 
                 steerConfig.PressureSensor ? "ENABLED" : "DISABLED");
        lastPressureSensorState = steerConfig.PressureSensor;
    }
    
    if (steerConfig.PressureSensor && steerState == 0) {
        float pressure = adProcessor.getPressureReading();
        if (pressure >= steerConfig.PulseCountMax) {
            LOG_WARNING(EventSource::AUTOSTEER, "KICKOUT: Pressure %.1f exceeded limit %d", 
                        pressure, steerConfig.PulseCountMax);
            emergencyStop();
            return;  // Skip the rest of this cycle
        }
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
    
    // Process kickout monitoring
    if (kickoutMonitor) {
        kickoutMonitor->process();
        
        // Check if kickout is active
        if (kickoutMonitor->hasKickout() && steerState == 0) {
            LOG_WARNING(EventSource::AUTOSTEER, "KICKOUT: %s", kickoutMonitor->getReasonString());
            emergencyStop();
            kickoutMonitor->clearKickout();  // Clear after handling
        }
    }
    
    // Update motor control
    updateMotorControl();
    
    // Note: LOCK output is handled by motor driver enable pin (dual-purpose)
    
    // Send PGN 253 status to AgOpenGPS
    sendPGN253();
    
    // Update LED status based on actual system state (no motor speed hysteresis)
    bool wasReady = true;  // ADProcessor is always available as an object
    bool armed = (steerState == 0);         // Button/OSB has armed autosteer
    bool guidance = guidanceActive;         // AgOpenGPS has active guidance line
    
    // Map states to LED FSM states - simple and clear
    LEDManagerFSM::SteerState ledState;
    if (!wasReady) {
        ledState = LEDManagerFSM::STEER_MALFUNCTION; // Red - hardware malfunction
    } else if (!armed) {
        ledState = LEDManagerFSM::STEER_READY;       // Amber - ready but not armed
    } else {
        ledState = LEDManagerFSM::STEER_ENGAGED;     // Green - engaged
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
    
    uint8_t scanReply[] = {
        0x80, 0x81,                    // Header
        0x7E,                          // Source: Steer module
        0xCB,                          // PGN: 203 Scan reply
        0x07,                          // Length (data only)
        netConfig.currentIP[0],        // IP octet 1
        netConfig.currentIP[1],        // IP octet 2
        netConfig.currentIP[2],        // IP octet 3
        netConfig.currentIP[3],        // IP octet 4
        netConfig.currentIP[0],        // Subnet octet 1 (repeat IP)
        netConfig.currentIP[1],        // Subnet octet 2 (repeat IP)
        netConfig.currentIP[2],        // Subnet octet 3 (repeat IP)
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
    steerConfig.InvertWAS = bitRead(sett0, 0);
    steerConfig.IsRelayActiveHigh = bitRead(sett0, 1);
    steerConfig.MotorDriveDirection = bitRead(sett0, 2);
    steerConfig.SingleInputWAS = bitRead(sett0, 3);
    steerConfig.CytronDriver = bitRead(sett0, 4);
    steerConfig.SteerSwitch = bitRead(sett0, 5);
    steerConfig.SteerButton = bitRead(sett0, 6);
    steerConfig.ShaftEncoder = bitRead(sett0, 7);
    
    steerConfig.PulseCountMax = data[1];  // Fixed: was data[2]
    steerConfig.MinSpeed = data[2];       // Fixed: was data[3]
    
    uint8_t sett1 = data[3];              // Fixed: was data[4]
    steerConfig.IsDanfoss = bitRead(sett1, 0);
    steerConfig.PressureSensor = bitRead(sett1, 1);
    steerConfig.CurrentSensor = bitRead(sett1, 2);
    steerConfig.IsUseY_Axis = bitRead(sett1, 3);
    
    // Read motor driver configuration from byte 8 of the message (data array index 3)
    // Message structure: Header(5) + Data(8) + CRC(1) = 14 bytes total
    // Byte 8 of the message = data[3] (since data array starts after 5-byte header)
    steerConfig.MotorDriverConfig = data[3];
    
    // Workaround: Clear Cytron bit when Danfoss is selected
    // AgOpenGPS doesn't always clear this bit when switching to Danfoss
    if (steerConfig.IsDanfoss || (steerConfig.MotorDriverConfig & 0x01)) {
        steerConfig.CytronDriver = false;
    }
    
    // Update motor driver manager with new configuration
    MotorDriverManager::getInstance()->updateMotorConfig(steerConfig.MotorDriverConfig);
    
    LOG_DEBUG(EventSource::AUTOSTEER, "InvertWAS: %d", steerConfig.InvertWAS);
    LOG_DEBUG(EventSource::AUTOSTEER, "MotorDriveDirection: %d", steerConfig.MotorDriveDirection);
    LOG_DEBUG(EventSource::AUTOSTEER, "SteerSwitch: %d", steerConfig.SteerSwitch);
    LOG_DEBUG(EventSource::AUTOSTEER, "SteerButton: %d", steerConfig.SteerButton);
    LOG_DEBUG(EventSource::AUTOSTEER, "PulseCountMax: %d", steerConfig.PulseCountMax);
    LOG_DEBUG(EventSource::AUTOSTEER, "MinSpeed: %d", steerConfig.MinSpeed);
    
    // Determine motor type from config
    const char* motorType = "Unknown";
    switch (steerConfig.MotorDriverConfig) {
        case 0x00: motorType = steerConfig.CytronDriver ? "Cytron IBT2" : "DRV8701"; break;
        case 0x01: motorType = "Danfoss"; break;
        case 0x02: motorType = steerConfig.CytronDriver ? "Cytron IBT2" : "DRV8701"; break;
        case 0x03: motorType = "Danfoss"; break;
        case 0x04: motorType = steerConfig.CytronDriver ? "Cytron IBT2" : "DRV8701"; break;
        default: motorType = "Unknown"; break;
    }
    
    // Determine steer enable type
    const char* steerType = "None";
    if (steerConfig.SteerButton) {
        steerType = "Button";
    } else if (steerConfig.SteerSwitch) {
        steerType = "Switch";
    }
    
    // Log all settings at INFO level in a single message so users see everything
    LOG_INFO(EventSource::AUTOSTEER, "Steer config: WAS=%s Motor=%s MinSpeed=%d Steer=%s Encoder=%s Pressure=%s(max=%d) MotorType=%s", 
             steerConfig.InvertWAS ? "Inv" : "Norm",
             steerConfig.MotorDriveDirection ? "Rev" : "Norm",
             steerConfig.MinSpeed,
             steerType,
             steerConfig.ShaftEncoder ? "Yes" : "No",
             steerConfig.PressureSensor ? "Yes" : "No",
             steerConfig.PulseCountMax,
             motorType);
    
    // Save config to EEPROM
    configManager.setInvertWAS(steerConfig.InvertWAS);
    configManager.setIsRelayActiveHigh(steerConfig.IsRelayActiveHigh);
    configManager.setMotorDriveDirection(steerConfig.MotorDriveDirection);
    configManager.setCytronDriver(steerConfig.CytronDriver);
    configManager.setSteerSwitch(steerConfig.SteerSwitch);
    configManager.setSteerButton(steerConfig.SteerButton);
    configManager.setPressureSensor(steerConfig.PressureSensor);
    configManager.setPulseCountMax(steerConfig.PulseCountMax);
    configManager.setMinSpeed(steerConfig.MinSpeed);
    configManager.setMotorDriverConfig(steerConfig.MotorDriverConfig);
    
    // Check for motor type changes
    bool motorTypeChanged = false;
    int8_t currentCytronDriver = steerConfig.CytronDriver ? 1 : 0;
    
    // Only check for changes if we've initialized from EEPROM
    if (motorConfigInitialized) {
        LOG_DEBUG(EventSource::AUTOSTEER, "Current motor state: Config=0x%02X, Cytron=%d (previous: Config=0x%02X, Cytron=%d)",
                  steerConfig.MotorDriverConfig, currentCytronDriver,
                  previousMotorConfig, previousCytronDriver);
        
        if (previousMotorConfig != steerConfig.MotorDriverConfig ||
            previousCytronDriver != currentCytronDriver) {
            LOG_INFO(EventSource::AUTOSTEER, "Motor change detected: Config 0x%02X->0x%02X, Cytron %d->%d",
                     previousMotorConfig, steerConfig.MotorDriverConfig,
                     previousCytronDriver, currentCytronDriver);
            motorTypeChanged = true;
        }
    } else {
        // First PGN 251 received, but we should already be initialized from init()
        LOG_WARNING(EventSource::AUTOSTEER, "Motor config not initialized - this shouldn't happen!");
    }
    
    // Update tracked values
    previousMotorConfig = steerConfig.MotorDriverConfig;
    previousCytronDriver = currentCytronDriver;
    
    configManager.saveSteerConfig();
    LOG_INFO(EventSource::AUTOSTEER, "Steer config saved to EEPROM");
    
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
    
    // Data array indices (after header/length removal):
    // [0] = Kp (proportional gain)
    // [1] = highPWM (max limit)
    // [2] = lowPWM (minimum to move)
    // [3] = minPWM
    // [4] = steerSensorCounts
    // [5-6] = wasOffset (int16)
    // [7] = ackermanFix
    
    steerSettings.Kp = data[0];  // Raw byte value from AgOpenGPS
    steerSettings.highPWM = data[1];
    steerSettings.lowPWM = data[2];
    steerSettings.minPWM = data[3];
    
    // V6-NG adjusts lowPWM
    float temp = (float)steerSettings.minPWM * 1.2;
    steerSettings.lowPWM = (uint8_t)temp;
    
    steerSettings.steerSensorCounts = data[4];
    
    // WAS offset is int16
    steerSettings.wasOffset = data[5] | (data[6] << 8);
    
    steerSettings.AckermanFix = (float)data[7] * 0.01f;
    
    
    // Kp is used directly in updateMotorControl()
    LOG_DEBUG(EventSource::AUTOSTEER, "Kp updated to %d", steerSettings.Kp);
    
    // Update ADProcessor with WAS calibration values
    adProcessor.setWASOffset(steerSettings.wasOffset);
    adProcessor.setWASCountsPerDegree(steerSettings.steerSensorCounts);
    
    // Log all settings at INFO level in a single message so users see everything
    LOG_INFO(EventSource::AUTOSTEER, "Steer settings: Kp=%d PWM=%d-%d-%d WAS_offset=%d counts=%d Ackerman=%.2f", 
             steerSettings.Kp,
             steerSettings.minPWM, steerSettings.lowPWM, steerSettings.highPWM,
             steerSettings.wasOffset, steerSettings.steerSensorCounts,
             steerSettings.AckermanFix);
    
    // Also log that we updated the ADProcessor
    LOG_INFO(EventSource::AUTOSTEER, "Updated ADProcessor with offset=%d, CPD=%d", 
             steerSettings.wasOffset, steerSettings.steerSensorCounts);
    
    // Save steer settings to EEPROM
    configManager.setKp(steerSettings.Kp);
    configManager.setHighPWM(steerSettings.highPWM);
    configManager.setLowPWM(steerSettings.lowPWM);
    configManager.setMinPWM(steerSettings.minPWM);
    configManager.setSteerSensorCounts(steerSettings.steerSensorCounts);
    configManager.setWasOffset(steerSettings.wasOffset);
    configManager.setAckermanFix(steerSettings.AckermanFix);
    configManager.saveSteerSettings();
    LOG_INFO(EventSource::AUTOSTEER, "Steer settings saved to EEPROM");
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
    // Commented out to reduce log noise
    // static uint32_t lastRawLog = 0;
    // if (millis() - lastRawLog > 500) { // Every 500ms
    //     lastRawLog = millis();
    //     char hexBuf[64];
    //     int pos = 0;
    //     for (int i = 0; i < len && i < 16 && pos < 60; i++) {
    //         pos += snprintf(hexBuf + pos, sizeof(hexBuf) - pos, "%02X ", data[i]);
    //     }
    //     LOG_DEBUG(EventSource::AUTOSTEER, "PGN254 raw (%d bytes): %s", len, hexBuf);
    // }
    
    
    // Data format (from PGNProcessor we get data starting at speed):
    // [0-1] = Speed (uint16, cm/s)
    // [2] = Status byte
    //       Bit 0: Guidance active
    //       Bit 6: Autosteer enable (this is what we need for button)
    // [3-4] = Steer angle setpoint (int16, divide by 100 for degrees)
    // [5] = XTE (cross track error)
    // [6] = Machine sections 1-8
    // [7] = Machine sections 9-16
    
    // Extract speed
    uint16_t speedCmS = (uint16_t)(data[1] << 8 | data[0]);
    vehicleSpeed = speedCmS * 0.036f; // Convert cm/s to km/h
    
    // Extract status
    uint8_t status = data[2];
    bool newAutosteerState = (status & 0x40) != 0;  // Bit 6 is autosteer enable
    
    
    // Track guidance status changes
    prevGuidanceStatus = guidanceActive;
    guidanceActive = (status & 0x01) != 0;           // Bit 0 is guidance active
    guidanceStatusChanged = (guidanceActive != prevGuidanceStatus);
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
    
    // Only print on state change to avoid spam
    if (newAutosteerState != autosteerEnabled) {
        LOG_INFO(EventSource::AUTOSTEER, "AgOpenGPS autosteer request: %s", 
                      newAutosteerState ? "ENGAGE" : "DISENGAGE");
        autosteerEnabled = newAutosteerState;
    }
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
        0x7D,                          // Source: Steer module
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
    // Get current steering angle - use VWAS if enabled and available
    if (configManager.getINSUseFusion() && wheelAngleFusionPtr && wheelAngleFusionPtr->isHealthy()) {
        currentAngle = wheelAngleFusionPtr->getFusedAngle();
    } else {
        // Fall back to physical WAS
        currentAngle = adProcessor.getWASAngle();
    }
    
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
    
    // Apply Ackerman fix to current angle if it's negative (left turn)
    actualAngle = currentAngle;
    if (actualAngle < 0) {
        actualAngle = actualAngle * steerSettings.AckermanFix;
        
        // Log Ackerman fix application periodically
        static uint32_t lastAckermanLog = 0;
        if (millis() - lastAckermanLog > 5000 && abs(actualAngle) > 1.0f) {
            lastAckermanLog = millis();
            LOG_DEBUG(EventSource::AUTOSTEER, "Ackerman fix applied: %.2f° * %.2f = %.2f°", 
                     currentAngle, steerSettings.AckermanFix, actualAngle);
        }
    }
    
    // Calculate angle error
    float angleError = actualAngle - targetAngle;
    float errorAbs = abs(angleError);
    
    // Calculate base PWM output (error * Kp)
    int16_t pValue = steerSettings.Kp * angleError;
    
    // Apply PWM calculation similar to V6
    if (steerSettings.highPWM > 0) {  // Check if we have valid settings
        // Start with base P value
        int16_t pwmDrive = pValue;
        
        // Add min throttle factor so no delay from motor resistance
        if (pwmDrive < 0) {
            pwmDrive -= steerSettings.minPWM;
        } else if (pwmDrive > 0) {
            pwmDrive += steerSettings.minPWM;
        } else {
            // Dead zone
            motorPWM = 0;
            return;
        }
        
        // Limit the PWM drive to highPWM setting
        if (pwmDrive > steerSettings.highPWM) {
            pwmDrive = steerSettings.highPWM;
        } else if (pwmDrive < -steerSettings.highPWM) {
            pwmDrive = -steerSettings.highPWM;
        }
        
        // Store final PWM value
        motorPWM = pwmDrive;
        
        // Log the PWM calculation
        LOG_DEBUG(EventSource::AUTOSTEER, "PWM calc: actual=%.1f° - target=%.1f° = error=%.1f° * Kp=%d = %d, +minPWM=%d, limit=%d, final=%d", 
                 actualAngle, targetAngle, angleError, steerSettings.Kp, pValue, steerSettings.minPWM, steerSettings.highPWM, pwmDrive);
        
        // Debug log final motor PWM periodically
        static uint32_t lastMotorPWMLog = 0;
        if (millis() - lastMotorPWMLog > 1000 && abs(motorPWM) > 10) {
            lastMotorPWMLog = millis();
            LOG_DEBUG(EventSource::AUTOSTEER, "Motor PWM: %d (highPWM=%d)", 
                      motorPWM, steerSettings.highPWM);
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
                    int16_t softStartLimit = (int16_t)(steerSettings.lowPWM * softStartMaxPWM * sineRamp);
                    
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
        LOG_DEBUG(EventSource::AUTOSTEER, "PWM Settings: highPWM=%d, lowPWM=%d, minPWM=%d", 
                  steerSettings.highPWM, steerSettings.lowPWM, steerSettings.minPWM);
    }
    
    // Motor speed is now properly scaled to respect highPWM limit
    // No additional limiting needed here since we scale by newHighPWM above
    
    
    // Send to motor
    if (motorPTR) {
        // Only enable motor if not in disabled state
        if (motorState != MotorState::DISABLED) {
            motorPTR->enable(true);
            motorPTR->setPWM(motorPWM);
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
                  (vehicleSpeed > 0.1f);      // Moving (TODO: use MinSpeed from config)
    
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

