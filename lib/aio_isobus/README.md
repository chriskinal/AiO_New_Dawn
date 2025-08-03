# ISOBUS VT Support for New Dawn

## Overview
This module provides ISOBUS Virtual Terminal (VT) support for the New Dawn board, allowing it to display information on ISOBUS-compatible displays commonly found in agricultural equipment.

## Current Status
The ISOBUS VT feature is **disabled by default** due to memory constraints. The AgIsoStack library is quite large and causes the Teensy 4.1's ITCM (Instruction Tightly Coupled Memory) to overflow when combined with all other New Dawn features.

## Memory Issue
- The Teensy 4.1 has 512KB of ITCM (fast instruction memory)
- AgIsoStack library adds approximately 141KB to ITCM usage
- Combined with existing code, this exceeds the ITCM limit

## Enabling ISOBUS VT

To enable ISOBUS VT support, you have several options:

### Option 1: Enable with Reduced Features (Recommended)
1. Edit `platformio.ini`
2. Uncomment the line: `-DENABLE_ISOBUS_VT`
3. Disable other features to make room (e.g., web server, some processors)

### Option 2: Create a Dedicated Build
Create a separate PlatformIO environment for ISOBUS builds:

```ini
[env:teensy41-isobus]
platform = teensy
board = teensy41
framework = arduino
upload_speed = 921600
upload_protocol = teensy-gui
extra_scripts = copy_hex.py
build_flags = 
    -DENABLE_ISOBUS_VT
    -DDISABLE_WEB_SERVER  ; Example: disable web server
    -DDISABLE_GNSS        ; Example: disable GNSS
lib_deps = 
```

### Option 3: Use External ISOBUS Module
Consider using a separate microcontroller (e.g., ESP32) as a dedicated ISOBUS gateway communicating with New Dawn via CAN or serial.

## Features When Enabled

The VT client provides:
- Automatic connection to ISOBUS VT on CAN2
- Hello World demonstration with counter
- Framework for custom object pools
- Soft key and button event handling
- Support for all standard VT objects

## Usage Example

When enabled, the VT client automatically:
1. Connects to any ISOBUS VT on the CAN2 network
2. Uploads the object pool
3. Displays "Hello World!" with a counter
4. Responds to soft key presses

## Future Improvements

Potential solutions for the memory issue:
1. Move AgIsoStack to external SDRAM (requires code modifications)
2. Implement dynamic loading of VT features
3. Create a stripped-down version of AgIsoStack
4. Use conditional compilation to include only needed VT objects

## Files

- `VTClient.h/cpp` - Main VT client implementation
- `HelloWorldObjectPool.h` - Example object pool
- `isobus_no_fastrun.h` - Attempt to keep code out of ITCM

## Testing

To test with an ISOBUS VT:
1. Enable the feature in platformio.ini
2. Connect ISOBUS VT to CAN2 (pins 0/1 on New Dawn)
3. Power on - the VT should show the Hello World screen