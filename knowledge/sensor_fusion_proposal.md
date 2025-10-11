# WAS-less Sensor Fusion Proposal for AiO_New_Dawn

## Executive Summary

This document proposes integrating a sensor fusion algorithm for wheel angle estimation without a traditional Wheel Angle Sensor (WAS). The approach is based on the proven Kalman filter implementation from the AOG_Teensy_UM98X project, which combines motor encoder feedback with GPS/INS heading rate to estimate steering angle.

## Background

### Current State
- AiO_New_Dawn currently requires a physical WAS (potentiometer) for steering feedback
- The system has all necessary sensors available but doesn't utilize them for angle estimation:
  - Keya motor with position feedback via CAN
  - IMU providing heading and rotation rates
  - GPS/GNSS providing heading and speed
  - Motor RPM and current feedback

### Reference Implementation
The AOG_Teensy_UM98X project demonstrates successful field-tested sensor fusion using:
- Keya motor encoder for high-frequency angle changes
- GPS/INS heading rate for drift-free reference
- Adaptive Kalman filter with variance-based gain adjustment

## Technical Overview

### Core Algorithm

The fusion algorithm uses a Kalman filter that combines:

1. **Prediction Step**: Uses encoder changes for rapid response
   ```
   angle_predicted = previous_angle + encoder_change
   ```

2. **Correction Step**: Uses GPS-derived angle for drift correction
   ```
   angle_gps = atan(heading_rate * wheelbase / speed) * RAD_TO_DEG
   angle_fused = angle_predicted + K * (angle_gps - angle_predicted)
   ```

3. **Adaptive Gain**: Kalman gain K adapts based on GPS quality
   ```
   K = P / (P + R * variance)
   ```

### Key Innovation

The algorithm continuously calculates the variance of GPS-derived angles during good conditions (straight driving, good GPS fix). This variance is used to scale the measurement noise (R), making the filter automatically adapt to GPS quality.

## Integration Architecture

### Proposed Class Structure

```cpp
class WheelAngleFusion {
private:
    // Kalman state
    float fusedAngle;          // Current estimate
    float predictionError;     // Uncertainty

    // Sensor inputs
    float motorPosition;       // From KeyaCANDriver
    float vehicleSpeed;        // From GNSS/AgOpenGPS
    float headingRate;         // From IMU or GPS

    // Adaptive parameters
    float measurementVariance;
    CircularBuffer<float> varianceBuffer;

    // Virtual Turn Sensor (Kickout Detection)
    class VirtualTurnSensor {
        float innovationScore;     // Cumulative unexpected behavior
        float expectedAngle;       // Model-based prediction
        CircularBuffer<float> errorPattern;  // Error history for pattern analysis

    public:
        bool detectManualIntervention(float commanded, float actual, float motorCurrent);
        void updateModel(float fusedAngle, float commandedAngle);
        float getInterventionConfidence();
    } virtualTurnSensor;

    // Configuration
    struct Config {
        float wheelbase;       // Vehicle wheelbase (m)
        float steeringRatio;   // Motor counts to wheel degrees
        float minSpeed;        // Minimum speed for fusion (m/s)
        float processNoise;    // Q parameter
        float measurementNoise;// R base parameter
        // Virtual turn sensor config
        float interventionThreshold;  // Confidence threshold for kickout
        bool enableVirtualTurnSensor; // Enable/disable virtual turn detection
    } config;

public:
    void update(float dt);
    float getFusedAngle();
    void calibrate();
    bool isHealthy();
    bool isManualInterventionDetected();  // Virtual turn sensor output
};
```

### Integration Points

1. **AutosteerProcessor**
   - Add fusion as alternative to WAS reading
   - Switch based on `insUseFusion` config flag
   - Maintain compatibility with existing PID control

2. **KeyaCANDriver**
   - Extract and accumulate motor position
   - Calculate position changes between updates
   - Handle position rollover

3. **IMUProcessor/GNSSProcessor**
   - Provide heading rate (either from IMU or GPS)
   - Supply vehicle speed
   - Quality indicators for sensor health

## Implementation Plan

