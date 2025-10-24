#ifndef HARDWAREMANAGER_H_
#define HARDWAREMANAGER_H_

#include "Arduino.h"
#include <map>
#include <vector>
#include <functional>

/**
 * Hardware Manager - Unified Hardware Resource Management
 * 
 * This class provides:
 * 1. Central pin definitions
 * 2. Dynamic pin ownership tracking
 * 3. Shared resource coordination (PWM, ADC, I2C)
 * 4. Conflict detection and resolution
 * 
 * PIN OWNERSHIP MODEL:
 * - HardwareManager defines all pin numbers but does NOT initialize them
 * - Each module requests ownership before using pins
 * - Ownership can be transferred with proper cleanup
 * - Shared resources are coordinated to prevent conflicts
 * 
 * See docs/HARDWARE_OWNERSHIP_MATRIX.md for complete pin ownership details
 */

// Pin definitions - Central location for all hardware pins
// ANALOG PINS - Owner: ADProcessor
#define WAS_SENSOR_PIN A15    // Wheel Angle Sensor
#define CURRENT_PIN A13       // Motor current sensor
#define KICKOUT_A_PIN A12     // Pressure sensor (analog) / Encoder A (digital)
#define WORK_PIN A17          // Work switch input

// DIGITAL PINS - Various owners
#define SPEEDPULSE_PIN 33     // Owner: PWMProcessor
#define SPEEDPULSE10_PIN 37   // Owner: PWMProcessor
#define BUZZER 36             // Owner: HardwareManager
#define SLEEP_PIN 4           // Owner: MotorDriverInterface
#define PWM1_PIN 5            // Owner: PWMMotorDriver
#define PWM2_PIN 6            // Owner: PWMMotorDriver
#define STEER_PIN 2           // Owner: ADProcessor
#define KICKOUT_D_PIN 3       // Owner: EncoderProcessor/KickoutMonitor

class HardwareManager
{
public:
    // Pin ownership tracking
    enum PinOwner {
        OWNER_NONE = 0,
        OWNER_SYSTEM,
        OWNER_ADPROCESSOR,
        OWNER_PWMPROCESSOR,
        OWNER_PWMMOTORDRIVER,
        OWNER_CYTRONMOTORDRIVER,
        OWNER_ENCODERPROCESSOR,
        OWNER_KICKOUTMONITOR,
        OWNER_AUTOSTEER,
        OWNER_MACHINEPROCESSOR,
        OWNER_USER
    };

    // PWM timer groups on Teensy 4.1
    enum PWMTimerGroup {
        TIMER_GROUP_1,  // Pins 0, 1, 24, 25, 28, 29
        TIMER_GROUP_2,  // Pins 2, 3
        TIMER_GROUP_3,  // Pins 4, 33
        TIMER_GROUP_4,  // Pins 5
        TIMER_GROUP_5,  // Pins 6, 9, 10, 11, 12, 13, 32
        TIMER_GROUP_6,  // Pins 7, 8, 36, 37
        TIMER_GROUP_7,  // Pins 14, 15, 18, 19
        TIMER_GROUP_8,  // Pins 22, 23
        TIMER_GROUP_UNKNOWN
    };

    // ADC modules
    enum ADCModule {
        ADC_MODULE_0,
        ADC_MODULE_1,
        ADC_MODULE_BOTH
    };

    // I2C buses
    enum I2CBus {
        I2C_BUS_0,  // Wire
        I2C_BUS_1,  // Wire1
        I2C_BUS_2   // Wire2
    };

    struct PinInfo {
        PinOwner owner;
        const char* ownerName;
        uint8_t pinMode;
        bool isOwned;
    };

    struct PWMConfig {
        uint32_t frequency;
        uint8_t resolution;
        const char* owner;
    };

    struct ADCConfig {
        uint8_t resolution;
        uint8_t averaging;
        const char* owner;
    };

    struct I2CConfig {
        uint32_t clockSpeed;
        const char* owner;
    };

private:
    static HardwareManager *instance;
    bool isInitialized;
    uint8_t pwmFrequencyMode;
    
    // Pin ownership tracking
    std::map<uint8_t, PinInfo> pinOwnership;
    
    // Shared resource tracking
    std::map<PWMTimerGroup, PWMConfig> pwmConfigs;
    std::map<ADCModule, ADCConfig> adcConfigs;
    std::map<I2CBus, I2CConfig> i2cConfigs;
    
    // Global PWM resolution (affects all timers)
    uint8_t globalPWMResolution;
    const char* pwmResolutionOwner;

public:
    HardwareManager();
    ~HardwareManager();

    static HardwareManager *getInstance();
    static void init();

    // Initialization methods
    bool initialize();
    bool initializeHardware(); // Main initialization method called from setup()
    bool initializePins();
    bool initializePWM();
    bool initializeADC();

    // PWM configuration
    bool setPWMFrequency(uint8_t mode);

    // Pin access methods (return values from pcb.h)
    uint8_t getWASSensorPin() const;
    uint8_t getSpeedPulsePin() const;
    uint8_t getSpeedPulse10Pin() const;
    uint8_t getBuzzerPin() const;
    uint8_t getSleepPin() const;
    uint8_t getPWM1Pin() const;
    uint8_t getPWM2Pin() const;
    uint8_t getSteerPin() const;
    uint8_t getWorkPin() const;
    uint8_t getKickoutDPin() const;
    uint8_t getCurrentPin() const;
    uint8_t getKickoutAPin() const;

    // Hardware control methods
    void enableBuzzer();
    void disableBuzzer();
    void performBuzzerTest();  // Play a test tone with current volume setting
    void enableSteerMotor();
    void disableSteerMotor();

    // Status and debug
    void printHardwareStatus();
    void printPinConfiguration();
    bool getInitializationStatus() const;
    
    // Pin ownership management
    bool requestPinOwnership(uint8_t pin, PinOwner owner, const char* ownerName);
    bool releasePinOwnership(uint8_t pin, PinOwner owner);
    bool transferPinOwnership(uint8_t pin, PinOwner fromOwner, PinOwner toOwner, 
                             const char* toOwnerName, void (*cleanupCallback)(uint8_t) = nullptr);
    PinOwner getPinOwner(uint8_t pin) const;
    const char* getPinOwnerName(uint8_t pin) const;
    bool isPinOwned(uint8_t pin) const;
    void updatePinMode(uint8_t pin, uint8_t mode);
    void printPinOwnership();
    
    // PWM resource management
    bool requestPWMFrequency(uint8_t pin, uint32_t frequency, const char* owner);
    bool requestPWMResolution(uint8_t resolution, const char* owner);
    uint32_t getPWMFrequency(PWMTimerGroup group);
    uint8_t getPWMResolution() const;
    PWMTimerGroup getPWMTimerGroup(uint8_t pin);
    
    // ADC resource management
    bool requestADCConfig(ADCModule module, uint8_t resolution, uint8_t averaging, const char* owner);
    
    // I2C resource management
    bool requestI2CSpeed(I2CBus bus, uint32_t speed, const char* owner);
    
    // Resource status
    void printResourceStatus();
};

// Global instance (following the same pattern as configManager)
extern HardwareManager hardwareManager;

#endif // HARDWAREMANAGER_H_