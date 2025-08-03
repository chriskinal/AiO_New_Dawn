# ISOBUS VT Implementation Knowledge Base

## Summary
This document captures all the learning from attempting to implement ISOBUS VT support for the New Dawn project.

## Problem Statement
AgOpenGPS users want ISOBUS Virtual Terminal (VT) support to display information on tractor displays. The AgIsoStack library provides comprehensive ISOBUS support but is too large to fit in the Teensy 4.1's memory alongside New Dawn's existing features.

## Memory Analysis

### Teensy 4.1 Memory Layout
- ITCM (Instruction Tightly Coupled Memory): 512KB total
- Current New Dawn usage: 310KB
- AgIsoStack requires: ~141KB additional
- Available space: ~202KB (not enough)

### Optimization Attempts and Results
1. **Initial state**: 141KB overflow
2. **Disabled web server**: Reduced to 35KB overflow (saved 52KB)
3. **Disabled serial menu**: Reduced to 31KB overflow (saved 4KB)
4. **Created custom CAN plugin**: Reduced to 23KB overflow (saved 8KB)
   - Discovered AgIsoStack includes its own FlexCAN_T4 instances
   - Created NewDawnCANPlugin to reuse existing CAN infrastructure
5. **Removed unused AgIsoStack modules**: Still 23KB overflow
   - Removed task controller, guidance, NMEA2000 support
   - Core VT functionality still too large

### Key Discovery
AgIsoStack bundles its own copy of FlexCAN_T4, causing duplicate code. Even with a custom plugin to share CAN infrastructure, the library is fundamentally too large.

## Solution: ESP32 Bridge

### Hardware Discovery
The AiO board includes an ESP32 module slot (XIAO ESP32C3) with:
- Serial connection to Teensy (TX/RX with level shifting)
- Shared signals (GPS, speed pulse, PWM, etc.)
- Power (3.3V/5V)
- WiFi capability for future features

### CAN Connection
The AiO board has an internal header breaking out all 3 CAN buses. The ESP32 can connect to CAN2 (ISOBUS) using:
- **MCP2515 SPI CAN controller** (recommended)
- Connect via SPI to ESP32
- Wire CAN_H/CAN_L to CAN2 header

### Architecture Design
```
┌─────────────┐         Serial         ┌─────────────┐
│   Teensy    │◄──────────────────────►│    ESP32    │
│  (Control)  │      115200 baud       │ (ISOBUS VT) │
└──────┬──────┘                        └──────┬──────┘
       │                                      │
    CAN1,3                                    │ SPI
       │                                      │
┌──────┴──────┐                        ┌──────▼──────┐
│ Machine Bus │                        │   MCP2515   │
└─────────────┘                        └──────┬──────┘
                                              │
                                           CAN2
                                              │
                                       ┌──────▼──────┐
                                       │  ISOBUS VT  │
                                       └─────────────┘
```

### Serial Protocol Design
Binary protocol for efficiency:
```
[SYNC_BYTE][LENGTH][MSG_TYPE][DATA...][CHECKSUM]
```

Message types defined for:
- Machine status (speed, heading, angles)
- Work state (sections, rate)
- GPS data
- VT events (button presses, value changes)

## Implementation Files Created

### For Teensy (attempted ISOBUS integration):
1. `/lib/aio_isobus/VTClient.h/cpp` - ISOBUS VT client wrapper
2. `/lib/aio_isobus/HelloWorldObjectPool.h` - Example VT display
3. `/lib/aio_isobus/NewDawnCANPlugin.h/cpp` - Custom CAN plugin
4. `/lib/aio_system/WebSystemFlashmem.h` - Attempt to move code to flash

### For ESP32 Bridge (future work):
1. `/ESP32_ISOBUS_BRIDGE.md` - Complete architecture documentation
2. `/ESP32_CAN_CONNECTION_OPTIONS.md` - Hardware connection guide

### Documentation:
1. `/ISOBUS_MEMORY_OPTIMIZATION.md` - Detailed memory analysis
2. `/lib/aio_isobus/README.md` - ISOBUS module documentation

## Platform.io Configurations Added
```ini
; ISOBUS build with web server in flash
[env:teensy41-isobus]
build_flags = 
    -DENABLE_ISOBUS_VT
    -DWEBSYSTEM_USE_FLASHMEM

; Minimal ISOBUS build (no web/serial)
[env:teensy41-isobus-minimal]
build_flags = 
    -DENABLE_ISOBUS_VT
    -DDISABLE_WEB_SERVER
    -DDISABLE_SERIAL_MENU
```

## Next Steps (When MCP2515 Arrives)

1. **Hardware Setup**:
   - Connect MCP2515 to ESP32 SPI pins
   - Wire to CAN2 header
   - Test CAN communication

2. **ESP32 Firmware**:
   - Create PlatformIO project for XIAO ESP32C3
   - Implement MCP2515 driver
   - Add AgIsoStack (will fit easily in ESP32)
   - Implement serial protocol

3. **Teensy Updates**:
   - Add serial protocol handler
   - Create data publisher for ESP32
   - Handle incoming VT events

4. **Integration Testing**:
   - Verify serial communication
   - Test with real ISOBUS VT
   - Optimize performance

## Lessons Learned

1. **Memory Constraints**: Always check library sizes before integration
2. **Duplicate Dependencies**: Libraries may include their own copies of common code
3. **Modular Architecture**: Having an ESP32 slot provides excellent expansion options
4. **CAN Isolation**: Important to keep CAN buses properly isolated

## Shopping List
- XIAO ESP32C3 module
- MCP2515 CAN module with TJA1050 transceiver
- Dupont wires for connections
- Optional: ISO1050 for isolated CAN

## Code State
- Teensy code has conditional ISOBUS support (disabled by default)
- ESP32 bridge architecture fully documented
- Ready to implement when hardware arrives

This approach turns a memory limitation into an architectural advantage, providing a dedicated processor for ISOBUS while keeping the main system unchanged.