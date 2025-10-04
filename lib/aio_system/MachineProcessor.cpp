#include "MachineProcessor.h"
#include "HardwareManager.h"
#include <Arduino.h>
#include "PGNProcessor.h"
#include "PGNUtils.h"
#include <Wire.h>
#include "Adafruit_PWMServoDriver.h"
#include "EventLogger.h"
#include "QNetworkBase.h"
#include <EEPROM.h>
#include "EEPROMLayout.h"
#include "ConfigManager.h"

extern void sendUDPbytes(uint8_t *message, int msgLen);

// Network configuration now handled by ConfigManager

// External config manager instance
extern ConfigManager configManager;

// PCA9685 PWM driver for section outputs - moved to static instance
// Note: Front panel LEDs use 0x70 on Wire, sections use 0x44

// Hardware pin mappings from schematic
// Section signal pins on PCA9685 (control the actual sections)
const uint8_t SECTION_PINS[6] = {0, 1, 4, 5, 10, 9};  // SEC1_SIG through SEC6_SIG

// DRV8243 control pins on PCA9685
const uint8_t DRVOFF_PINS[3] = {2, 6, 8};    // DRVOFF pins (must be LOW to enable)

// DRV8243 sleep pins - these need a reset pulse to activate
const uint8_t SLEEP_PINS[3] = {
    13, // Section 1/2 nSLEEP
    3,  // Section 3/4 nSLEEP
    7   // Section 5/6 nSLEEP
};

// Special DRV8243 sleep pins for LOCK and AUX (from NG-V6)
const uint8_t AUX_SLEEP_PIN = 15;    // AUX nSLEEP on PCA9685
const uint8_t LOCK_SLEEP_PIN = 14;   // LOCK nSLEEP on PCA9685

// Hardware configuration:
// - nSLEEP: Also has 4.7K pullup, but needs reset pulse
// - MODE: Pulled LOW through 8.2K resistor (independent mode)
// - Solder jumpers OPEN = Independent mode

// In independent mode:
// - Each INx directly controls OUTx
// - HIGH on INx = OUTx sources current (LED ON, Output pushed HIGH)
// - LOW on INx = OUTx sinks current (LED OFF, Output pulled LOW)

// Static instance pointers
MachineProcessor* MachineProcessor::instance = nullptr;
MachineProcessor* machinePTR = nullptr;

// Static member to avoid global constructor
static Adafruit_PWMServoDriver& getSectionOutputs() {
    static Adafruit_PWMServoDriver sectionOutputs(0x44, Wire);  // Changed from Wire1 to Wire
    return sectionOutputs;
}

MachineProcessor::MachineProcessor() {
    LOG_DEBUG(EventSource::MACHINE, "Constructor called");
}

MachineProcessor* MachineProcessor::getInstance() {
    if (instance == nullptr) {
        instance = new MachineProcessor();
        machinePTR = instance;
    }
    return instance;
}

bool MachineProcessor::init() {
    LOG_INFO(EventSource::MACHINE, "Initializing MachineProcessor (Phase 4 - Full functionality with EEPROM)");
    getInstance();
    return instance->initialize();
}

bool MachineProcessor::initialize() {
    LOG_INFO(EventSource::MACHINE, "Initializing...");
    
    // Clear initial state
    memset(&machineState, 0, sizeof(machineState));
    memset(&pinConfig, 0, sizeof(pinConfig));
    configReceived = false;
    
    // Set default pin assignments (pins 1-6 = sections 1-6)
    for (int i = 1; i <= 6; i++) {
        pinConfig.pinFunction[i] = i;  // Default: pin N controls section N
    }
    
    // Load saved configuration from EEPROM
    loadPinConfig();
    // Machine config is loaded by ConfigManager at startup
    
    // Log loaded configuration
    LOG_INFO(EventSource::MACHINE, "Loaded config: RaiseTime=%ds, LowerTime=%ds, HydEnable=%d, ActiveHigh=%d",
             configManager.getRaiseTime(), configManager.getLowerTime(), 
             configManager.getHydraulicLift(), configManager.getIsPinActiveHigh());
    
    // If we have valid config from EEPROM, mark it as received
    if (configManager.getRaiseTime() > 0 && configManager.getLowerTime() > 0) {
        configReceived = true;
        LOG_INFO(EventSource::MACHINE, "Valid config loaded from EEPROM - hydraulic functions enabled");
    }
    
    machineState.lastPGN239Time = 0;
    
    // Initialize hardware
    if (!initializeSectionOutputs()) {
        LOG_ERROR(EventSource::MACHINE, "Failed to initialize section outputs!");
        return false;
    }
    
    // Register PGN handlers
    LOG_DEBUG(EventSource::MACHINE, "Registering PGN callbacks...");
    // Register for broadcast PGNs (200, 202)
    bool regBroadcast = PGNProcessor::instance->registerBroadcastCallback(handleBroadcastPGN, "Machine");
    bool reg236 = PGNProcessor::instance->registerCallback(MACHINE_PGN_PIN_CONFIG, handlePGN236, "Machine-PinConfig");
    bool reg238 = PGNProcessor::instance->registerCallback(MACHINE_PGN_CONFIG, handlePGN238, "Machine-Config");
    bool reg239 = PGNProcessor::instance->registerCallback(MACHINE_PGN_DATA, handlePGN239, "Machine-Data");
    LOG_INFO(EventSource::MACHINE, "PGN registrations - Broadcast:%d, 236:%d, 238:%d, 239:%d", 
             regBroadcast, reg236, reg238, reg239);
    
    LOG_INFO(EventSource::MACHINE, "Initialized successfully");
    return true;
}

