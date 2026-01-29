# Sensor Fusion Implementation Plan

## Overview
This plan breaks down the sensor fusion implementation into small, testable increments with frequent compile/test/commit points.

## Phase 1: Foundation (Week 1-2)

### Step 1.1: Basic Infrastructure
**Goal**: Create the foundation classes and integrate with existing system

1. Create `WheelAngleFusion` class skeleton
   - Basic class structure with empty methods
   - Configuration struct
   - Integration with ConfigManager
   
2. Add fusion enable flag to existing config
   - Use existing `insUseFusion` flag in ConfigManager
   - Add web UI toggle in Device Settings page
   
**Test**: Compile and verify no existing functionality broken
**Commit**: "feat: add WheelAngleFusion class skeleton"

### Step 1.2: Motor Position Tracking
**Goal**: Extract and track motor position from Keya CAN messages

1. Update `KeyaCANDriver` to expose motor position
   - Add `getMotorPosition()` method
   - Add `getPositionDelta()` for changes since last call
   - Handle position rollover properly
   
2. Add position tracking variables
   - Current position
   - Previous position
   - Delta accumulator
   
**Test**: Log motor position values, verify they change with steering
**Commit**: "feat: add motor position tracking to KeyaCANDriver"

### Step 1.3: Basic Kalman Filter
**Goal**: Implement core Kalman filter algorithm

1. Implement basic Kalman filter in `WheelAngleFusion`
   - State variables (X, P)
   - Fixed parameters (Q, R)
   - Prediction step using motor encoder
   - Update step (placeholder for GPS angle)
   
2. Add update() method
   - Take encoder delta as input
   - Run prediction step
   - Return current estimate
   
**Test**: Feed known encoder values, verify output changes
**Commit**: "feat: implement basic Kalman filter algorithm"

### Step 1.3a: Tilt-Compensated Position (PREREQUISITE)
**Goal**: Transform GPS antenna position to ground-level vehicle position

**Why this matters**: GPS reports antenna position, but we need ground-level position. On a 15° sidehill with 2m antenna height, the antenna is 0.52m laterally offset from where the wheels actually are. Without compensation, ALL downstream calculations (heading, heading rate, wheel angle) inherit this error.

```
        GPS Antenna ●───────┐
                    │       │ lateral_error = h × sin(roll)
                    │       │ = 2m × sin(15°) = 0.52m
                    │       ▼
    ════════════════●═══════════════  Actual ground position
                Vehicle centerline
```

**Dependency chain**:
```
Good tilt readings → Tilt-compensated position → Accurate heading → Correct wheel angle
       ▲
       │
    THIS STEP
```

**GPS receiver categories for tilt compensation**:
```
┌──────────────────────────────────────────────────────────────────────────┐
│ Receiver Type        │ Internal IMU? │ Tilt Problem?  │ Solution        │
├──────────────────────────────────────────────────────────────────────────┤
│ UM981 (INS)          │ YES           │ SOLVED         │ Uses internal   │
│                      │               │                │ fusion, delivers│
│                      │               │                │ corrected pos   │
├──────────────────────────────────────────────────────────────────────────┤
│ UM982 dual-antenna   │ NO            │ CHICKEN & EGG  │ Same as F9P     │
│ (no internal IMU)    │               │                │ dual - limited  │
├──────────────────────────────────────────────────────────────────────────┤
│ F9P dual + ext IMU   │ External      │ WE SOLVE IT    │ Use BNO085/TM171│
│                      │ (BNO/TM171)   │                │ for correction  │
├──────────────────────────────────────────────────────────────────────────┤
│ F9P/UM982 dual,no IMU│ NO            │ CHICKEN & EGG  │ Limited - see   │
│ (RELPOSNED/KSXT)     │               │                │ mitigations     │
├──────────────────────────────────────────────────────────────────────────┤
│ Single GPS + ext IMU │ External      │ WE SOLVE IT    │ Correct position│
├──────────────────────────────────────────────────────────────────────────┤
│ Single GPS, no IMU   │ NO            │ UNSOLVABLE     │ Cannot correct  │
└──────────────────────────────────────────────────────────────────────────┘
```

**Chicken-and-egg problem applies to: F9P dual-antenna WITHOUT external IMU**
- Roll from dual-GPS geometry is valid, but:
```
┌─────────────────────────────────────────────────────────────────┐
│ Problem                      │ With IMU        │ Dual-GPS Only │
├─────────────────────────────────────────────────────────────────┤
│ Update rate                  │ 100Hz           │ 5-10Hz        │
│ Startup (before convergence) │ IMU ready in 1s │ 30s+ wait     │
│ GPS blockage (trees/building)│ IMU continues   │ Both fail     │
│ Roll/position synchronization│ Independent     │ Same source   │
│ Latency                      │ <10ms           │ 50-200ms      │
└─────────────────────────────────────────────────────────────────┘
```

