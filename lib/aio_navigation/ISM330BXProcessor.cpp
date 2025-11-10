#include "ISM330BXProcessor.h"
#include "EventLogger.h"
#include <math.h>

// MLC configuration from mlc.json (converted to C array)
// This is the register configuration exported from MEMS Studio
#include "ISM330BX_MLC_Config.h"

// Physical constants
#define GRAVITY 9.80665f  // m/s²
#define DEG_TO_RAD (PI / 180.0f)
#define RAD_TO_DEG (180.0f / PI)

// Sensor sensitivity factors (from datasheet)
// Accelerometer: ±4g full scale = 0.122 mg/LSB
#define ACCEL_SENSITIVITY (0.122f / 1000.0f * GRAVITY)  // Convert to m/s²
// Gyroscope: ±250 dps full scale = 8.75 mdps/LSB
#define GYRO_SENSITIVITY (8.75f / 1000.0f)  // Convert to deg/s

// Filter coefficients for 5 Hz low-pass IIR2 at 30 Hz ODR
// From AN6124 Table 3, page 8
// These match the MLC internal filtering
#define IIR2_B0  0.0181f
#define IIR2_B1  0.0361f
#define IIR2_B2  0.0181f
#define IIR2_A1 -1.4770f
#define IIR2_A2  0.5495f

FLASHMEM ISM330BXProcessor::ISM330BXProcessor()
    : initialized(false),
      lastReadTime(0),
      currentMLCState(MLCState::UNKNOWN),
      lastMLCUpdate(0),
      roll(0), pitch(0), yaw(0), yawRate(0),
      gyroIntegratedRoll(0), lastAccelRoll(0) {

    // Initialize arrays
    memset(accelRaw, 0, sizeof(accelRaw));
    memset(gyroRaw, 0, sizeof(gyroRaw));
    memset(accel, 0, sizeof(accel));
    memset(gyro, 0, sizeof(gyro));
    memset(gyroBias, 0, sizeof(gyroBias));
    memset(filteredAccel, 0, sizeof(filteredAccel));
    memset(filteredGyro, 0, sizeof(filteredGyro));

    // Default config
    config.i2cAddress = ISM330BX_I2C_ADDR_PRIMARY;
    config.wire = &Wire;
    config.intPin = 255;  // No interrupt pin
    config.enableMLC = true;
    config.enableLowPassFilter = true;
}

ISM330BXProcessor::~ISM330BXProcessor() {
    // Nothing to clean up
}

FLASHMEM bool ISM330BXProcessor::begin(TwoWire* wire, uint8_t address) {
    ISM330BXConfig cfg;
    cfg.wire = wire;
    cfg.i2cAddress = address;
    cfg.intPin = 255;
    cfg.enableMLC = true;
    cfg.enableLowPassFilter = true;
    return begin(cfg);
}

FLASHMEM bool ISM330BXProcessor::begin(const ISM330BXConfig& cfg) {
    config = cfg;

    LOG_INFO(EventSource::IMU, "Initializing ISM330BX at address 0x%02X on I2C bus", config.i2cAddress);

    // I2C bus should already be initialized by I2CManager
    // Just ensure clock speed is set (won't hurt if already set)
    config.wire->setClock(400000);  // 400kHz

    // Small delay to allow sensor to stabilize
    delay(10);

    // Check if device is present
    if (!detectDevice()) {
        LOG_ERROR(EventSource::IMU, "ISM330BX not detected");
        return false;
    }

    // Reset device
    if (!writeRegister(ISM330BX_REG_CTRL3_C, 0x01)) {
        LOG_ERROR(EventSource::IMU, "Failed to reset ISM330BX");
        return false;
    }
    delay(100);  // Wait for reset

    // Configure sensor (accelerometer and gyroscope)
    if (!configureSensor()) {
        LOG_ERROR(EventSource::IMU, "Failed to configure ISM330BX");
        return false;
    }

    // Load MLC configuration if enabled
    if (config.enableMLC) {
        if (!loadMLCConfiguration()) {
            LOG_WARNING(EventSource::IMU, "Failed to load MLC configuration, continuing without MLC");
            config.enableMLC = false;
        } else {
            LOG_INFO(EventSource::IMU, "MLC configuration loaded successfully");
        }
    }

    // Initialize filters
    if (config.enableLowPassFilter) {
        initializeFilters();
    }

    initialized = true;
    LOG_INFO(EventSource::IMU, "ISM330BX initialized successfully");

    return true;
}

