# Motor Driver Hot-Swap Implementation Plan

## Overview
Enable dynamic motor driver switching without requiring a system reboot. This builds on the existing HardwareManager resource management architecture to provide seamless motor type changes via PGN 251.

## Current State
- Motor type changes trigger system reboot (AutosteerProcessor.cpp:842)
- Sensor changes (encoder/pressure/current) work dynamically
- HardwareManager provides pin ownership tracking and transfer
- No cleanup mechanism exists for motor drivers

## Why Reboot is Currently Required
1. **No cleanup mechanism** - Old driver doesn't release resources
2. **Pin conflicts** - Different motors use different pins (PWM vs CAN vs Danfoss)
3. **PWM settings** - Different frequencies and resolutions per driver type
4. **CAN callbacks** - TractorCANDriver/KeyaSerial register callbacks that persist
5. **Resource leaks** - Old driver object remains in memory

## Architecture Already in Place
✅ **HardwareManager** - Pin ownership tracking and transfer capability
✅ **MotorDriverInterface** - Common interface for all motor types
✅ **MotorDriverManager** - Centralized motor driver creation
✅ **CANManager** - CAN message routing (supports callback unregistration)
✅ **Resource coordination** - PWM timer tracking via SharedResourceManager

## Implementation Steps

### Phase 1: Add Cleanup Interface (Easy - 1 hour)

#### 1.1 Update MotorDriverInterface.h
Add cleanup method to base interface:
```cpp
class MotorDriverInterface {
public:
    virtual ~MotorDriverInterface() = default;

    // Add cleanup method
    virtual void deinit() = 0;  // Release all resources (pins, callbacks, etc.)

    // Existing methods...
};
```

#### 1.2 Document Cleanup Contract
Each `deinit()` implementation must:
- Release all owned pins via HardwareManager
- Unregister all CAN/Serial callbacks
- Stop PWM output
- Set outputs to safe state (LOW or center position)
- Clear any internal state

### Phase 2: Implement Cleanup for Each Driver (Moderate - 3 hours)

#### 2.1 PWMMotorDriver::deinit()
```cpp
void PWMMotorDriver::deinit() {
    LOG_INFO(EventSource::AUTOSTEER, "PWMMotorDriver: Cleaning up resources");

    // Stop PWM output
    setPWM(0);
    enable(false);

    // Release pins via HardwareManager
    if (hwManager) {
        // PWM pins
        if (usePWM1) hwManager->releasePin(PIN_PWM1, "PWMMotorDriver");
        if (usePWM2) hwManager->releasePin(PIN_PWM2, "PWMMotorDriver");

        // Direction pins (if used)
        if (hasDirectionPins) {
            hwManager->releasePin(PIN_DIR1, "PWMMotorDriver");
            hwManager->releasePin(PIN_DIR2, "PWMMotorDriver");
        }

        // Enable pin (if used)
        if (hasEnablePin) {
            hwManager->releasePin(PIN_MOTOR_ENABLE, "PWMMotorDriver");
        }
    }

    // Set pins to safe state before release
    analogWrite(PIN_PWM1, 0);
    analogWrite(PIN_PWM2, 0);
    if (hasDirectionPins) {
        digitalWrite(PIN_DIR1, LOW);
        digitalWrite(PIN_DIR2, LOW);
    }

    LOG_INFO(EventSource::AUTOSTEER, "PWMMotorDriver: Cleanup complete");
}
```

#### 2.2 TractorCANDriver::deinit()
```cpp
void TractorCANDriver::deinit() {
    LOG_INFO(EventSource::AUTOSTEER, "TractorCANDriver: Cleaning up resources");

    // Send disable command to motor
    enable(false);
    setPWM(0);

    // Unregister CAN callbacks
    if (canManager) {
        // Unregister from all CAN message IDs this driver was listening to
        switch (currentBrand) {
            case TractorBrand::KEYA:
            case TractorBrand::GENERIC:
                canManager->unregisterCallback(0x7000001); // Keya heartbeat
                canManager->unregisterCallback(0x5800001); // Keya acknowledgment
                break;
            case TractorBrand::FENDT:
                canManager->unregisterCallback(0x18FE6E17); // Fendt messages
                break;
            // ... other brands
        }
    }

    // Clear internal state
    motorPosition = 0;
    actualRPM = 0.0f;
    isOnline = false;
    lastHeartbeatTime = 0;

    LOG_INFO(EventSource::AUTOSTEER, "TractorCANDriver: Cleanup complete");
}
```

