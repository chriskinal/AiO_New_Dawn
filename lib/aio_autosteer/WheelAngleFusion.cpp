// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

// WheelAngleFusion.cpp - Implementation of sensor fusion for wheel angle
#include "WheelAngleFusion.h"
#include "KeyaCANDriver.h"
#include "GNSSProcessor.h"
#include "IMUProcessor.h"
#include <cmath>

// Global instance pointer
WheelAngleFusion* wheelAngleFusionPtr = nullptr;

// External sensor instances
extern GNSSProcessor* gnssProcessorPtr;
extern IMUProcessor imuProcessor;

WheelAngleFusion::WheelAngleFusion() :
    keyaDriver(nullptr),
    gnssProcessor(nullptr),
    imuProcessor(nullptr),
    fusedAngle(0.0f),
    predictedAngle(0.0f),
    uncertainty(10.0f),
    kalmanGain(0.0f),
    encoderAngle(0.0f),
    gpsAngle(0.0f),
    gpsAngleValid(false),
    lastEncoderPosition(0),
    encoderOffset(0),
    vehicleSpeed(0.0f),
    headingRate(0.0f),
    lastHeading(0.0f),
    measurementVariance(1.0f),
    varianceBuffer(nullptr),
    varianceIndex(0),
    varianceCount(0),
    encoderDrift(0.0f),
    driftRate(0.0f),
    driftStartTime(0),
    lastUpdateTime(0),
    lastGPSTime(0),
    calibrationMode(false),
    calibrationMinAngle(-40.0f),
    calibrationMaxAngle(40.0f),
    calibrationMinPosition(0),
    calibrationMaxPosition(65535)
{
    // Set global pointer
    wheelAngleFusionPtr = this;
}

bool WheelAngleFusion::init(KeyaCANDriver* keya, GNSSProcessor* gnss, IMUProcessor* imu) {
    LOG_INFO(EventSource::AUTOSTEER, "Initializing Virtual WAS (VWAS)");
    
    // Store sensor interfaces
    keyaDriver = keya;
    gnssProcessor = gnss;
    imuProcessor = imu;
    
    // Validate interfaces
    if (!keyaDriver) {
        LOG_ERROR(EventSource::AUTOSTEER, "VWAS: No Keya driver provided");
        return false;
    }
    
    if (!gnssProcessor && !imuProcessor) {
        LOG_ERROR(EventSource::AUTOSTEER, "VWAS: No heading rate source (GPS or IMU)");
        return false;
    }
    
    // Allocate variance buffer
    if (varianceBuffer) {
        delete[] varianceBuffer;
    }
    varianceBuffer = new float[config.varianceBufferSize];
    if (!varianceBuffer) {
        LOG_ERROR(EventSource::AUTOSTEER, "VWAS: Failed to allocate variance buffer");
        return false;
    }
    
    // Initialize variance buffer
    for (uint16_t i = 0; i < config.varianceBufferSize; i++) {
        varianceBuffer[i] = 0.0f;
    }
    
    // Initialize Kalman filter state
    fusedAngle = 0.0f;
    predictedAngle = 0.0f;
    encoderAngle = 0.0f;
    gpsAngle = 0.0f;
    uncertainty = config.initialUncertainty;
    
    // Initialize timing
    lastUpdateTime = millis();
    lastGPSTime = millis();
    driftStartTime = millis();
    
    LOG_INFO(EventSource::AUTOSTEER, "Virtual WAS initialized successfully");
    LOG_INFO(EventSource::AUTOSTEER, "  Wheelbase: %.2f m", config.wheelbase);
    LOG_INFO(EventSource::AUTOSTEER, "  Counts/degree: %.1f", config.countsPerDegree);
    LOG_INFO(EventSource::AUTOSTEER, "  Min GPS speed: %.1f m/s", config.minSpeedForGPS);
    
    return true;
}