Mitigations for dual-GPS-only systems (F9P without external IMU):
- **Startup**: Don't apply position correction until dual-GPS converged (heading quality > threshold)
- **Synchronization**: Use roll from SAME message as position (KSXT includes both)
- **Blockage**: Fall back to last known roll with increasing uncertainty, or disable correction
- **Between updates**: Hold previous roll (acceptable at 10Hz, problematic at 5Hz on rough terrain)
- **Recommendation**: Add external IMU (BNO085 ~$15) or upgrade to UM981

**UM981 is the best solution**: Internal IMU handles tilt compensation automatically, delivers corrected position via INSPVAXA, no chicken-and-egg problem. Note: UM982 is dual-antenna only (no internal IMU) - same limitations as F9P.

1. Implement tilt source manager
   - Priority 1: UM981 INS (position already corrected, just use roll for display)
   - Priority 2: External IMU roll/pitch (BNO085/TM171) - 100Hz, fast response
   - Priority 3: Dual-GPS roll (from RELPOSNED or KSXT) - 5-10Hz, limited
   - Automatic failover with quality tracking
   - **Flag when operating in dual-GPS-only mode** (warn user, recommend IMU)
   - Output: `getTiltData()` returns roll, pitch, source, quality, timestamp

2. Add tilt calibration
   - Store roll/pitch zero offsets in EEPROM (existing `insRollOffset`, `insPitchOffset`)
   - Add web UI calibration: "Park on level ground, click Calibrate"
   - Apply offsets before any calculations:
     ```
     roll_corrected = roll_raw - rollOffset
     pitch_corrected = pitch_raw - pitchOffset
     ```

3. Add tilt filtering
   - Low-pass filter for noise reduction (cutoff ~2Hz for roll, ~1Hz for pitch)
   - Reject outliers (|roll| > 45°, |pitch| > 30°)
   - Track tilt rate for dynamic detection:
     ```
     roll_rate = (roll_current - roll_previous) / dt
     if |roll_rate| > 20°/s: flag as dynamic
     ```

4. Implement position compensation in NAVProcessor
   - Add `antennaHeight` to ConfigManager (default 2.0m)

   **CRITICAL: Know your position source!**
   ```
   ┌────────────────────────────────────────────────────────────────────┐
   │ Source          │ Has internal IMU? │ Position tilt-compensated?  │
   ├────────────────────────────────────────────────────────────────────┤
   │ UM981 INSPVAXA  │ YES (onboard)     │ YES - DO NOT double-correct │
   │ UM982 KSXT      │ NO (dual-ant only)│ NO - we must correct        │
   │ F9P RELPOSNED   │ NO                │ NO - we must correct        │
   │ Dual-GPS KSXT   │ NO                │ NO - we must correct        │
   │ Single GPS+IMU  │ External BNO/TM   │ NO - we must correct        │
   │ Single GPS only │ NO                │ NO - cannot correct (no roll)│
   └────────────────────────────────────────────────────────────────────┘
   ```

   - **For UM981 (INSPVAXA with INS):**
     - Position is ALREADY tilt-compensated by internal IMU fusion
     - Pass through position unchanged
     - Use roll/pitch from INSPVAXA for display and heading rate compensation
     - Set `position_compensated = true`

   - **For dual-GPS without internal IMU (F9P RELPOSNED, UM982 KSXT):**
     - Position is raw antenna position - NOT compensated
     - Use roll from SAME message as position (synchronized)
     - Apply our tilt correction
     - KSXT: roll is in pitch field (heading/pitch/roll naming is confusing)

   - **For single GPS + external IMU (BNO085/TM171):**
     - Position is raw antenna position - NOT compensated
     - Interpolate IMU roll to GPS timestamp (100Hz → 10Hz)
     - Apply our tilt correction

   - **For single GPS without IMU:**
     - Cannot correct - no roll source
     - Flag as degraded, warn user

   - Apply tilt correction (when needed):
     ```
     // Skip if source already compensated (UM981 INS only - NOT UM982)
     if (gnssData.hasINS && gnssData.insStatus >= INS_ALIGNED) {
         // Position already tilt-compensated by UM981 internal IMU
         lat_corrected = lat;
         lon_corrected = lon;
         position_compensated = true;
     }
     else if (tilt_quality >= MINIMUM_QUALITY && tilt_age_ms < 200) {
         // Apply our correction
         east_correction = antenna_height × sin(roll)
         north_correction = antenna_height × sin(pitch) × cos(roll)

         lat_corrected = lat + north_correction / meters_per_degree_lat
         lon_corrected = lon - east_correction / meters_per_degree_lon
         position_compensated = true;
     } else {
         // No correction possible
         lat_corrected = lat
         lon_corrected = lon
         position_compensated = false;
         set_degraded_flag()
     }
     ```
   - Send corrected position in PAOGI message
   - Keep roll/pitch fields for AgOpenGPS display
   - **Add flag indicating whether position is tilt-compensated**