#### 2.3 KeyaSerialDriver::deinit()
```cpp
void KeyaSerialDriver::deinit() {
    LOG_INFO(EventSource::AUTOSTEER, "KeyaSerialDriver: Cleaning up resources");

    // Send disable command
    enable(false);
    setPWM(0);

    // Note: Serial port managed by SerialManager, don't close it
    // Just stop using it

    // Clear internal state
    motorPosition = 0;
    isOnline = false;

    LOG_INFO(EventSource::AUTOSTEER, "KeyaSerialDriver: Cleanup complete");
}
```

#### 2.4 DanfossMotorDriver::deinit()
```cpp
void DanfossMotorDriver::deinit() {
    LOG_INFO(EventSource::AUTOSTEER, "DanfossMotorDriver: Cleaning up resources");

    // Center the valve (50% PWM on both outputs)
    setPWM(0);
    enable(false);

    // Release PCA9685 outputs 7 & 8
    if (hwManager) {
        hwManager->releasePin(PCA9685_OUTPUT_7, "DanfossMotorDriver");
        hwManager->releasePin(PCA9685_OUTPUT_8, "DanfossMotorDriver");
    }

    // Restore outputs 5 & 6 to MachineProcessor if they were protected
    // (This is handled by HardwareManager pin ownership transfer)

    LOG_INFO(EventSource::AUTOSTEER, "DanfossMotorDriver: Cleanup complete");
}
```

### Phase 3: Add Motor Switching Logic (Moderate - 2 hours)

#### 3.1 Add switchMotorDriver() to MotorDriverManager
```cpp
// MotorDriverManager.h
class MotorDriverManager {
public:
    // Add switching method
    bool switchMotorDriver(MotorDriverType newType,
                          HardwareManager* hwMgr,
                          CANManager* canMgr,
                          MotorDriverInterface** currentDriverPtr);
private:
    MotorDriverInterface* currentDriver = nullptr;
};

// MotorDriverManager.cpp
bool MotorDriverManager::switchMotorDriver(MotorDriverType newType,
                                          HardwareManager* hwMgr,
                                          CANManager* canMgr,
                                          MotorDriverInterface** currentDriverPtr) {
    LOG_INFO(EventSource::AUTOSTEER, "Switching motor driver from %s to %s",
             currentDriver ? currentDriver->getTypeName() : "NONE",
             getTypeName(newType));

    // 1. Safety check - ensure autosteer is disabled
    if (currentDriver && currentDriver->getStatus().enabled) {
        LOG_ERROR(EventSource::AUTOSTEER, "Cannot switch motor while enabled");
        return false;
    }

    // 2. Cleanup old driver
    if (currentDriver) {
        LOG_INFO(EventSource::AUTOSTEER, "Cleaning up old driver...");
        currentDriver->enable(false);
        currentDriver->setPWM(0);
        currentDriver->deinit();
        delete currentDriver;
        currentDriver = nullptr;
        *currentDriverPtr = nullptr;
    }

    // 3. Create new driver
    LOG_INFO(EventSource::AUTOSTEER, "Creating new driver...");
    currentDriver = createMotorDriver(newType, hwMgr, canMgr);
    if (!currentDriver) {
        LOG_ERROR(EventSource::AUTOSTEER, "Failed to create new motor driver");
        return false;
    }

    // 4. Initialize new driver
    LOG_INFO(EventSource::AUTOSTEER, "Initializing new driver...");
    if (!currentDriver->init()) {
        LOG_ERROR(EventSource::AUTOSTEER, "Failed to initialize new motor driver");
        delete currentDriver;
        currentDriver = nullptr;
        return false;
    }

    // 5. Update caller's pointer
    *currentDriverPtr = currentDriver;

    LOG_INFO(EventSource::AUTOSTEER, "Motor driver switch complete: %s",
             currentDriver->getTypeName());
    return true;
}

const char* MotorDriverManager::getTypeName(MotorDriverType type) {
    switch (type) {
        case MotorDriverType::CYTRON_MD30C: return "Cytron MD30C";
        case MotorDriverType::IBT2: return "IBT-2";
        case MotorDriverType::DRV8701: return "DRV8701";
        case MotorDriverType::KEYA_CAN: return "Keya CAN";
        case MotorDriverType::KEYA_SERIAL: return "Keya Serial";
        case MotorDriverType::DANFOSS: return "Danfoss";
        case MotorDriverType::GENERIC_PWM: return "Generic PWM";
        case MotorDriverType::TRACTOR_CAN: return "Tractor CAN";
        default: return "Unknown";
    }
}
```

### Phase 4: Update AutosteerProcessor (Easy - 1 hour)

#### 4.1 Update PGN 251 Handler
Replace reboot code with motor switching:

```cpp
// AutosteerProcessor.cpp - handlePGN251Settings()
if (motorTypeChanged) {
    LOG_WARNING(EventSource::AUTOSTEER, "Motor type changed - switching driver...");

    // Ensure autosteer is disabled
    autosteerEnabled = false;
    if (motorPTR) {
        motorPTR->enable(false);
        motorPTR->setPWM(0);
    }

    // Switch motor driver
    MotorDriverManager* motorMgr = MotorDriverManager::getInstance();
    extern HardwareManager* hardwareManagerPtr;
    extern CANManager* canManagerPtr;

    if (motorMgr->switchMotorDriver(newMotorType, hardwareManagerPtr,
                                    canManagerPtr, &motorPTR)) {
        LOG_INFO(EventSource::AUTOSTEER, "Motor driver switched successfully");

        // Update dependent modules
        KickoutMonitor::getInstance()->setMotorDriver(motorPTR);

        // Re-initialize VWAS if enabled and new motor has encoder
        if (configManager.getINSUseFusion()) {
            if (wheelAngleFusionPtr) {
                delete wheelAngleFusionPtr;
                wheelAngleFusionPtr = nullptr;
            }
            initializeFusion();
        }
    } else {
        LOG_ERROR(EventSource::AUTOSTEER, "Motor driver switch failed - rebooting...");
        delay(2000);
        SCB_AIRCR = 0x05FA0004; // Fallback to reboot if switch fails
    }
}
```

#### 4.2 Add setMotorDriver() Method
```cpp
// AutosteerProcessor.h
class AutosteerProcessor {
public:
    void setMotorDriver(MotorDriverInterface* driver);
};

// AutosteerProcessor.cpp
void AutosteerProcessor::setMotorDriver(MotorDriverInterface* driver) {
    motorPTR = driver;
    LOG_INFO(EventSource::AUTOSTEER, "Motor driver updated: %s",
             driver ? driver->getTypeName() : "NONE");
}
```

### Phase 5: Handle VWAS Dependency (Moderate - 1 hour)

#### 5.1 Update initializeFusion() for Hot-Swap
```cpp
void AutosteerProcessor::initializeFusion() {
    LOG_INFO(EventSource::AUTOSTEER, "Virtual WAS enabled - initializing VWAS system");

    // Delete existing fusion instance if present
    if (wheelAngleFusionPtr) {
        delete wheelAngleFusionPtr;
        wheelAngleFusionPtr = nullptr;
    }

    // Create new fusion instance
    wheelAngleFusionPtr = new WheelAngleFusion();

    // Extract encoder interface from motor driver
    KeyaCANDriver* keyaDriver = nullptr;
    if (motorPTR) {
        MotorDriverType motorType = motorPTR->getType();

        // Check if motor provides encoder feedback
        if (motorType == MotorDriverType::KEYA_CAN) {
            keyaDriver = static_cast<KeyaCANDriver*>(motorPTR);
            LOG_INFO(EventSource::AUTOSTEER, "VWAS: Keya driver encoder available");
        } else if (motorType == MotorDriverType::TRACTOR_CAN) {
            TractorCANDriver* tractorDriver = static_cast<TractorCANDriver*>(motorPTR);
            if (tractorDriver->hasPositionFeedback()) {
                keyaDriver = reinterpret_cast<KeyaCANDriver*>(motorPTR);
                LOG_INFO(EventSource::AUTOSTEER, "VWAS: TractorCAN encoder available");
            }
        }

        if (!keyaDriver) {
            LOG_WARNING(EventSource::AUTOSTEER,
                       "VWAS: Motor type %s has no encoder - using GPS/IMU only",
                       motorPTR->getTypeName());
        }
    }

    // Initialize with available sensors
    extern GNSSProcessor* gnssProcessorPtr;
    extern IMUProcessor imuProcessor;

    if (wheelAngleFusionPtr->init(keyaDriver, gnssProcessorPtr, &imuProcessor)) {
        LOG_INFO(EventSource::AUTOSTEER, "Virtual WAS initialized: %s",
                 wheelAngleFusionPtr->getFusionMode());
    } else {
        LOG_ERROR(EventSource::AUTOSTEER, "Virtual WAS initialization failed");
        delete wheelAngleFusionPtr;
        wheelAngleFusionPtr = nullptr;
    }
}
```

### Phase 6: Testing Plan (2-3 hours)

#### 6.1 Test Matrix
Test all motor type transitions:

