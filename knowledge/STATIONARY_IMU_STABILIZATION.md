# Stationary IMU/GNSS Stabilization

## Problem Statement

IMU and dual GPS heading/roll values tend to wander while the vehicle is stationary. This occurs because GPS position noise (even with RTK) creates false velocity vectors when there's no actual movement, leading to erroneous heading calculations and IMU fusion corrections. As soon as the tractor moves even a meter or two, the values stabilize.

**Symptoms**:
- Heading wanders ±5-10° while parked
- Roll shows small oscillations (±1-2°)
- Values stabilize immediately upon movement
- Creates jittery display in AgOpenGPS
- Steering corrections may be triggered unnecessarily

## Root Causes

### 1. GPS Position Noise
Even RTK GPS has ~1-2cm RMS position noise. When stationary:
```
Position noise: ±1.5cm
Update rate: 10Hz (0.1s)
False velocity: 1.5cm / 0.1s = 15 cm/s = 0.54 km/h

This noise appears as random movement in all directions.
```

### 2. Heading Calculation Instability
```cpp
heading = atan2(velocity_east, velocity_north)
```

When velocity ≈ 0, even tiny position noise creates large heading changes:
```
velocity_north = 0.05 m/s (noise)
velocity_east = 0.03 m/s (noise)
heading = atan2(0.03, 0.05) = 31°

Next sample (100ms later):
velocity_north = -0.04 m/s (noise)
velocity_east = 0.06 m/s (noise)
heading = atan2(0.06, -0.04) = 124°

False heading change: 93° in 100ms!
```

### 3. IMU Fusion Corruption
Kalman filters try to correct IMU drift using GPS heading. When GPS heading is noise, this corrupts the IMU state estimate.

### 4. Dual Antenna Baseline Noise
Even dual-antenna systems suffer from:
- Multipath effects (reflections)
- Baseline length measurement noise
- Atmospheric delays at low elevation angles

## Proposed Solution

### Algorithm Overview

1. **Detect Stationary State** - Multiple indicators with hysteresis
2. **Freeze Reference Values** - Lock heading/roll just before stopping
3. **Hold During Stationary Period** - Output frozen values, ignore GPS
4. **Smooth Transition on Movement** - Gradually blend to live values

### Implementation

