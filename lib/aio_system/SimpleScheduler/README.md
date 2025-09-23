# SimpleScheduler

A lightweight, zero-allocation task scheduler for the AiO New Dawn agricultural control system.

## Features

- **Zero Dynamic Allocation**: All memory statically allocated at compile time
- **Frequency Groups**: Organized by common frequencies (100Hz, 50Hz, 10Hz, etc.)
- **Minimal Overhead**: Optimized for maintaining 320kHz+ main loop frequency
- **Simple API**: Easy to integrate with existing code
- **Optional Statistics**: Compile-time flag for performance metrics

## Usage

```cpp
#include "SimpleScheduler.h"

// Use global instance or create your own
extern SimpleScheduler scheduler;

void setup() {
    // Add tasks to frequency groups
    scheduler.addTask(SimpleScheduler::EVERY_LOOP,
                     []{ readSensors(); },
                     "Sensors");

    scheduler.addTask(SimpleScheduler::HZ_100,
                     []{ autosteerProcessor.process(); },
                     "Autosteer");

    scheduler.addTask(SimpleScheduler::HZ_10,
                     []{ updateLEDs(); },
                     "LEDs");
}

void loop() {
    scheduler.run();  // Execute all due tasks
}
```

## Frequency Groups

| Group | Frequency | Interval | Typical Use |
|-------|-----------|----------|-------------|
| EVERY_LOOP | Every loop | 0ms | Sensor reading, serial processing |
| HZ_100 | 100Hz | 10ms | Critical control loops |
| HZ_50 | 50Hz | 20ms | Motor control |
| HZ_10 | 10Hz | 100ms | UI updates, telemetry |
| HZ_5 | 5Hz | 200ms | Status updates |
| HZ_1 | 1Hz | 1000ms | Slow monitoring |
| HZ_0_2 | 0.2Hz | 5000ms | Very slow tasks |

## Configuration

### Enable Timing Statistics

```cpp
// In your build flags or before including SimpleScheduler.h
#define SCHEDULER_TIMING_STATS

// Then use:
scheduler.printStats();
scheduler.resetStats();
```

### Runtime Control

```cpp
// Enable/disable tasks
scheduler.enableTask(SimpleScheduler::HZ_10, "LEDs");
scheduler.disableTask(SimpleScheduler::HZ_100, "Telemetry");

// Enable/disable entire groups
scheduler.disableGroup(SimpleScheduler::HZ_0_2);

// Adjust group frequency
scheduler.setGroupInterval(SimpleScheduler::HZ_10, 50);  // Change 10Hz to 20Hz
```

## Performance

- Scheduling overhead: < 0.5μs per group
- Memory usage: ~700 bytes total
- No heap allocation
- Direct function calls (no virtual functions)

## Limitations

- Maximum 8 tasks per frequency group (configurable)
- Maximum 7 frequency groups (configurable)
- Task names must be string literals (not copied)