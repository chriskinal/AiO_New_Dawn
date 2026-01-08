# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**AiO New Dawn** is a Teensy 4.1-based "All-in-One" agricultural autosteer controller for AgOpenGPS. It provides:
- GPS/GNSS positioning with dual-antenna heading
- IMU-based roll/heading compensation
- Autosteer motor control (PWM, Keya CAN, Danfoss hydraulic)
- CAN bus integration with tractor brands (steering angle, work switch, speed)
- Machine section control (up to 8 relays)
- Web-based configuration interface
- OTA firmware updates

The firmware communicates with AgOpenGPS PC software via UDP on port 8888 using PGN (Parameter Group Number) messages.

## Build Commands

- **Build firmware**: `~/.platformio/penv/bin/pio run -e teensy41`
- **Clean build**: `~/.platformio/penv/bin/pio run -e teensy41 --target clean`
- **Upload to device**: `~/.platformio/penv/bin/pio run -e teensy41 --target upload`
- **Monitor serial**: `~/.platformio/penv/bin/pio device monitor`

## Project Structure

```
AiO_New_Dawn/
├── src/
│   └── main.cpp                 # Entry point, setup() and loop()
├── lib/
│   ├── aio_autosteer/           # Steering control subsystem
│   ├── aio_communications/      # CAN, I2C, Serial managers
│   ├── aio_config/              # Configuration and hardware management
│   ├── aio_navigation/          # GPS, IMU, navigation processing
│   ├── aio_system/              # Core system (PGN, web, networking, logging)
│   │   ├── SimpleScheduler/     # Cooperative task scheduler
│   │   └── web_pages/           # HTML/CSS/JS for web UI (as PROGMEM strings)
│   ├── BNOAiOParser/            # BNO085 IMU parser
│   ├── DHCPLite/                # Lightweight DHCP server
│   ├── TM171AiOParser/          # TM171 IMU parser
│   ├── UBXParser/               # u-blox GPS parser
│   └── [third-party libs]       # ArduinoJson, FlexCAN_T4, QNEthernet, etc.
├── knowledge/                   # Design docs, protocol references
├── docs/                        # User-facing documentation
└── CanConfig_Examples/          # JSON CAN configs for tractor brands
```

## Library Organization

### aio_autosteer/ - Steering Control
| File | Purpose |
|------|---------|
| `AutosteerProcessor.h/cpp` | Main 100Hz control loop, PID steering, PGN handling |
| `MotorDriverInterface.h` | Abstract base for motor drivers |
| `MotorDriverManager.h/cpp` | Factory for motor driver instantiation |
| `PWMMotorDriver.h/cpp` | Standard PWM motor control (Cytron, IBT-2) |
| `KeyaCANDriver.h` | Keya BLDC motor via CAN bus |
| `KeyaSerialDriver.h/cpp` | Keya motor via RS485 serial |
| `DanfossMotorDriver.h` | Danfoss hydraulic valve control |
| `TractorCANDriver.h/cpp` | Read steering angle/speed from tractor CAN |
| `CANFilterHelper.h/cpp` | Hardware CAN mailbox filtering by brand |
| `WheelAngleFusion.h/cpp` | Sensor fusion for wheel angle |
| `EncoderProcessor.h/cpp` | Steering wheel encoder input |
| `KickoutMonitor.h/cpp` | Steer disengage detection |
| `ADProcessor.h/cpp` | Analog-to-digital for WAS, pressure, current |
| `PWMProcessor.h/cpp` | PWM input capture for remote sensors |

### aio_communications/ - Hardware Interfaces
| File | Purpose |
|------|---------|
| `CANManager.h/cpp` | FlexCAN_T4 wrapper, message routing |
| `CANGlobals.h/cpp` | CAN bus instances (CAN1, CAN2, CAN3) |
| `I2CManager.h/cpp` | I2C bus management for IMU, ADC |
| `SerialManager.h/cpp` | Serial port management |
| `ESP32Interface.h/cpp` | Communication with optional ESP32 bridge |

### aio_config/ - Configuration
| File | Purpose |
|------|---------|
| `ConfigManager.h/cpp` | EEPROM read/write, settings structures |
| `HardwareManager.h/cpp` | Pin definitions, hardware initialization |
| `EEPROMLayout.h` | EEPROM address map |

### aio_navigation/ - Positioning
| File | Purpose |
|------|---------|
| `GNSSProcessor.h/cpp` | GPS message parsing, position/heading |
| `IMUProcessor.h/cpp` | BNO085/TM171 IMU handling, roll/heading |
| `NAVProcessor.h/cpp` | Navigation data fusion |
| `NavigationTypes.h` | Shared data structures |
| `MessageBuilder.h` | PGN message construction helpers |