FLASHMEM bool ISM330BXProcessor::detectDevice() {
    uint8_t whoAmI = 0;

    // Try reading WHO_AM_I register
    bool readSuccess = readRegister(ISM330BX_REG_WHO_AM_I, &whoAmI);

    LOG_DEBUG(EventSource::IMU, "WHO_AM_I register read: %s, value: 0x%02X (expected 0x%02X)",
              readSuccess ? "SUCCESS" : "FAILED", whoAmI, ISM330BX_WHO_AM_I_VALUE);

    if (!readSuccess) {
        LOG_WARNING(EventSource::IMU, "Failed to read WHO_AM_I register - I2C communication error");
        return false;
    }

    if (whoAmI != ISM330BX_WHO_AM_I_VALUE) {
        LOG_WARNING(EventSource::IMU, "Unexpected WHO_AM_I: 0x%02X (expected 0x%02X)",
                    whoAmI, ISM330BX_WHO_AM_I_VALUE);
        return false;
    }

    LOG_INFO(EventSource::IMU, "ISM330BX detected, WHO_AM_I = 0x%02X", whoAmI);
    return true;
}

FLASHMEM bool ISM330BXProcessor::configureSensor() {
    // The MLC configuration will set up the sensor parameters
    // If MLC is disabled, we configure manually

    if (!config.enableMLC) {
        // Configure accelerometer: 30 Hz ODR, ±4g full scale
        // CTRL1_XL: ODR_XL[3:0] = 0100 (30 Hz), FS_XL[1:0] = 00 (±4g)
        if (!writeRegister(ISM330BX_REG_CTRL1_XL, 0x40)) {
            LOG_ERROR(EventSource::IMU, "Failed to configure accelerometer");
            return false;
        }

        // Configure gyroscope: 30 Hz ODR, ±250 dps full scale
        // CTRL2_G: ODR_G[3:0] = 0100 (30 Hz), FS_G[2:0] = 000 (±250 dps)
        if (!writeRegister(ISM330BX_REG_CTRL2_G, 0x40)) {
            LOG_ERROR(EventSource::IMU, "Failed to configure gyroscope");
            return false;
        }
    }

    return true;
}

FLASHMEM bool ISM330BXProcessor::loadMLCConfiguration() {
    LOG_INFO(EventSource::IMU, "Loading MLC configuration...");

    // Load all register writes from the MLC configuration
    for (size_t i = 0; i < mlc_config_len; i++) {
        if (!writeRegister(mlc_config[i].address, mlc_config[i].data)) {
            LOG_ERROR(EventSource::IMU, "Failed to write MLC register 0x%02X", mlc_config[i].address);
            return false;
        }

        // Small delay between writes for embedded function registers
        if (mlc_config[i].address >= 0x02 && mlc_config[i].address <= 0x60) {
            delayMicroseconds(100);
        }
    }

    // Verify MLC is enabled
    if (!verifyMLCConfiguration()) {
        return false;
    }

    LOG_INFO(EventSource::IMU, "MLC configuration loaded (%d registers)", mlc_config_len);
    return true;
}

FLASHMEM bool ISM330BXProcessor::verifyMLCConfiguration() {
    // Read MLC status to verify it's running
    uint8_t mlcStatus = 0;
    if (!enableEmbeddedAccess()) {
        return false;
    }

    if (!readRegister(ISM330BX_REG_MLC_STATUS, &mlcStatus)) {
        disableEmbeddedAccess();
        return false;
    }

    disableEmbeddedAccess();

    LOG_DEBUG(EventSource::IMU, "MLC Status: 0x%02X", mlcStatus);
    return true;
}

