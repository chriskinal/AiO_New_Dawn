# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

- **Build firmware**: `~/.platformio/penv/bin/pio run -e teensy41`
- **Clean build**: `~/.platformio/penv/bin/pio run -e teensy41 --target clean`
- **Upload to device**: `~/.platformio/penv/bin/pio run -e teensy41 --target upload`
- **Generate file tree**: `tree -J > file_tree.json`

## Architecture Overview

AiO New Dawn is a Teensy 4.1-based agricultural control system for AgOpenGPS. Key architectural patterns:

### Communication Flow
1. **UDP Messages** arrive at AsyncUDPHandler on port 8888
2. **PGN Messages** are routed to registered processors via PGNProcessor
3. **Processors** handle specific PGNs and control hardware
4. **Status Messages** are sent back to AgOpenGPS via UDP

### Module Registration Pattern
- Modules register for specific PGNs in their init() method
- PGN 200 and 202 are broadcast messages - modules should NOT register for these
- Use `registerBroadcastCallback()` to receive all broadcast messages

### Hardware Management
- **HardwareManager** provides pin definitions and basic initialization
- **PinOwnershipManager** tracks dynamic pin ownership and prevents conflicts
- **SharedResourceManager** coordinates shared hardware (PWM timers, ADC, I2C)
- Pin modes: Use `INPUT_DISABLE` for analog pins, not `INPUT`

### Critical Timing
- AutosteerProcessor runs at 100Hz (10ms loop)
- PGN 253 (status) sent every 100Hz cycle
- PGN 250 (sensor data) sent at 10Hz
- Web endpoints update at client request rate

## Key Implementation Details

### Motor Driver Architecture
- **MotorDriverInterface** is the abstract base
- Implementations: PWMMotorDriver, KeyaCANDriver, DanfossMotorDriver
- Motor type detection happens in MotorDriverManager
- Danfoss valve uses outputs 7&8 and protects MachineProcessor outputs 5&6

### Sensor Handling
- **Encoder**: Can be single channel or quadrature, configured via web UI
- **Current Sensor**: Zero-offset calibration stored in EEPROM
- **Pressure Sensor**: Analog input with configurable threshold
- **WAS (Wheel Angle Sensor)**: Supports calibration and Ackerman correction

### Network Configuration
- Static IP: 192.168.5.126 (Steer module)
- Subnet scanning responds on PGN 202
- DHCP supported but static preferred for AgOpenGPS

### Version Management
- Version stored in `lib/aio_system/Version.h`
- Format: `MAJOR.MINOR.PATCH-alpha`
- Increment patch version for bug fixes
- Update with each commit that changes functionality

## Debugging Principles

- When code just written doesn't work, the issue is likely in that new code
- Start debugging at the most recent change
- Use EventLogger for runtime debugging (supports Serial and UDP syslog)
- Check pin ownership conflicts if hardware isn't responding

## CAN Messages (Keya Motor)

Example CAN traffic for debugging:
- `0x7000001` - Heartbeat from Keya
- `0x6000001` - Commands to Keya
- `0x5800001` - Acknowledgments from Keya

## Common Gotchas

1. **Pin Conflicts**: Check HARDWARE_OWNERSHIP_MATRIX.md for pin assignments
2. **PGN Registration**: Never register for PGN 200 or 202 (use broadcast callback)
3. **Analog Pins**: Must use INPUT_DISABLE mode, not INPUT
4. **Motor Detection**: Happens at boot - motor type changes require reboot
5. **Web Changes**: Device settings saved to EEPROM require module reinitialization

## Testing Tools

- Serial menu: Press '?' at boot for interactive menu
- Web interface: http://192.168.5.126/
- Event logger control: 'e' in serial menu
- Current monitor: 'u' in serial menu for live readings

## Debugging Principles
- When some code we just wrote or modified isn't working, it is most likely that code. Hypothisizing about something external that has been working for days or weeks wastes time and effort. Start with the most likely cause.
- Look within to find the source of an error when working on code you just wrote.

## Command Line Tips
- Use apple mdfind could be useful for:
  - Quick file name searches across the project
  - Finding files by type/extension
  - Content searches (though it depends on Spotlight indexing)
  - Finding files modified recently

## Version Control
- When comiting fixes, increment the patch version number in ./sys/version.h

## File Tree
- File tree for the project is stored in /Users/chris/Documents/Code/AiO_New_Dawn/file_tree.json
- The bash command 'tree -J' will output the latest tree in JSON format