5. Add tilt data quality indicators
   - Tilt source in use (IMU/Dual-GPS/INS)
   - Tilt data age (staleness detection)
   - Tilt variance (noise level)
   - Dynamic flag (rapid tilt changes)

**Test**:
- Calibrate on level ground, verify zero offsets stored
- Tilt vehicle manually, verify roll/pitch values correct
- Park on known slope, verify position correction magnitude
- Compare PAOGI lat/lon with and without correction on sidehill

**Commit**: "feat: add tilt-compensated position calculation"

### Step 1.4: GPS Angle Calculation
**Goal**: Calculate steering angle from GPS/INS data

**Critical Issue - Antenna Lateral Displacement on Sidehills**:
When vehicle rolls on a sidehill, the GPS antenna (mounted high on cab) moves laterally:
```
lateral_offset = antenna_height × sin(roll)
```
Example: 2m antenna height, 15° roll → 0.52m lateral displacement

This corrupts GPS heading rate and must be compensated before wheel angle calculation.

1. Add antenna height compensation
   - Add `antennaHeight` to vehicle configuration (default 2.0m)
   - Get roll angle from IMU (preferred) or dual-GPS
   - Calculate lateral velocity component from roll:
     ```
     // Antenna lateral velocity due to roll rate
     v_lateral_roll = antenna_height × cos(roll) × roll_rate

     // Correct GPS velocity before heading calculation
     v_north_corrected = v_north - v_lateral_roll × sin(heading)
     v_east_corrected = v_east - v_lateral_roll × cos(heading)
     ```
   - For steady-state roll (constant sidehill), the effect is on position not velocity
   - For dynamic roll (entering/exiting sidehill), velocity correction is critical

2. Add GPS angle calculation with roll compensation
   - Get heading rate from NAVProcessor
   - Apply antenna displacement correction to heading rate:
     ```
     // Raw heading rate includes antenna swing component
     heading_rate_raw = atan2(v_east, v_north) derivative

     // Antenna swing rate due to roll change
     antenna_swing_rate = (antenna_height / speed) × roll_rate × cos(roll)

     // Corrected heading rate
     heading_rate = heading_rate_raw - antenna_swing_rate
     ```
   - Get vehicle speed from NAVProcessor
   - Calculate wheel angle using Ackermann geometry:
     ```
     wheel_angle = atan(wheelbase × heading_rate / speed)
     ```
   - Add wheelbase to configuration

3. Add roll-dependent measurement noise
   - Increase GPS measurement noise R when roll is significant:
     ```
     R_adjusted = R_base × (1 + k × |roll|)
     ```
   - Higher roll = more antenna displacement uncertainty
   - Helps Kalman filter weight encoder more on sidehills

4. Add sanity checks
   - Minimum speed threshold (>0.5 m/s for reliable heading)
   - Maximum angle limits (±45°)
   - Roll rate limit check (reject during rapid roll changes)
   - Invalid data detection

**Test**:
- Log calculated GPS angles at various speeds
- Test on sidehill: compare corrected vs uncorrected angles
- Verify roll compensation reduces cross-track error on slopes

**Commit**: "feat: add GPS-derived wheel angle with roll compensation"

### Step 1.5: Connect to AutosteerProcessor
**Goal**: Use fusion output for steering control

1. Modify `AutosteerProcessor::updateMotorControl()`
   - Check if fusion is enabled
   - Get angle from fusion instead of ADProcessor
   - Maintain WAS fallback option

2. Add fusion status to PGN 253
   - Include fusion health status
   - Send fusion angle vs WAS angle

**Test**: Enable fusion, verify steering still responds
**Commit**: "feat: connect sensor fusion to autosteer control"

### Step 1.6: Virtual Turn Sensor Foundation
**Goal**: Add virtual turn sensor for manual intervention detection

1. Create `VirtualTurnSensor` class within `WheelAngleFusion`
   - Error accumulation tracking
   - Pattern buffer for error history
   - Intervention detection method stub

2. Add basic detection logic
   - Track commanded vs actual angle deviation
   - Simple threshold-based detection
   - Configurable sensitivity parameter

**Test**: Manually grab wheel during autosteer, verify detection
**Commit**: "feat: add virtual turn sensor foundation"

## Phase 2: Calibration & Tuning (Week 3-4)

### Step 2.1: Orbital Valve Compensation
**Goal**: Handle non-linear relationship between motor and wheel angle

**CRITICAL**: Orbital valves are the primary steering system in most tractors and have significant non-linearity that MUST be addressed for acceptable accuracy.