bool MachineProcessor::initializeSectionOutputs() {
    LOG_DEBUG(EventSource::MACHINE, "Initializing section outputs...");
    
    // 1. Check for PCA9685 at expected address
    if (!checkPCA9685()) {
        return false;
    }
    
    // 2. Initialize PCA9685
    getSectionOutputs().begin();
    
    // Request higher I2C speed through HardwareManager
    HardwareManager* hwMgr = HardwareManager::getInstance();
    if (hwMgr->requestI2CSpeed(HardwareManager::I2C_BUS_0, 1000000, "MachineProcessor")) {
        Wire.setClock(1000000);  // Set to 1MHz for PCA9685
    } else {
        LOG_WARNING(EventSource::MACHINE, "Failed to set I2C speed to 1MHz, using current speed");
    }
    
    // 3. Wake PCA9685 from sleep mode
    getSectionOutputs().reset();  // This clears MODE1 sleep bit
    delay(1);  // Oscillator stabilization
    
    // 4. Configure PCA9685
    getSectionOutputs().setPWMFreq(1526);  // Max frequency
    getSectionOutputs().setOutputMode(true);  // Push-pull outputs
    
    // Set MODE2_OCH bit to update outputs on ACK instead of STOP
    // This may help with missed pulses during rapid updates
    Wire.beginTransmission(0x44);
    Wire.write(0x01);  // MODE2 register
    Wire.write(0x04 | 0x08);  // OUTDRV (push-pull) | OCH (update on ACK)
    Wire.endTransmission();
    
    // 5. Put all DRV8243s to sleep initially (including LOCK and AUX)
    LOG_DEBUG(EventSource::MACHINE, "Putting all DRV8243 drivers to sleep");
    for (uint8_t pin : SLEEP_PINS) {
        getSectionOutputs().setPin(pin, 0, 0); // Set LOW for sleep mode
    }
    // Also put LOCK and AUX to sleep
    getSectionOutputs().setPin(LOCK_SLEEP_PIN, 0, 0); // LOCK sleep
    getSectionOutputs().setPin(AUX_SLEEP_PIN, 0, 0);  // AUX sleep
    
    delayMicroseconds(150); // Wait for sleep mode to settle
    
    // 6. Set all section outputs to their OFF state before waking drivers
    // For active low outputs, OFF means HIGH
    LOG_DEBUG(EventSource::MACHINE, "Setting all outputs to OFF state (considering active high/low)");
    
    // Clear all function states first
    memset(machineState.functions, 0, sizeof(machineState.functions));
    machineState.sectionStates = 0;
    machineState.hydLift = 0;
    machineState.tramline = 0;
    machineState.geoStop = 0;
    
    // Check EEPROM for Danfoss configuration (the golden source)
    uint8_t motorConfig = configManager.getMotorDriverConfig();
    // 0x01 = Danfoss + Wheel Encoder, 0x03 = Danfoss + Pressure Sensor
    bool isDanfossConfigured = (motorConfig == 0x01 || motorConfig == 0x03);
    if (isDanfossConfigured) {
        LOG_INFO(EventSource::MACHINE, "Danfoss configuration detected (EEPROM: 0x%02X)", motorConfig);
    }
    
    // For each output, determine the OFF state based on its assigned function
    for (int outputNum = 1; outputNum <= 6; outputNum++) {
        uint8_t pcaPin = SECTION_PINS[outputNum - 1];
        uint8_t assignedFunction = pinConfig.pinFunction[outputNum];
        
        // Special handling for Danfoss outputs
        if (isDanfossConfigured) {
            if (outputNum == 5) {
                // Output 5 is Danfoss enable - start disabled (LOW)
                getSectionOutputs().setPin(pcaPin, 0, 0);
                LOG_INFO(EventSource::MACHINE, "Output 5 (Danfoss enable) set to LOW (disabled)");
                continue;
            } else if (outputNum == 6) {
                // Output 6 is Danfoss PWM control - set to 50% (center position)
                // 50% of 255 = 128, convert to 12-bit: (128 * 4095) / 255 = 2056
                setPinPWM(SECTION_PINS[5], 2056);  // Output 6 uses pin index 5
                LOG_INFO(EventSource::MACHINE, "Output 6 (Danfoss PWM) set to 50%% (centered)");
                continue;
            }
        }
        
        // Default OFF state
        uint16_t offValue = 0;  // LOW for sections and active high machine functions
        
        // If this is a machine function (17-21) with active low configuration
        if (assignedFunction >= 17 && assignedFunction <= 21 && !configManager.getIsPinActiveHigh()) {
            offValue = 4095;  // HIGH for active low machine functions when OFF
        }
        
        getSectionOutputs().setPin(pcaPin, offValue, 0);
        LOG_DEBUG(EventSource::MACHINE, "Output %d (pin %d, func %d) set to %s", 
                  outputNum, pcaPin, assignedFunction, offValue ? "HIGH" : "LOW");
    }
    
    // 7. Wake up LOCK and AUX first (like NG-V6)
    // LOCK still needs signal from Autosteer code before its output is HIGH
    LOG_INFO(EventSource::MACHINE, "Enabling LOCK DRV on pin %d, output controlled by Autosteer", LOCK_SLEEP_PIN);
    getSectionOutputs().setPin(LOCK_SLEEP_PIN, 187, 1); // LOW pulse, 187/4096 is 30µs at 1526Hz
    
    // AUX's output is HIGH as soon as it wakes up
    LOG_INFO(EventSource::MACHINE, "Enabling AUX Output on pin %d (always HIGH)", AUX_SLEEP_PIN);
    getSectionOutputs().setPin(AUX_SLEEP_PIN, 187, 1); // LOW pulse, 187/4096 is 30µs at 1526Hz
    
    // 7a. Then wake up the section DRV8243s with reset pulse
    LOG_DEBUG(EventSource::MACHINE, "Waking section DRV8243 drivers");
    getSectionOutputs().setPin(13, 187, 1); // Section 1/2 - 30µs LOW pulse
    getSectionOutputs().setPin(3, 187, 1);  // Section 3/4 - 30µs LOW pulse
    getSectionOutputs().setPin(7, 187, 1);  // Section 5/6 - 30µs LOW pulse
    
    // The actual LOCK control comes from Teensy SLEEP_PIN (pin 4)
    LOG_INFO(EventSource::MACHINE, "LOCK control via Teensy pin 4, DRV8243 awakened on PCA9685 pin %d", LOCK_SLEEP_PIN);
    
    // 8. Enable DRV8243 outputs by setting DRVOFF LOW
    LOG_DEBUG(EventSource::MACHINE, "Enabling DRV8243 outputs (DRVOFF = LOW)");
    for (uint8_t pin : DRVOFF_PINS) {
        getSectionOutputs().setPin(pin, 0, 0); // Set LOW to enable outputs
    }
    
    LOG_INFO(EventSource::MACHINE, "Section outputs initialized - all outputs OFF");
    return true;
}

