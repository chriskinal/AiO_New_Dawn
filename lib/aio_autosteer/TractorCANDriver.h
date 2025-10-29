// TractorCANDriver.h - Unified CAN driver for all tractor brands
#ifndef TRACTOR_CAN_DRIVER_H
#define TRACTOR_CAN_DRIVER_H

#include "MotorDriverInterface.h"
#include "CANGlobals.h"
#include "EventLogger.h"
#include "ConfigManager.h"
#include "ConfigGlobals.h"

// Tractor brands enumeration (alphabetized except DISABLED)
enum class TractorBrand : uint8_t {
    DISABLED = 0,
    CASEIH_NH = 1,      // Case IH/New Holland
    CAT_MT = 2,         // CAT MT Series
    CLAAS = 3,
    FENDT = 4,          // Fendt SCR/S4/Gen6
    FENDT_ONE = 5,      // Fendt One
    GENERIC = 6,        // Generic (Keya)
    JCB = 7,
    LINDNER = 8,
    VALTRA_MASSEY = 9   // Valtra/Massey Ferguson
};

class TractorCANDriver : public MotorDriverInterface {
private:
    // Configuration
    CANSteerConfig config;

    // CAN bus pointers (assigned based on config)
    // Using void* to handle different template instantiations
    void* steerCAN = nullptr;
    void* buttonCAN = nullptr;
    void* hitchCAN = nullptr;

    // Track which bus numbers are assigned
    uint8_t steerBusNum = 0;
    uint8_t buttonBusNum = 0;
    uint8_t hitchBusNum = 0;

    // Common state
    bool enabled = false;
    int16_t targetPWM = 0;
    bool steerReady = false;
    uint32_t lastSteerReadyTime = 0;
    uint32_t lastCommandTime = 0;

    // Keya-specific state (used when brand == KEYA)
    float actualRPM = 0.0f;
    float commandedRPM = 0.0f;
    uint16_t motorPosition = 0;
    uint16_t motorCurrent = 0;
    uint16_t motorErrorCode = 0;
    bool heartbeatValid = false;
    uint32_t lastHeartbeat = 0;

    // Command alternation for Keya
    enum CommandState {
        SEND_ENABLE,
        SEND_SPEED
    };
    CommandState nextCommand = SEND_ENABLE;

    // Massey K_Bus tracking
    uint8_t mfRollingCounter[8] = {0};  // Track last K_Bus message for F1/F2
    bool engageButtonPressed = false;   // Track K_Bus engage button

    // Fendt K_Bus tracking
    bool fendtButtonPressed = false;    // Track Fendt armrest button

    // Case IH K_Bus tracking
    bool caseIHEngaged = false;         // Track Case IH engage state

    // CAT MT tracking
    bool catMTEngaged = false;          // Track CAT MT engage state

    // CLAAS tracking
    bool claasEngaged = false;          // Track CLAAS engage state

    // JCB tracking
    bool jcbEngaged = false;            // Track JCB engage state

    // Lindner tracking
    bool lindnerEngaged = false;        // Track Lindner engage state

    // Helper methods
    void assignCANBuses();
    void* getBusPointer(uint8_t busNum);
    bool readCANMessage(uint8_t busNum, CAN_message_t& msg);
    void writeCANMessage(uint8_t busNum, const CAN_message_t& msg);
    void processIncomingMessages();
    void sendSteerCommands();
    bool hasKeyaFunction() const;

    // Brand-specific message handlers
    void processKeyaMessage(const CAN_message_t& msg);
    void processCaseIHMessage(const CAN_message_t& msg);
    void processCaseIHKBusMessage(const CAN_message_t& msg);
    void processFendtMessage(const CAN_message_t& msg);
    void processFendtKBusMessage(const CAN_message_t& msg);
    void processValtraMessage(const CAN_message_t& msg);
    void processMasseyKBusMessage(const CAN_message_t& msg);
    void processCATMessage(const CAN_message_t& msg);
    void processCATKBusMessage(const CAN_message_t& msg);
    void processClaasMessage(const CAN_message_t& msg);
    void processClaasKBusMessage(const CAN_message_t& msg);
    void processJcbMessage(const CAN_message_t& msg);
    void processJcbKBusMessage(const CAN_message_t& msg);
    void processLindnerMessage(const CAN_message_t& msg);
    void processLindnerKBusMessage(const CAN_message_t& msg);

    // Brand-specific command senders
    void sendKeyaCommands();
    void sendCaseIHCommands();
    void sendFendtCommands();
    void sendValtraCommands();
    void sendCATCommands();
    void sendClaasCommands();
    void sendJcbCommands();
    void sendLindnerCommands();
    void sendMasseyF1();
    void sendMasseyF2();

public:
    TractorCANDriver() {}

    bool init() override;
    void enable(bool en) override;
    void setPWM(int16_t pwm) override;
    void stop() override;
    void process() override;
    MotorStatus getStatus() const override;

    // Configuration
    void setConfig(const CANSteerConfig& newConfig);
    CANSteerConfig getConfig() const { return config; }

    // Motor type identification
    MotorDriverType getType() const override { return MotorDriverType::TRACTOR_CAN; }
    const char* getTypeName() const override;
    bool hasCurrentSensing() const override { return false; }
    bool hasPositionFeedback() const override {
        return hasKeyaFunction();
    }

    // Detection and safety
    bool isDetected() override { return steerReady; }
    void handleKickout(KickoutType type, float value) override;
    float getCurrentDraw() override { return 0.0f; }

    // Keya-specific methods (for compatibility)
    float getActualRPM() const { return actualRPM; }
    float getCommandedRPM() const { return commandedRPM; }
    uint16_t getMotorPosition() const { return motorPosition; }
    bool hasRPMFeedback() const {
        return hasKeyaFunction() && heartbeatValid;
    }

    // Massey-specific methods
    bool isEngageButtonPressed() const { return engageButtonPressed; }
    void pressMasseyF1() { sendMasseyF1(); }
    void pressMasseyF2() { sendMasseyF2(); }

    // Fendt-specific methods
    bool isFendtButtonPressed() const { return fendtButtonPressed; }

    // Case IH-specific methods
    bool isCaseIHEngaged() const { return caseIHEngaged; }

    // CAT MT-specific methods
    bool isCATMTEngaged() const { return catMTEngaged; }

    // CLAAS-specific methods
    bool isClaasEngaged() const { return claasEngaged; }

    // JCB-specific methods
    bool isJcbEngaged() const { return jcbEngaged; }

    // Lindner-specific methods
    bool isLindnerEngaged() const { return lindnerEngaged; }

    // Valve ready status methods (for engagement safety check)
    bool isValveReady() const { return steerReady; }
    uint32_t getTimeSinceLastValveReady() const {
        return steerReady ? 0 : (millis() - lastSteerReadyTime);
    }

    // Heartbeat validity (for Keya/GENERIC brand)
    bool isHeartbeatValid() const { return heartbeatValid; }

    // Brand identification
    TractorBrand getCurrentBrand() const { return static_cast<TractorBrand>(config.brand); }
};

#endif // TRACTOR_CAN_DRIVER_H