### Phase 1: Basic Fusion (2-3 weeks)
- [ ] Create `SensorFusion` class in `lib/aio_autosteer/`
- [ ] Implement basic Kalman filter with fixed parameters
- [ ] Add motor position tracking to `KeyaCANDriver`
- [ ] Create calibration routine for motor-to-wheel ratio
- [ ] Enable/disable via existing `insUseFusion` config flag
- [ ] Basic testing with logged data

### Phase 2: Enhanced Algorithm (2-3 weeks)
- [ ] Implement adaptive variance calculation
- [ ] Add vehicle dynamics model (bicycle model)
- [ ] Include Ackermann compensation
- [ ] Add sensor health monitoring and fallback logic
- [ ] Implement auto-calibration during straight driving
- [ ] Web interface for monitoring and tuning

### Phase 3: Multi-Sensor Support (3-4 weeks)
- [ ] Generalize for different sensor combinations
- [ ] Add support for steering rate from motor RPM
- [ ] Implement sensor fault detection and switching
- [ ] Create configuration wizard
- [ ] Comprehensive testing suite

## Sensor Configurations

### 1. Premium: Dual GPS + IMU + Encoder
- Best accuracy, full redundancy
- Heading from dual GPS
- Rotation rate from IMU
- Position from encoder

### 2. Standard: Single GPS/INS + Encoder
- Good accuracy, proven in field
- Heading rate from GPS
- Position from encoder
- This is the configuration used in AOG_Teensy_UM98X

### 3. Basic: IMU + Encoder
- Works at all speeds including standstill
- Rotation rate from IMU
- Position from encoder
- Subject to IMU drift over time

### 4. Fallback: Encoder Only
- Emergency mode when other sensors fail
- Dead reckoning with accumulated drift
- Requires periodic recalibration

## Dead Reckoning and Degraded Operation

### GPS Disruption Scenarios

The system must handle various GPS disruption scenarios gracefully:

1. **Temporary Signal Loss (0-30 seconds)**
   - Tree lines, buildings, underpasses
   - Expected multiple times per field operation
   - Must maintain steering accuracy

2. **Extended Signal Loss (30 seconds - 5 minutes)**
   - Dense canopy, valleys, weather
   - May occur 1-2 times per day
   - Gradual accuracy degradation acceptable

3. **Severe Interference (>5 minutes)**
   - Solar storms, jamming, equipment failure
   - Rare but must be handled safely
   - Operator notification required

### Dead Reckoning Architecture

```cpp
class DeadReckoningSystem {
private:
    enum class OperationMode {
        FULL_FUSION,      // GPS + IMU + Encoder
        IMU_AIDED,        // IMU + Encoder (no GPS)
        ENCODER_ONLY,     // Encoder only (no GPS/IMU)
        COAST_MODE        // No sensors, maintain last angle
    };

    struct DRState {
        float cumulativeDrift;     // Accumulated error estimate
        float timeSinceGPS;        // Seconds since last GPS fix
        float confidenceLevel;     // 0-1 confidence in estimate
        OperationMode currentMode;
        bool operatorWarned;
    };

    // Drift models for each sensor
    struct DriftModel {
        float encoderDriftRate;   // degrees/second
        float imuBiasDrift;        // degrees/second²
        float temperatureCoeff;    // drift vs temperature
    };

public:
    void updateWithoutGPS(float dt);
    float getConfidenceLevel();
    bool requiresOperatorIntervention();
    void recalibrateOnGPSReturn();
};
```

### Degradation Strategy

#### Level 1: Short-term GPS Loss (0-30 seconds)
**Mode**: IMU_AIDED or ENCODER_ONLY
- Continue normal operation
- Use last known GPS-derived parameters
- Increase Kalman process noise (Q) gradually
- Monitor cumulative drift estimate
- No operator warning needed