void ISM330BXProcessor::update() {
    if (!initialized) {
        return;
    }

    // No internal rate limiting - scheduler controls the update rate
    // This function should be called at 50Hz from the scheduler
    // (ISM330BX ODR is 30Hz, so 50Hz gives good headroom)

    // Read raw sensor data
    readRawSensorData();

    // Convert to physical units
    convertRawData();

    // Apply low-pass filtering (matches MLC internal filtering)
    if (config.enableLowPassFilter) {
        applyLowPassFilter();
    } else {
        memcpy(filteredAccel, accel, sizeof(filteredAccel));
        memcpy(filteredGyro, gyro, sizeof(filteredGyro));
    }

    // Calculate orientation (roll, pitch, yaw)
    calculateOrientation();

    // Read MLC state
    if (config.enableMLC) {
        readMLCState();
    }

    lastReadTime = millis();
}

FLASHMEM void ISM330BXProcessor::readRawSensorData() {
    uint8_t buffer[12];

    // Read gyro (6 bytes starting at 0x22)
    if (readRegisters(ISM330BX_REG_OUTX_L_G, buffer, 6)) {
        gyroRaw[0] = (int16_t)((buffer[1] << 8) | buffer[0]);
        gyroRaw[1] = (int16_t)((buffer[3] << 8) | buffer[2]);
        gyroRaw[2] = (int16_t)((buffer[5] << 8) | buffer[4]);
    }

    // Read accelerometer (6 bytes starting at 0x28)
    if (readRegisters(ISM330BX_REG_OUTX_L_A, buffer, 6)) {
        accelRaw[0] = (int16_t)((buffer[1] << 8) | buffer[0]);
        accelRaw[1] = (int16_t)((buffer[3] << 8) | buffer[2]);
        accelRaw[2] = (int16_t)((buffer[5] << 8) | buffer[4]);
    }
}

FLASHMEM void ISM330BXProcessor::convertRawData() {
    // Convert accelerometer (LSB to m/s²)
    for (int i = 0; i < 3; i++) {
        accel[i] = accelRaw[i] * ACCEL_SENSITIVITY;
    }

    // Convert gyroscope (LSB to deg/s) and remove bias
    for (int i = 0; i < 3; i++) {
        gyro[i] = (gyroRaw[i] * GYRO_SENSITIVITY) - gyroBias[i];
    }
}

FLASHMEM void ISM330BXProcessor::initializeFilters() {
    // Initialize IIR2 filter coefficients (5 Hz low-pass at 30 Hz ODR)
    for (int i = 0; i < 3; i++) {
        // Numerator coefficients (feedforward)
        accelFilter[i].b[0] = IIR2_B0;
        accelFilter[i].b[1] = IIR2_B1;
        accelFilter[i].b[2] = IIR2_B2;

        // Denominator coefficients (feedback)
        accelFilter[i].a[0] = 1.0f;
        accelFilter[i].a[1] = IIR2_A1;
        accelFilter[i].a[2] = IIR2_A2;

        // Initialize history to zero
        memset(accelFilter[i].x, 0, sizeof(accelFilter[i].x));
        memset(accelFilter[i].y, 0, sizeof(accelFilter[i].y));

        // Same for gyro filters
        gyroFilter[i].b[0] = IIR2_B0;
        gyroFilter[i].b[1] = IIR2_B1;
        gyroFilter[i].b[2] = IIR2_B2;
        gyroFilter[i].a[0] = 1.0f;
        gyroFilter[i].a[1] = IIR2_A1;
        gyroFilter[i].a[2] = IIR2_A2;
        memset(gyroFilter[i].x, 0, sizeof(gyroFilter[i].x));
        memset(gyroFilter[i].y, 0, sizeof(gyroFilter[i].y));
    }

    LOG_DEBUG(EventSource::IMU, "Low-pass filters initialized (5 Hz cutoff)");
}

FLASHMEM float ISM330BXProcessor::applyIIR2(IIR2Filter& filter, float input) {
    // Shift history
    filter.x[2] = filter.x[1];
    filter.x[1] = filter.x[0];
    filter.x[0] = input;

    filter.y[2] = filter.y[1];
    filter.y[1] = filter.y[0];

    // Apply IIR2 difference equation:
    // y[n] = b0*x[n] + b1*x[n-1] + b2*x[n-2] - a1*y[n-1] - a2*y[n-2]
    filter.y[0] = filter.b[0] * filter.x[0] +
                  filter.b[1] * filter.x[1] +
                  filter.b[2] * filter.x[2] -
                  filter.a[1] * filter.y[1] -
                  filter.a[2] * filter.y[2];

    return filter.y[0];
}