1. Multi-point calibration system
   - Collect motor position vs actual angle at multiple points (-40° to +40° in 4° steps)
   - Build interpolation tables for different load conditions (low/medium/high)
   - Track steering rate effects on the ratio
   - Implement smooth curve fitting between points
   - Save calibration curves to EEPROM
   
2. Load detection system
   - Monitor motor current for load estimation
   - Track position change rate vs wheel angle change rate
   - Use vehicle speed as additional load indicator
   - Build load-dependent compensation curves
   
3. Real-time learning and adaptation
   - Continuously update calibration during operation with GPS available
   - Use exponential smoothing for gradual adaptation
   - Detect and compensate for temperature effects
   - Track long-term valve wear patterns
   - Only learn from high-confidence GPS data (straight sections, good fix)
   
4. Web interface for calibration
   - Add calibration wizard page
   - Real-time angle display with motor position
   - Visual curve editor with interpolation preview
   - Load condition indicators
   - Import/export calibration data
   - Compare current vs learned curves
   
**Test**: 
- Collect data under various loads (empty field, heavy implement, slopes)
- Compare linear vs compensated accuracy
- Verify learning improves accuracy over time
- Test with different hydraulic temperatures

**Expected Results**:
- Improve accuracy from ±2° (linear) to ±0.5° (compensated)
- Consistent performance across all load conditions
- Reduced need for manual recalibration

**Commit**: "feat: add orbital valve compensation with multi-point calibration"

### Step 2.2: Wheelbase Calibration
**Goal**: Determine vehicle wheelbase for GPS angle calculation

1. Add auto-calibration during straight driving
   - Detect straight sections (low heading rate)
   - Compare encoder angle to GPS angle
   - Adjust wheelbase to minimize error
   
2. Manual wheelbase entry
   - Add to vehicle settings
   - Persist to EEPROM
   
**Test**: Drive straight sections, verify wheelbase converges
**Commit**: "feat: add wheelbase calibration"

### Step 2.3: Fixed Parameter Tuning
**Goal**: Tune Q and R parameters for optimal performance

1. Add parameter adjustment
   - Web interface sliders for Q and R
   - Real-time parameter updates
   - Visual feedback of filter behavior
   
2. Add performance metrics
   - Innovation (measurement residual)
   - Uncertainty (P value)
   - Lag between encoder and output
   
**Test**: Adjust parameters while steering, find optimal values
**Commit**: "feat: add Kalman filter parameter tuning"

## Phase 3: Adaptive Algorithm & Dead Reckoning (Week 5-6)

### Step 3.1: Variance Calculation
**Goal**: Calculate GPS angle variance for adaptive gain

1. Implement circular buffer for variance
   - Store recent GPS angles during straight driving
   - Calculate running mean and variance
   - Update only during good conditions

2. Add variance monitoring
   - Display current variance
   - Show buffer status
   - Log variance changes

**Test**: Drive various patterns, verify variance calculation
**Commit**: "feat: add adaptive variance calculation"

### Step 3.2: Adaptive Kalman Gain
**Goal**: Make filter adapt to GPS quality

1. Implement adaptive R calculation
   - Scale R based on variance
   - Add min/max limits
   - Smooth transitions

2. Add quality indicators
   - GPS fix quality
   - Speed adequacy
   - Heading rate validity

**Test**: Simulate GPS degradation, verify adaptation
**Commit**: "feat: implement adaptive Kalman gain"

### Step 3.3: Dead Reckoning System
**Goal**: Maintain operation during GPS outages

**Key insight from GPS-IMU fusion research**: Use the Kalman filter's process model (equation 1: `xt = f(xt-1, ut-1) + wt-1`) to propagate state during GPS outages, with covariance growth naturally tracking confidence degradation.

1. Implement process model for prediction-only mode
   - State vector: `[θ, θ_dot, drift_rate]` (angle, angular rate, learned drift)
   - Process model:
     ```
     θ_new = θ_old + encoder_delta × scale + drift_rate × dt
     θ_dot_new = encoder_delta / dt
     drift_rate_new = drift_rate_old  // constant during outage
     ```
   - Covariance propagation: `P_new = F × P_old × F' + Q`
   - Q (process noise) grows uncertainty each prediction step
   - No arbitrary time thresholds—covariance tells us confidence

2. Implement `DeadReckoningSystem` class
   - Operation mode state machine (FULL_FUSION, IMU_AIDED, ENCODER_ONLY, COAST_MODE)
   - Mode transitions based on covariance thresholds, not timers:
     - `P < P_good`: FULL_FUSION (normal operation)
     - `P < P_degraded`: IMU_AIDED (no GPS, but acceptable)
     - `P < P_warning`: ENCODER_ONLY (warn operator)
     - `P >= P_critical`: COAST_MODE (recommend manual)
   - Suggested thresholds: P_good=1°², P_degraded=4°², P_warning=9°², P_critical=25°²
   - GPS outage timer for logging/diagnostics only