**Implementation**:
```cpp
if (timeSinceGPS < 30.0) {
    // Increase uncertainty gradually
    Q = Q_nominal * (1.0 + timeSinceGPS * 0.1);

    // Use IMU if available and healthy
    if (imu.isHealthy()) {
        // IMU provides heading rate
        headingRate = imu.getYawRate();
        wheelAngle = integrateWithIMU(headingRate, encoderDelta);
    } else {
        // Pure encoder dead reckoning
        wheelAngle = integrateEncoder(encoderDelta);
    }

    confidence = 1.0 - (timeSinceGPS / 60.0);
}
```

#### Level 2: Extended GPS Loss (30 seconds - 5 minutes)
**Mode**: Gradual transition to degraded operation
- Reduce autosteer aggressiveness (lower gains)
- Increase dead band to prevent oscillation
- Visual/audio warning to operator
- Attempt to maintain heading using IMU
- Track accumulated drift for recovery

**Implementation**:
```cpp
if (timeSinceGPS > 30.0 && timeSinceGPS < 300.0) {
    // Alert operator
    if (!drState.operatorWarned) {
        buzzer.warning();
        display.showGPSLost();
        drState.operatorWarned = true;
    }

    // Reduce control gains
    float degradationFactor = min(1.0, 300.0 / timeSinceGPS);
    kp_effective = kp * degradationFactor * 0.7;

    // Increase dead band
    deadBand = deadBand_nominal * (1.0 + timeSinceGPS / 100.0);

    // Estimate drift accumulation
    drState.cumulativeDrift += driftModel.encoderDriftRate * dt;
    drState.cumulativeDrift += driftModel.imuBiasDrift * dt * dt;
}
```

#### Level 3: Severe GPS Loss (>5 minutes)
**Mode**: Safe mode with manual override option
- Recommend disengaging autosteer
- Allow manual steering with power assist only
- Log all sensor data for later analysis
- Prepare for GPS reacquisition

### Drift Compensation Techniques

#### 1. Learned Drift Models
Build statistical models of sensor drift patterns:
- Temperature-dependent encoder drift
- IMU bias instability over time
- Hydraulic system compliance changes
- Tire pressure effects on wheelbase

#### 2. Constraint-Based Corrections
Use physical constraints to bound drift:
- Maximum steering angle limits
- Maximum steering rate limits
- Field boundary constraints (geofence)
- Row-following vision assistance (if available)

#### 3. Opportunistic Recalibration
Detect and use calibration opportunities:
- Straight driving sections (zero angle assumption)
- Field headland turns (known patterns)
- Stopped periods (zero rate assumption)
- Return to previously surveyed points

### GPS Reacquisition and Recovery

When GPS signal returns after disruption:

```cpp
void recalibrateOnGPSReturn() {
    if (gps.isHealthy() && previousMode != FULL_FUSION) {
        // Don't immediately trust GPS
        float reacquisitionTime = 5.0; // seconds

        // Gradually blend GPS back in
        float blendFactor = min(1.0, timeSinceReacquisition / reacquisitionTime);

        // Estimate and remove accumulated drift
        float driftEstimate = drState.cumulativeDrift;
        float correctedAngle = currentAngle - driftEstimate * (1.0 - blendFactor);

        // Reset Kalman filter with higher initial uncertainty
        P = P_nominal * 10.0;
        X = correctedAngle;

        // Gradually reduce uncertainty as GPS proves stable
        R = R_nominal * (1.0 + 5.0 * (1.0 - blendFactor));

        // Learn from the drift pattern
        updateDriftModel(driftEstimate, timeSinceGPS);

        // Clear warnings after stable operation
        if (timeSinceReacquisition > 10.0) {
            drState.operatorWarned = false;
            drState.cumulativeDrift = 0;
            currentMode = FULL_FUSION;
        }
    }
}
```

### Performance During Degraded Operation

| Time Without GPS | Mode | Expected Accuracy | Operator Action |
|-----------------|------|-------------------|-----------------|
| 0-10 sec | IMU+Encoder | ±0.5° | None required |
| 10-30 sec | IMU+Encoder | ±1.0° | Monitor display |
| 30-60 sec | IMU+Encoder | ±2.0° | Reduce speed |
| 1-2 min | Encoder only | ±3.0° | Consider manual |
| 2-5 min | Degraded | ±5.0° | Manual recommended |
| >5 min | Safe mode | N/A | Manual required |