void WheelAngleFusion::update(float dt) {
    // Update timing
    uint32_t now = millis();
    lastUpdateTime = now;
    
    // Update sensor angles
    updateEncoderAngle();
    updateGPSAngle();
    
    // Run Kalman filter
    updateKalmanFilter(dt);
    
    // Update adaptive parameters
    if (isValidGPSConditions()) {
        updateVariance();
    }
    
    // Update drift compensation
    if (config.enableDriftCompensation) {
        updateDriftCompensation(dt);
    }
}

void WheelAngleFusion::updateEncoderAngle() {
    if (!keyaDriver) {
        encoderAngle = 0.0f;
        return;
    }
    
    // Get position delta from motor
    int32_t deltaPosition = keyaDriver->getPositionDelta();
    
    // Convert delta to degrees
    float deltaAngle = (float)deltaPosition / config.countsPerDegree;
    
    // Update encoder angle (accumulate changes)
    encoderAngle += deltaAngle;
    
    // Apply centering offset
    float centeredAngle = encoderAngle - encoderOffset;
    
    // Constrain to reasonable limits
    if (abs(centeredAngle) > config.maxSteeringAngle) {
        centeredAngle = constrain(centeredAngle, -config.maxSteeringAngle, config.maxSteeringAngle);
    }
    
    encoderAngle = centeredAngle;
    
    // Log significant changes
    static float lastLoggedAngle = 0.0f;
    if (abs(encoderAngle - lastLoggedAngle) > 1.0f) {
        LOG_DEBUG(EventSource::AUTOSTEER, "Encoder angle: %.2f° (delta: %d counts)", 
                  encoderAngle, deltaPosition);
        lastLoggedAngle = encoderAngle;
    }
}

void WheelAngleFusion::updateGPSAngle() {
    // Get vehicle speed from GNSS
    if (gnssProcessor) {
        const auto& gpsData = gnssProcessor->getData();
        if (gpsData.hasVelocity) {
            // Convert knots to m/s
            vehicleSpeed = gpsData.speedKnots * 0.514444f;
        } else {
            vehicleSpeed = 0.0f;
        }
        
        // Store heading for rate calculation
        float currentHeading = gpsData.headingTrue;
        
        // Calculate heading rate from GPS heading changes
        if (!config.useIMUHeadingRate) {
            uint32_t now = millis();
            if (lastGPSTime > 0 && now - lastGPSTime < 500) {
                float dt = (now - lastGPSTime) / 1000.0f;
                float headingDelta = currentHeading - lastHeading;
                
                // Handle wrap-around at 0/360 degrees
                if (headingDelta > 180.0f) headingDelta -= 360.0f;
                if (headingDelta < -180.0f) headingDelta += 360.0f;
                
                headingRate = headingDelta / dt;
            }
            lastHeading = currentHeading;
            lastGPSTime = now;
        }
    }
    
    // Get heading rate from IMU if available and configured
    if (config.useIMUHeadingRate && imuProcessor) {
        const auto& imuData = imuProcessor->getCurrentData();
        if (imuData.isValid) {
            headingRate = imuData.yawRate;
        }
    }
    
    // Calculate GPS angle using Ackermann geometry
    gpsAngle = calculateGPSAngleFromHeadingRate(headingRate, vehicleSpeed);
    
    // GPS angle is valid if we have sufficient speed and reasonable heading rate
    gpsAngleValid = (vehicleSpeed >= config.minSpeedForGPS) && 
                    (abs(headingRate) < config.maxHeadingRate);
    
    if (gpsAngleValid) {
        LOG_DEBUG(EventSource::AUTOSTEER, "GPS angle: %.2f° (speed: %.1f m/s, rate: %.1f°/s)", 
                  gpsAngle, vehicleSpeed, headingRate);
    }
}