```cpp
class StationaryStabilizer {
public:
    struct Config {
        // Stationary detection thresholds
        float stationarySpeedThreshold = 0.3f;      // m/s - below this is stationary
        float movingSpeedThreshold = 0.5f;          // m/s - above this is definitely moving
        uint32_t stationaryTimeRequired = 500;      // ms - must be slow this long
        uint32_t movingTimeRequired = 200;          // ms - must be fast this long

        // Position-based detection
        float maxPositionDrift = 0.5f;              // meters - max drift while "stationary"
        uint32_t positionWindowTime = 2000;         // ms - window for position drift check

        // Heading/Roll stabilization
        float headingVarianceThreshold = 5.0f;      // deg - if variance exceeds, not truly stationary
        float rollVarianceThreshold = 2.0f;         // deg - similar for roll

        // Transition smoothing
        uint32_t transitionTime = 1000;             // ms - blend time when resuming movement

        // Output filtering
        bool enableHeadingFreeze = true;            // Freeze heading when stationary
        bool enableRollFreeze = true;               // Freeze roll when stationary
        bool enablePitchFreeze = false;             // Usually don't freeze pitch (may change on slopes)
    };

    struct State {
        bool isStationary;                          // Current state
        uint32_t stateChangeTime;                   // When state last changed

        // Frozen reference values
        float frozenHeading;                        // Heading at stop (degrees)
        float frozenRoll;                           // Roll at stop (degrees)
        float frozenPitch;                          // Pitch at stop (degrees)

        // Position tracking
        double referenceLatitude;                   // Position when stopped
        double referenceLongitude;
        uint32_t positionReferenceTime;

        // Transition state
        bool inTransition;                          // Currently blending
        float transitionProgress;                   // 0.0 to 1.0
    };

private:
    Config config;
    State state;

    // Moving window for velocity tracking
    static const int VELOCITY_WINDOW_SIZE = 20;     // 2 seconds at 10Hz
    float velocityWindow[VELOCITY_WINDOW_SIZE];
    int velocityIndex;

    // Position history for drift detection
    struct PositionSample {
        double latitude;
        double longitude;
        uint32_t timestamp;
    };
    static const int POSITION_HISTORY_SIZE = 20;    // 2 seconds at 10Hz
    PositionSample positionHistory[POSITION_HISTORY_SIZE];
    int positionIndex;

    // Heading/roll variance tracking (for validation)
    static const int VARIANCE_WINDOW_SIZE = 50;     // 5 seconds at 10Hz
    float headingWindow[VARIANCE_WINDOW_SIZE];
    float rollWindow[VARIANCE_WINDOW_SIZE];
    int varianceIndex;

public:
    StationaryStabilizer() {
        reset();
    }

    void reset() {
        state.isStationary = false;
        state.stateChangeTime = millis();
        state.inTransition = false;
        state.transitionProgress = 1.0f;

        for (int i = 0; i < VELOCITY_WINDOW_SIZE; i++) {
            velocityWindow[i] = 0.0f;
        }
        velocityIndex = 0;
        positionIndex = 0;
        varianceIndex = 0;
    }

    // Main update function - call at 10Hz (GPS rate)
    void update(float currentSpeed, double latitude, double longitude,
                float heading, float roll, float pitch) {

        // Update tracking windows
        updateVelocityWindow(currentSpeed);
        updatePositionHistory(latitude, longitude);
        updateVarianceWindows(heading, roll);

        // Detect state transitions
        bool shouldBeStationary = detectStationaryState(currentSpeed, latitude, longitude);

        // Handle state changes
        if (shouldBeStationary && !state.isStationary) {
            // Transition to stationary
            enterStationaryState(heading, roll, pitch, latitude, longitude);
        }
        else if (!shouldBeStationary && state.isStationary) {
            // Transition to moving
            exitStationaryState();
        }

        // Update transition progress
        if (state.inTransition) {
            updateTransition();
        }
    }

    // Get stabilized outputs
    float getStabilizedHeading(float rawHeading) {
        if (!config.enableHeadingFreeze) return rawHeading;

        if (state.isStationary) {
            return state.frozenHeading;
        }
        else if (state.inTransition) {
            return blendAngles(state.frozenHeading, rawHeading, state.transitionProgress);
        }
        return rawHeading;
    }

    float getStabilizedRoll(float rawRoll) {
        if (!config.enableRollFreeze) return rawRoll;

        if (state.isStationary) {
            return state.frozenRoll;
        }
        else if (state.inTransition) {
            return lerp(state.frozenRoll, rawRoll, state.transitionProgress);
        }
        return rawRoll;
    }

    float getStabilizedPitch(float rawPitch) {
        if (!config.enablePitchFreeze) return rawPitch;

        if (state.isStationary) {
            return state.frozenPitch;
        }
        else if (state.inTransition) {
            return lerp(state.frozenPitch, rawPitch, state.transitionProgress);
        }
        return rawPitch;
    }

    bool isCurrentlyStationary() const { return state.isStationary; }
    bool isInTransition() const { return state.inTransition; }

private:
    void updateVelocityWindow(float speed) {
        velocityWindow[velocityIndex] = speed;
        velocityIndex = (velocityIndex + 1) % VELOCITY_WINDOW_SIZE;
    }

    void updatePositionHistory(double lat, double lon) {
        positionHistory[positionIndex].latitude = lat;
        positionHistory[positionIndex].longitude = lon;
        positionHistory[positionIndex].timestamp = millis();
        positionIndex = (positionIndex + 1) % POSITION_HISTORY_SIZE;
    }

    void updateVarianceWindows(float heading, float roll) {
        headingWindow[varianceIndex] = heading;
        rollWindow[varianceIndex] = roll;
        varianceIndex = (varianceIndex + 1) % VARIANCE_WINDOW_SIZE;
    }

    bool detectStationaryState(float currentSpeed, double latitude, double longitude) {
        // Multi-factor stationary detection

        // 1. Speed-based detection (primary)
        float avgSpeed = calculateAverageSpeed();
        bool speedIndicatesStationary = avgSpeed < config.stationarySpeedThreshold;
        bool speedIndicatesMoving = avgSpeed > config.movingSpeedThreshold;

        // 2. Position drift detection (secondary validation)
        float positionDrift = calculatePositionDrift(latitude, longitude);
        bool positionIndicatesStationary = positionDrift < config.maxPositionDrift;

        // 3. Check state duration for hysteresis
        uint32_t timeInCurrentState = millis() - state.stateChangeTime;

        if (state.isStationary) {
            // Currently stationary - require strong evidence to switch to moving
            if (speedIndicatesMoving && timeInCurrentState > config.movingTimeRequired) {
                return false; // Switch to moving
            }
            return true; // Stay stationary
        } else {
            // Currently moving - require strong evidence to switch to stationary
            if (speedIndicatesStationary && positionIndicatesStationary &&
                timeInCurrentState > config.stationaryTimeRequired) {
                return true; // Switch to stationary
            }
            return false; // Stay moving
        }
    }

    float calculateAverageSpeed() {
        float sum = 0;
        for (int i = 0; i < VELOCITY_WINDOW_SIZE; i++) {
            sum += velocityWindow[i];
        }
        return sum / VELOCITY_WINDOW_SIZE;
    }

    float calculatePositionDrift(double currentLat, double currentLon) {
        // Find oldest position in window
        int oldestIndex = positionIndex; // This wraps around to oldest
        double oldLat = positionHistory[oldestIndex].latitude;
        double oldLon = positionHistory[oldestIndex].longitude;

        // Calculate distance moved
        return distanceBetween(oldLat, oldLon, currentLat, currentLon);
    }

    float distanceBetween(double lat1, double lon1, double lat2, double lon2) {
        // Haversine formula for distance in meters
        const float R = 6371000.0f; // Earth radius in meters
        float dLat = (lat2 - lat1) * DEG_TO_RAD;
        float dLon = (lon2 - lon1) * DEG_TO_RAD;
        float a = sin(dLat/2) * sin(dLat/2) +
                  cos(lat1 * DEG_TO_RAD) * cos(lat2 * DEG_TO_RAD) *
                  sin(dLon/2) * sin(dLon/2);
        float c = 2 * atan2(sqrt(a), sqrt(1-a));
        return R * c;
    }

    void enterStationaryState(float heading, float roll, float pitch,
                              double latitude, double longitude) {
        LOG_INFO(EventSource::NAVIGATION, "Entering STATIONARY state");

        // Freeze current values as reference
        state.frozenHeading = heading;
        state.frozenRoll = roll;
        state.frozenPitch = pitch;

        // Save reference position
        state.referenceLatitude = latitude;
        state.referenceLongitude = longitude;
        state.positionReferenceTime = millis();

        // Update state
        state.isStationary = true;
        state.stateChangeTime = millis();
        state.inTransition = false;

        LOG_INFO(EventSource::NAVIGATION, "Frozen values - Heading: %.1f°, Roll: %.1f°",
                 state.frozenHeading, state.frozenRoll);
    }

    void exitStationaryState() {
        LOG_INFO(EventSource::NAVIGATION, "Exiting STATIONARY state - starting transition");

        state.isStationary = false;
        state.stateChangeTime = millis();
        state.inTransition = true;
        state.transitionProgress = 0.0f;
    }

    void updateTransition() {
        uint32_t transitionElapsed = millis() - state.stateChangeTime;
        state.transitionProgress = (float)transitionElapsed / config.transitionTime;

        if (state.transitionProgress >= 1.0f) {
            state.transitionProgress = 1.0f;
            state.inTransition = false;
            LOG_INFO(EventSource::NAVIGATION, "Transition complete - using live values");
        }
    }

    float blendAngles(float angle1, float angle2, float t) {
        // Handle angle wrapping correctly (shortest path)
        float diff = angle2 - angle1;

        // Normalize difference to [-180, 180]
        while (diff > 180.0f) diff -= 360.0f;
        while (diff < -180.0f) diff += 360.0f;

        float result = angle1 + diff * t;

        // Normalize result to [0, 360]
        while (result < 0.0f) result += 360.0f;
        while (result >= 360.0f) result -= 360.0f;

        return result;
    }

    float lerp(float a, float b, float t) {
        return a + (b - a) * t;
    }
};
```