3. IMU cross-validation during dead reckoning
   - Compare encoder-derived angular rate to IMU yaw rate:
     ```
     encoder_rate = encoder_delta × scale / dt
     imu_rate = yaw_rate × wheelbase / speed  // Ackermann conversion
     rate_error = |encoder_rate - imu_rate|
     ```
   - If `rate_error > threshold` for sustained period:
     - Possible encoder slip, valve lag, or intervention
     - Increase Q temporarily (faster covariance growth)
     - Flag for virtual turn sensor
   - If rates agree well, encoder is trustworthy

4. Adaptive process noise Q
   - Base Q from learned drift variance during GPS-available periods
   - Increase Q based on operating conditions:
     - `Q_adjusted = Q_base × speed_factor × roughness_factor × temp_factor`
     - Speed factor: higher speed = faster angle changes = more uncertainty
     - Roughness factor: from IMU accelerometer variance (bumpy field)
     - Temperature factor: hydraulic viscosity changes affect response
   - This makes covariance growth realistic, not pessimistic

5. Implement sensor fallback hierarchy
   - Primary: GPS + IMU + Encoder (full measurement update)
   - Fallback 1: IMU + Encoder (IMU provides partial observability)
     - Use IMU yaw rate as weak measurement: `z_imu = yaw_rate × wheelbase / speed`
     - Higher R (less trust) than GPS-derived angle
   - Fallback 2: Encoder only (prediction only, no measurement update)
     - Covariance grows unbounded
   - Emergency: Coast on last known angle
     - If encoder also fails, hold last angle
     - Maximum covariance, strong warnings

6. Bounded covariance with physical constraints
   - Cap P at physical maximum: wheel can't be more than ±45° uncertain
   - `P_max = (45°)² = 2025°²`
   - Apply angle constraints in prediction: clamp θ to [-45°, +45°]
   - Prevents filter divergence during extended outages

**Test**:
- Disable GPS at various durations, verify smooth degradation
- Monitor covariance growth rate, verify matches expected drift
- Test IMU cross-validation with simulated encoder errors
- Verify mode transitions occur at appropriate uncertainty levels

**Commit**: "feat: add dead reckoning with covariance-based degradation"

### Step 3.4: GPS Reacquisition
**Goal**: Smooth recovery when GPS returns

**Key insight from GPS-IMU fusion research**: Rather than fixed blend periods, use the Kalman filter's natural covariance mechanics for mathematically optimal reacquisition.

1. Implement innovation-based validation
   - Calculate innovation: `v = z_gps - z_predicted` (difference between GPS measurement and encoder prediction)
   - Compute normalized innovation squared (NIS): `NIS = v² / (P + R)`
   - Apply chi-squared gating: reject measurement if `NIS > threshold` (typically 5.99 for 95% confidence, 1 DOF)
   - This detects GPS jumps vs legitimate position changes
   - Log rejected measurements for diagnostics

2. Covariance-driven trust rebuilding (replaces fixed 5-second blend)
   - On GPS return, DON'T reinitialize filter state
   - Instead, inflate state covariance P based on outage duration:
     - `P_inflated = P_current + (drift_rate² × outage_time²)`
   - Let Kalman gain naturally increase due to high P
   - Trust rebuilds organically as measurements reduce P
   - Typical convergence: 2-10 updates depending on drift accumulated

3. Sequential measurement acceptance
   - First GPS fix after outage: apply extra-conservative gating (NIS < 3.0)
   - If rejected, wait for next fix rather than forcing acceptance
   - After 3 consecutive accepted fixes, return to normal gating
   - Prevents single bad fix from corrupting state

4. Add drift learning
   - Track drift patterns during outages
   - On reacquisition, compute actual drift: `drift = z_gps - z_predicted`
   - Update drift model: `drift_rate = α × (drift / outage_time) + (1-α) × drift_rate_old`
   - Use exponential smoothing (α ≈ 0.1) for gradual adaptation
   - Correlate with temperature, load, and outage duration
   - Store learned drift rates for predictive compensation

5. Add operator notifications
   - GPS lost warning (after 30 seconds)
   - Degraded mode indication with confidence percentage
   - GPS recovered confirmation
   - "Stabilizing..." indicator during covariance convergence

**Test**:
- Cycle GPS on/off, verify smooth transitions
- Inject simulated GPS jumps, verify rejection
- Measure convergence time vs outage duration
- Verify drift model improves over multiple outages

**Commit**: "feat: add GPS reacquisition with innovation gating"

### Step 3.5: Drift Compensation
**Goal**: Minimize error accumulation during dead reckoning

1. Add drift models
   - Encoder drift rate (degrees/second)
   - IMU bias drift (degrees/second²)
   - Temperature compensation
   - Load-dependent drift