void WheelAngleFusion::updateKalmanFilter(float dt) {
    // Prediction step using encoder angle
    // The encoder provides our best estimate of angle change
    predictedAngle = encoderAngle;
    
    // Increase uncertainty over time (process noise)
    // Q represents how much we trust the encoder over time
    float predictedUncertainty = uncertainty + config.processNoise * dt;
    
    // Update step - fuse with GPS angle if available
    if (gpsAngleValid && vehicleSpeed >= config.minSpeedForGPS) {
        // Calculate adaptive measurement noise
        // R represents how much we trust the GPS measurement
        float adaptiveR = config.measurementNoise * measurementVariance;
        
        // Calculate Kalman gain
        // K determines how much we trust the new measurement vs prediction
        kalmanGain = predictedUncertainty / (predictedUncertainty + adaptiveR);
        
        // Calculate innovation (measurement residual)
        float innovation = gpsAngle - predictedAngle;
        
        // Sanity check innovation - large values indicate potential GPS error
        if (abs(innovation) > 30.0f) {
            LOG_WARNING(EventSource::AUTOSTEER, "Large innovation: %.1f° - GPS may be unreliable", 
                        innovation);
            // Reduce Kalman gain for large innovations
            kalmanGain *= 0.1f;
        }
        
        // Update estimate
        fusedAngle = predictedAngle + kalmanGain * innovation;
        
        // Update uncertainty
        uncertainty = (1.0f - kalmanGain) * predictedUncertainty;
        
        // Apply encoder drift correction based on GPS
        if (config.enableDriftCompensation && abs(innovation) < 5.0f) {
            // Small, consistent differences indicate encoder drift
            encoderDrift = encoderDrift * 0.99f + innovation * 0.01f;
        }
        
        LOG_DEBUG(EventSource::AUTOSTEER, "Kalman: enc=%.1f° gps=%.1f° fused=%.1f° K=%.3f innov=%.1f°", 
                  encoderAngle, gpsAngle, fusedAngle, kalmanGain, innovation);
    } else {
        // No GPS update, just use encoder prediction
        fusedAngle = predictedAngle;
        uncertainty = predictedUncertainty;
        
        // Apply drift compensation if we have it
        if (config.enableDriftCompensation && abs(encoderDrift) > 0.01f) {
            fusedAngle -= encoderDrift * dt;
        }
    }
    
    // Constrain final angle to reasonable limits
    if (fusedAngle > config.maxSteeringAngle) {
        fusedAngle = config.maxSteeringAngle;
    } else if (fusedAngle < -config.maxSteeringAngle) {
        fusedAngle = -config.maxSteeringAngle;
    }
    
    // Constrain uncertainty to reasonable bounds
    if (uncertainty > 100.0f) {
        uncertainty = 100.0f;
    } else if (uncertainty < 0.001f) {
        uncertainty = 0.001f;
    }
}

void WheelAngleFusion::updateVariance() {
    // Using fixed variance - adaptive calculation not yet implemented
    measurementVariance = 1.0f;
}

void WheelAngleFusion::updateDriftCompensation(float dt) {
    // Drift compensation not yet implemented
    encoderDrift = 0.0f;
    driftRate = 0.0f;
}

float WheelAngleFusion::calculateGPSAngleFromHeadingRate(float headingRate, float speed) {
    // Ackermann steering geometry
    // wheel_angle = atan(heading_rate * wheelbase / speed) * RAD_TO_DEG
    
    if (speed < config.minSpeedForGPS) {
        return 0.0f;
    }
    
    float headingRateRad = headingRate * DEG_TO_RAD;
    float angleRad = atan(headingRateRad * config.wheelbase / speed);
    float angleDeg = angleRad * RAD_TO_DEG;
    
    // Sanity check
    if (angleDeg > config.maxSteeringAngle) {
        angleDeg = config.maxSteeringAngle;
    } else if (angleDeg < -config.maxSteeringAngle) {
        angleDeg = -config.maxSteeringAngle;
    }
    
    return angleDeg;
}

bool WheelAngleFusion::isValidGPSConditions() const {
    // Check if conditions are suitable for GPS angle calculation
    return (vehicleSpeed >= config.minSpeedForGPS &&
            abs(headingRate) < config.maxHeadingRate &&
            abs(fusedAngle) < 30.0f);  // Relatively straight driving
}