### aio_system/ - Core System
| File | Purpose |
|------|---------|
| `PGNProcessor.h/cpp` | PGN message routing and dispatch |
| `QNEthernetUDPHandler.h/cpp` | UDP send/receive on port 8888 |
| `QNetworkBase.h/cpp` | Ethernet initialization |
| `SimpleHTTPServer.h/cpp` | Lightweight web server |
| `SimpleWebManager.h/cpp` | Web endpoint registration |
| `SimpleWebSocket.h/cpp` | WebSocket implementation |
| `SimpleOTAHandler.h/cpp` | Over-the-air firmware updates |
| `MachineProcessor.h/cpp` | Section control, relay outputs |
| `LEDManagerFSM.h/cpp` | Status LED state machine |
| `EventLogger.h/cpp` | Debug logging (Serial + UDP syslog) |
| `CommandHandler.h/cpp` | Serial command menu |
| `Version.h` | Firmware version string |
| `CANConfigStorage.h` | LittleFS storage for CAN configs |

## Architecture Overview

### Communication Flow
```
AgOpenGPS (PC)
    ↓ UDP port 8888
QNEthernetUDPHandler
    ↓ raw bytes
PGNProcessor
    ↓ routes by PGN number
[AutosteerProcessor, MachineProcessor, GNSSProcessor, etc.]
    ↓ hardware control
Motors, Relays, LEDs
```

### Module Registration Pattern
```cpp
// In processor init():
pgnProcessor.registerCallback(PGN_STEER_DATA,
    [this](const uint8_t* data, size_t len) {
        this->handleSteerData(data, len);
    });

// For broadcast PGNs (200, 202):
pgnProcessor.registerBroadcastCallback(
    [this](uint8_t pgn, const uint8_t* data, size_t len) {
        // Handle subnet scan, hello messages
    });
```

### Main Loop Structure (SimpleScheduler)
```cpp
void loop() {
    scheduler.run();  // Executes registered tasks at their intervals
}

// Tasks registered with priorities and intervals:
// - 100Hz: AutosteerProcessor (steering control)
// - 50Hz:  IMU processing
// - 10Hz:  PGN 250 sensor data broadcast
// - 1Hz:   Heartbeats, watchdogs
```

## PGN Protocol Reference

### Key Incoming PGNs (from AgOpenGPS)
| PGN | Name | Description |
|-----|------|-------------|
| 200 | Hello/Scan | Subnet discovery broadcast |
| 201 | Scanner | Detailed module discovery |
| 202 | Scan Reply | Response with module info |
| 254 | Steer Data | Steer angle setpoint, speed, enable |
| 251 | Steer Config | PID settings, motor config |
| 239 | Machine Data | Section states, work switch |

### Key Outgoing PGNs (to AgOpenGPS)
| PGN | Name | Description |
|-----|------|-------------|
| 253 | Steer Status | Current angle, PWM, switch states |
| 250 | Sensor Data | Heading, roll, GPS status |
| 126 | PAOGI | Position, heading, speed message |

### PGN Message Format
```
Byte 0: 0x80 (header)
Byte 1: 0x81 (header)
Byte 2: PGN number
Byte 3: Length
Byte 4+: Payload
```

## CAN Bus Architecture

### Physical Buses
- **CAN1**: Primary tractor bus (J1939 style)
- **CAN2**: Secondary/implement bus
- **CAN3**: Keya motor communication

### Tractor Brand Support
Configured via web UI, stored in EEPROM. Each brand has specific CAN IDs:
- Fendt (SCR/S4/Gen6, One)
- Claas
- Valtra/Massey Ferguson
- Case IH/New Holland
- JCB
- Lindner
- CAT MT Series

### CAN Configuration Files
Located in `CanConfig_Examples/`. Users can upload custom JSON configs via web UI.

## Web Interface

### Endpoints
| Path | Handler |
|------|---------|
| `/` | Home page with status |
| `/device` | Device settings (motor type, sensors) |
| `/network` | Network configuration |
| `/can` | CAN bus configuration |
| `/canupload` | Upload custom CAN config |
| `/gps` | GPS/UM98x configuration |
| `/ota` | Firmware update |
| `/logger` | Event log viewer |
| `/ws` | WebSocket for live data |

### Web Page Files
All in `lib/aio_system/web_pages/` as PROGMEM strings:
- `TouchFriendly*.h` - Mobile-optimized pages
- `Simple*.h` - Desktop pages
- `CommonStyles.h` - Shared CSS