bool MachineProcessor::checkPCA9685() {
    Wire.beginTransmission(0x44);
    uint8_t error = Wire.endTransmission();
    if (error == 0) {
        LOG_DEBUG(EventSource::MACHINE, "Found PCA9685 at 0x44");
        return true;
    }
    LOG_ERROR(EventSource::MACHINE, "PCA9685 not found at 0x44!");
    return false;
}

void MachineProcessor::process() {
    // Check ethernet link state
    static bool previousLinkState = true;
    bool currentLinkState = QNetworkBase::isConnected();
    
    if (previousLinkState && !currentLinkState) {
        // Link just went down - turn off all functions immediately
        LOG_INFO(EventSource::MACHINE, "All outputs turned off - ethernet link down");
        memset(machineState.functions, 0, sizeof(machineState.functions));
        machineState.sectionStates = 0;
        machineState.hydLift = 0;
        machineState.tramline = 0;
        machineState.geoStop = 0;
        updateMachineOutputs();
        
        // Clear the timer
        machineState.lastPGN239Time = 0;
    }
    previousLinkState = currentLinkState;
    
    // Watchdog timer - turn off all outputs if no PGN 239 for 2 seconds
    if (machineState.lastPGN239Time > 0 && 
        (millis() - machineState.lastPGN239Time) > 2000) {
        
        LOG_INFO(EventSource::MACHINE, "All outputs turned off - watchdog timeout");
        memset(machineState.functions, 0, sizeof(machineState.functions));
        machineState.sectionStates = 0;
        machineState.hydLift = 0;
        machineState.tramline = 0;
        machineState.geoStop = 0;
        updateMachineOutputs();
        
        // Reset timer to prevent repeated messages
        machineState.lastPGN239Time = 0;
    }
    
    // Hydraulic timing - auto shutoff after configured time
    if (configManager.getHydraulicLift() && configReceived) {
        // Check for timeout on active one-shot timer
        if (machineState.hydLift != 0 && machineState.hydStartTime > 0) {
            uint32_t maxTime = (machineState.hydLift == 2) ? 
                               configManager.getRaiseTime() : configManager.getLowerTime();
            uint32_t elapsed = millis() - machineState.hydStartTime;
            
            if (elapsed > (maxTime * 1000)) {
                // Timeout - turn off hydraulic
                LOG_INFO(EventSource::MACHINE, "*** Hydraulic AUTO-SHUTOFF after %d seconds (elapsed=%dms) ***", 
                         maxTime, elapsed);
                machineState.hydLift = 0;
                machineState.hydStartTime = 0;
                // DO NOT reset lastHydLift - we need to remember the last command from AgOpenGPS
                // to prevent retriggering when AgOpenGPS continues sending the same command
                updateFunctionStates();
                updateMachineOutputs();
                
            }
        }
    }
}