bool WheelAngleFusion::isHealthy() const {
    // Check overall system health
    uint32_t now = millis();
    
    // Check if we're getting updates
    if (now - lastUpdateTime > 1000) {
        return false;  // No updates for 1 second
    }
    
    // Check if we've had at least one GPS update
    if (lastGPSTime == 0 || !gpsAngleValid) {
        return false;  // No valid GPS data yet
    }
    
    // Check if uncertainty is reasonable
    if (uncertainty > 50.0f) {
        return false;  // Too uncertain
    }
    
    // Check if angle is reasonable
    if (abs(fusedAngle) > config.maxSteeringAngle * 1.5f) {
        return false;  // Angle out of bounds
    }
    
    // Check if we have minimum speed for reliable fusion
    if (vehicleSpeed < config.minSpeedForGPS) {
        return false;  // Too slow for reliable GPS angle
    }
    
    return true;
}

void WheelAngleFusion::startCalibration() {
    LOG_INFO(EventSource::AUTOSTEER, "Starting wheel angle fusion calibration");
    calibrationMode = true;
    calibrationMinAngle = 999.0f;
    calibrationMaxAngle = -999.0f;
    calibrationMinPosition = INT32_MAX;
    calibrationMaxPosition = INT32_MIN;
}

void WheelAngleFusion::stopCalibration() {
    if (!calibrationMode) return;
    
    calibrationMode = false;
    
    // Calculate counts per degree from calibration data
    if (calibrationMaxAngle > calibrationMinAngle && 
        calibrationMaxPosition > calibrationMinPosition) {
        
        float angleRange = calibrationMaxAngle - calibrationMinAngle;
        int32_t positionRange = calibrationMaxPosition - calibrationMinPosition;
        
        config.countsPerDegree = (float)positionRange / angleRange;
        
        LOG_INFO(EventSource::AUTOSTEER, "Calibration complete:");
        LOG_INFO(EventSource::AUTOSTEER, "  Angle range: %.1f° to %.1f°", 
                 calibrationMinAngle, calibrationMaxAngle);
        LOG_INFO(EventSource::AUTOSTEER, "  Position range: %d to %d", 
                 calibrationMinPosition, calibrationMaxPosition);
        LOG_INFO(EventSource::AUTOSTEER, "  Counts per degree: %.2f", config.countsPerDegree);
    } else {
        LOG_ERROR(EventSource::AUTOSTEER, "Calibration failed - insufficient data");
    }
}

void WheelAngleFusion::setEncoderCenter() {
    if (!keyaDriver) {
        LOG_ERROR(EventSource::AUTOSTEER, "Cannot set encoder center - no Keya driver");
        return;
    }
    
    // Get current position and reset accumulated angle
    uint16_t currentPos = keyaDriver->getMotorPosition();
    encoderOffset = encoderAngle;  // Store current angle as offset
    encoderAngle = 0.0f;           // Reset to zero
    
    // Reset the delta tracking in the driver
    keyaDriver->getPositionDelta(); // Call to reset internal tracking
    
    LOG_INFO(EventSource::AUTOSTEER, "Encoder center set at position %u (offset: %.2f°)", 
             currentPos, encoderOffset);
}

void WheelAngleFusion::reset() {
    LOG_INFO(EventSource::AUTOSTEER, "Resetting wheel angle fusion");
    
    // Reset Kalman filter
    fusedAngle = 0.0f;
    predictedAngle = 0.0f;
    uncertainty = config.initialUncertainty;
    kalmanGain = 0.0f;
    
    // Reset sensor angles
    encoderAngle = 0.0f;
    gpsAngle = 0.0f;
    gpsAngleValid = false;
    
    // Reset drift compensation
    encoderDrift = 0.0f;
    driftRate = 0.0f;
    driftStartTime = millis();
    
    // Clear variance buffer
    for (uint16_t i = 0; i < config.varianceBufferSize; i++) {
        varianceBuffer[i] = 0.0f;
    }
    varianceIndex = 0;
    varianceCount = 0;
    measurementVariance = 1.0f;
}

void WheelAngleFusion::resetDriftCompensation() {
    LOG_INFO(EventSource::AUTOSTEER, "Resetting drift compensation");
    encoderDrift = 0.0f;
    driftRate = 0.0f;
    driftStartTime = millis();
}