FLASHMEM void ISM330BXProcessor::applyLowPassFilter() {
    for (int i = 0; i < 3; i++) {
        filteredAccel[i] = applyIIR2(accelFilter[i], accel[i]);
        filteredGyro[i] = applyIIR2(gyroFilter[i], gyro[i]);
    }
}

FLASHMEM void ISM330BXProcessor::calculateOrientation() {
    // Calculate roll and pitch from filtered accelerometer
    calculateRollPitch();

    // Integrate yaw from filtered gyroscope
    float dt = (millis() - lastReadTime) / 1000.0f;
    if (dt > 0 && dt < 0.1f) {  // Sanity check (max 100ms)
        integrateYaw(dt);
    }

    // Update yaw rate
    yawRate = filteredGyro[2];
}

FLASHMEM void ISM330BXProcessor::calculateRollPitch() {
    // Use filtered accelerometer data
    float ax = filteredAccel[0];
    float ay = filteredAccel[1];
    float az = filteredAccel[2];

    // ISM330BX mounting: X-axis points DOWN when board is horizontal
    // For this orientation:
    // - Roll is rotation when Y-axis tilts (left/right)
    // - Pitch is rotation when Z-axis tilts (forward/back)
    // When flat: ax ≈ +9.8 m/s², ay ≈ 0, az ≈ 0

    // Calculate accelerometer-based roll (Y-axis tilt)
    // Use Y and X axes
    float accelRoll = atan2f(ay, ax) * RAD_TO_DEG;

    // Calculate accelerometer-based pitch (Z-axis tilt)
    // Use Z and X axes
    pitch = atan2f(az, ax) * RAD_TO_DEG;

    // Apply turn-aware roll compensation using MLC state
    float dt = (millis() - lastReadTime) / 1000.0f;
    if (dt > 0 && dt < 0.1f) {
        // Integrate gyro for gyro-based roll
        gyroIntegratedRoll += filteredGyro[0] * dt;

        // Compensate roll based on current state
        roll = compensateRollForTurn(accelRoll, gyroIntegratedRoll);

        // Update integrated roll with corrected value to prevent drift
        gyroIntegratedRoll = roll;
    } else {
        roll = accelRoll;
    }

    lastAccelRoll = accelRoll;
}

FLASHMEM float ISM330BXProcessor::compensateRollForTurn(float accelRoll, float gyroRoll) {
    switch (currentMLCState) {
        case MLCState::TURNING: {
            // During turns, centrifugal force corrupts accelerometer roll
            // Blend mostly gyro with some accel for drift correction
            float adaptiveWeight = getAdaptiveFilterWeight();
            float compensatedRoll = adaptiveWeight * accelRoll + (1.0f - adaptiveWeight) * gyroRoll;

            LOG_DEBUG(EventSource::IMU, "Turn compensation: accel=%.2f gyro=%.2f weight=%.3f result=%.2f",
                      accelRoll, gyroRoll, adaptiveWeight, compensatedRoll);

            return compensatedRoll;
        }

        case MLCState::STRAIGHT:
        case MLCState::SLOPE:
            // Straight or slope - accelerometer is trustworthy
            // Use complementary filter: mostly accel, some gyro
            return 0.98f * accelRoll + 0.02f * gyroRoll;

        case MLCState::ROUGH:
            // Rough terrain - blend 50/50 to handle vibrations
            return 0.5f * accelRoll + 0.5f * gyroRoll;

        case MLCState::STATIONARY:
            // Stationary - 100% accelerometer, reset gyro drift
            gyroIntegratedRoll = accelRoll;
            return accelRoll;

        case MLCState::UNKNOWN:
        default:
            // Unknown state - use standard complementary filter
            return 0.98f * accelRoll + 0.02f * gyroRoll;
    }
}