void MachineProcessor::handleBroadcastPGN(uint8_t pgn, const uint8_t* data, size_t len) {
    if (!instance) {
        LOG_ERROR(EventSource::MACHINE, "No instance for broadcast PGN!");
        return;
    }

    // Check if onboard section control should respond
    if (!instance->isOnboardSectionControlActive()) {
        // Sleep mode active - don't respond to hello or scan requests
        return;
    }

    if (pgn == 200) {
        
        uint8_t helloReply[] = {
            0x80, 0x81,               // Header
            MACHINE_HELLO_REPLY,      // Source: Machine module (123)
            MACHINE_HELLO_REPLY,      // PGN: Machine reply (123)
            5,                        // Length
            0, 0, 0, 0, 0,           // Data
            0                         // CRC placeholder
        };
        
        calculateAndSetCRC(helloReply, sizeof(helloReply));
        sendUDPbytes(helloReply, sizeof(helloReply));
        
    }
    else if (pgn == 202) {
        
        uint8_t ip[4];
        configManager.getIPAddress(ip);
        
        uint8_t scanReply[] = {
            0x80, 0x81,                    // Header
            MACHINE_HELLO_REPLY,           // Source: Machine module (123)
            0xCB,                          // PGN: 203 Scan reply
            7,                             // Length
            ip[0],
            ip[1],
            ip[2],
            ip[3],
            ip[0],        // Subnet (repeat IP)
            ip[1],
            ip[2],
            0                              // CRC placeholder
        };
        
        calculateAndSetCRC(scanReply, sizeof(scanReply));
        sendUDPbytes(scanReply, sizeof(scanReply));
        
    }
}



