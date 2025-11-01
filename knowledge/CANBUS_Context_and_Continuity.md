# CANBUS Implementation Context and Continuity Guide

## Project Overview
Integrating CANBUS support into AiO New Dawn to enable steer-ready tractor control. This document ensures continuity across conversation compaction events.

## Current State
- **Project**: AiO New Dawn (Teensy 4.1 agricultural control system)
- **Goal**: Add native CANBUS support for modern tractors
- **Reference**: AgOpenGPS CANBUS firmware (analyzed from GitHub)
- **Performance Baseline**: 541kHz loop frequency (must maintain)

## Architecture Summary

### Three CAN Buses
1. **K_Bus** (CAN1): Tractor/Control Bus - Pins 22/23
2. **ISO_Bus** (CAN2): ISOBUS communication - Pins 0/1
3. **V_Bus** (CAN3): Steering Valve Bus - Pins 30/31

### Supported Brands
1. Fendt (SCR, S4, Gen6, FendtOne) - Primary target
2. Valtra/Massey Ferguson
3. Claas
4. Case IH/New Holland
5. JCB, Lindner, CAT MT (future)

### Key Design Decisions
1. **CANBUSManager**: Singleton pattern, manages all three buses
2. **BrandHandler**: Interface pattern for brand-specific logic
3. **CANSteerReadyDriver**: Implements MotorDriverInterface
4. **SimpleScheduler**: 50Hz task for CAN processing
5. **Zero dynamic allocation**: Follow New Dawn patterns

## Implementation Checkpoints

### Milestone 1.0: Foundation ✅
**File Structure**:
```
lib/aio_canbus/
├── CANBUSConfig.h       # Constants, IDs, timing
├── CANBUSManager.h/cpp   # Main coordinator
└── CANBUSProcessor.h/cpp # PGN integration
```

**Key Classes Started**:
```cpp
class CANBUSManager {
    static CANBUSManager* getInstance();
    bool init();
    void process();  // 50Hz via SimpleScheduler
    bool isSteerReady() const;
private:
    FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> K_Bus;
    FlexCAN_T4<CAN2, RX_SIZE_256, TX_SIZE_16> ISO_Bus;
    FlexCAN_T4<CAN3, RX_SIZE_256, TX_SIZE_16> V_Bus;
};
```

**Test Requirements**:
- Loopback test on each CAN bus
- External PCAN-USB communication verified
- Loop frequency maintained >500kHz

### Milestone 2.0: Message Handling ⬜
**Add Files**:
```
lib/aio_canbus/
├── BrandHandlerInterface.h  # Abstract base
├── GenericBrandHandler.h/cpp # Basic implementation
└── CANDiagnostics.h/cpp     # Web endpoints
```

**Key Messages to Handle**:
- Steer Ready: ID 0x18EF1C00 (K_Bus)
- Generic status polling
- Timeout detection (200ms)

**Web Endpoint**: `/api/canbus/status` returns:
```json
{
    "enabled": true,
    "steerReady": false,
    "brand": "unknown",
    "messageCount": {
        "k_bus": 1234,
        "iso_bus": 567,
        "v_bus": 890
    }
}
```

### Milestone 3.0: Fendt Complete ⬜
**Add Files**:
```
lib/aio_canbus/brands/
└── FendtBrandHandler.h/cpp
lib/aio_canbus/
└── CANSteerReadyDriver.h/cpp
```

**Fendt Message IDs**:
- Steer Ready: 0x0CF02300 (V_Bus) - Data[1] = 0x16 means ready
- Steer Command: 0x0CF02220 (V_Bus) - Data[1] = curve value
- Engage: Via K_Bus message sequence

**Integration Points**:
1. `MotorDriverManager::detectMotorType()` - Add CAN detection
2. `AutosteerProcessor::setSteerAngle()` - Route through CAN driver
3. Web config page - Add brand selection dropdown

### Milestone 4.0: Multi-Brand ⬜
**Add Files**:
```
lib/aio_canbus/brands/
├── ValtraBrandHandler.h/cpp
├── ClaasBrandHandler.h/cpp
└── CaseIHBrandHandler.h/cpp
```

**Brand Detection Logic**:
- Monitor all buses for 1 second
- Score confidence based on message IDs seen
- Auto-select highest confidence brand
- Allow manual override via web

### Milestone 5.0: Advanced Features ⬜
**Additional Functionality**:
- Hitch position monitoring/control
- Work switch via CAN
- Section control integration
- Enhanced safety timeouts

## Critical Implementation Notes

### Memory Management
- NO dynamic allocation (no new/malloc)
- Fixed-size message buffers
- Reuse existing New Dawn patterns

### Timing Constraints
- CAN processing must complete in <100μs
- 50Hz update rate (20ms intervals)
- Steering commands within 10ms of PGN receipt

### Pin Management
```cpp
// In CANBUSManager::init()
if (!pinOwnership.requestPin(22, PinOwner::CANBUS, "CAN1_TX")) return false;
if (!pinOwnership.requestPin(23, PinOwner::CANBUS, "CAN1_RX")) return false;
// ... repeat for all 6 pins
```

### State Machine
```
States: DISABLED → DETECTING → READY → ENGAGED
        ↑_______________________|

Transitions:
- DISABLED→DETECTING: When CAN enabled
- DETECTING→READY: When brand detected & steer ready
- READY→ENGAGED: When autosteer activated
- ANY→DISABLED: On timeout or disable command
```

## Test Message Reference

### Fendt S4 Simulation
```
# PCAN-View script for Fendt S4
# V_Bus messages
10ms: 0CF02300h 8 00h 16h 00h 00h 00h 00h 00h 00h  # Steer ready

# K_Bus messages
100ms: 18EF1C00h 8 [status bytes vary]              # General status
```

### Valtra Simulation
```
# K_Bus messages
10ms: 18EF52A0h 8 00h 10h 00h 00h 00h 00h 00h 00h  # Steer ready
```

## Performance Tracking
Must maintain throughout implementation:
- Loop frequency: >500kHz (current: 541kHz)
- CAN task: <100μs per iteration
- No message drops at 1000 msgs/sec/bus
- Memory usage: <8KB additional RAM

## Common Issues and Solutions

1. **FlexCAN_T4 Library**: Already in platformio.ini
2. **Pin Conflicts**: Check HARDWARE_OWNERSHIP_MATRIX.md
3. **Message Buffer Overflow**: Increase RX_SIZE if needed
4. **Brand Not Detected**: Check message ID filters

## Next Steps After Compaction

1. Check which milestone was last completed
2. Review test results from that milestone
3. Look for any TODO comments in code
4. Continue with next milestone's tasks
5. Always run milestone tests before proceeding

## Key Files to Check
- `/docs/CANBUS_Implementation_Plan.md` - Detailed milestone plan
- `/docs/CANBUS_Integration_Proposal.md` - Original design
- `/lib/aio_canbus/*` - Implementation files
- `/src/main.cpp` - SimpleScheduler integration
- `platformio.ini` - FlexCAN_T4 dependency

## Questions to Resolve
1. Should CAN be mutually exclusive with other motor drivers?
2. How to handle partial CAN implementations (steer only vs full)?
3. Brand-specific quirks documentation needed?

This context should provide everything needed to continue implementation after any conversation break.