2. Implement constraint-based corrections
   - Maximum angle limits (±45°)
   - Maximum rate limits (based on hydraulic specs)
   - Sanity checks on sensor readings

3. Add opportunistic recalibration
   - Detect straight driving (low yaw rate)
   - Zero-angle assumption when stopped
   - Pattern recognition for headland turns

**Test**: Extended operation without GPS, measure drift rates
**Commit**: "feat: add drift compensation and constraints"

## Phase 4: Enhanced Features (Week 7-8)

### Step 4.1: Multi-Sensor Support
**Goal**: Support different sensor combinations with robust roll compensation

**Key challenge**: GPS antenna mounted high on cab rocks ±0.5m laterally on sidehills. IMU provides essential roll data for compensation.

1. Roll source priority system
   - Priority 1: IMU roll (BNO085/TM171) - fastest, most reliable
   - Priority 2: Dual-GPS roll (from RELPOSNED or KSXT pitch field)
   - Priority 3: INS roll (from INSPVAXA if available)
   - Automatic failover between sources
   - Roll source health monitoring (staleness, variance)

2. Add IMU heading rate option
   - Get yaw rate from IMU gyroscope
   - Advantage: Not affected by antenna displacement
   - Use when GPS heading rate is unreliable (low speed, high roll rate)
   - Cross-validate with GPS-derived heading rate when both available:
     ```
     // IMU yaw rate is clean (body-fixed)
     // GPS heading rate includes antenna motion artifacts
     // Agreement indicates good GPS conditions
     rate_agreement = |imu_yaw_rate - gps_heading_rate_corrected|
     ```

3. Sidehill-specific fusion weights
   - Detect sidehill operation: `|roll| > 5°`
   - On sidehill, increase trust in IMU yaw rate vs GPS heading rate:
     ```
     R_gps_heading = R_base × (1 + k_sidehill × |roll|)
     ```
   - IMU gyro not affected by antenna position, so more reliable
   - Log sidehill operation statistics for tuning

4. Roll rate limiting for GPS corrections
   - During rapid roll changes (entering/exiting sidehill):
     ```
     if |roll_rate| > threshold:  // e.g., 5°/second
         // Antenna is swinging, GPS heading unreliable
         // Rely more on IMU and encoder
         R_gps = R_gps × 5  // Temporarily distrust GPS
     ```
   - Resume normal GPS trust when roll stabilizes

5. Add sensor priority system
   - Define sensor hierarchy per measurement type:
     - Roll: IMU > Dual-GPS > INS
     - Heading rate: IMU (sidehill) / GPS (flat) > encoder-derived
     - Speed: GPS > wheel speed sensor
   - Automatic failover with covariance adjustment
   - Health monitoring and staleness detection

**Test**:
- Disable various sensors, verify failover
- Test on 15° sidehill: verify roll compensation reduces error
- Compare GPS-only vs IMU-aided heading on sidehills
- Verify rapid roll transitions handled correctly

**Commit**: "feat: add multi-sensor support with roll compensation"

### Step 4.2: Enhanced Virtual Turn Sensor
**Goal**: Improve intervention detection with advanced algorithms

1. Implement pattern recognition
   - Sliding window error analysis
   - Frequency domain analysis for human tremor detection
   - Step change detection for sudden interventions

2. Add motor current analysis
   - Monitor current spikes indicating external force
   - Correlate current with angle errors
   - Detect fighting against autopilot

3. Context-aware detection
   - Different thresholds for straight/turning/rough terrain
   - Speed-dependent sensitivity
   - Automatic rough terrain detection from IMU

**Test**: Test in various conditions, measure false positive rate
**Commit**: "feat: enhance virtual turn sensor with pattern recognition"

### Step 4.3: Advanced Diagnostics
**Goal**: Comprehensive diagnostics and monitoring

1. Add diagnostic web page
   - Real-time sensor values
   - Fusion algorithm state
   - Performance metrics
   - Graphical displays
   
2. Add data logging
   - Record all sensor inputs
   - Log fusion outputs
   - Export for analysis
   
**Test**: Monitor during various conditions
**Commit**: "feat: add fusion diagnostics interface"

### Step 4.4: Vehicle Dynamics Model
**Goal**: Improve prediction with vehicle model

1. Add bicycle model
   - Include steering rate limits
   - Model steering system inertia
   - Account for tire slip
   
2. Add Ackermann compensation
   - Inside/outside wheel angles
   - Speed-dependent effects
   - Calibratable parameters
   
**Test**: High-speed steering, verify improved tracking
**Commit**: "feat: add vehicle dynamics model"

## Phase 5: Testing & Optimization (Week 9-10)

### Step 5.1: Test Suite
**Goal**: Comprehensive testing infrastructure

