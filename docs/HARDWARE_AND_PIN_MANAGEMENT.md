# Hardware and Pin Management

This document describes the hardware abstraction layer and pin management system in AiO New Dawn.

## Overview

AiO New Dawn uses a sophisticated pin ownership and resource management system to prevent hardware conflicts and ensure reliable operation across different hardware configurations.

## Key Components

### HardwareManager

The `HardwareManager` class provides centralized pin definitions and hardware initialization:

- **Pin Definitions**: All hardware pins are defined in one place
- **Processor Detection**: Supports both Teensy 4.1 and Little Dawn processors
- **Hardware Initialization**: Consistent initialization sequence for all peripherals
- **Buzzer Control**: Built-in buzzer management for audio feedback

### PinOwnershipManager

Prevents pin conflicts by tracking ownership:

- **Compile-time Validation**: Detects conflicts during compilation
- **Runtime Validation**: Verifies pin availability before use
- **Clear Error Messages**: Identifies which modules are conflicting
- **Dynamic Ownership**: Supports runtime pin assignment changes

### SharedResourceManager

Manages shared hardware resources:

- **PWM Timers**: Allocates timers to prevent conflicts
- **ADC Channels**: Manages analog input assignments
- **I2C Bus**: Coordinates access to I2C devices
- **Thread Safety**: Mutex protection for concurrent access

## Pin Assignments

### Teensy 4.1 Pin Map

#### Motor Control Pins
- **PWM Motor**:
  - PWM Pin: 4
  - Direction Pin: 5
  - Enable Pin: 6 (optional)
  - Sleep Pin: 7 (optional)
  
- **Danfoss Valve**:
  - Uses PCA9685 outputs 7 & 8
  - Protects outputs 5 & 6 from MachineProcessor

- **Keya CAN Motor**:
  - Uses CAN3 interface
  - No GPIO pins required

#### Sensor Pins
- **Wheel Angle Sensor (WAS)**: A0 (pin 14)
- **Current Sensor**: A1 (pin 15)
- **Pressure Sensor**: A3 (pin 17)
- **Work Switch**: A2 (pin 16)
- **Remote Switch**: 36
- **Steer Switch**: 32

#### Encoder Pins
- **Encoder A**: 2
- **Encoder B**: 3 (quadrature mode only)

#### Communication Pins
- **GPS1 Serial**: Serial5 (RX5/TX5)
- **GPS2 Serial**: Serial8 (RX8/TX8)
- **Radio Serial**: Serial3
- **IMU Serial**: Serial4 (RX4/TX4)
- **RS232**: Serial7
- **ESP32**: Serial2
- **Keya Serial**: Serial1 (when not using CAN)

#### I2C Devices
- **I2C Bus**: SDA/SCL (pins 18/19)
- **PCA9685 #1**: Address 0x44 (Machine/Section control)
- **PCA9685 #2**: Address 0x70 (LED control)
- **BNO08x IMU**: Address 0x4A or 0x4B

#### Other Pins
- **Speed Pulse Output**: 9
- **Buzzer**: 36
- **Ethernet Status LED**: Built into magjack

### Little Dawn Processor

Little Dawn uses a different pin mapping optimized for agricultural equipment:

- Pin assignments are detected at runtime
- Compatible subset of Teensy 4.1 functionality
- Automatic configuration based on processor type

## Pin Configuration Guidelines

### Analog Pins
```cpp
// CORRECT - Use INPUT_DISABLE for analog pins
pinMode(WAS_PIN, INPUT_DISABLE);

// INCORRECT - Don't use INPUT for analog
pinMode(WAS_PIN, INPUT);  // This enables digital input buffer!
```

### Pin Ownership
```cpp
// Check ownership before using a pin
if (PinOwnershipManager::isPinAvailable(4)) {
    PinOwnershipManager::claimPin(4, "PWMMotor");
    pinMode(4, OUTPUT);
}
```

### Shared Resources
```cpp
// Request a PWM timer
int timer = SharedResourceManager::requestPWMTimer("SpeedPulse");
if (timer >= 0) {
    // Configure PWM on allocated timer
}
```

## Hardware Initialization Sequence

1. **Processor Detection**
   - Identify Teensy 4.1 or Little Dawn
   - Load appropriate pin mappings

2. **Pin Ownership Setup**
   - Register static pin assignments
   - Validate no conflicts exist

3. **I2C Initialization**
   - Start I2C bus at 1MHz
   - Scan for connected devices
   - Initialize PCA9685 controllers

4. **Serial Port Setup**
   - Configure baud rates
   - Enable required ports
   - Set up pin modes

5. **Motor Detection**
   - Check for Keya CAN presence
   - Read motor type from EEPROM
   - Initialize appropriate driver

6. **Sensor Initialization**
   - Configure analog inputs
   - Set up interrupt pins
   - Calibrate sensors

## Resource Conflict Resolution

### Common Conflicts

1. **PWM Timer Conflicts**
   - Multiple modules requesting same timer
   - Solution: Use SharedResourceManager

2. **Pin Double Assignment**
   - Two modules trying to use same pin
   - Solution: Check PinOwnershipManager first

3. **I2C Address Conflicts**
   - Multiple devices on same address
   - Solution: Use different addresses or multiplexer

### Debugging Tools

- **Serial Menu**: Press '?' for event logger controls
- **Loop Timing**: Press 'L' to toggle loop diagnostics
- **Event Statistics**: Press 'S' to show log statistics
- **Conflict Detection**: Automatic reporting of conflicts

## Best Practices

### Pin Management
1. Always check pin availability before use
2. Release pins when module is disabled
3. Use meaningful owner names for debugging
4. Document special pin requirements

### Resource Allocation
1. Request resources during initialization
2. Handle allocation failures gracefully
3. Release resources when done
4. Use minimum resources needed

### Hardware Abstraction
1. Use HardwareManager for pin access
2. Don't hardcode pin numbers
3. Support multiple hardware variants
4. Test on all supported platforms

## Motor Driver Pin Requirements

### PWM Motor Drivers
- **Cytron MD30C**: PWM + Direction
- **IBT-2**: PWM + Direction + Enable
- **DRV8701**: PWM + Direction + Enable + Sleep

### CAN Motor
- **Keya**: CAN3 interface only

### Hydraulic Valve
- **Danfoss**: PCA9685 outputs 7 & 8

## Sensor Interface Requirements

### Analog Sensors
- **WAS**: 0-5V analog input
- **Current**: Requires zero-offset calibration
- **Pressure**: Configurable threshold

### Digital Sensors
- **Encoder**: Interrupt-capable pins
- **Switches**: Internal pullup support

## I2C Device Management

### Device Addresses
- **0x44**: PCA9685 for sections
- **0x70**: PCA9685 for LEDs
- **0x4A/0x4B**: BNO08x IMU

### Bus Management
- 1MHz operation for reliability
- Automatic retry on errors
- Mutex protection for thread safety

## Troubleshooting

### Pin Conflicts
```
ERROR: Pin 4 already owned by PWMMotor, requested by SpeedPulse
```
Solution: Check module configuration, disable conflicting module

### Resource Exhaustion
```
ERROR: No PWM timers available
```
Solution: Share timers or reduce PWM usage

### I2C Communication
```
ERROR: I2C device not responding at 0x44
```
Solution: Check connections, verify pull-up resistors

## Future Considerations

- Dynamic pin remapping support
- Hot-plug hardware detection
- Extended processor support
- GPIO expander integration