void MachineProcessor::handlePGN239(uint8_t pgn, const uint8_t* data, size_t len) {
    if (!instance) return;
    
    // First check if this is a broadcast PGN
    if (pgn == 200 || pgn == 202) {
        handleBroadcastPGN(pgn, data, len);
        return;
    }
    
    // Need at least 12 bytes for all machine data
    // PGN 239 format: uturn(5), speed(6), hydLift(7), tram(8), geo(9), reserved(10), SC1-8(11), SC9-16(12)
    if (len < 8) return;
    
    // Update watchdog timer
    instance->machineState.lastPGN239Time = millis();
    
    
    
    // For Phase 1, just log the data we're receiving
    // Log machine control data for Phase 1
    if (len >= 5) {
        // uint8_t uturn = data[0];   // Byte 5 - not used yet
        // uint8_t speed = data[1];   // Byte 6 - speed * 10
        
        if (len >= 5) {  // Have hydraulic, tram, geo data
            uint8_t hydLift = data[2];   // Byte 7: 0=off, 1=down, 2=up
            uint8_t tram = data[3];      // Byte 8: bit0=right, bit1=left
            uint8_t geoStop = data[4];   // Byte 9: 0=inside, 1=outside
            
            
            // Store machine control data
            // For hydraulic: implement one-shot timer logic
            // uint8_t prevHydLift = instance->machineState.hydLift;  // Not needed - using lastHydLift instead
            
            
            // Check for state change that should trigger a one-shot timer
            
            // Only process hydraulic if enabled
            if (configManager.getHydraulicLift() && instance->configReceived) {
                // Check if this is a new command (different from last command from AgOpenGPS)
                if (hydLift != instance->machineState.lastHydLift) {
                    // Command changed
                    if (hydLift != 0) {
                        // New raise or lower command - start timer
                        instance->machineState.hydLift = hydLift;
                        instance->machineState.hydStartTime = millis();
                        LOG_INFO(EventSource::MACHINE, "*** Hydraulic %s one-shot STARTED for %d seconds ***",
                                 hydLift == 2 ? "RAISE" : "LOWER",
                                 hydLift == 2 ? configManager.getRaiseTime() : configManager.getLowerTime());
                    } else {
                        // Command went to 0 - clear everything
                        instance->machineState.hydLift = 0;
                        instance->machineState.hydStartTime = 0;
                    }
                    // Update last command
                    instance->machineState.lastHydLift = hydLift;
                } else {
                    // Same command as before - ignore it
                }
            } else {
            }
            
            instance->machineState.tramline = tram;
            instance->machineState.geoStop = geoStop;
        }
    }
    
    if (len >= 8) {
        // Extract section states from bytes 11 & 12 (array indices 6 & 7)
        // If in sleep mode, ignore external section control commands
        uint16_t sectionStates;
        if (instance->isOnboardSectionControlActive()) {
            sectionStates = data[6] | (data[7] << 8);
        } else {
            sectionStates = 0;  // Sleep mode - turn off all sections
        }

        // Track if any states changed
        bool statesChanged = false;
        
        // Check if section states changed
        if (sectionStates != instance->machineState.sectionStates) {
            instance->machineState.sectionStates = sectionStates;
            statesChanged = true;
        }
        
        // Update all function states
        instance->updateFunctionStates();
        
        // Check if any function changed
        if (instance->machineState.functionsChanged) {
            statesChanged = true;
            instance->machineState.functionsChanged = false;  // Reset flag
        }
        
        // Only log and update outputs if something changed
        if (statesChanged) {
            // Rate limit the "Active functions" logging to prevent spam during hydraulic operations
            static uint32_t lastActiveFunctionLog = 0;
            uint32_t now = millis();
            bool shouldLogActive = (now - lastActiveFunctionLog) >= 1000; // Log at most once per second
            
            if (shouldLogActive) {
                lastActiveFunctionLog = now;
                
                // Show active functions for our 6 outputs
                char activeMsg[256];
                snprintf(activeMsg, sizeof(activeMsg), "Active functions:");
                
                // Check what function each output is assigned to
                for (int pin = 1; pin <= 6; pin++) {
                    uint8_t func = instance->pinConfig.pinFunction[pin];
                    if (func > 0 && func <= MAX_FUNCTIONS) {
                        if (instance->machineState.functions[func]) {
                            char buf[50];
                            snprintf(buf, sizeof(buf), " Out%d=%s", pin, instance->getFunctionName(func));
                            strncat(activeMsg, buf, sizeof(activeMsg) - strlen(activeMsg) - 1);
                        }
                    }
                }
                
                LOG_INFO(EventSource::MACHINE, "%s", activeMsg);
            }
            
            
            // Update outputs using new unified handler
            instance->updateMachineOutputs();
        }
    }
    
}