### Testing Dead Reckoning Performance

1. **Simulated GPS Outage Tests**
   - Disable GPS in software while operating
   - Measure angle drift over time
   - Verify smooth mode transitions
   - Test recovery procedures

2. **Real-world Disruption Tests**
   - Operation under tree canopy
   - Driving through barns/structures
   - Testing in valleys/canyons
   - RF interference testing

3. **Drift Model Validation**
   - Long-term data collection
   - Statistical analysis of drift patterns
   - Temperature correlation studies
   - Machine learning for drift prediction

## Virtual Turn Sensor Integration

### Overview

The Virtual Turn Sensor is a software-based safety mechanism that detects manual steering wheel intervention without requiring additional hardware. It works synergistically with the sensor fusion system to provide comprehensive safety monitoring.

### How It Enhances Sensor Fusion

1. **Shared State Information**
   - The fusion system provides high-quality angle estimates for baseline comparison
   - Virtual turn sensor uses fusion confidence levels to adjust sensitivity
   - Both systems share pattern recognition and anomaly detection

2. **Complementary Safety Layers**
   - Sensor fusion handles sensor failures and GPS outages
   - Virtual turn sensor handles manual intervention detection
   - Combined system provides redundant safety mechanisms

### Virtual Turn Sensor Algorithm

```cpp
class EnhancedVirtualTurnSensor {
private:
    // Multi-signal fusion for intervention detection
    struct DetectionState {
        // Primary indicators
        float angleInnovation;     // Difference from expected
        float motorCurrentSpike;   // Sudden current change
        float commandTracking;     // Command vs actual deviation

        // Pattern analysis
        CircularBuffer<float> errorHistory{50};
        float errorTrend;
        float errorFrequency;

        // Context awareness
        OperatingContext context;
        float adaptiveThreshold;
    };

    // Detection methods
    enum DetectionMethod {
        KALMAN_INNOVATION,    // Model-based prediction error
        PATTERN_MATCHING,     // Error pattern analysis
        CURRENT_ANALYSIS,     // Motor current anomalies
        MULTI_SIGNAL_FUSION   // Combined approach
    };

public:
    bool detectIntervention(const FusionState& fusion,
                           float motorCurrent,
                           float commandedAngle) {

        // 1. Calculate innovation from fusion prediction
        float innovation = abs(fusion.angle - fusion.predicted);

        // 2. Analyze motor current for external force
        bool currentAnomaly = detectCurrentAnomaly(motorCurrent);

        // 3. Pattern matching on error history
        bool patternMatch = matchInterventionPattern();

        // 4. Context-aware threshold adjustment
        float threshold = calculateAdaptiveThreshold(fusion.confidence);

        // 5. Multi-signal decision
        float interventionScore =
            innovation * 0.4 +
            currentAnomaly * 0.3 +
            patternMatch * 0.3;

        return interventionScore > threshold;
    }
};
```

### Integration Benefits

1. **Enhanced Accuracy**
   - Fusion system provides clean angle estimates reducing false positives
   - Virtual sensor can detect subtle interventions missed by hardware sensors

2. **Graceful Degradation**
   - If fusion quality degrades, virtual sensor adjusts sensitivity
   - During GPS outages, relies more on motor current and pattern analysis

3. **Cost Reduction**
   - Eliminates need for physical turn sensor ($50-200 savings)
   - No mechanical wear or maintenance

4. **Adaptive Learning**
   - Both systems learn from operational patterns
   - Share calibration data and drift models

### Performance Metrics with Integration

| Scenario | Without Virtual Sensor | With Virtual Sensor |
|----------|------------------------|-------------------|
| Normal Operation | ±0.5° accuracy | ±0.5° accuracy + intervention detection |
| GPS Outage | Degraded accuracy | Degraded accuracy + safety monitoring |
| Manual Override | No detection | 0.3-0.6s detection time |
| Rough Terrain | Higher error | Adaptive thresholds prevent false positives |
| Cost | +$100-200 for sensor | Software only |

### Implementation Synergy

The virtual turn sensor leverages the sensor fusion infrastructure:

