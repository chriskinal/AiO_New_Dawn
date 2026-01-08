// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

// WheelAngleFusion.h - Virtual WAS (VWAS) using sensor fusion
#ifndef WHEEL_ANGLE_FUSION_H
#define WHEEL_ANGLE_FUSION_H

#include <Arduino.h>
#include "EventLogger.h"

// Math constants are already defined in Arduino.h
// DEG_TO_RAD and RAD_TO_DEG

// Forward declarations
class KeyaCANDriver;
class GNSSProcessor;
class IMUProcessor;

/**
 * WheelAngleFusion - Virtual Wheel Angle Sensor (VWAS)
 * 
 * Creates a virtual WAS by combining multiple sensor inputs to estimate 
 * steering angle. Uses an adaptive Kalman filter to fuse motor encoder 
 * data with GPS/INS heading rate.
 * 
 * This provides a software-based wheel angle sensor using existing hardware.
 * Based on proven algorithm from AOG_Teensy_UM98X project.
 */
class WheelAngleFusion {
public:
    // Configuration structure
    struct Config {
        // Vehicle parameters
        float wheelbase = 2.5f;           // Vehicle wheelbase in meters
        float trackWidth = 1.8f;          // Vehicle track width in meters
        
        // Motor calibration
        float countsPerDegree = 100.0f;   // Encoder counts per steering degree
        int32_t centerPosition = 32768;   // Encoder position when wheels straight
        float maxSteeringAngle = 40.0f;   // Maximum steering angle (degrees)
        
        // Kalman filter parameters
        float processNoise = 0.1f;        // Q - process noise covariance
        float measurementNoise = 1.0f;    // R - measurement noise covariance
        float initialUncertainty = 10.0f; // P - initial error covariance
        
        // Fusion parameters
        float minSpeedForGPS = 0.5f;      // Minimum speed for GPS fusion (m/s)
        float maxHeadingRate = 50.0f;     // Maximum valid heading rate (deg/s)
        uint16_t varianceBufferSize = 50; // Size of variance calculation buffer
        
        // Sensor selection
        bool useIMUHeadingRate = false;   // Use IMU instead of GPS for heading rate
        bool enableDriftCompensation = true; // Enable encoder drift compensation
    };
    
    // Constructor
    WheelAngleFusion();
    ~WheelAngleFusion() = default;
    
    // Initialization
    bool init(KeyaCANDriver* keya, GNSSProcessor* gnss, IMUProcessor* imu);
    
    // Configuration
    void setConfig(const Config& cfg) { config = cfg; }
    Config& getConfig() { return config; }
    const Config& getConfig() const { return config; }
    
    // Main update function - call at 100Hz
    void update(float dt);
    
    // Get fusion results
    float getFusedAngle() const { return fusedAngle; }
    float getPredictedAngle() const { return predictedAngle; }
    float getGPSAngle() const { return gpsAngle; }
    float getEncoderAngle() const { return encoderAngle; }
    
    // Get quality metrics
    float getUncertainty() const { return uncertainty; }
    float getMeasurementVariance() const { return measurementVariance; }
    float getKalmanGain() const { return kalmanGain; }
    
    // Health and status
    bool isHealthy() const;
    bool hasValidGPSAngle() const { return gpsAngleValid; }
    uint32_t getLastUpdateTime() const { return lastUpdateTime; }
    
    // Calibration
    void startCalibration();
    void stopCalibration();
    bool isCalibrating() const { return calibrationMode; }
    void setEncoderCenter(); // Set current position as center (0 degrees)
    
    // Reset and recovery
    void reset();
    void resetDriftCompensation();
    
private:
    // Sensor interfaces
    KeyaCANDriver* keyaDriver;
    GNSSProcessor* gnssProcessor;
    IMUProcessor* imuProcessor;
    
    // Configuration
    Config config;
    
    // Kalman filter state
    float fusedAngle;        // X - Current angle estimate
    float predictedAngle;    // Xp - Predicted angle
    float uncertainty;       // P - Estimation error covariance
    float kalmanGain;        // K - Kalman gain
    
    // Sensor angles
    float encoderAngle;      // Angle from motor encoder
    float gpsAngle;          // Angle from GPS heading rate
    bool gpsAngleValid;      // Is GPS angle valid this update
    
    // Motor position tracking
    int32_t lastEncoderPosition;
    int32_t encoderOffset;   // Offset for centering
    
    // GPS angle calculation
    float vehicleSpeed;      // Current speed (m/s)
    float headingRate;       // Rate of heading change (deg/s)
    float lastHeading;       // Previous heading for rate calculation
    
    // Adaptive variance calculation
    float measurementVariance;
    float* varianceBuffer;   // Circular buffer for variance calculation
    uint16_t varianceIndex;
    uint16_t varianceCount;
    
    // Drift compensation
    float encoderDrift;      // Accumulated drift (degrees)
    float driftRate;         // Drift rate (degrees/second)
    uint32_t driftStartTime;
    
    // Timing
    uint32_t lastUpdateTime;
    uint32_t lastGPSTime;
    
    // Calibration
    bool calibrationMode;
    float calibrationMinAngle;
    float calibrationMaxAngle;
    int32_t calibrationMinPosition;
    int32_t calibrationMaxPosition;
    
    // Private methods
    void updateEncoderAngle();
    void updateGPSAngle();
    void updateKalmanFilter(float dt);
    void updateVariance();
    void updateDriftCompensation(float dt);
    float calculateGPSAngleFromHeadingRate(float headingRate, float speed);
    bool isValidGPSConditions() const;
};

// Global instance pointer for external access
extern WheelAngleFusion* wheelAngleFusionPtr;

#endif // WHEEL_ANGLE_FUSION_H