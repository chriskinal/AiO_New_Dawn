#ifndef KICKOUT_MONITOR_H
#define KICKOUT_MONITOR_H

#include <Arduino.h>
#include "MotorDriverInterface.h"

class ConfigManager;
class ADProcessor;
class MotorDriverInterface;
class EncoderProcessor;

class KickoutMonitor
{
public:
    enum KickoutReason
    {
        NONE = 0,
        ENCODER_OVERSPEED = 1,
        PRESSURE_HIGH = 2,
        CURRENT_HIGH = 3,
        MOTOR_SLIP = 4,
        KEYA_SLIP = 5,
        KEYA_ERROR = 6,
        JD_PWM_MOTION = 7
    };

    KickoutMonitor();
    ~KickoutMonitor();

    // Get singleton instance
    static KickoutMonitor *getInstance();

    // Initialize kickout monitoring with motor driver reference
    bool init(MotorDriverInterface *driver = nullptr);

    // Set motor driver (can be called after init)
    void setMotorDriver(MotorDriverInterface *driver) { motorDriver = driver; }

    // Process kickout checks (call frequently)
    void process();

    // Send PGN250 turn sensor data
    void sendPGN250();

    // Kickout status
    bool hasKickout() const { return kickoutActive; }
    KickoutReason getReason() const { return kickoutReason; }
    const char *getReasonString() const;

    // Clear kickout (after steering disabled)
    void clearKickout();

    // Get current sensor value for PGN250 based on active turn sensor type
    uint8_t getTurnSensorReading() const;

    // Get current counts for debugging
    uint32_t getEncoderPulseCount() const { return encoderPulseCount; }
    uint16_t getPressureReading() const { return lastPressureReading; }
    uint16_t getCurrentReading() const { return lastCurrentReading; }

private:
    // Singleton instance
    static KickoutMonitor *instance;

    // External dependencies
    ConfigManager *configMgr;
    ADProcessor *adProcessor;
    MotorDriverInterface *motorDriver;
    EncoderProcessor *encoderProc;

    // Encoder monitoring
    uint32_t encoderPulseCount;
    uint32_t lastPulseCheck;
    uint32_t lastPulseCount;
    bool lastEncoderState;

    // PGN250 timing
    uint32_t lastPGN250Time;
    static constexpr uint32_t PGN250_INTERVAL_MS = 100; // Send at 10Hz

    // Sensor readings
    uint16_t lastPressureReading;
    uint16_t lastCurrentReading;

    // Current sensor spike filtering
    uint32_t currentHighStartTime;
    static constexpr uint32_t CURRENT_SPIKE_FILTER_MS = 1000; // 1 second - enough time for intentional wheel grab

    // Kickout state
    bool kickoutActive;
    KickoutReason kickoutReason;
    uint32_t kickoutTime;

    // Check individual kickout conditions
    bool checkEncoderKickout();
    bool checkPressureKickout();
    bool checkCurrentKickout();
    bool checkMotorSlipKickout();
    bool checkJDPWMKickout();
};

#endif // KICKOUT_MONITOR_H