FLASHMEM float ISM330BXProcessor::getAdaptiveFilterWeight() {
    // Adaptive weighting based on yaw rate intensity
    // Higher yaw rate = trust accelerometer less
    float turnIntensity = fabsf(yawRate) / 50.0f;  // Normalize by max expected yaw rate (50 deg/s)
    turnIntensity = constrain(turnIntensity, 0.0f, 1.0f);

    // Weight goes from 0.98 (straight) to 0.05 (sharp turn)
    float accelWeight = 0.98f - (turnIntensity * 0.93f);

    return accelWeight;
}

FLASHMEM void ISM330BXProcessor::integrateYaw(float dt) {
    // Simple yaw integration from gyroscope
    yaw += filteredGyro[2] * dt;

    // Wrap to 0-360 degrees
    while (yaw >= 360.0f) yaw -= 360.0f;
    while (yaw < 0.0f) yaw += 360.0f;
}

FLASHMEM bool ISM330BXProcessor::readMLCState() {
    if (!config.enableMLC) {
        return false;
    }

    uint8_t mlcOutput = 0;
    if (!readRegister(ISM330BX_REG_MLC0_SRC, &mlcOutput)) {
        return false;
    }

    // Convert to MLC state enum
    MLCState newState = static_cast<MLCState>(mlcOutput);

    // Validate state
    if (newState != MLCState::STATIONARY &&
        newState != MLCState::ROUGH &&
        newState != MLCState::STRAIGHT &&
        newState != MLCState::TURNING &&
        newState != MLCState::SLOPE) {
        newState = MLCState::UNKNOWN;
    }

    // Update state if changed
    if (newState != currentMLCState) {
        LOG_DEBUG(EventSource::IMU, "MLC state changed: %s -> %s",
                  getMLCStateName(currentMLCState), getMLCStateName(newState));
        currentMLCState = newState;
        lastMLCUpdate = millis();
    }

    return true;
}

FLASHMEM const char* ISM330BXProcessor::getMLCStateName(MLCState state) {
    switch (state) {
        case MLCState::STATIONARY: return "Stationary";
        case MLCState::ROUGH:      return "Rough";
        case MLCState::STRAIGHT:   return "Straight";
        case MLCState::TURNING:    return "Turning";
        case MLCState::SLOPE:      return "Slope";
        case MLCState::UNKNOWN:
        default:                   return "Unknown";
    }
}

FLASHMEM const char* ISM330BXProcessor::getMLCStateString() {
    return getMLCStateName(currentMLCState);
}

FLASHMEM uint8_t ISM330BXProcessor::getQuality() {
    // Quality score 0-10 based on MLC state
    switch (currentMLCState) {
        case MLCState::STRAIGHT:
        case MLCState::SLOPE:
        case MLCState::STATIONARY:
            return 10;  // Highest quality

        case MLCState::TURNING:
            return 7;   // Good quality, but compensating for centrifugal effects

        case MLCState::ROUGH:
            return 5;   // Medium quality due to vibrations

        case MLCState::UNKNOWN:
        default:
            return 3;   // Low quality, unknown state
    }
}

FLASHMEM IMUData ISM330BXProcessor::getIMUData() {
    IMUData data;
    data.heading = yaw;
    data.roll = roll;
    data.pitch = pitch;
    data.yawRate = yawRate;
    data.quality = getQuality();
    data.timestamp = lastReadTime;
    data.isValid = initialized && (millis() - lastReadTime < 100);

    return data;
}

// Raw data access methods
FLASHMEM void ISM330BXProcessor::getRawAccel(int16_t* x, int16_t* y, int16_t* z) {
    if (x) *x = accelRaw[0];
    if (y) *y = accelRaw[1];
    if (z) *z = accelRaw[2];
}

FLASHMEM void ISM330BXProcessor::getRawGyro(int16_t* x, int16_t* y, int16_t* z) {
    if (x) *x = gyroRaw[0];
    if (y) *y = gyroRaw[1];
    if (z) *z = gyroRaw[2];
}

FLASHMEM void ISM330BXProcessor::getAccel(float* x, float* y, float* z) {
    if (x) *x = accel[0];
    if (y) *y = accel[1];
    if (z) *z = accel[2];
}