1. **Shared Kalman Filter**: Uses fusion's Kalman predictions for baseline
2. **Common Pattern Recognition**: Reuses error pattern analysis
3. **Unified Calibration**: Single calibration process for both systems
4. **Combined Diagnostics**: Integrated health monitoring

### Configuration Parameters

```cpp
struct VirtualSensorConfig {
    // Sensitivity settings
    float baseThreshold = 5.0;        // degrees
    float currentThreshold = 2.0;     // amps
    float timeToTrigger = 0.6;        // seconds

    // Adaptive parameters
    bool enableAdaptive = true;
    float roughTerrainMultiplier = 1.5;
    float lowSpeedMultiplier = 2.0;

    // Integration with fusion
    bool useFusionPrediction = true;
    bool sharePatternAnalysis = true;
    float fusionConfidenceWeight = 0.3;
};
```

## Pros and Cons

### Advantages
- ✅ **Cost Savings**: Eliminates $100+ WAS hardware
- ✅ **Reliability**: No potentiometer wear or mechanical linkage issues
- ✅ **Maintenance**: Immune to dirt/moisture on steering components
- ✅ **Installation**: No sensor mounting or alignment required
- ✅ **Proven**: Algorithm field-tested with good results
- ✅ **Adaptive**: Automatically adjusts to GPS quality

### Disadvantages
- ❌ **Minimum Speed**: Requires ~0.5 m/s for GPS-based correction
- ❌ **Calibration**: Initial setup of steering ratio and wheelbase
- ❌ **Complexity**: More difficult to troubleshoot
- ❌ **GPS Dependent**: Degraded performance in poor GPS conditions
- ❌ **Motor Required**: Only works with motors having position feedback

## Technical Improvements Over Reference

### Algorithm Enhancements
1. **Multi-rate Kalman Filter**
   - Properly handle different sensor update rates
   - Time-synchronized prediction steps

2. **Outlier Rejection**
   - Statistical detection of GPS heading jumps
   - Smooth handling of sensor dropouts

3. **Extended State Vector**
   - Include steering rate and sensor biases
   - Better modeling of system dynamics

4. **Adaptive Process Noise**
   - Increase Q during active steering
   - Decrease Q during straight driving

5. **Latency Compensation**
   - Account for GPS/INS processing delays
   - Timestamp-based synchronization

### Implementation Quality
1. **Modern C++ Design**
   - Template-based for different numeric types
   - RAII for resource management
   - Clear separation of concerns

2. **Testing Infrastructure**
   - Unit tests for algorithm components
   - Integration tests with recorded data
   - Hardware-in-loop simulation

3. **Diagnostics**
   - Real-time web interface for tuning
   - Data logging for offline analysis
   - Performance metrics tracking

## Limitations and Mitigations

| Limitation | Impact | Mitigation Strategy |
|------------|--------|-------------------|
| Minimum speed requirement | No angle estimate when stopped | Use last known angle, detect stationary state |
| GPS outages (tunnels, trees) | Temporary degraded accuracy | Fall back to IMU+encoder, limit integration time |
| Orbital valve non-linearity | Variable motor/wheel ratio | Multi-point calibration, real-time learning, load compensation |
| Calibration drift | Growing angle offset | Auto-calibration during straight sections |
| Initial setup complexity | User frustration | Guided calibration wizard with visual feedback |
| Encoder resolution | Angle quantization noise | Interpolation, recommend high-resolution encoders |
| Steering play/backlash | Hysteresis in measurements | Deadband compensation, direction detection |

## Success Criteria

### Performance Metrics
- **Accuracy**: ±0.5° RMS (compared to ±0.25° for good WAS)
- **Response Time**: <50ms to steering input
- **Minimum Speed**: 0.3 m/s for full accuracy
- **GPS Outage Tolerance**: 30 seconds before significant drift
- **CPU Usage**: <5% on Teensy 4.1

### Functional Requirements
- Seamless fallback to physical WAS if available
- No changes required to AgOpenGPS
- Compatible with existing autosteer tuning
- Works with all supported motor controllers

