# CANBUS Integration Proposal for AiO New Dawn

## Executive Summary

This proposal outlines the integration of CANBUS functionality into the AiO New Dawn system to support steer-ready tractors. The implementation will be based on the proven CANBUS firmware from AgOpenGPS, adapted to New Dawn's modular architecture.

## Current CANBUS Implementations Analysis

### Key Features from Existing Sketches

1. **Triple CAN Bus Architecture**
   - K_Bus: Tractor/Control Bus
   - ISO_Bus: ISOBUS communication
   - V_Bus: Steering Valve Bus

2. **Supported Tractor Brands**
   - Claas
   - Valtra/Massey Ferguson
   - Case IH/New Holland
   - Fendt (SCR, S4, Gen6, FendtOne)
   - JCB
   - Lindner
   - CAT MT

3. **Core Functionality**
   - Steering valve ready detection
   - Engage/disengage via CAN messages
   - PWM curve control through CAN
   - Hitch position monitoring
   - Brand-specific message protocols

## Proposed Architecture for New Dawn

### 1. New Library Structure
```
lib/aio_canbus/
├── CANBUSManager.h/cpp          # Main CANBUS coordination
├── CANBUSProcessor.h/cpp        # PGN message handler
├── SteerReadyInterface.h        # Abstract interface
├── brands/
│   ├── FendtCANHandler.h/cpp
│   ├── ClaasCANHandler.h/cpp
│   ├── ValtraCANHandler.h/cpp
│   └── ... (other brands)
└── CANBUSConfig.h               # Configuration constants
```

### 2. Integration Points

#### A. Hardware Integration
- Utilize Teensy 4.1's three CAN controllers (CAN1, CAN2, CAN3)
- Pin assignments managed through SharedResourceManager
- Conflict detection with existing motor drivers

#### B. PGN Integration
- Register for PGN 239 (Machine Data) for CAN control
- Send status via PGN 253 extension
- New PGN for CAN-specific configuration (proposed: PGN 238)

#### C. Motor Driver Coordination
```cpp
class CANSteerReadyDriver : public MotorDriverInterface {
    // Implements steering through CAN messages
    // Coordinates with brand-specific handlers
};
```

### 3. Key Classes

#### CANBUSManager (Singleton)
```cpp
class CANBUSManager {
public:
    static CANBUSManager* getInstance();

    bool init();
    void process();  // Called by SimpleScheduler at 50Hz

    bool isSteerReady() const;
    bool setSteerCurve(int8_t curve);
    bool engageAutoSteer(bool engage);

    void setBrand(TractorBrand brand);
    TractorBrand getCurrentBrand() const;

private:
    FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> K_Bus;
    FlexCAN_T4<CAN2, RX_SIZE_256, TX_SIZE_16> ISO_Bus;
    FlexCAN_T4<CAN3, RX_SIZE_256, TX_SIZE_16> V_Bus;

    std::unique_ptr<BrandHandler> brandHandler;
    bool steerReady = false;
};
```

#### BrandHandler Interface
```cpp
class BrandHandler {
public:
    virtual ~BrandHandler() = default;

    virtual void processKBusMessage(const CAN_message_t& msg) = 0;
    virtual void processISOMessage(const CAN_message_t& msg) = 0;
    virtual void processVBusMessage(const CAN_message_t& msg) = 0;

    virtual bool sendSteerCommand(int16_t steerAngle) = 0;
    virtual bool sendEngageCommand(bool engage) = 0;
    virtual bool isSteerReady() const = 0;
};
```

### 4. Configuration

#### Web Interface Updates
- Add CANBUS configuration page
- Tractor brand selection
- Module ID configuration
- CAN bus enable/disable
- Diagnostic message viewer

#### EEPROM Storage
```cpp
struct CANBUSSettings {
    bool enabled;
    uint8_t tractorBrand;
    uint8_t moduleID;
    bool useCANSteer;
    bool useCANWorkSwitch;
    bool useCANHitch;
};
```

### 5. Integration with Existing Systems

#### AutosteerProcessor
- Check CANBUSManager::isSteerReady() before engaging
- Route steering commands through CANSteerReadyDriver when CAN mode active
- Fallback to PWM/Danfoss when CAN unavailable

#### SimpleScheduler Integration
```cpp
// In main.cpp
scheduler.addTask(SimpleScheduler::HZ_50, taskCANBUS, "CANBUS");

void taskCANBUS() {
    TIME_PROCESS("CANBUS");
    if (canBusManager.isEnabled()) {
        canBusManager.process();
    }
}
```

### 6. Safety Considerations

1. **Mutual Exclusion**
   - Only one motor driver type active at a time
   - Automatic detection with manual override option

2. **Timeout Protection**
   - Lost CAN communication triggers autosteer disengage
   - Configurable timeout (default 200ms)

3. **State Validation**
   - Verify steer ready before allowing engage
   - Monitor CAN bus health

### 7. Implementation Phases

#### Phase 1: Core Infrastructure (Week 1-2)
- [ ] Create CANBUSManager and base classes
- [ ] Implement FlexCAN integration
- [ ] Add to SimpleScheduler
- [ ] Basic PGN message handling

#### Phase 2: Brand Implementation (Week 3-4)
- [ ] Implement Fendt handler (most common)
- [ ] Implement Valtra/Massey handler
- [ ] Add brand detection logic
- [ ] Test with simulator

#### Phase 3: Integration (Week 5-6)
- [ ] Integrate with AutosteerProcessor
- [ ] Add web configuration pages
- [ ] Implement diagnostics
- [ ] Field testing

#### Phase 4: Additional Brands (Week 7-8)
- [ ] Add remaining brand handlers
- [ ] Optimize message processing
- [ ] Documentation
- [ ] Release preparation

## Benefits

1. **Native Steer-Ready Support**: Direct integration with tractor steering systems
2. **Improved Safety**: Use manufacturer's built-in safety systems
3. **Better Performance**: Eliminate external motor/valve latency
4. **Simplified Installation**: No additional hydraulic valves needed
5. **Brand Flexibility**: Support multiple tractor manufacturers

## Risks and Mitigations

1. **Risk**: CAN bus conflicts with existing systems
   - **Mitigation**: Careful message filtering, read-only by default

2. **Risk**: Brand-specific quirks
   - **Mitigation**: Modular handler design, extensive testing

3. **Risk**: Timing conflicts with existing tasks
   - **Mitigation**: Dedicated 50Hz scheduler slot, efficient message processing

## Resource Requirements

- **Memory**: ~50KB additional flash, 8KB RAM
- **CPU**: <5% at 50Hz update rate
- **Pins**: 6 pins (2 per CAN controller)

## Conclusion

Integrating CANBUS support into New Dawn will significantly expand its compatibility with modern agricultural equipment while maintaining the system's modular architecture and performance standards. The proposed design leverages proven implementations while adapting them to New Dawn's superior framework.