FLASHMEM void ISM330BXProcessor::getGyro(float* x, float* y, float* z) {
    if (x) *x = gyro[0];
    if (y) *y = gyro[1];
    if (z) *z = gyro[2];
}

FLASHMEM void ISM330BXProcessor::getFilteredAccel(float* x, float* y, float* z) {
    if (x) *x = filteredAccel[0];
    if (y) *y = filteredAccel[1];
    if (z) *z = filteredAccel[2];
}

FLASHMEM void ISM330BXProcessor::getFilteredGyro(float* x, float* y, float* z) {
    if (x) *x = filteredGyro[0];
    if (y) *y = filteredGyro[1];
    if (z) *z = filteredGyro[2];
}

// I2C communication methods
FLASHMEM bool ISM330BXProcessor::writeRegister(uint8_t reg, uint8_t value) {
    config.wire->beginTransmission(config.i2cAddress);
    config.wire->write(reg);
    config.wire->write(value);
    return (config.wire->endTransmission() == 0);
}

FLASHMEM bool ISM330BXProcessor::readRegister(uint8_t reg, uint8_t* value) {
    // Use repeated START (no STOP between write and read)
    config.wire->beginTransmission(config.i2cAddress);
    config.wire->write(reg);
    if (config.wire->endTransmission(false) != 0) {  // Repeated START
        return false;
    }

    if (config.wire->requestFrom(config.i2cAddress, (uint8_t)1) != 1) {
        return false;
    }

    *value = config.wire->read();
    return true;
}

FLASHMEM bool ISM330BXProcessor::readRegisters(uint8_t reg, uint8_t* buffer, uint8_t length) {
    config.wire->beginTransmission(config.i2cAddress);
    config.wire->write(reg);
    if (config.wire->endTransmission(false) != 0) {
        return false;
    }

    if (config.wire->requestFrom(config.i2cAddress, length) != length) {
        return false;
    }

    for (uint8_t i = 0; i < length; i++) {
        buffer[i] = config.wire->read();
    }

    return true;
}

FLASHMEM bool ISM330BXProcessor::enableEmbeddedAccess() {
    return writeRegister(ISM330BX_REG_FUNC_CFG_ACCESS, 0x80);
}

FLASHMEM bool ISM330BXProcessor::disableEmbeddedAccess() {
    return writeRegister(ISM330BX_REG_FUNC_CFG_ACCESS, 0x00);
}

// Debug methods
FLASHMEM void ISM330BXProcessor::printStatus() {
    LOG_INFO(EventSource::IMU, "\n=== ISM330BX Status ===");
    LOG_INFO(EventSource::IMU, "Initialized: %s", initialized ? "Yes" : "No");
    LOG_INFO(EventSource::IMU, "I2C Address: 0x%02X", config.i2cAddress);
    LOG_INFO(EventSource::IMU, "MLC Enabled: %s", config.enableMLC ? "Yes" : "No");
    LOG_INFO(EventSource::IMU, "Time since last read: %d ms", getTimeSinceLastRead());
    LOG_INFO(EventSource::IMU, "Current state: %s", getMLCStateString());
    LOG_INFO(EventSource::IMU, "Quality: %d/10", getQuality());
    LOG_INFO(EventSource::IMU, "Roll: %.2f°, Pitch: %.2f°, Yaw: %.2f°", roll, pitch, yaw);
    LOG_INFO(EventSource::IMU, "Yaw Rate: %.2f°/s", yawRate);
    LOG_INFO(EventSource::IMU, "========================\n");
}

FLASHMEM void ISM330BXProcessor::printMLCState() {
    LOG_INFO(EventSource::IMU, "MLC State: %s (0x%02X)",
             getMLCStateString(), static_cast<uint8_t>(currentMLCState));
}

FLASHMEM void ISM330BXProcessor::printSensorData() {
    LOG_INFO(EventSource::IMU, "Accel: X=%.3f Y=%.3f Z=%.3f m/s²",
             filteredAccel[0], filteredAccel[1], filteredAccel[2]);
    LOG_INFO(EventSource::IMU, "Gyro:  X=%.2f Y=%.2f Z=%.2f deg/s",
             filteredGyro[0], filteredGyro[1], filteredGyro[2]);
}
