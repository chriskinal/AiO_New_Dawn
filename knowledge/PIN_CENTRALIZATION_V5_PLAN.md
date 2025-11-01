# Pin Assignment Centralization Plan (v5.0 Only)

## Problem Statement

**Issue**: Pin assignments are scattered across multiple files. HardwareManager defines pins but modules ignore it and use their own hardcoded constants.

**Impact**:
- Have to hunt through multiple files to find/change pin assignments
- Pin ownership tracking exists but actual pin numbers are duplicated everywhere
- Architecture half-finished - infrastructure exists but isn't used consistently

**Scope**: This refactor is for **v5.0 New Dawn hardware ONLY**. We are NOT creating a multi-board variant system.

## Current State Analysis

### Pin Definitions Are Scattered

**HardwareManager.h** defines pins (lines 28-42):
```cpp
const uint8_t WAS_SENSOR_PIN = A15;
const uint8_t CURRENT_PIN = A13;
#define KICKOUT_A_PIN A12
#define WORK_PIN A17
#define SPEEDPULSE_PIN 33
#define SLEEP_PIN 4
#define PWM1_PIN 5
#define PWM2_PIN 6
#define STEER_PIN 2
#define KICKOUT_D_PIN 3
```

**But modules duplicate these values instead of using HardwareManager:**

- **ADProcessor.h** (lines 96-101): Defines AD_STEER_PIN, AD_WORK_PIN, AD_WAS_PIN, etc.
- **PWMProcessor.h** (lines 52-53): Defines SPEED_PULSE_PIN, SPEED_PULSE_LED_PIN
- **AutosteerProcessor.h** (line 24): Defines BUTTON_PIN
- **DanfossMotorDriver.h** (lines 16-17): Hardcodes PCA9685 outputs

**Only MotorDriverManager does it correctly** (lines 76-83):
```cpp
case MotorDriverType::DRV8701:
    return new PWMMotorDriver(
        MotorDriverType::DRV8701,
        hwMgr->getPWM1Pin(),       // ✅ Reads from HardwareManager
        hwMgr->getPWM2Pin(),
        hwMgr->getSleepPin(),
        hwMgr->getCurrentPin()
    );
```

## Proposed Solution

**Goal**: All modules read pins from HardwareManager. Single source of truth.

### Step 1: Ensure HardwareManager Has All Getter Methods

Verify HardwareManager has getters for every pin (already exists):
```cpp
uint8_t getWASPin() const;
uint8_t getCurrentPin() const;
uint8_t getKickoutAPin() const;
uint8_t getWorkPin() const;
uint8_t getSteerPin() const;
uint8_t getKickoutDPin() const;
uint8_t getSpeedPulsePin() const;
uint8_t getSpeedPulseLEDPin() const;
uint8_t getSleepPin() const;
uint8_t getPWM1Pin() const;
uint8_t getPWM2Pin() const;
uint8_t getBuzzerPin() const;
uint8_t getButtonPin() const;  // May need to add this one
```

### Step 2: Refactor Each Module

#### ADProcessor - Remove Hardcoded Pins

**BEFORE** (ADProcessor.h):
```cpp
class ADProcessor {
private:
    static constexpr uint8_t AD_STEER_PIN = 2;
    static constexpr uint8_t AD_WORK_PIN = A17;
    static constexpr uint8_t AD_WAS_PIN = A15;
    static constexpr uint8_t AD_KICKOUT_A_PIN = A12;
    static constexpr uint8_t AD_KICKOUT_D_PIN = 3;
    static constexpr uint8_t AD_CURRENT_PIN = A13;
};
```

**AFTER**:
```cpp
class ADProcessor {
private:
    HardwareManager* hwManager;

    // Cache pin values during init (no overhead in hot paths)
    uint8_t steerPin;
    uint8_t workPin;
    uint8_t wasPin;
    uint8_t kickoutAPin;
    uint8_t kickoutDPin;
    uint8_t currentPin;

public:
    bool init(HardwareManager* hwMgr) {
        hwManager = hwMgr;

        // Read pins from HardwareManager
        steerPin = hwManager->getSteerPin();
        workPin = hwManager->getWorkPin();
        wasPin = hwManager->getWASPin();
        kickoutAPin = hwManager->getKickoutAPin();
        kickoutDPin = hwManager->getKickoutDPin();
        currentPin = hwManager->getCurrentPin();

        // Rest of init...
    }
};
```

#### PWMProcessor - Remove Hardcoded Pins

**BEFORE** (PWMProcessor.h):
```cpp
class PWMProcessor {
private:
    static constexpr uint8_t SPEED_PULSE_PIN = 33;
    static constexpr uint8_t SPEED_PULSE_LED_PIN = 37;
};
```

**AFTER**:
```cpp
class PWMProcessor {
private:
    HardwareManager* hwManager;
    uint8_t speedPulsePin;
    uint8_t speedPulseLEDPin;

public:
    bool init(HardwareManager* hwMgr) {
        hwManager = hwMgr;
        speedPulsePin = hwManager->getSpeedPulsePin();
        speedPulseLEDPin = hwManager->getSpeedPulseLEDPin();
        // Rest of init...
    }
};
```

#### AutosteerProcessor - Remove Hardcoded PIN

**BEFORE** (AutosteerProcessor.h):
```cpp
class AutosteerProcessor {
private:
    static constexpr uint8_t BUTTON_PIN = 2;
};
```