## Development Timeline

| Week | Tasks |
|------|-------|
| 1-2 | Basic Kalman implementation, motor position tracking |
| 3-4 | Calibration routines, configuration UI |
| 5-6 | Adaptive variance, vehicle dynamics model |
| 7-8 | Multi-sensor support framework |
| 9-10 | Testing suite, performance optimization |
| 11-12 | Field testing, documentation |

Total estimated time: 12 weeks for full implementation

## Recommendations

1. **Start with Phase 1** implementing basic Keya motor + GPS fusion
2. **Maintain WAS compatibility** throughout development
3. **Focus on single GPS + encoder** configuration initially (most common)
4. **Develop comprehensive diagnostics** early for easier debugging
5. **Create simulator** using recorded field data for testing

## Conclusion

The proposed sensor fusion system offers a cost-effective and reliable alternative to traditional wheel angle sensors. By leveraging existing sensors already present in the AiO system, we can provide accurate steering feedback without additional hardware. The adaptive Kalman filter approach from the AOG_Teensy_UM98X project provides a solid foundation, with clear paths for enhancement and generalization.

The phased implementation approach allows for incremental development and testing while maintaining compatibility with existing systems. This ensures that users can benefit from the technology as it develops, without requiring a complete system overhaul.

## References

- AOG_Teensy_UM98X source code: [GitHub Repository]
- Kalman Filter theory: "Optimal State Estimation" by Dan Simon
- Vehicle dynamics: "Vehicle Dynamics and Control" by Rajamani
- AgOpenGPS documentation: [Official Wiki]

## Appendix: Key Code Snippets from Reference

### Variance Calculation (zKalmanKeya.ino)
```cpp
// Calculate running variance for adaptive Kalman gain
if (speed > (settings.minSpeedKalman * 0.7) && 
    abs(insWheelAngle) < 30 && 
    strstr(insStatus, "INS_SOLUTION") != NULL) {
    
    varianceBuffer[indexVarBuffer++] = insWheelAngle;
    indexVarBuffer = indexVarBuffer % lenVarianceBuffer;
    
    // Compute mean
    varianceMean = 0;
    for (uint16_t i = 0; i < lenVarianceBuffer; i++)
        varianceMean += varianceBuffer[i];
    varianceMean /= lenVarianceBuffer;
    
    // Compute variance
    angleVariance = 0;
    for (int i = 0; i < lenVarianceBuffer; i++) {
        angleVariance += (varianceBuffer[i] - varianceMean) * 
                        (varianceBuffer[i] - varianceMean);
    }
    angleVariance /= (lenVarianceBuffer - 1);
}
```

### Kalman Update (zKalmanKeya.ino)
```cpp
void KalmanUpdate() {
    // Encoder change since last update
    float angleDiff = (keyaEncoder / steerSettings.keyaSteerSensorCounts) - 
                      steerAngleActualOld;
    
    X = KalmanWheelAngle;
    
    // Prediction step
    Pp = P + settings.kalmanQ;  // Prediction uncertainty
    Xp = X + angleDiff;         // Predicted angle
    
    // Correction step with adaptive gain
    K = Pp / (Pp + (settings.kalmanR * angleVariance));
    P = (1 - K) * Pp;           // Updated uncertainty
    X = Xp + K * (insWheelAngle - Xp);  // Fused estimate
    
    KalmanWheelAngle = X;
    steerAngleActualOld = keyaEncoder / steerSettings.keyaSteerSensorCounts;
}
```

### GPS Wheel Angle Derivation
```cpp
// Calculate wheel angle from vehicle dynamics
heading_rate = (heading - headingOld) / settings.intervalINS;

// Handle wrap-around
if (heading_rate > 300) heading_rate -= 360;
if (heading_rate < -300) heading_rate += 360;

// Ackermann steering geometry
insWheelAngle = atan(heading_rate / RAD_TO_DEG * 
                     calibrationData.wheelBase / speed) * 
                RAD_TO_DEG * workingDir;

// Sanity check
if (!(insWheelAngle < 50 && insWheelAngle > -50))
    insWheelAngle = 0;
```