1. Unit tests
   - Test each component in isolation
   - Edge cases and error conditions
   - Performance benchmarks
   
2. Integration tests
   - Full system tests
   - Recorded data playback
   - Regression testing
   
**Test**: Run full test suite
**Commit**: "test: add sensor fusion test suite"

### Step 5.2: Performance Optimization
**Goal**: Optimize for Teensy 4.1

1. Profile code performance
   - Identify bottlenecks
   - Optimize math operations
   - Reduce memory allocations
   
2. Add performance monitoring
   - CPU usage tracking
   - Update rate monitoring
   - Latency measurement
   
**Test**: Verify <5% CPU usage
**Commit**: "perf: optimize sensor fusion performance"

### Step 5.3: Documentation
**Goal**: Complete user and developer documentation

1. User documentation
   - Setup guide
   - Calibration procedures
   - Troubleshooting guide
   
2. Developer documentation
   - API reference
   - Algorithm details
   - Extension guide
   
**Test**: Follow documentation as new user
**Commit**: "docs: add sensor fusion documentation"

## Testing Strategy

### After Each Step:
1. **Compile**: Ensure no build errors
2. **Basic Function**: Verify existing features still work
3. **New Feature**: Test the specific new functionality
4. **Logging**: Add temporary debug logs to verify behavior
5. **Commit**: Make atomic commit with clear message

### Integration Testing Points:
- End of Phase 1: Basic fusion working
- End of Phase 2: Calibrated and tuned
- End of Phase 3: Adaptive algorithm and dead reckoning active
- End of Phase 4: All features integrated
- End of Phase 5: Production ready

### Field Testing Milestones:
1. **Stationary**: Verify angle tracking with manual steering
2. **Low Speed**: Test in yard at <5 mph
3. **Field Speed**: Normal operation at 5-10 mph
4. **Edge Cases**: Test GPS loss, tight turns, rough terrain

### Dead Reckoning Test Protocol:

#### Test 1: Short GPS Outage (10-30 seconds)
1. **Setup**: Normal field operation at 5 mph
2. **Action**: Disable GPS for 20 seconds
3. **Expected**:
   - Steering continues normally
   - Mode remains IMU_AIDED (P < P_degraded = 4°²)
   - No operator warning
   - Accuracy within ±1°
4. **Recovery**: Re-enable GPS
5. **Verify**:
   - Innovation gating accepts first fix (NIS < 5.99)
   - Covariance drops back to P_good within 3-5 updates
   - Smooth transition back to full fusion

#### Test 2: Extended GPS Outage (1-2 minutes)
1. **Setup**: Straight line operation
2. **Action**: Disable GPS for 90 seconds
3. **Expected**:
   - Mode transitions: FULL_FUSION → IMU_AIDED → ENCODER_ONLY
   - Warning when P exceeds P_warning (9°²)
   - Covariance grows predictably per process noise Q
   - Accuracy within ±3°
4. **Recovery**: Re-enable GPS
5. **Verify**:
   - First GPS fix may be rejected if drift was large (NIS > threshold)
   - Drift correction applied over 3-5 accepted measurements
   - Covariance converges, not instant reset

#### Test 3: GPS Cycling (Intermittent Loss)
1. **Setup**: Normal operation
2. **Action**: Cycle GPS on/off every 15 seconds for 5 minutes
3. **Expected**:
   - Covariance oscillates but stays bounded
   - No mode oscillation (hysteresis prevents rapid switching)
   - Innovation gating prevents bad fix acceptance
   - Drift model updates with each reacquisition
4. **Verify**:
   - Drift rate estimate converges to true value
   - System stability improves as drift model learns

#### Test 4: IMU Cross-Validation
1. **Setup**: Normal operation with GPS disabled
2. **Action**: Introduce encoder error (e.g., slip, miscalibration)
3. **Expected**:
   - IMU yaw rate disagrees with encoder-derived rate
   - `rate_error = |encoder_rate - imu_rate|` exceeds threshold
   - Process noise Q increases (faster covariance growth)
   - Virtual turn sensor flag raised
4. **Verify**: System detects inconsistency, doesn't blindly trust encoder

#### Test 5: IMU Failure During GPS Outage
1. **Setup**: Disable GPS
2. **Action**: Disable IMU after 10 seconds
3. **Expected**:
   - Mode transition: IMU_AIDED → ENCODER_ONLY
   - Covariance growth rate increases (lost cross-validation)
   - Additional warning to operator
   - Degraded but functional steering
4. **Recovery**: Re-enable both sensors
5. **Verify**:
   - IMU cross-validation resumes
   - Proper sensor priority restoration
   - Covariance converges faster with IMU available