// Deprecated - replaced by updateMachineOutputs()
void MachineProcessor::updateSectionOutputs() {
    updateMachineOutputs();
}
void MachineProcessor::handlePGN236(uint8_t pgn, const uint8_t* data, size_t len) {
    if (!instance) return;
    
    // PGN 236 - Machine Pin Config
    // Expected length: 30 bytes (5 header + 24 pin configs + 1 reserved)
    if (len < 24) {
        LOG_ERROR(EventSource::MACHINE, "PGN 236 too short: %d bytes", len);
        return;
    }
    
    LOG_INFO(EventSource::MACHINE, "PGN 236 - Machine Pin Config received");
    
    // Parse pin function assignments (bytes 0-23 map to pins 1-24)
    for (int i = 0; i < 24 && i < len; i++) {
        uint8_t function = data[i];
        
        // Validate function number (0=unassigned, 1-21=valid functions)
        if (function > MAX_FUNCTIONS) {
            LOG_WARNING(EventSource::MACHINE, "Pin %d: Invalid function %d (max %d)", 
                        i + 1, function, MAX_FUNCTIONS);
            function = 0;  // Set to unassigned
        }
        
        instance->pinConfig.pinFunction[i + 1] = function;
        
        // Log assignments for first 6 pins (our physical outputs)
        if (i < 6) {
            LOG_INFO(EventSource::MACHINE, "Output %d assigned to %s", 
                     i + 1, instance->getFunctionName(function));
        }
    }
    
    instance->pinConfig.configReceived = true;
    
    // Log configuration summary if both configs received
    if (instance->configReceived) {
        LOG_INFO(EventSource::MACHINE, "Machine configuration complete:");
        for (int i = 1; i <= 6; i++) {
            uint8_t func = instance->pinConfig.pinFunction[i];
            LOG_INFO(EventSource::MACHINE, "  Output %d -> %s", i, instance->getFunctionName(func));
        }
    }
    
    // Save to EEPROM
    instance->savePinConfig();
}

void MachineProcessor::handlePGN238(uint8_t pgn, const uint8_t* data, size_t len) {
    if (!instance) return;
    
    // PGN 238 - Machine Config
    // Expected length: 14 bytes (5 header + 8 config + 1 reserved)
    if (len < 8) {
        LOG_ERROR(EventSource::MACHINE, "PGN 238 too short: %d bytes", len);
        return;
    }
    
    LOG_INFO(EventSource::MACHINE, "PGN 238 - Machine Config received");
    
    // Parse PGN 238 data directly to local variables
    uint8_t raiseTime = data[0];        // Byte 5
    uint8_t lowerTime = data[1];        // Byte 6
    // Byte 7 is not used for hydraulic enable - skip data[2]
    
    // Byte 8: bit 0 = relay active state, bit 1 = hydraulic enable
    uint8_t byte8 = data[3];
    bool isPinActiveHigh = (byte8 & 0x01);  // Bit 0: relay active high/low
    bool hydEnable = (byte8 & 0x02) >> 1;   // Bit 1: hydraulic enable
    
    uint8_t user1 = data[4];            // Byte 9
    uint8_t user2 = data[5];            // Byte 10
    uint8_t user3 = data[6];            // Byte 11
    uint8_t user4 = data[7];            // Byte 12
    
    instance->configReceived = true;
    
    LOG_INFO(EventSource::MACHINE, "Machine Config: RaiseTime=%ds, LowerTime=%ds, HydEnable=%d, ActiveHigh=%d (byte8=0x%02X)",
             raiseTime, lowerTime, hydEnable, isPinActiveHigh, byte8);
    
    LOG_DEBUG(EventSource::MACHINE, "User values: U1=%d, U2=%d, U3=%d, U4=%d",
              user1, user2, user3, user4);
    
    // Save to ConfigManager
    configManager.setRaiseTime(raiseTime);
    configManager.setLowerTime(lowerTime);
    configManager.setHydraulicLift(hydEnable);
    configManager.setIsPinActiveHigh(isPinActiveHigh);
    configManager.setUser1(user1);
    configManager.setUser2(user2);
    configManager.setUser3(user3);
    configManager.setUser4(user4);
    
    // Save to EEPROM
    LOG_INFO(EventSource::MACHINE, "Saving machine configuration to EEPROM...");
    configManager.saveMachineConfig();
    
    // Update all outputs immediately with new active high/low setting
    LOG_INFO(EventSource::MACHINE, "Updating all outputs with new active high/low setting");
    instance->updateMachineOutputs();
}

