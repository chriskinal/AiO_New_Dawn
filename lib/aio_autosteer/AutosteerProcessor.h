#ifndef AUTOSTEER_PROCESSOR_H
#define AUTOSTEER_PROCESSOR_H

#include <Arduino.h>
// PIDController removed - functionality absorbed into AutosteerProcessor

// External pointers
class ADProcessor;
extern ADProcessor adProcessor;

class MotorDriverInterface;
extern MotorDriverInterface motorDriver;

class KickoutMonitor;

// PGN data is parsed directly to ConfigManager
// No intermediate structs needed

class AutosteerProcessor {
private:
    static AutosteerProcessor* instance;
    
    // Pin definitions
    static constexpr uint8_t BUTTON_PIN = 2;  // Physical button on pin 2
    
    // Private constructor for singleton
    AutosteerProcessor();
    
    // Configuration storage
    
    // State tracking
    bool autosteerEnabled = false;
    float targetAngle = 0.0f;
    uint32_t lastPGN254Time = 0;
    
    // PGN 254 data
    float vehicleSpeed = 0.0f;      // km/h
    bool guidanceActive = false;     // Guidance line active
    int8_t crossTrackError = 0;      // cm off line
    uint16_t machineSections = 0;    // 16 section states
    
    // Button state tracking
    bool physicalButtonState = false;     // Current physical button state
    bool lastPhysicalButtonState = false; // Previous physical button state
    uint32_t lastButtonDebounceTime = 0;  // Debounce timer
    static constexpr uint32_t DEBOUNCE_DELAY = 50; // 50ms debounce
    
    // Autosteer loop timing - Now handled by SimpleScheduler at 100Hz
    
    // Steer state management
    uint8_t steerState = 1;              // 0 = steering active, 1 = steering inactive
    bool prevGuidanceStatus = false;     // Previous guidance status from AgOpenGPS
    bool guidanceStatusChanged = false;  // Flag for guidance status change
    
    // Motor control
    float currentAngle = 0.0f;           // Current WAS angle
    float actualAngle = 0.0f;            // Ackerman-corrected angle
    int16_t motorPWM = 0;                // Current motor PWM command (-255 to +255)
    
    // Watchdog
    uint32_t lastCommandTime = 0;        // Last time we received PGN 254
    static constexpr uint32_t WATCHDOG_TIMEOUT = 2000; // 2 seconds
    
    // Kickout
    uint32_t kickoutTime = 0;            // Time of last kickout
    static constexpr uint32_t KICKOUT_COOLDOWN_MS = 2000; // 2 second cooldown
    KickoutMonitor* kickoutMonitor = nullptr;
    
    // Soft-start motor control
    enum class MotorState {
        DISABLED,
        SOFT_START,
        NORMAL_CONTROL
    };
    
    MotorState motorState = MotorState::DISABLED;
    uint32_t softStartBeginTime = 0;
    float softStartRampValue = 0.0f;
    
    // Soft-start parameters (configurable via Web UI)
    uint16_t softStartDurationMs = 175;     // Duration of soft-start ramp (150-200ms range)
    float softStartMaxPWM = 0.4f;           // Maximum PWM during soft-start (percentage of lowPWM)
    
    // Link state tracking
    bool linkWasDown = false;               // Track if link was down
    
    // Motor config change tracking
    uint8_t previousMotorConfig = 0xFF;     // Previous motor config byte
    int8_t previousCytronDriver = -1;       // Previous Cytron bit state
    bool motorConfigInitialized = false;    // Track if we've initialized from EEPROM
    
    
public:
    // Singleton access
    static AutosteerProcessor* getInstance();
    
    // Initialization
    bool init();
    void process();
    void initializeFusion();  // Initialize sensor fusion separately
    
    // PGN handlers
    void handleBroadcastPGN(uint8_t pgn, const uint8_t* data, size_t len);
    void handleSteerConfig(uint8_t pgn, const uint8_t* data, size_t len);
    void handleSteerSettings(uint8_t pgn, const uint8_t* data, size_t len);
    void handleSteerData(uint8_t pgn, const uint8_t* data, size_t len);
    
    // Send replies
    void sendHelloReply();
    void sendScanReply();
    
    // Button handling
    void readPhysicalButton();
    void sendButtonStateToAOG(bool buttonPressed);
    
    // Send PGN 253 status to AgOpenGPS
    void sendPGN253();
    
    // Motor control
    void updateMotorControl();
    void emergencyStop();
    bool shouldSteerBeActive() const;
    
    // Static callback wrapper for PGN registration
    static void handlePGNStatic(uint8_t pgn, const uint8_t* data, size_t len);
    
    
    // Public getters for state
    bool isEnabled() const { return autosteerEnabled; }
    float getTargetAngle() const { return targetAngle; }

    // Public getter for PGN254 vehicle speed 
    float getVehicleSpeed() const { return vehicleSpeed; }

    // Soft-start configuration
    uint16_t getSoftStartDuration() const { return softStartDurationMs; }
    void setSoftStartDuration(uint16_t durationMs) { 
        softStartDurationMs = constrain(durationMs, 0, 500); // Max 500ms
    }
    
    float getSoftStartMaxPWM() const { return softStartMaxPWM; }
    void setSoftStartMaxPWM(float maxPWM) { 
        softStartMaxPWM = constrain(maxPWM, 0.0f, 1.0f); // 0-100% of lowPWM
    }
};

// Global instance
extern AutosteerProcessor autosteerProcessor;

#endif // AUTOSTEER_PROCESSOR_H