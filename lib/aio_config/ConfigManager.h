// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

#ifndef CONFIGMANAGER_H_
#define CONFIGMANAGER_H_

#include "Arduino.h"
#include <EEPROM.h>
#include "EEPROMLayout.h"

// CAN bus functions (bitfield - multiple can be selected)
enum class CANFunction : uint8_t {
    NONE      = 0x00,  // No function
    STEERING  = 0x01,  // Steering control (valve/motor)
    BUTTONS   = 0x02,  // Button inputs (engage/disengage)
    HITCH     = 0x04,  // 3-point hitch control
    IMPLEMENT = 0x08,  // ISO implement control
    KEYA      = 0x10   // Keya motor (special case)
};

enum class CANBusName : uint8_t {
    NONE = 0,
    KEYA = 1,
    V_BUS = 2,
    K_BUS = 3,
    ISO_BUS = 4
};

// CAN Steer configuration structure
struct CANSteerConfig {
    // Brand selection
    uint8_t brand = 9;          // 0=Disabled, 1=Fendt, 2=Valtra, etc, 9=Generic (default)

    // CAN1 configuration
    uint8_t can1Speed = 0;      // 0=250k, 1=500k
    uint8_t can1Function = 0;   // CANFunction enum
    uint8_t can1Name = 0;       // 0=None, 1=V_Bus, 2=K_Bus, 3=ISO_Bus

    // CAN2 configuration
    uint8_t can2Speed = 0;      // 0=250k, 1=500k
    uint8_t can2Function = 0;   // CANFunction enum
    uint8_t can2Name = 0;       // 0=None, 1=V_Bus, 2=K_Bus, 3=ISO_Bus

    // CAN3 configuration
    uint8_t can3Speed = 0;      // 0=250k, 1=500k
    uint8_t can3Function = 0;   // CANFunction enum
    uint8_t can3Name = 0;       // 0=None, 1=V_Bus, 2=K_Bus, 3=ISO_Bus

    uint8_t moduleID = 0x1C;    // Module ID for protocols that need it
    uint8_t reserved[1];        // Future expansion
};

// ConfigManager Pattern for PGN Settings Access
// ============================================
// All runtime access to PGN settings should go through ConfigManager methods.
// Direct access to PGN data structures (e.g., steerSettings, machineConfig)
// should only be used for:
// 1. Parsing incoming PGN data
// 2. Initial loading from ConfigManager to structs
// 3. Logging what was received
//
// This ensures settings can be changed at runtime and take effect immediately.
// The structs are maintained for backward compatibility and PGN parsing.

class ConfigManager
{
private:
    static ConfigManager *instance;

    // Steer configuration (EEPROM 200-299)
    bool invertWAS;
    bool isRelayActiveHigh;
    bool motorDriveDirection;
    bool singleInputWAS;
    bool cytronDriver;
    bool steerSwitch;
    bool steerButton;
    bool shaftEncoder;
    bool isDanfoss;
    bool pressureSensor;
    bool currentSensor;
    bool isUseYAxis;
    bool pwmBrakeMode;  // false = coast mode (default), true = brake mode
    uint16_t softStartDurationMs;  // 0=disabled, 100-1000ms enabled (soft-accel is auto-calculated as 1/2)
    uint8_t pulseCountMax;
    uint8_t minSpeed;
    uint8_t motorDriverConfig;  // From PGN251 Byte 8

    // Steer settings (EEPROM 300-399)
    float kp;
    uint8_t highPWM;
    float lowPWM;
    uint8_t minPWM;
    uint8_t steerSensorCounts;
    int16_t wasOffset;
    float ackermanFix;

    // GPS configuration (EEPROM 400-499)
    uint32_t gpsBaudRate;
    bool gpsSyncMode;
    bool gpsPassThrough;
    uint8_t gpsProtocol;
    uint32_t serialRadioBaudRate;  // RTK radio baud rate (4800-921600)

    // Machine settings (EEPROM 500-599)
    uint8_t sectionCount;
    bool hydraulicLift;
    bool tramlineControl;
    uint16_t workWidth;
    uint8_t raiseTime;
    uint8_t lowerTime;
    bool isPinActiveHigh;
    bool sectionControlSleepMode;  // If true, onboard SC goes silent when external SC detected
    uint8_t user1;
    uint8_t user2;
    uint8_t user3;
    uint8_t user4;

    // KWAS configuration (EEPROM 600-699)
    bool kwasEnabled;
    uint8_t kwasMode;
    float kwasGain;
    uint16_t kwasDeadband;
    uint8_t kwasFilterLevel;

    // INS configuration (EEPROM 700-1199)
    bool insEnabled;
    uint8_t insMode;
    float insHeadingOffset;
    float insRollOffset;
    float insPitchOffset;
    uint8_t insFilterLevel;
    bool insUseFusion;
    float insVarianceHeading;
    float insVarianceRoll;
    float insVariancePitch;