const char* MachineProcessor::getFunctionName(uint8_t functionNum) {
    static const char* functionNames[] = {
        "Unassigned",     // 0
        "Section 1",      // 1
        "Section 2",      // 2
        "Section 3",      // 3
        "Section 4",      // 4
        "Section 5",      // 5
        "Section 6",      // 6
        "Section 7",      // 7
        "Section 8",      // 8
        "Section 9",      // 9
        "Section 10",     // 10
        "Section 11",     // 11
        "Section 12",     // 12
        "Section 13",     // 13
        "Section 14",     // 14
        "Section 15",     // 15
        "Section 16",     // 16
        "Hyd Up",         // 17
        "Hyd Down",       // 18
        "Tramline Right", // 19
        "Tramline Left",  // 20
        "Geo Stop"        // 21
    };
    
    if (functionNum <= MAX_FUNCTIONS) {
        return functionNames[functionNum];
    }
    return "Invalid";
}

void MachineProcessor::updateFunctionStates() {
    // Save previous states to detect changes
    bool previousStates[MAX_FUNCTIONS + 1];
    memcpy(previousStates, machineState.functions, sizeof(previousStates));
    
    // Clear all function states first
    memset(machineState.functions, 0, sizeof(machineState.functions));
    
    // Map section states to functions 1-16
    for (int i = 0; i < 16; i++) {
        machineState.functions[i + 1] = (machineState.sectionStates & (1 << i)) != 0;
    }
    
    // Map hydraulic states to functions 17-18
    // hydLift: 0=off, 1=down, 2=up
    if (machineState.hydLift == 2) {
        machineState.functions[17] = true;  // Hyd Up
        machineState.functions[18] = false; // Hyd Down
    } else if (machineState.hydLift == 1) {
        machineState.functions[17] = false; // Hyd Up
        machineState.functions[18] = true;  // Hyd Down
    } else {
        machineState.functions[17] = false; // Hyd Up
        machineState.functions[18] = false; // Hyd Down
    }
    
    // Map tramline bits to functions 19-20
    // tramline: bit0=right, bit1=left
    machineState.functions[19] = (machineState.tramline & 0x01) != 0; // Tram Right
    machineState.functions[20] = (machineState.tramline & 0x02) != 0; // Tram Left
    
    // Map geo stop to function 21
    // geoStop: 0=inside boundary, 1=outside boundary
    machineState.functions[21] = (machineState.geoStop != 0);
    
    // Check if any function state changed
    machineState.functionsChanged = false;
    for (int i = 1; i <= MAX_FUNCTIONS; i++) {
        if (machineState.functions[i] != previousStates[i]) {
            machineState.functionsChanged = true;
            
        }
    }
}

void MachineProcessor::updateMachineOutputs() {
    // Phase 3: Full machine output control
    
    // Check EEPROM for Danfoss configuration (the golden source)
    uint8_t motorConfig = configManager.getMotorDriverConfig();
    // 0x01 = Danfoss + Wheel Encoder, 0x03 = Danfoss + Pressure Sensor
    bool isDanfossConfigured = (motorConfig == 0x01 || motorConfig == 0x03);
    
    // Loop through our 6 physical outputs
    for (int outputNum = 1; outputNum <= 6; outputNum++) {
        // Skip outputs 5 & 6 if Danfoss is configured - they're controlled by DanfossMotorDriver
        if (isDanfossConfigured && (outputNum == 5 || outputNum == 6)) {
            static bool loggedOnce = false;
            if (!loggedOnce) {
                LOG_INFO(EventSource::MACHINE, "Skipping output %d - reserved for Danfoss valve control", outputNum);
                loggedOnce = true;
            }
            continue;
        }
        
        // Get the function assigned to this output pin
        uint8_t assignedFunction = pinConfig.pinFunction[outputNum];
        
        // Skip if no function assigned (0) or invalid
        if (assignedFunction == 0 || assignedFunction > MAX_FUNCTIONS) {
            continue;
        }
        
        // Get the state of the assigned function
        bool functionState = machineState.functions[assignedFunction];
        
        // Determine output state based on function type and settings
        bool outputState;
        
        // Apply active high/low setting to ALL functions (sections and machine)
        // isPinActiveHigh from AOG (PGN238 Byte 8 Bit 0):
        // - true: relay turns ON when pin goes HIGH  
        // - false: relay turns ON when pin goes LOW
        if (configManager.getIsPinActiveHigh()) {
            // Active high: function state directly maps to output
            outputState = functionState;
        } else {
            // Active low: invert the function state
            outputState = !functionState;
        }
        
        // Get the actual PCA9685 pin number for this output
        uint8_t pcaPin = SECTION_PINS[outputNum - 1];
        
        // Set the output (INVERTED to fix tester feedback)
        if (outputState) {
            getSectionOutputs().setPin(pcaPin, 0, 0);  // LOW when state is true
        } else {
            getSectionOutputs().setPin(pcaPin, 0, 1);  // HIGH when state is false
        }
        
    }
}