## Configuration System

### EEPROM Layout
See `lib/aio_config/EEPROMLayout.h` for address map. Key sections:
- Network settings (IP, subnet, gateway)
- Steer settings (PID, motor type, sensor config)
- Machine settings (section count, relay logic)
- CAN settings (brand, bus assignments)

### Settings Flow
1. AgOpenGPS sends PGN 251 (steer config)
2. AutosteerProcessor extracts settings
3. ConfigManager writes to EEPROM
4. Module reinitializes with new settings

## Hardware Pin Reference

See `knowledge/HARDWARE_OWNERSHIP_MATRIX.md` for complete pin assignments.

### Critical Pins
| Pin | Function |
|-----|----------|
| 2 | PWM_OUT (motor) |
| 3 | DIR_OUT (motor direction) |
| 4 | ENABLE_OUT (motor enable) |
| 15 | WAS_INPUT (wheel angle sensor) |
| 22/23 | CAN1 TX/RX |
| 0/1 | CAN2 TX/RX |
| 30/31 | CAN3 TX/RX |

## Motor Driver Types

### Detection at Boot
MotorDriverManager auto-detects based on:
1. CAN bus presence (Keya heartbeat)
2. Configuration in EEPROM
3. Fallback to PWM

### PWM Motor (Cytron/IBT-2)
- Standard DC motor with H-bridge
- PWM on pin 2, DIR on pin 3
- Soft-start ramp for smooth engagement

### Keya BLDC (CAN)
- Communicates via CAN3 at 250kbps
- Torque/speed control modes
- Heartbeat monitoring

### Danfoss Hydraulic
- Uses outputs 7 & 8 (protects machine outputs 5 & 6)
- Proportional valve control

## Debugging

### Serial Menu
Press `?` at boot or during runtime:
```
a - Autosteer status
c - CAN status
e - Event logger toggle
g - GPS status
i - IMU status
m - Motor test
n - Network status
u - Current sensor calibration
```

### EventLogger
```cpp
EventLogger::log(EventLogger::INFO, "message");
EventLogger::log(EventLogger::DEBUG, "value: %d", val);
```
Outputs to Serial and optionally UDP syslog.

### Web Log Viewer
Navigate to `http://192.168.5.126/logger` for live log stream via WebSocket.

## Common Development Tasks

### Adding a New PGN Handler
1. Define PGN number constant
2. Create handler method in appropriate processor
3. Register callback in processor's `init()` method
4. Add outgoing message builder if needed

### Adding a Web Page
1. Create `TouchFriendly[Name]Page.h` in `web_pages/`
2. Define page content as PROGMEM string
3. Register endpoint in `SimpleWebManager.cpp`

### Adding a Motor Driver
1. Create class inheriting `MotorDriverInterface`
2. Implement `init()`, `setMotor()`, `getStatus()`
3. Add detection logic to `MotorDriverManager`
4. Update web UI motor type dropdown

### Adding CAN Brand Support
1. Create JSON config in `CanConfig_Examples/`
2. Add brand enum to `TractorCANDriver.h`
3. Implement message parsing in `TractorCANDriver.cpp`
4. Add filter setup in `CANFilterHelper.cpp`

## Common Gotchas

1. **Pin Conflicts**: Check HARDWARE_OWNERSHIP_MATRIX.md before using pins
2. **PGN 200/202**: Never register directly - use `registerBroadcastCallback()`
3. **Analog Pins**: Must use `INPUT_DISABLE` mode, not `INPUT`
4. **Motor Changes**: Motor type changes require reboot
5. **EEPROM Writes**: Minimize writes; batch changes when possible
6. **CAN Buffers**: Keep small (16-32 messages) to save RAM
7. **Web Responses**: Use chunked/paced writes for large responses
8. **Timing Critical**: Don't block in 100Hz loop for more than 1ms

## Version Control

- Version in `lib/aio_system/Version.h`
- Format: `MAJOR.MINOR.PATCH-beta`
- Increment PATCH for bug fixes
- Increment MINOR for new features
- Update version with each functional change

## Third-Party Libraries (Do Not Modify)

| Library | Purpose |
|---------|---------|
| ArduinoJson | JSON parsing for configs |
| FlexCAN_T4 | CAN bus driver for Teensy |
| QNEthernet | Ethernet stack |
| Adafruit_BusIO | I2C helpers |
| Adafruit_PWMServoDriver | PCA9685 PWM expansion |
| Encoder | Quadrature encoder library |
| FlasherX | Flash programming for OTA |
| Streaming | Stream operators |

## License

GPL v3 - All project-created files have the license header. See LICENSE.md.