**AFTER**:
```cpp
class AutosteerProcessor {
private:
    HardwareManager* hwManager;
    uint8_t buttonPin;

public:
    bool init() {
        extern HardwareManager* hardwareManagerPtr;
        hwManager = hardwareManagerPtr;
        buttonPin = hwManager->getButtonPin();
        // Rest of init...
    }
};
```

#### DanfossMotorDriver - Document PCA9685 Outputs

**BEFORE** (DanfossMotorDriver.h):
```cpp
static constexpr uint8_t ENABLE_OUTPUT = 5;   // PCA9685 output
static constexpr uint8_t CONTROL_OUTPUT = 6;  // PCA9685 output
```

**KEEP AS IS**: These are PCA9685 output channel numbers (not Teensy pins), so they're appropriate as class constants. Just add comment clarifying they're NOT Teensy pins:

```cpp
// PCA9685 output channel numbers (not Teensy GPIO pins)
static constexpr uint8_t ENABLE_OUTPUT = 5;   // PCA9685 channel 5
static constexpr uint8_t CONTROL_OUTPUT = 6;  // PCA9685 channel 6
```

### Step 3: Add Missing Getter (if needed)

Check if `getButtonPin()` exists in HardwareManager. If not, add it:

```cpp
// HardwareManager.h
uint8_t getButtonPin() const { return STEER_PIN; }  // Button shares STEER_PIN
```

Or define separate BUTTON_PIN in HardwareManager if button should be configurable independently.

## Implementation Plan

### Phase 1: Update ADProcessor (1.5 hours)
1. Remove hardcoded pin constants from ADProcessor.h
2. Add hwManager pointer and cached pin members
3. Update init() to read pins from HardwareManager
4. Update all references throughout ADProcessor.cpp to use cached pins
5. Test thoroughly - AD functionality critical

### Phase 2: Update PWMProcessor (1 hour)
1. Remove hardcoded pin constants from PWMProcessor.h
2. Add hwManager pointer and cached pin members
3. Update init() to read pins from HardwareManager
4. Update all references in PWMProcessor.cpp
5. Test speed pulse output

### Phase 3: Update AutosteerProcessor (1 hour)
1. Remove hardcoded BUTTON_PIN constant
2. Add hwManager pointer and cached buttonPin member
3. Update init() to read from HardwareManager
4. Update button handling code
5. Test button functionality

### Phase 4: Document & Verify (0.5 hours)
1. Update comments in DanfossMotorDriver (clarify PCA9685 channels)
2. Verify MotorDriverManager still works correctly
3. Grep for any remaining hardcoded pin patterns
4. Update CLAUDE.md to document centralized pin architecture

**Total Estimated Time**: 4 hours

## Benefits

1. **Single source of truth** - All pins defined in HardwareManager
2. **Easier maintenance** - Change pins in ONE place
3. **Clear architecture** - Consistent pattern across all modules
4. **No runtime overhead** - Pins cached during init()
5. **Better documentation** - Clear where to find pin assignments

## Risks & Mitigation

### Risk: Breaking existing functionality
**Mitigation**:
- Refactor one module at a time
- Test thoroughly after each module
- Keep pin VALUES identical (only changing HOW they're accessed)

### Risk: Initialization order dependencies
**Mitigation**:
- HardwareManager must init first (already does)
- Modules cache pins in their init() methods
- No change to initialization sequence

### Risk: Forgetting a reference
**Mitigation**:
- Grep for old constant names after refactor
- Compiler will catch undefined constants
- Extensive testing

## Success Criteria

- [ ] All pin assignments defined ONLY in HardwareManager
- [ ] ADProcessor uses hwManager->getXXXPin() methods
- [ ] PWMProcessor uses hwManager->getXXXPin() methods
- [ ] AutosteerProcessor uses hwManager->getButtonPin()
- [ ] No `static constexpr` pin definitions in module headers (except PCA9685 channels)
- [ ] All functionality works identically to before refactor
- [ ] Code grep shows no remaining hardcoded pin duplicates

## Testing Checklist

After refactor, verify:
- [ ] WAS sensor reads correctly
- [ ] Current sensor reads correctly
- [ ] Pressure sensor reads correctly
- [ ] Work switch reads correctly
- [ ] Steer button/switch works
- [ ] Speed pulse output works
- [ ] Motor PWM outputs work
- [ ] Buzzer works
- [ ] Encoder inputs work (if enabled)
- [ ] Danfoss valve works (if used)

## Comparison to ConfigManager

This is EXACTLY what we did with ConfigManager:
- ✅ ConfigManager: Centralized all settings, modules read from it (~95% compliance)
- ⚠️ HardwareManager: Centralized pin definitions but modules ignore it (~40% compliance)

This refactor brings HardwareManager to the same high compliance level as ConfigManager.

## Related Issues

- GitHub Issue #78: Motivation - wanted to adapt to v4.5 boards, discovered pins scattered
- MOTOR_HOT_SWAP_PLAN.md: Pin ownership transfer during motor switching
- HARDWARE_OWNERSHIP_MATRIX.md: Current pin ownership tracking

## Next Steps

1. ✅ Get confirmation v5.0-only approach is correct (no board variants)
2. Create feature branch: `feature/pin-centralization-v5`
3. Implement Phase 1 (ADProcessor refactor)
4. Test thoroughly
5. Implement Phase 2 (PWMProcessor refactor)
6. Test thoroughly
7. Implement Phase 3 (AutosteerProcessor refactor)
8. Final testing & documentation

---

**Document Status**: Planning
**Created**: 2025-01-16
**Scope**: v5.0 New Dawn hardware only
**Priority**: Medium (architectural cleanup, not critical bug)
