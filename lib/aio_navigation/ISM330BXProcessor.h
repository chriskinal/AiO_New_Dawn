#ifndef ISM330BX_PROCESSOR_H
#define ISM330BX_PROCESSOR_H

#include <Arduino.h>
#include <Wire.h>
#include "NavigationTypes.h"

// ISM330BX I2C addresses
#define ISM330BX_I2C_ADDR_PRIMARY   0x6A  // SDO/SA0 = GND
#define ISM330BX_I2C_ADDR_SECONDARY 0x6B  // SDO/SA0 = VDD

// ISM330BX Register Map
#define ISM330BX_REG_WHO_AM_I       0x0F
#define ISM330BX_WHO_AM_I_VALUE     0x71  // Per datasheet section 9.12
#define ISM330BX_REG_CTRL1_XL       0x10  // Accelerometer control
#define ISM330BX_REG_CTRL2_G        0x11  // Gyroscope control
#define ISM330BX_REG_CTRL3_C        0x12  // Control register 3
#define ISM330BX_REG_STATUS_REG     0x1E  // Status register
#define ISM330BX_REG_OUT_TEMP_L     0x20  // Temperature output
#define ISM330BX_REG_OUTX_L_G       0x22  // Gyro X-axis low byte
#define ISM330BX_REG_OUTX_L_A       0x28  // Accel X-axis low byte
#define ISM330BX_REG_MLC_STATUS     0x38  // MLC status
#define ISM330BX_REG_EMB_FUNC_EN_A  0x04  // Embedded functions enable (page 1)
#define ISM330BX_REG_EMB_FUNC_EN_B  0x05  // Embedded functions enable (page 1)
#define ISM330BX_REG_MLC0_SRC       0x70  // MLC decision tree output
#define ISM330BX_REG_FUNC_CFG_ACCESS 0x01 // Access embedded functions

// MLC States (from mlc.json)
enum class MLCState : uint8_t {
    STATIONARY = 0x00,
    ROUGH      = 0x01,
    STRAIGHT   = 0x04,
    TURNING    = 0x08,
    SLOPE      = 0x0C,
    UNKNOWN    = 0xFF
};

// ISM330BX configuration structure
struct ISM330BXConfig {
    uint8_t i2cAddress;
    TwoWire* wire;
    uint8_t intPin;
    bool enableMLC;
    bool enableLowPassFilter;
};

class ISM330BXProcessor {
private:
    ISM330BXConfig config;
    bool initialized;
    uint32_t lastReadTime;

    // Raw sensor data
    int16_t accelRaw[3];  // X, Y, Z
    int16_t gyroRaw[3];   // X, Y, Z
    int16_t tempRaw;

    // Calibrated sensor data
    float accel[3];       // m/s²
    float gyro[3];        // deg/s
    float temperature;    // °C

    // MLC state
    MLCState currentMLCState;
    uint32_t lastMLCUpdate;

    // Orientation data
    float roll;           // degrees
    float pitch;          // degrees
    float yaw;            // degrees
    float yawRate;        // degrees/second

    // Gyro bias for drift compensation
    float gyroBias[3];

    // Low-pass filter state (5 Hz IIR2)
    struct IIR2Filter {
        float x[3];  // Input history [n, n-1, n-2]
        float y[3];  // Output history [n, n-1, n-2]
        float b[3];  // Numerator coefficients
        float a[3];  // Denominator coefficients
    };

    IIR2Filter accelFilter[3];  // One filter per axis
    IIR2Filter gyroFilter[3];
    float filteredAccel[3];
    float filteredGyro[3];

    // Turn-aware roll filtering
    float gyroIntegratedRoll;
    float lastAccelRoll;

    // I2C communication
    bool writeRegister(uint8_t reg, uint8_t value);
    bool readRegister(uint8_t reg, uint8_t* value);
    bool readRegisters(uint8_t reg, uint8_t* buffer, uint8_t length);
    bool writeMasked(uint8_t reg, uint8_t mask, uint8_t value);

    // Embedded function page access
    bool enableEmbeddedAccess();
    bool disableEmbeddedAccess();

    // Sensor configuration
    bool configureSensor();
    bool loadMLCConfiguration();
    bool verifyMLCConfiguration();

    // Data processing
    void readRawSensorData();
    void convertRawData();
    void applyLowPassFilter();
    void initializeFilters();
    float applyIIR2(IIR2Filter& filter, float input);

    // Orientation calculation
    void calculateOrientation();
    void calculateRollPitch();
    void integrateYaw(float dt);

    // Turn-aware roll filtering
    float compensateRollForTurn(float accelRoll, float gyroRoll);
    float getAdaptiveFilterWeight();

    // MLC processing
    bool readMLCState();
    const char* getMLCStateName(MLCState state);

    // Calibration
    void calibrateGyroBias();

public:
    ISM330BXProcessor();
    ~ISM330BXProcessor();

    // Initialization
    bool begin(const ISM330BXConfig& cfg);
    bool begin(TwoWire* wire = &Wire, uint8_t address = ISM330BX_I2C_ADDR_PRIMARY);
    bool detectDevice();

    // Main processing loop
    void update();

    // Data access - compatible with IMUData structure
    IMUData getIMUData();
    float getRoll() const { return roll; }
    float getPitch() const { return pitch; }
    float getYaw() const { return yaw; }
    float getYawRate() const { return yawRate; }
    uint8_t getQuality();  // Quality based on MLC state

    // MLC state access
    MLCState getMLCState() const { return currentMLCState; }
    const char* getMLCStateString();
    bool isStationary() const { return currentMLCState == MLCState::STATIONARY; }
    bool isTurning() const { return currentMLCState == MLCState::TURNING; }
    bool isRough() const { return currentMLCState == MLCState::ROUGH; }
    bool onSlope() const { return currentMLCState == MLCState::SLOPE; }

    // Raw data access
    void getRawAccel(int16_t* x, int16_t* y, int16_t* z);
    void getRawGyro(int16_t* x, int16_t* y, int16_t* z);
    void getAccel(float* x, float* y, float* z);
    void getGyro(float* x, float* y, float* z);
    void getFilteredAccel(float* x, float* y, float* z);
    void getFilteredGyro(float* x, float* y, float* z);

    // Status
    bool isInitialized() const { return initialized; }
    float getTemperature() const { return temperature; }
    uint32_t getTimeSinceLastRead() const { return millis() - lastReadTime; }

    // Calibration
    void startCalibration();
    bool isCalibrating();

    // Debug
    void printStatus();
    void printMLCState();
    void printSensorData();
};

#endif // ISM330BX_PROCESSOR_H
