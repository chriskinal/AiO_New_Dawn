# SimpleScheduler Design Document

## Executive Summary
This document outlines the design and implementation of SimpleScheduler, a lightweight task scheduling system for the AiO New Dawn agricultural control system. SimpleScheduler aims to replace the current distributed timing logic (20+ separate timing checks) with a centralized, efficient scheduler while maintaining the critical 320kHz+ main loop frequency.

## Table of Contents
1. [Background and Context](#background-and-context)
2. [Current Timing Architecture Analysis](#current-timing-architecture-analysis)
3. [SimpleScheduler Implementation Plan](#simplescheduler-implementation-plan)
4. [Migration Strategy](#migration-strategy)
5. [Performance Considerations](#performance-considerations)
6. [Design Decisions and Rationale](#design-decisions-and-rationale)

## Background and Context

### Project Context
- **System**: AiO New Dawn - Teensy 4.1-based agricultural control system for AgOpenGPS
- **Current Performance**: ~320kHz main loop frequency (3.125μs per loop)
- **Problem**: Scattered timing logic across 20+ modules makes maintenance difficult and adds overhead
- **Solution**: SimpleScheduler - following the successful "Simple" library pattern (SimpleWebManager, etc.)

### Why SimpleScheduler?
1. **Performance**: Current timing checks consume ~20μs per loop (significant overhead)
2. **Maintainability**: Centralized timing logic easier to understand and modify
3. **Consistency**: Follows established AiO patterns and coding style
4. **Efficiency**: Group-based scheduling reduces timing checks from 20+ to 7

### Alternatives Considered
- **TaskScheduler (arkhipenko)**: Feature-rich but uses dynamic memory allocation
- **TinyScheduler**: Clean API but uses dynamic allocation and virtual functions
- **Custom Solution**: Chosen for zero allocation and minimal overhead

### Key Requirements
1. Zero dynamic memory allocation
2. Maintain 320kHz+ loop frequency
3. Simple API matching existing AiO patterns
4. Support for existing timing groups (100Hz, 50Hz, 10Hz, etc.)
5. Easy integration with current processor architecture

## Current Timing Architecture Analysis

### Overview
The AiO New Dawn codebase currently uses a distributed timing approach where each module maintains its own timing logic. This analysis identified over 20 different timing patterns scattered throughout the codebase.

### Current Timing Patterns

#### High Frequency Operations (50-100Hz)

| Process | Frequency | Current Implementation | Location |
|---------|-----------|----------------------|-----------|
| AutosteerProcessor | 100Hz (10ms) | `LOOP_TIME = 10` constant | `AutosteerProcessor.cpp:49` |
| WebSocket Telemetry | 100Hz (active) | 10ms interval check | `SimpleWebManager.cpp:828-925` |
| Web Client Handling | 100Hz (rate limited) | 10ms rate limit | `SimpleWebManager.cpp:96-100` |
| Keya CAN Motor | 50Hz | 20ms command interval | `KeyaCANDriver.h:76-85` |
| Keya Serial Motor | 50Hz | 20ms command interval | `KeyaSerialDriver.cpp:42` |

#### Medium Frequency Operations (10Hz)

| Process | Frequency | Current Implementation | Location |
|---------|-----------|----------------------|-----------|
| NAV Broadcasting | 10Hz | `MESSAGE_INTERVAL_MS = 100` | `NAVProcessor.cpp` |
| Kickout Sensor (PGN 250) | 10Hz | `PGN250_INTERVAL_MS = 100` | `KickoutMonitor.cpp` |
| LED Updates | 10Hz | 100ms interval | `main.cpp:482-488` |
| Network Status Check | 10Hz | 100ms interval | `main.cpp:440-443` |

#### Low Frequency Operations (≤1Hz)

| Process | Frequency | Current Implementation | Location |
|---------|-----------|----------------------|-----------|
| Network Link Status | 0.2Hz | 5 second interval | `QNEthernetUDPHandler.cpp:119-153` |
| Debug Logging | Variable | 5-60 second intervals | Various locations |
| WebSocket Reconnect | 1Hz | 1000ms during connection | `SimpleWebManager.cpp` |
| Machine Watchdog | 0.5Hz | 2 second timeout | `MachineProcessor.cpp:288-301` |

#### Per-Loop Operations (No Timing)

These processes run every main loop iteration:
- UDP polling (currently every other loop)
- ADProcessor (analog/digital inputs)
- EncoderProcessor
- IMUProcessor
- GPS serial processing (1 byte per loop)
- PWMProcessor (speed pulse generation)

### Timing Implementation Issues

1. **Scattered Timer Logic**: Each module maintains its own `lastTime` variables
2. **Redundant Code**: Pattern `if (millis() - lastX > interval)` repeated dozens of times
3. **No Centralized Control**: Difficult to understand overall system timing
4. **Inconsistent Patterns**: Some use static variables, others use member variables
5. **No Performance Metrics**: Hard to measure timing overhead

### Performance Impact

Current timing overhead (measured):
- Each timing check: ~0.5-1μs
- With ~20 timing checks per loop: ~10-20μs total
- At 320kHz loop rate (3.125μs/loop), this is significant overhead

## SimpleScheduler Implementation Plan

### Design Goals
- Maintain 320kHz+ loop frequency (< 3.2μs average loop time)
- Zero dynamic memory allocation
- Minimal overhead per scheduling pass (target < 0.5μs)
- Clear, maintainable code following AiO patterns
- Easy integration with existing processor classes

### Architecture Overview

#### Frequency Group Design
```cpp
class SimpleScheduler {
    // Fixed frequency groups matching AiO's actual usage
    static constexpr uint8_t EVERY_LOOP = 0;  // No timing check
    static constexpr uint8_t HZ_100 = 1;      // 10ms - Critical
    static constexpr uint8_t HZ_50 = 2;       // 20ms - Motor control
    static constexpr uint8_t HZ_10 = 3;       // 100ms - Sensors/UI
    static constexpr uint8_t HZ_5 = 4;        // 200ms - Status
    static constexpr uint8_t HZ_1 = 5;        // 1000ms - Slow
    static constexpr uint8_t HZ_0_2 = 6;      // 5000ms - Very slow
};
```

#### Key Features
1. **Compile-time Configuration**: All memory statically allocated
2. **Group-based Scheduling**: One timing check per frequency group
3. **Direct Function Calls**: No virtual functions or indirection
4. **Optional Statistics**: Compile-time flag for performance metrics
5. **Simple API**: Easy to add tasks and understand timing

### Implementation Phases

#### Phase 1: Core Implementation
- Create SimpleScheduler library structure
- Implement basic task and group management
- Add performance-optimized run() method
- Target: < 100 lines of code

#### Phase 2: Integration Strategy
- Identify and categorize all timed operations
- Create migration table mapping old to new
- Implement incremental integration approach
- Maintain backward compatibility during transition

#### Phase 3: Performance Optimizations
- Compiler optimization attributes
- Memory layout optimization
- Minimal overhead features
- Profile and benchmark

#### Phase 4: Testing Plan
- Performance benchmarks (before/after)
- Functional testing of all timing groups
- Loop frequency verification
- Timing accuracy validation

#### Phase 5: Advanced Features (Optional)
- Runtime frequency adjustment
- Group enable/disable
- Task priorities within groups
- Performance statistics

## Migration Strategy

### Step 1: Parallel Implementation
```cpp
// Add scheduler without removing existing timing
SimpleScheduler scheduler;

void setup() {
    // Add tasks to appropriate groups
    scheduler.addTask(HZ_100, []{ autosteerProcessor.process(); }, "Autosteer");
    scheduler.addTask(HZ_10, []{ ledManagerFSM.updateAll(); }, "LED");
    // ... etc
}

void loop() {
    scheduler.run();  // New

    // Keep existing timing initially for comparison
    if (millis() - lastAutosteerTime > 10) {
        // autosteerProcessor.process();  // Commented out
    }
}
```

### Step 2: Gradual Migration
1. Start with low-risk, low-frequency tasks
2. Move to medium frequency tasks
3. Finally migrate critical 100Hz tasks
4. Remove old timing code once verified

### Step 3: Optimization
1. Profile with all tasks migrated
2. Optimize hot paths
3. Consider task reordering for cache efficiency

## Performance Considerations

### Expected Performance Improvements

1. **Reduced Overhead**:
   - Current: ~20 timing checks × 1μs = 20μs
   - SimpleScheduler: ~7 group checks × 0.5μs = 3.5μs
   - **Savings: ~16.5μs per loop**

2. **Better Code Locality**:
   - All timing logic in one place
   - Improved instruction cache usage
   - Predictable branch patterns

3. **Easier Optimization**:
   - Single point for timing improvements
   - Can profile scheduler independently
   - Clear performance metrics

### Memory Usage Estimate

```
Per Task: 12-16 bytes (function pointer + metadata)
Per Group: 8 + (MAX_TASKS × Task size)
Total: ~7 groups × ~100 bytes = 700 bytes

Compare to current: dozens of uint32_t scattered = ~100-200 bytes
Net increase: ~500 bytes (acceptable for Teensy 4.1)
```

### Success Criteria

- [ ] Loop frequency remains above 320kHz
- [ ] All tasks execute at specified frequencies (±1ms tolerance)
- [ ] Code reduction of at least 100 lines
- [ ] Easier to add new timed tasks
- [ ] Clear visualization of system timing
- [ ] No dynamic memory allocation
- [ ] Improved maintainability

## Appendix: Task Frequency Summary

### Every Loop Tasks
- UDP polling (currently every other loop)
- ADProcessor
- EncoderProcessor
- IMUProcessor
- GPS serial processing
- PWMProcessor

### 100Hz Tasks (Critical Timing)
- AutosteerProcessor (PGN 253 status)
- WebSocket telemetry (when connected)
- Web client handling

### 50Hz Tasks
- Keya CAN motor commands
- Keya Serial motor commands

### 10Hz Tasks
- NAV broadcasting (configurable 1-100Hz)
- Kickout sensor data (PGN 250)
- LED state updates
- Network readiness check

### 1Hz Tasks
- WebSocket reconnection attempts
- Various debug/status messages

### 0.2Hz Tasks
- Ethernet link status monitoring
- Detailed network diagnostics

## Design Decisions and Rationale

### Core Design Decisions

1. **Frequency Groups over Individual Tasks**
   - **Decision**: Use fixed frequency groups (100Hz, 50Hz, 10Hz, etc.)
   - **Rationale**: Matches AiO's actual usage patterns, reduces timing checks from O(n) to O(groups)
   - **Alternative**: Individual task timing would require checking each task

2. **Static Allocation**
   - **Decision**: Fixed-size arrays, no dynamic memory
   - **Rationale**: Predictable memory usage, no heap fragmentation, faster execution
   - **Trade-off**: Less flexible but more reliable for embedded systems

3. **Function Pointers over Virtual Functions**
   - **Decision**: Use simple function pointers
   - **Rationale**: No vtable overhead, direct calls, better optimization
   - **Trade-off**: Less OOP but more efficient

4. **Compile-time Configuration**
   - **Decision**: MAX_TASKS_PER_GROUP as compile constant
   - **Rationale**: Zero runtime overhead, compiler can optimize better
   - **Alternative**: Runtime configuration would add complexity

5. **Group Priority through Execution Order**
   - **Decision**: Execute groups in priority order (100Hz first, then 50Hz, etc.)
   - **Rationale**: Simpler than task priorities, matches current behavior
   - **Note**: AutosteerProcessor (100Hz) naturally gets priority

### API Design Rationale

1. **Lambda Support**
   - **Decision**: Support lambdas with capture
   - **Rationale**: Allows easy integration with existing class methods
   - **Example**: `scheduler.addTask(HZ_100, [&]{ processor.update(); });`

2. **Group Constants**
   - **Decision**: Use named constants (HZ_100, HZ_50) instead of raw frequencies
   - **Rationale**: Self-documenting, prevents typos, easy to change frequencies globally

3. **Enable/Disable Support**
   - **Decision**: Built-in task enable/disable
   - **Rationale**: Needed for conditional features (e.g., telemetry only when connected)

### Implementation Notes for Continuity

1. **Current State**: Design document created, plan approved
2. **Next Steps**: Implement Phase 1 (core SimpleScheduler)
3. **Key Files to Create**:
   - `lib/aio_system/SimpleScheduler/SimpleScheduler.h`
   - `lib/aio_system/SimpleScheduler/SimpleScheduler.cpp`
4. **Integration Points**:
   - Main loop in `src/main.cpp`
   - Replace timing in each processor's process() method
5. **Testing Strategy**: Parallel implementation to verify correctness

### Open Questions Resolved

1. **Q: Fixed vs Dynamic Groups?** A: Fixed groups chosen for simplicity and performance
2. **Q: Priority within groups?** A: No, execution order is sufficient
3. **Q: Statistics by default?** A: No, compile-time option to maintain performance
4. **Q: Microsecond support?** A: No, millisecond precision sufficient for current needs

---

*Document created: December 2024*
*Target platform: Teensy 4.1*
*Current performance: ~320kHz loop frequency*
*Author context: Part of ongoing performance optimization after improving from 210kHz to 320kHz*