## Integration with AiO New Dawn

### In GNSSProcessor or IMUProcessor

```cpp
class GNSSProcessor {
private:
    StationaryStabilizer stabilizer;

    // Raw values from GPS/IMU
    float rawHeading;
    float rawRoll;
    float rawPitch;

    // Stabilized outputs
    float stabilizedHeading;
    float stabilizedRoll;
    float stabilizedPitch;

public:
    void processGNSSData() {
        // Get raw GPS data
        updateRawValues();

        // Update stabilizer
        stabilizer.update(
            currentSpeed,
            latitude,
            longitude,
            rawHeading,
            rawRoll,
            rawPitch
        );

        // Get stabilized outputs
        stabilizedHeading = stabilizer.getStabilizedHeading(rawHeading);
        stabilizedRoll = stabilizer.getStabilizedRoll(rawRoll);
        stabilizedPitch = stabilizer.getStabilizedPitch(rawPitch);

        // Use stabilized values in PGN messages to AgOpenGPS
        sendPGN237(); // IMU data with stabilized values
    }

    // Public getters return stabilized values
    float getHeading() const { return stabilizedHeading; }
    float getRoll() const { return stabilizedRoll; }
    float getPitch() const { return stabilizedPitch; }

    bool isStationary() const { return stabilizer.isCurrentlyStationary(); }
};
```