    // LED settings
    uint8_t ledBrightness;
    
    // Buzzer settings
    bool buzzerLoudMode;         // true = loud for field use, false = quiet for development
    
    // Turn sensor configuration
    uint8_t turnSensorType;      // 0=None, 1=Encoder, 2=Pressure, 3=Current, 4=JD PWM
    uint8_t encoderType;         // 1=Single, 2=Quadrature
    uint8_t turnMaxPulseCount;   // Max encoder pulses before kickout
    uint8_t pressureThreshold;   // Pressure sensor threshold
    uint8_t currentThreshold;    // Current sensor threshold
    uint16_t currentZeroOffset;  // Current sensor zero offset
    
    // John Deere PWM encoder configuration
    bool jdPWMEnabled;           // Enable JD PWM mode for pressure input
    uint8_t jdPWMSensitivity;    // JD PWM sensitivity 1-10 (1=least sensitive, 10=most sensitive)
    
    // Analog work switch configuration
    bool analogWorkSwitchEnabled;
    uint8_t workSwitchSetpoint;     // 0-100% stored as 0-100
    uint8_t workSwitchHysteresis;   // 5-25% stored as 5-25
    bool invertWorkSwitch;
    
    // Network configuration
    uint8_t ipAddress[4];
    uint8_t subnet[4];
    uint8_t gateway[4];
    uint8_t dns[4];
    uint8_t destIP[4];
    uint16_t destPort;
    
    // Version control
    uint16_t eeVersion;

    // CAN Steer configuration
    CANSteerConfig canSteerConfig;

    // Initialization tracking
    bool initialized;

public:
    ConfigManager();
    ~ConfigManager();

    // Singleton access
    static ConfigManager *getInstance();
    void init();

    // Steer configuration methods
    bool getInvertWAS() const { return invertWAS; }
    void setInvertWAS(bool value) { invertWAS = value; }
    bool getIsRelayActiveHigh() const { return isRelayActiveHigh; }
    void setIsRelayActiveHigh(bool value) { isRelayActiveHigh = value; }
    bool getMotorDriveDirection() const { return motorDriveDirection; }
    void setMotorDriveDirection(bool value) { motorDriveDirection = value; }
    bool getSingleInputWAS() const { return singleInputWAS; }
    void setSingleInputWAS(bool value) { singleInputWAS = value; }
    bool getCytronDriver() const { return cytronDriver; }
    void setCytronDriver(bool value) { cytronDriver = value; }
    bool getSteerSwitch() const { return steerSwitch; }
    void setSteerSwitch(bool value) { steerSwitch = value; }
    bool getSteerButton() const { return steerButton; }
    void setSteerButton(bool value) { steerButton = value; }
    bool getShaftEncoder() const { return shaftEncoder; }
    void setShaftEncoder(bool value) { shaftEncoder = value; }
    bool getIsDanfoss() const { return isDanfoss; }
    void setIsDanfoss(bool value) { isDanfoss = value; }
    bool getPressureSensor() const { return pressureSensor; }
    void setPressureSensor(bool value) { pressureSensor = value; }
    bool getCurrentSensor() const { return currentSensor; }
    void setCurrentSensor(bool value) { currentSensor = value; }
    bool getIsUseYAxis() const { return isUseYAxis; }
    void setIsUseYAxis(bool value) { isUseYAxis = value; }
    bool getPWMBrakeMode() const { return pwmBrakeMode; }
    void setPWMBrakeMode(bool value) { pwmBrakeMode = value; }
    uint16_t getSoftStartDurationMs() const { return softStartDurationMs; }
    void setSoftStartDurationMs(uint16_t value) { softStartDurationMs = constrain(value, 0, 1000); }
    uint8_t getPulseCountMax() const { return pulseCountMax; }
    void setPulseCountMax(uint8_t value) { pulseCountMax = value; }
    uint8_t getMinSpeed() const { return minSpeed; }
    void setMinSpeed(uint8_t value) { minSpeed = value; }
    uint8_t getMotorDriverConfig() const { return motorDriverConfig; }
    void setMotorDriverConfig(uint8_t value) { motorDriverConfig = value; }

    // Steer settings methods
    float getKp() const { return kp; }
    void setKp(float value) { kp = value; }
    uint8_t getHighPWM() const { return highPWM; }
    void setHighPWM(uint8_t value) { highPWM = value; }
    float getLowPWM() const { return lowPWM; }
    void setLowPWM(float value) { lowPWM = value; }
    uint8_t getMinPWM() const { return minPWM; }
    void setMinPWM(uint8_t value) { minPWM = value; }
    uint8_t getSteerSensorCounts() const { return steerSensorCounts; }
    void setSteerSensorCounts(uint8_t value) { steerSensorCounts = value; }
    int16_t getWasOffset() const { return wasOffset; }
    void setWasOffset(int16_t value) { wasOffset = value; }
    float getAckermanFix() const { return ackermanFix; }
    void setAckermanFix(float value) { ackermanFix = value; }