#### Test 6: Complete Sensor Loss
1. **Setup**: Normal operation
2. **Action**: Disable all sensors sequentially
3. **Expected**:
   - Mode transitions: FULL → IMU_AIDED → ENCODER_ONLY → COAST_MODE
   - Covariance hits P_critical (25°²) then P_max (2025°²)
   - Strong warning to operator
   - Maintains last known angle (clamped to ±45°)
   - Suggests manual control
4. **Recovery**: Re-enable sensors
5. **Verify**: System recovers gracefully with high initial uncertainty

#### Test 7: Innovation Gating Validation
1. **Setup**: Normal operation
2. **Action**: Inject simulated GPS jump (sudden 10° offset)
3. **Expected**:
   - Innovation `v = z_gps - z_predicted` is large (~10°)
   - NIS = v² / (P + R) exceeds threshold
   - Measurement rejected, state unchanged
   - Log shows "GPS measurement rejected: NIS=XX.X"
4. **Verify**: Bad GPS fix doesn't corrupt filter state

#### Test 8: Real-World Scenarios
1. **Tree Line Navigation**:
   - Drive along tree lines with intermittent GPS
   - Monitor covariance and mode transitions
   - Verify innovation gating handles multipath

2. **Building/Structure Passage**:
   - Drive through barn or under structure
   - Test 30-60 second GPS loss
   - Verify covariance-based warnings appropriate

3. **Valley Operations**:
   - Test in areas with poor GPS coverage
   - Monitor drift model learning over multiple passes
   - Verify adaptive Q responds to terrain roughness

#### Performance Metrics to Track:
- **Covariance growth rate** (°²/second) vs predicted Q
- **Innovation statistics**: mean, variance, NIS distribution
- **Drift rate estimate** accuracy (compare to actual on reacquisition)
- **Mode transition thresholds**: verify P-based triggers work
- **GPS rejection rate**: should be low (<5%) in good conditions
- **Recovery convergence time**: updates to reach P_good
- **IMU cross-validation error rate**: false positive/negative rates
- **CPU usage during dead reckoning**
- Maximum drift rate (degrees/minute)

## Risk Mitigation

### Technical Risks:
1. **Performance**: Start with 10Hz update rate, optimize later
2. **Accuracy**: Always maintain WAS fallback option
3. **Stability**: Extensive testing before removing WAS support

### Schedule Risks:
1. **Delays**: Each phase is independently useful
2. **Complexity**: Can stop at Phase 2 for basic functionality
3. **Testing**: Allocate 20% extra time for unforeseen issues

## Success Metrics

### Phase 1 Complete:
- Tilt-compensated position operational:
  - Tilt calibration working (zero offsets stored)
  - Position correction applied in PAOGI (verified on 10°+ slope)
  - Tilt source failover: IMU → Dual-GPS → INS
- Fusion angle within 2° of WAS
- No impact on existing functionality
- Basic operation at >1 m/s

### Phase 2 Complete:
- Orbital valve compensation working with 3-level load detection
- Multi-point calibration implemented (21+ points per load level)
- Real-time learning adapting to changing conditions
- Angle accuracy ±0.5° RMS across all conditions (improved from ±2°)
- Stable operation under varying loads and temperatures
- Web-based calibration wizard functional

### Phase 3 Complete:
- Adapts to GPS quality changes via adaptive R calculation
- Covariance-based mode transitions working (FULL_FUSION → IMU_AIDED → ENCODER_ONLY → COAST)
- Dead reckoning operational for 0-5 minute GPS outages:
  - 30-second outage: P stays below P_degraded (4°²), accuracy ±1°
  - 90-second outage: P reaches P_warning (9°²), accuracy ±3°
  - 5-minute outage: controlled degradation, accuracy ±5°
- Innovation gating operational:
  - GPS jump rejection working (NIS threshold = 5.99)
  - GPS rejection rate <5% in good conditions
  - No state corruption from multipath or reacquisition errors
- IMU cross-validation detecting encoder inconsistencies
- Smooth GPS reacquisition:
  - Covariance-driven trust rebuilding (no fixed blend time)
  - Recovery to P_good within 5-10 measurements
  - Drift learning improving over multiple outage cycles
- Drift rate <2°/minute during dead reckoning (matches Q prediction)
- Operator warnings triggered by covariance thresholds, not timers
- No manual tuning required—filter self-calibrates

### Phase 4 Complete:
- Supports 3+ sensor configurations with automatic failover
- Roll compensation operational:
  - Sidehill accuracy within ±1° (same as flat ground)
  - Antenna displacement correction working (tested at 15° roll)
  - Roll source failover: IMU → Dual-GPS → INS
  - Rapid roll transitions handled without GPS corruption
- IMU yaw rate integration reducing GPS dependency on sidehills
- Comprehensive diagnostics available
- Field-tested in various conditions including sidehills

### Phase 5 Complete:
- Meets all performance targets
- Complete documentation
- Ready for production use