| From          | To            | Expected Result                          |
|---------------|---------------|------------------------------------------|
| PWM (Cytron)  | CAN (Tractor) | Releases PWM pins, initializes CAN       |
| PWM (IBT2)    | PWM (Cytron)  | Releases IBT2 pins, claims Cytron pins   |
| CAN (Tractor) | PWM (DRV8701) | Unregisters CAN, initializes PWM         |
| CAN (Tractor) | Danfoss       | Unregisters CAN, claims PCA9685 outputs  |
| Danfoss       | PWM (Cytron)  | Releases PCA9685, initializes PWM        |
| Any           | NONE          | Full cleanup, no new driver              |

#### 6.2 Test Scenarios
1. **Basic switching**: Change motor type via AgOpenGPS settings, verify no reboot
2. **VWAS handling**: Enable VWAS with encoder motor, switch to non-encoder, verify graceful degradation
3. **Autosteer active**: Attempt switch while autosteer enabled, verify rejection
4. **Pin conflicts**: Switch between motors using same pins, verify proper transfer
5. **CAN cleanup**: Switch from TractorCAN, verify no CAN messages leak
6. **Multiple switches**: Perform multiple back-to-back switches, check for resource leaks

#### 6.3 Validation Checks
- [ ] No memory leaks (check available RAM before/after)
- [ ] No zombie CAN callbacks (monitor CAN traffic)
- [ ] All pins properly released (check HardwareManager ownership)
- [ ] Motor responds correctly after switch
- [ ] VWAS mode adapts correctly
- [ ] No reboots occur during normal switches
- [ ] System reboots gracefully if switch fails

## Implementation Order

1. **Day 1**: Phase 1 + Phase 2 (Interface + Driver cleanup implementations)
2. **Day 2**: Phase 3 + Phase 4 (Switching logic + AutosteerProcessor updates)
3. **Day 3**: Phase 5 + Phase 6 (VWAS handling + Testing)

Total: ~6-8 hours development + 2-3 hours testing = **8-11 hours**

## Safety Considerations

### Critical Safety Checks
1. **Autosteer must be disabled** - Never switch while motor is active
2. **Motor outputs to safe state** - Set to 0 PWM / center position before cleanup
3. **Graceful fallback** - Reboot if switching fails to prevent undefined state
4. **User notification** - Log all switch events clearly
5. **Timeout protection** - Complete switch within reasonable time or fail

### Risk Mitigation
- Test extensively on bench before field testing
- Implement rollback to reboot if anything fails
- Add extensive logging at each step
- Verify pin states with multimeter during development

## Benefits

### User Experience
- **Faster configuration**: No wait for reboot (2-3 seconds vs 15-20 seconds)
- **Easier experimentation**: Try different motor types without disruption
- **Professional feel**: Seamless like sensor hot-swapping

### Technical Benefits
- **Validates architecture**: Proves resource management system works
- **Consistent behavior**: Motors now behave like sensors
- **Better diagnostics**: Explicit cleanup helps identify resource issues
- **Easier maintenance**: Clear ownership and lifecycle

## Future Enhancements

Once hot-swapping works:
1. **Web UI preview**: Show which pins will be used before switching
2. **Pin conflict detection**: Warn user if new motor conflicts with other modules
3. **Configuration profiles**: Save/load complete motor configurations
4. **Zero-downtime switching**: Pre-initialize new driver before releasing old one

## Dependencies

- ✅ HardwareManager with pin ownership tracking
- ✅ CANManager with callback registration/unregistration
- ✅ MotorDriverInterface common interface
- ✅ MotorDriverManager factory pattern
- ⚠️ Need to verify CANManager supports unregisterCallback()
- ⚠️ Need to verify all drivers properly track their resource usage

## Open Questions

1. **CAN callback unregistration**: Does CANManager support unregisterCallback()?
   - If not, need to add this capability first

2. **Serial port sharing**: KeyaSerialDriver uses Serial6 - is this exclusive or shared?
   - If shared, need coordination with SerialManager

3. **PWM timer conflicts**: Do we need to reset PWM configuration between drivers?
   - Probably yes for frequency changes

4. **VWAS without encoder**: Should VWAS be allowed without encoder feedback?
   - Current implementation requires encoder, but could work GPS/IMU only

## Success Criteria

- [ ] All motor type switches work without reboot
- [ ] No resource leaks after switching
- [ ] VWAS adapts correctly to encoder availability
- [ ] System refuses switch if unsafe (autosteer active)
- [ ] Clear logging shows what's happening
- [ ] Field testing confirms reliability
- [ ] Code passes review for safety and clarity

---

**Document Status**: Planning phase
**Created**: 2025-01-16
**Target Implementation**: TBD (after VWAS field testing complete)
**Priority**: Medium (nice-to-have, not critical)