    // LED configuration
    uint8_t getLEDBrightness() const { return ledBrightness; }
    void setLEDBrightness(uint8_t value) { 
        ledBrightness = constrain(value, 5, 100); 
    }
    
    // Buzzer configuration
    bool getBuzzerLoudMode() const { return buzzerLoudMode; }
    void setBuzzerLoudMode(bool value) { buzzerLoudMode = value; }
    
    // GPS configuration methods
    uint32_t getGPSBaudRate() const { return gpsBaudRate; }
    void setGPSBaudRate(uint32_t value) { gpsBaudRate = value; }
    bool getGPSSyncMode() const { return gpsSyncMode; }
    void setGPSSyncMode(bool value) { gpsSyncMode = value; }
    bool getGPSPassThrough() const { return gpsPassThrough; }
    void setGPSPassThrough(bool value) { gpsPassThrough = value; }
    uint8_t getGPSProtocol() const { return gpsProtocol; }
    void setGPSProtocol(uint8_t value) { gpsProtocol = value; }
    uint32_t getSerialRadioBaudRate() const { return serialRadioBaudRate; }
    void setSerialRadioBaudRate(uint32_t value) { serialRadioBaudRate = value; }

    // Machine configuration methods
    uint8_t getSectionCount() const { return sectionCount; }
    void setSectionCount(uint8_t value) { sectionCount = value; }
    bool getHydraulicLift() const { return hydraulicLift; }
    void setHydraulicLift(bool value) { hydraulicLift = value; }
    bool getTramlineControl() const { return tramlineControl; }
    void setTramlineControl(bool value) { tramlineControl = value; }
    uint16_t getWorkWidth() const { return workWidth; }
    void setWorkWidth(uint16_t value) { workWidth = value; }
    uint8_t getRaiseTime() const { return raiseTime; }
    void setRaiseTime(uint8_t value) { raiseTime = value; }
    uint8_t getLowerTime() const { return lowerTime; }
    void setLowerTime(uint8_t value) { lowerTime = value; }
    bool getIsPinActiveHigh() const { return isPinActiveHigh; }
    void setIsPinActiveHigh(bool value) { isPinActiveHigh = value; }
    bool getSectionControlSleepMode() const { return sectionControlSleepMode; }
    void setSectionControlSleepMode(bool value) { sectionControlSleepMode = value; }
    uint8_t getUser1() const { return user1; }
    void setUser1(uint8_t value) { user1 = value; }
    uint8_t getUser2() const { return user2; }
    void setUser2(uint8_t value) { user2 = value; }
    uint8_t getUser3() const { return user3; }
    void setUser3(uint8_t value) { user3 = value; }
    uint8_t getUser4() const { return user4; }
    void setUser4(uint8_t value) { user4 = value; }

    // KWAS configuration methods
    bool getKWASEnabled() const { return kwasEnabled; }
    void setKWASEnabled(bool value) { kwasEnabled = value; }
    uint8_t getKWASMode() const { return kwasMode; }
    void setKWASMode(uint8_t value) { kwasMode = value; }
    float getKWASGain() const { return kwasGain; }
    void setKWASGain(float value) { kwasGain = value; }
    uint16_t getKWASDeadband() const { return kwasDeadband; }
    void setKWASDeadband(uint16_t value) { kwasDeadband = value; }
    uint8_t getKWASFilterLevel() const { return kwasFilterLevel; }
    void setKWASFilterLevel(uint8_t value) { kwasFilterLevel = value; }

    // INS configuration methods
    bool getINSEnabled() const { return insEnabled; }
    void setINSEnabled(bool value) { insEnabled = value; }
    uint8_t getINSMode() const { return insMode; }
    void setINSMode(uint8_t value) { insMode = value; }
    float getINSHeadingOffset() const { return insHeadingOffset; }
    void setINSHeadingOffset(float value) { insHeadingOffset = value; }
    float getINSRollOffset() const { return insRollOffset; }
    void setINSRollOffset(float value) { insRollOffset = value; }
    float getINSPitchOffset() const { return insPitchOffset; }
    void setINSPitchOffset(float value) { insPitchOffset = value; }
    uint8_t getINSFilterLevel() const { return insFilterLevel; }
    void setINSFilterLevel(uint8_t value) { insFilterLevel = value; }
    bool getINSUseFusion() const { return insUseFusion; }
    void setINSUseFusion(bool value) { insUseFusion = value; }
    float getINSVarianceHeading() const { return insVarianceHeading; }
    void setINSVarianceHeading(float value) { insVarianceHeading = value; }
    float getINSVarianceRoll() const { return insVarianceRoll; }
    void setINSVarianceRoll(float value) { insVarianceRoll = value; }
    float getINSVariancePitch() const { return insVariancePitch; }
    void setINSVariancePitch(float value) { insVariancePitch = value; }