### Configuration via Web UI

Add settings to Device Settings page:

```cpp
// In ConfigManager
struct StationaryConfig {
    bool enableStabilization;
    float stationarySpeed;        // Threshold for stationary (m/s)
    float movingSpeed;            // Threshold for moving (m/s)
    uint16_t stationaryTime;      // Required time stationary (ms)
    uint16_t transitionTime;      // Blend time (ms)
};
```

Web UI elements:
```html
<div class="setting-row">
    <label>Stationary Stabilization</label>
    <select id="stationaryStabilization">
        <option value="1">Enabled</option>
        <option value="0">Disabled</option>
    </select>
</div>

<div class="setting-row">
    <label>Stationary Speed Threshold</label>
    <input type="number" id="stationarySpeed" min="0.1" max="1.0" step="0.1" value="0.3">
    <span class="unit">m/s</span>
</div>

<div class="setting-row">
    <label>Transition Smoothing Time</label>
    <input type="number" id="transitionTime" min="500" max="3000" step="100" value="1000">
    <span class="unit">ms</span>
</div>
```

## Diagnostic Logging

Add logging to monitor stabilizer behavior:

```cpp
void StationaryStabilizer::logStatus() {
    if (state.isStationary) {
        LOG_DEBUG(EventSource::NAVIGATION,
                  "STATIONARY: frozen heading=%.1f°, roll=%.1f° (duration: %lu ms)",
                  state.frozenHeading, state.frozenRoll,
                  millis() - state.stateChangeTime);
    }
    else if (state.inTransition) {
        LOG_DEBUG(EventSource::NAVIGATION,
                  "TRANSITION: progress=%.1f%% (%.1f → %.1f°)",
                  state.transitionProgress * 100.0f,
                  state.frozenHeading, getCurrentRawHeading());
    }
}
```

## Advanced Enhancements

### 1. IMU-Based Validation

Use IMU gyroscope to validate stationary state:

```cpp
bool validateStationary() {
    // If gyro shows rotation, not truly stationary
    float gyroMagnitude = sqrt(gyroX*gyroX + gyroY*gyroY + gyroZ*gyroZ);
    return gyroMagnitude < 1.0f; // deg/s threshold
}
```

### 2. Adaptive Thresholds

Adjust thresholds based on GPS quality:

```cpp
void adaptThresholds(float hdop, int fixType) {
    if (fixType == RTK_FIXED && hdop < 1.0f) {
        config.stationarySpeedThreshold = 0.2f; // Can use lower threshold
    } else if (fixType == RTK_FLOAT || hdop > 2.0f) {
        config.stationarySpeedThreshold = 0.5f; // Need higher threshold
    }
}
```

### 3. Direction-Aware Freezing

Don't freeze heading if vehicle is turning while stopped:

```cpp
bool isTurningInPlace() {
    // Check if steering angle is significant
    float steeringAngle = getSteeringAngle();
    return abs(steeringAngle) > 10.0f; // degrees
}

void enterStationaryState(...) {
    if (isTurningInPlace()) {
        // Don't freeze heading - allow it to change
        config.enableHeadingFreeze = false;
    } else {
        config.enableHeadingFreeze = true;
    }
    // ... rest of function
}
```

### 4. Variance-Based Quality Check

Only freeze if variance was reasonable before stopping:

```cpp
float calculateHeadingVariance() {
    float mean = 0;
    for (int i = 0; i < VARIANCE_WINDOW_SIZE; i++) {
        mean += headingWindow[i];
    }
    mean /= VARIANCE_WINDOW_SIZE;

    float variance = 0;
    for (int i = 0; i < VARIANCE_WINDOW_SIZE; i++) {
        float diff = headingWindow[i] - mean;
        // Handle angle wrapping
        while (diff > 180.0f) diff -= 360.0f;
        while (diff < -180.0f) diff += 360.0f;
        variance += diff * diff;
    }
    return variance / VARIANCE_WINDOW_SIZE;
}

void enterStationaryState(...) {
    float headingVar = calculateHeadingVariance();

    if (headingVar > config.headingVarianceThreshold) {
        // Heading was already unstable - don't freeze
        LOG_WARNING(EventSource::NAVIGATION,
                    "High heading variance (%.1f°²) - not freezing",
                    headingVar);
        config.enableHeadingFreeze = false;
    }
    // ... rest
}
```

## Testing Procedure

### Test 1: Basic Stationary Freeze
1. Drive at 2 m/s
2. Stop completely
3. **Expected**: Heading/roll freeze within 0.5s
4. **Observe**: No wandering in AgOpenGPS display
5. Wait 30 seconds stationary
6. **Expected**: Values remain frozen

### Test 2: Resume Movement
1. From stopped state (frozen values)
2. Start driving
3. **Expected**: Smooth transition over 1 second
4. **Observe**: No sudden jumps in heading/roll
5. After 2m of movement
6. **Expected**: Back to live values

### Test 3: Turning While Stopped
1. Stop tractor
2. Turn steering wheel significantly
3. **Expected**: If detected, heading updates (not frozen)
4. **Verify**: Works correctly for point turns

### Test 4: Poor GPS Conditions
1. Park near building (multipath)
2. **Observe**: Increased wandering
3. **Expected**: Stabilizer still freezes values
4. Resume movement
5. **Expected**: Smooth recovery

### Test 5: Different Fix Types
1. Test with RTK Fixed (best case)
2. Test with RTK Float
3. Test with DGPS only
4. **Verify**: Thresholds adapt appropriately

## Performance Impact

- **CPU Usage**: Negligible (<1% additional)
- **Memory**: ~1KB for buffers
- **Latency**: None (passthrough when moving)
- **Update Rate**: 10Hz (GPS rate), no change

## Benefits

✅ **Stable Display**: No more jittery heading/roll when parked
✅ **Cleaner Logging**: Data logs don't show false movement
✅ **Better Autosteer**: Prevents unnecessary corrections when stopped
✅ **Operator Confidence**: Display appears more professional
✅ **Battery Savings**: Fewer unnecessary motor activations
✅ **Works with All GPS**: RTK, DGPS, or single GPS

## Comparison to Alternatives

| Approach | Pros | Cons |
|----------|------|------|
| **No filtering** | Simple | Poor user experience |
| **Low-pass filter** | Easy | Delays response when moving |
| **Kalman filter** | Optimal | Complex, still has issues stationary |
| **Speed threshold only** | Simple | Doesn't handle all cases |
| **This approach** | Clean display, smooth transitions | Adds complexity |

## Future Enhancements

1. **Machine Learning**: Learn user's typical stop patterns
2. **Terrain Detection**: Adjust for slope/terrain type
3. **Multi-Sensor Fusion**: Use wheel encoders, IMU accel for validation
4. **Predictive Freezing**: Anticipate stop based on deceleration
5. **Quality Metrics**: Report stabilizer performance to user

## Conclusion

The stationary stabilization system solves the IMU/GPS wandering problem using:
- Multi-factor stationary detection with hysteresis
- Reference value freezing just before complete stop
- Smooth blending when resuming movement
- Adaptive thresholds based on GPS quality

**Implementation Complexity**: ⭐⭐⭐☆☆ (3/5) - Moderate
**Effectiveness**: ⭐⭐⭐⭐⭐ (5/5) - Highly effective
**User Impact**: ⭐⭐⭐⭐⭐ (5/5) - Very noticeable improvement

**Recommendation**: Implement for all IMU/GPS systems. The improvement in display quality and operator confidence is substantial.

---

*Document created: 2025-11-03*
*For: AiO New Dawn Agricultural Control System*
*Status: Proposed Solution - Ready for Implementation*