// Helper methods for clarity
void MachineProcessor::setPinHigh(uint8_t pin) {
    // For PCA9685: HIGH = no PWM, full ON
    getSectionOutputs().setPWM(pin, 4096, 0);
}

void MachineProcessor::setPinLow(uint8_t pin) {
    // For PCA9685: LOW = no PWM, full OFF
    getSectionOutputs().setPWM(pin, 0, 4096);
}

void MachineProcessor::setPinPWM(uint8_t pin, uint16_t pwmValue) {
    // For PCA9685: Set PWM value (0-4095)
    // pwmValue should be 0-4095 (12-bit resolution)
    // Use standard PWM mode: ON at 0, OFF at pwmValue
    getSectionOutputs().setPWM(pin, 0, pwmValue);
}

// EEPROM persistence methods
void MachineProcessor::savePinConfig() {
    // Save pin configuration starting at MACHINE_CONFIG_ADDR + 50
    // This leaves room for existing machine config at base address
    int addr = MACHINE_CONFIG_ADDR + 50;
    
    LOG_DEBUG(EventSource::MACHINE, "Saving pin config to EEPROM at address %d", addr);
    
    // Write a magic number to validate config
    uint16_t magic = 0xAA55;
    EEPROM.put(addr, magic);
    addr += sizeof(magic);
    
    // Write pin function array (24 bytes)
    for (int i = 1; i <= MAX_PIN_CONFIG; i++) {
        EEPROM.put(addr, pinConfig.pinFunction[i]);
        if (i <= 6) {
            LOG_DEBUG(EventSource::MACHINE, "  Saved pin %d = function %d (%s)", 
                      i, pinConfig.pinFunction[i], getFunctionName(pinConfig.pinFunction[i]));
        }
        addr++;
    }
    
    LOG_INFO(EventSource::MACHINE, "Pin configuration saved to EEPROM (24 pins, final addr=%d)", addr);
}

void MachineProcessor::loadPinConfig() {
    // Load pin configuration from EEPROM
    int addr = MACHINE_CONFIG_ADDR + 50;
    
    // Check magic number
    uint16_t magic;
    EEPROM.get(addr, magic);
    addr += sizeof(magic);
    
    if (magic == 0xAA55) {
        // Valid config found, load it
        for (int i = 1; i <= MAX_PIN_CONFIG; i++) {
            EEPROM.get(addr, pinConfig.pinFunction[i]);
            
            // Validate function number
            if (pinConfig.pinFunction[i] > MAX_FUNCTIONS) {
                pinConfig.pinFunction[i] = 0;  // Reset invalid
            }
            addr++;
        }
        
        pinConfig.configReceived = true;
        LOG_INFO(EventSource::MACHINE, "Pin configuration loaded from EEPROM");
        
        // Log first 6 assignments
        for (int i = 1; i <= 6; i++) {
            LOG_INFO(EventSource::MACHINE, "  Output %d -> %s", 
                     i, getFunctionName(pinConfig.pinFunction[i]));
        }
    } else {
        // No valid config, keep defaults
        LOG_INFO(EventSource::MACHINE, "No saved pin config, using defaults");
    }
}

void MachineProcessor::saveMachineConfig() {
    // This method is no longer needed - all machine config is now saved
    // through ConfigManager::saveMachineConfig()
    LOG_DEBUG(EventSource::MACHINE, "saveMachineConfig() deprecated - use ConfigManager");
}

void MachineProcessor::loadMachineConfig() {
    // This method is no longer needed - all machine config is now loaded
    // through ConfigManager::loadMachineConfig()
    LOG_DEBUG(EventSource::MACHINE, "loadMachineConfig() deprecated - use ConfigManager");
}

bool MachineProcessor::isOnboardSectionControlActive() const {
    // Check if section control sleep mode is enabled
    if (!configManager.getSectionControlSleepMode()) {
        return true;  // Sleep mode disabled - always active
    }

    // Sleep mode enabled - check for external section control activity
    // External SC is detected if we've recently received PGN 239 from a non-zero source
    // This is a simple heuristic: if PGN 239 comes from external source, we sleep
    // Note: This could be enhanced to track source IPs or have explicit wake/sleep commands

    // For now, sleep mode with this simple check:
    // If sleep mode is enabled, we assume external SC may be active
    // In practice, users would enable this setting when they have external SC
    return false;  // Sleep when enabled (external SC assumed active)
}

