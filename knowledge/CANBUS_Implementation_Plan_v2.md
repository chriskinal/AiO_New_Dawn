# CANBUS Implementation Plan v2 - Unified Approach

## Overview
Based on lessons learned from Milestone 1.0, we're pivoting to a simpler, unified approach that treats all CAN-based systems (including Keya) the same way through web configuration.

## Key Design Change
**Single TractorCANDriver** configurable via web UI instead of:
- Complex auto-detection framework
- Multiple brand-specific drivers
- CANBUSManager/BrandHandler architecture

## Architecture

### Simple Design:
```
MotorDriverManager
    ↓
TractorCANDriver (single driver for all CAN steering)
    ↓ (configured via web)
CAN1/CAN2/CAN3 (global instances)
```

### Configuration Options:
```
Brand: [Keya / Fendt / Valtra / Claas / Case IH / JCB / etc.]

Bus Assignments:
- Steering Commands: [None / K_Bus / ISO_Bus / V_Bus]
- Work Switch:       [None / K_Bus / ISO_Bus / V_Bus]
- Hitch Control:     [None / K_Bus / ISO_Bus / V_Bus]
```

**Note**: "None" option allows button-only implementations where steering isn't decoded yet.

## Implementation Plan

### Phase 1: Rollback and Prepare
1. Roll back to commit before CANBUS changes
2. Study existing KeyaCANDriver pattern
3. Design unified configuration structure

### Phase 2: Create TractorCANDriver
1. Copy KeyaCANDriver as starting template
2. Add brand enumeration
3. Add bus assignment configuration
4. Implement brand-specific message handlers

### Phase 3: Web Configuration
1. Create CAN configuration page
2. Add to device settings menu
3. Save configuration to EEPROM
4. Test configuration changes

### Phase 4: Brand Implementation
1. Start with Keya (already working)
2. Add Fendt support
3. Add Valtra support
4. Test each thoroughly

## Configuration Structure
```cpp
struct CANSteerConfig {
    uint8_t brand;           // TractorBrand enum
    uint8_t steerBus;        // 0=None, 1=K_Bus, 2=ISO_Bus, 3=V_Bus
    uint8_t buttonBus;       // 0=None, 1=K_Bus, 2=ISO_Bus, 3=V_Bus
    uint8_t hitchBus;        // 0=None, 1=K_Bus, 2=ISO_Bus, 3=V_Bus
    uint8_t moduleID;        // For brands that need module ID
};
```

## Benefits of New Approach
1. **Simplicity**: One driver, configured via web
2. **Flexibility**: Users can map buses as needed
3. **Maintainability**: All CAN code in one place
4. **User-friendly**: No code changes needed
5. **Incremental**: Can add steering support later for button-only brands

## Rollback Plan
1. Commit current state to preserve learning
2. Create new branch from main
3. Implement unified approach
4. Delete complex framework code

## Success Criteria
- Single driver handles all CAN steering brands
- Web configuration works reliably
- Existing Keya functionality preserved
- Easy to add new brands
- Clean, maintainable code