    // Turn sensor configuration methods
    uint8_t getTurnSensorType() const { return turnSensorType; }
    void setTurnSensorType(uint8_t value) { turnSensorType = value; }
    uint8_t getEncoderType() const { return encoderType; }
    void setEncoderType(uint8_t value) { encoderType = value; }
    uint8_t getTurnMaxPulseCount() const { return turnMaxPulseCount; }
    void setTurnMaxPulseCount(uint8_t value) { turnMaxPulseCount = value; }
    uint8_t getPressureThreshold() const { return pressureThreshold; }
    void setPressureThreshold(uint8_t value) { pressureThreshold = value; }
    uint8_t getCurrentThreshold() const { return currentThreshold; }
    void setCurrentThreshold(uint8_t value) { currentThreshold = value; }
    uint16_t getCurrentZeroOffset() const { return currentZeroOffset; }
    void setCurrentZeroOffset(uint16_t value) { currentZeroOffset = value; }
    
    // John Deere PWM encoder methods
    bool getJDPWMEnabled() const { return jdPWMEnabled; }
    void setJDPWMEnabled(bool value) { jdPWMEnabled = value; }
    uint8_t getJDPWMSensitivity() const { return jdPWMSensitivity; }
    void setJDPWMSensitivity(uint8_t value) { jdPWMSensitivity = constrain(value, 1, 10); }

    // Analog work switch methods
    bool getAnalogWorkSwitchEnabled() const { return analogWorkSwitchEnabled; }
    void setAnalogWorkSwitchEnabled(bool value) { analogWorkSwitchEnabled = value; }
    uint8_t getWorkSwitchSetpoint() const { return workSwitchSetpoint; }
    void setWorkSwitchSetpoint(uint8_t value) { workSwitchSetpoint = constrain(value, 0, 100); }
    uint8_t getWorkSwitchHysteresis() const { return workSwitchHysteresis; }
    void setWorkSwitchHysteresis(uint8_t value) { workSwitchHysteresis = constrain(value, 5, 25); }
    bool getInvertWorkSwitch() const { return invertWorkSwitch; }
    void setInvertWorkSwitch(bool value) { invertWorkSwitch = value; }

    // Network configuration methods
    void getIPAddress(uint8_t* ip) const { memcpy(ip, ipAddress, 4); }
    void setIPAddress(const uint8_t* ip) { memcpy(ipAddress, ip, 4); }
    void getSubnet(uint8_t* sub) const { memcpy(sub, subnet, 4); }
    void setSubnet(const uint8_t* sub) { memcpy(subnet, sub, 4); }
    void getGateway(uint8_t* gw) const { memcpy(gw, gateway, 4); }
    void setGateway(const uint8_t* gw) { memcpy(gateway, gw, 4); }
    void getDNS(uint8_t* d) const { memcpy(d, dns, 4); }
    void setDNS(const uint8_t* d) { memcpy(dns, d, 4); }
    void getDestIP(uint8_t* dest) const { memcpy(dest, destIP, 4); }
    void setDestIP(const uint8_t* dest) { memcpy(destIP, dest, 4); }
    uint16_t getDestPort() const { return destPort; }
    void setDestPort(uint16_t port) { destPort = port; }

    // EEPROM operations
    void saveSteerConfig();
    void loadSteerConfig();
    void saveSteerSettings();
    void loadSteerSettings();
    void saveGPSConfig();
    void loadGPSConfig();
    void saveMachineConfig();
    void loadMachineConfig();
    void saveKWASConfig();
    void loadKWASConfig();
    void saveINSConfig();
    void loadINSConfig();
    void saveTurnSensorConfig();
    void loadTurnSensorConfig();
    void saveAnalogWorkSwitchConfig();
    void loadAnalogWorkSwitchConfig();
    void saveMiscConfig();
    void loadMiscConfig();
    void saveNetworkConfig();
    void loadNetworkConfig();
    void loadAllConfigs();
    void saveAllConfigs();
    void resetToDefaults();
    bool checkVersion();
    void updateVersion();

    // CAN Steer configuration methods
    CANSteerConfig getCANSteerConfig() const;
    void setCANSteerConfig(const CANSteerConfig& config);
    void saveCANSteerConfig();
    void loadCANSteerConfig();
};

#endif // CONFIGMANAGER_H_