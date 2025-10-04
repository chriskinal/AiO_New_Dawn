# CAN Bus Functions by Tractor Brand

This document outlines the CAN bus functions available for each supported tractor brand in the AiO New Dawn system.

## Overview

The system supports flexible CAN bus configuration where each of the three CAN buses can be assigned different functions:
- **CAN1 (K_Bus)** - Typically used for tractor control bus
- **CAN2 (ISO_Bus)** - ISOBUS for implement control
- **CAN3 (V_Bus)** - Valve/steering commands

## Supported Brands and Functions

### 1. Generic (Brand ID: 9) - Default
- **Purpose**: Use when mixing functions from different brands
- **CAN Functions**: Fully configurable per bus
- **Typical Use**: Custom setups, mixed-brand configurations

### 2. Fendt SCR/S4/Gen6 (Brand ID: 1)
- **V_Bus Functions**:
  - Steering angle control (complex curve calculation)
  - Steer engage/disengage
- **K_Bus Functions**:
  - Armrest button controls
  - Engage messages
- **ISO_Bus Functions**:
  - Additional engage messages
- **Special Features**: Unique steering curve algorithm

### 3. Valtra/Massey Ferguson (Brand ID: 2)
- **V_Bus Functions**:
  - Steering wheel angle (0x0CAC1C13)
  - Multiple engage message formats
  - Work switch messages
- **Supported Sub-brands**: McCormick, Massey Ferguson
- **Special Features**: Multiple engage mechanisms for compatibility

### 4. Case IH/New Holland (Brand ID: 3)
- **V_Bus Functions**:
  - Steering wheel angle (0x0CACAA08)
  - Engage messages (0x18FFBB03)
- **K_Bus Functions**:
  - Rear hitch position information
  - Hitch control commands
- **Special Features**: Integrated hitch control

### 5. Fendt One (Brand ID: 4)
- **V_Bus Functions**: Similar to Fendt SCR but with updated protocol
- **K_Bus Functions**: Enhanced button control
- **ISO_Bus Functions**: Advanced implement control
- **Special Features**: Next-generation Fendt protocol

### 6. Claas (Brand ID: 5)
- **V_Bus Functions**:
  - Steering wheel angle (0x0CAC1E13)
  - Engage control (0x18EF1CD2)
  - Work switch messages
- **Special Features**: Multiple engage message formats

### 7. JCB (Brand ID: 6)
- **V_Bus Functions**:
  - Steering wheel angle (0x0CACAB13)
  - Multiple engage message types
- **Special Features**: Flexible engage mechanisms

### 8. Lindner (Brand ID: 7)
- **V_Bus Functions**:
  - Steering wheel angle (0x0CACF013)
  - Basic engage control
- **Special Features**: Simple implementation

### 9. CAT MT Series (Brand ID: 8)
- **V_Bus Functions**:
  - Unique curve calculation
  - Status messages
  - Gear detection
- **Special Features**: Reverse tracking support

### 0. Disabled (Brand ID: 0)
- No CAN functions active
- Used when CAN steering is not needed

## CAN Function Types

### Keya (Function ID: 1)
- **Protocol**: Keya motor control protocol
- **Messages**:
  - Command: 0x06000001 (extended)
  - Heartbeat: 0x07000001 (extended)
- **Features**: RPM control, position feedback, current monitoring
- **Speed**: 250 kbps or 500 kbps

### V_Bus (Function ID: 2)
- **Protocol**: Valve/steering commands
- **Purpose**: Primary steering control for most brands
- **Speed**: Typically 250 kbps

### ISO_Bus (Function ID: 3)
- **Protocol**: ISOBUS implement control
- **Purpose**: Standardized implement communication
- **Speed**: 250 kbps standard

### K_Bus (Function ID: 4)
- **Protocol**: Tractor control bus
- **Purpose**: Button inputs, hitch control, auxiliary functions
- **Speed**: Brand dependent (250-500 kbps)

### None (Function ID: 0)
- Bus not used for any function

## Configuration Guidelines

1. **Single Brand Setup**: Set the brand and assign appropriate buses to their typical functions
2. **Mixed Brand Setup**: Use "Generic" brand and assign specific functions to each bus
3. **Keya Motor**: Can be assigned to any bus regardless of tractor brand
4. **Speed Selection**: Most brands use 250 kbps, but some (notably Fendt K_Bus) may use 500 kbps

## Implementation Status

| Feature | Status |
|---------|---------|
| Keya Motor Control | ✅ Fully Implemented |
| Fendt Steering | ⚠️ Placeholder |
| Valtra/Massey Steering | ⚠️ Placeholder |
| Case IH/NH Steering | ❌ Not Implemented |
| Claas Steering | ❌ Not Implemented |
| JCB Steering | ❌ Not Implemented |
| Lindner Steering | ❌ Not Implemented |
| CAT MT Steering | ❌ Not Implemented |
| Button Reading | ❌ Not Implemented |
| Hitch Control | ❌ Not Implemented |

## Notes

- The current implementation focuses on Keya motor control as a CAN function
- Other brands have placeholder implementations that need to be completed
- The flexible architecture allows mixing functions from different brands on different buses
- Always verify CAN bus speeds with your specific tractor model