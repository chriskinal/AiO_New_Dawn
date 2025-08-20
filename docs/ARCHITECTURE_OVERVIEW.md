# AiO New Dawn Architecture Overview

This document provides a high-level overview of the AiO New Dawn system architecture and its key components.

## System Overview

AiO New Dawn is a modular agricultural control system built on the Teensy 4.1 platform. It integrates GPS/GNSS navigation, automated steering, section control, and network communication to work with AgOpenGPS.

## Core Architecture Principles

- **Modular Design**: Each subsystem is self-contained with clear interfaces
- **Event-Driven Communication**: Uses PGN (Parameter Group Number) messages for inter-module communication
- **Real-Time Processing**: Critical control loops run at consistent intervals (100Hz for autosteer, 10Hz for sensors)
- **Network-First**: Built around QNEthernet for reliable UDP communication
- **Hardware Abstraction**: Pin ownership and resource management prevents conflicts
- **Finite State Machines**: Used for LED control and system state management
- **WebSocket Integration**: Real-time telemetry and configuration via web interface

## Main Components

### System Management

**main.cpp**
- Entry point that initializes all subsystems in the correct order
- Manages the main loop that polls each processor

**ConfigManager** (`lib/aio_config/`)
- Stores and manages all system configuration in EEPROM
- Provides centralized access to hardware pins, network settings, and operational parameters

**EventLogger** (`lib/aio_system/`)
- Centralized logging system with configurable output to Serial and UDP syslog
- Supports multiple severity levels and event sources with rate limiting

**CommandHandler** (`lib/aio_system/`)
- Provides interactive serial menu for configuration and debugging
- Handles user input for logging control and system status

### Network & Communication

**QNetworkBase** (`lib/aio_system/`)
- Manages Ethernet initialization and connection status
- Handles DHCP/static IP configuration and monitors link state

**AsyncUDPHandler** (`lib/aio_system/`)
- Manages all UDP communication with AgOpenGPS
- Routes incoming PGN messages to appropriate processors

**PGN Message Routing**
- PGN 200 and 202 are broadcast messages - handled via broadcast callbacks
- Other PGNs are routed to registered processors
- Subnet scanning responds on PGN 202 with module identification

**SimpleWebManager** (`lib/aio_system/`)
- Lightweight HTTP server without external dependencies
- Provides configuration pages for network, device settings, and sensors
- WebSocket support for real-time telemetry and event logging
- OTA firmware update support via web interface

**SerialManager** (`lib/aio_communications/`)
- Manages multiple hardware serial ports for GPS, Radio, IMU, and RS232
- Provides bridging functionality between serial devices and UDP

### Navigation & Positioning

**GNSSProcessor** (`lib/aio_navigation/`)
- Parses NMEA and UBX messages from GPS/GNSS receivers
- Manages dual GPS inputs with automatic failover

**RTCMProcessor** (`lib/aio_system/`)
- Handles RTK correction data (RTCM messages)
- Routes corrections between network/serial sources and GPS receivers
- Supports both serial and network RTCM sources
- Visual indication via blue pulse on GPS LED

**IMUProcessor** (`lib/aio_navigation/`)
- Interfaces with BNO08x IMU for heading and roll data
- Supports multiple IMU types: BNO08x, TM171, UM981 integrated INS
- Processes IMU data and forwards to AgOpenGPS via PGN
- INS alignment status tracked for UM981

**NAVProcessor** (`lib/aio_navigation/`)
- Combines GPS and IMU data for complete navigation solution
- Manages coordinate transformations and heading calculations

### Vehicle Control

**AutosteerProcessor** (`lib/aio_autosteer/`)
- Implements automated steering control at 100Hz update rate
- Supports multiple motor driver types via abstract interface
- Handles steering angle sensing with WAS and optional encoder fusion
- Sends PGN253 status messages with current wheel angle even when autosteer is off
- Includes KickoutMonitor for safety threshold monitoring
- Button press detection with blue LED pulse feedback

**MachineProcessor** (`lib/aio_system/`)
- Manages section control for implements (sprayers, seeders, etc.)
- Controls up to 16 sections via PCA9685 PWM controller at address 0x44
- Supports machine functions: hydraulic lift, tramlines, and geo-stop
- Automatically protects outputs 5 & 6 when Danfoss valve is configured
- Initializes Danfoss valve to 50% PWM (centered) at boot
- Configurable output types: simple, motor, motor reversed

**MotorDriverInterface** (`lib/aio_autosteer/`)
- Abstract interface for different motor driver types
- Implementations include:
  - PWM drivers: Cytron MD30C, IBT-2, DRV8701
  - CAN-based: Keya motor
  - Hydraulic: Danfoss proportional valve
- Supports coast/brake mode configuration for PWM drivers

**PWMProcessor** (`lib/aio_autosteer/`)
- Generates speed pulse output for rate controllers
- Configurable frequency and duty cycle based on ground speed
- Supports both distance-based and time-based pulse generation
- Direct hardware PWM output on pin 9 for accuracy

### Hardware Interfaces

**HardwareManager** (`lib/aio_config/`)
- Manages hardware initialization and pin assignments
- Provides buzzer control and hardware status reporting

**I2CManager** (`lib/aio_communications/`)
- Manages I2C bus with thread-safe access
- Devices:
  - PCA9685 at 0x44: Section outputs and machine control
  - PCA9685 at 0x70: RGB LED control
  - BNO08x IMU (when configured)
- 1MHz bus speed with automatic error recovery
- Shared resource management prevents conflicts

**CANManager** (`lib/aio_communications/`)
- Manages FlexCAN interfaces for motor control and implement communication
- Auto-detects connected CAN devices like Keya steering motors
- Supports CAN3 for Keya motor control with automatic baud rate detection
- Global CAN message storage for debugging and monitoring

**LEDManagerFSM** (`lib/aio_system/`)
- Controls RGB LEDs via PCA9685 at address 0x70 using FSM pattern
- Four status LEDs: PWR/ETH, GPS, STEER, INS
- Blue pulse overlays for RTCM data (GPS LED) and button press (STEER LED)
- Updates every 100ms with 25% default brightness

**ADProcessor** (`lib/aio_autosteer/`)
- High-speed analog input processing for steering sensors
- Reads WAS (Wheel Angle Sensor) with configurable filtering
- Monitors work switch and remote switch states
- Current sensor support with zero-offset calibration
- Pressure sensor monitoring with configurable thresholds
- Uses INPUT_DISABLE mode for proper analog pin configuration

### Data Processing

**PGNProcessor** (`lib/aio_system/`)
- Central router for all PGN messages in the system
- Manages callback registration and message distribution

**UBXParser** (`lib/aio_navigation/`)
- Parses u-blox UBX protocol messages
- Extracts high-precision position and timing data

**BNO_RVC** (`lib/aio_navigation/`)
- Interfaces with BNO08x IMU in RVC (Robot Vacuum Cleaner) mode
- Simplified binary protocol for heading and motion data

## Data Flow

1. **GPS/IMU Data** → GNSSProcessor/IMUProcessor → NAVProcessor → PGN messages → UDP to AgOpenGPS
2. **Steering Commands** ← UDP from AgOpenGPS → PGN messages → AutosteerProcessor → Motor Driver
3. **Section Control** ← UDP from AgOpenGPS → PGN messages → MachineProcessor → PCA9685 → Sections
4. **Configuration** ← Web Interface/Serial Menu → ConfigManager → EEPROM
5. **Telemetry** → All processors → EventLogger → WebSocket/Serial/UDP syslog
6. **Sensor Fusion** → ADProcessor + EncoderProcessor → WheelAngleFusion → AutosteerProcessor

## Key Design Patterns

- **Singleton Pattern**: Used for managers and processors that need global access
- **Factory Pattern**: Motor drivers created based on detected hardware or EEPROM configuration
- **Observer Pattern**: PGN callback system for message routing
- **Finite State Machine**: LED control (LEDManagerFSM) and system state management
- **Strategy Pattern**: Multiple motor driver implementations with common interface
- **Resource Management**: Pin ownership tracking prevents hardware conflicts
- **Abstract Interface**: MotorDriverInterface allows polymorphic motor control

## Building and Development

The project uses PlatformIO with the Teensy 4.1 platform. Key directories:
- `lib/` - All modular components organized by functionality
  - `aio_autosteer/` - Steering control and motor drivers
  - `aio_communications/` - Serial, CAN, and I2C interfaces
  - `aio_config/` - Configuration and hardware management
  - `aio_navigation/` - GPS, IMU, and navigation processing
  - `aio_system/` - Core system services and web interface
- `src/` - Main application entry point
- `docs/` - System documentation
- `knowledge/` - Extended documentation and implementation guides
- `data/` - Web interface static files

## Network Protocol

Uses AgOpenGPS PGN protocol over UDP port 8888:
- Static IP: 192.168.5.126 (default for Steer module)
- Receives control commands from AgOpenGPS
- Sends position, heading, and status data at defined rates:
  - PGN 253 (autosteer status): 100Hz
  - PGN 250 (sensor data): 10Hz
  - PGN 203 (IMU data): As received
- Module discovery via subnet scanning (PGN 202)
- DHCP supported but static IP preferred for reliability

## Recent Improvements

### Motor Driver System
- **Multiple Driver Support**: 
  - PWM drivers: Cytron MD30C, IBT-2, DRV8701
  - CAN-based: Keya motor with auto-detection
  - Hydraulic: Danfoss proportional valve
- **Danfoss Valve**: Proper initialization at boot with 50% PWM centering
- **PWM Control**: Standard PWM mode for PCA9685 eliminates pulse glitches
- **Coast/Brake Mode**: Configurable motor behavior for PWM drivers
- **Persistent Configuration**: Motor type stored in EEPROM

### System Features
- **Real-time Status**: PGN253 wheel angle reported even when autosteer is disabled
- **Output Protection**: Automatic reservation of outputs 5 & 6 for Danfoss valve
- **Visual Feedback**: 
  - Four status LEDs with color-coded states
  - Blue pulse overlays for RTCM data and button press
  - FSM-based LED control for reliable state transitions
- **Sensor Fusion**: WheelAngleFusion combines WAS and encoder data
- **Safety Monitoring**: KickoutMonitor tracks thresholds and disengages on limits

### Hardware Compatibility
- **Motor Drivers**: Cytron, IBT-2, DRV8701, Keya CAN, Danfoss hydraulic
- **GPS/GNSS**: 
  - Dual GPS support with automatic failover
  - NMEA and UBX protocol support
  - RTK corrections via serial or network
- **IMU/INS Options**: 
  - BNO08x in RVC mode
  - TM171 external IMU
  - UM981 integrated INS with alignment tracking
- **Sensors**:
  - Wheel angle sensor (analog)
  - Quadrature encoder support
  - Current and pressure sensors
  - Work and remote switches
- **Little Dawn Support**: Processor type detection and compatibility

## Web Interface

The system includes a comprehensive web interface accessible at the device IP address:

### Core Pages
- **Home**: System status overview and quick links
- **Network Settings**: IP configuration, DHCP/static mode
- **Device Settings**: Hardware configuration and calibration
- **Analog/Work Switch**: Sensor calibration and testing
- **Event Logger**: Real-time system logs via WebSocket
- **OTA Update**: Firmware update via web upload

### Features
- **WebSocket Support**: Real-time telemetry and logging
- **Responsive Design**: Works on desktop and mobile devices
- **No External Dependencies**: Custom lightweight HTTP server
- **Multi-language Ready**: Template system supports localization

## Pin Management and Resource Allocation

The system uses sophisticated resource management to prevent hardware conflicts:

### Pin Ownership Manager
- Tracks which module owns each pin
- Prevents multiple modules from using the same pin
- Runtime validation of pin assignments
- Clear error messages for conflicts

### Shared Resource Manager
- Manages PWM timer allocation
- Coordinates I2C bus access
- Handles ADC channel assignment
- Thread-safe resource locking

### Hardware Abstraction
- HardwareManager provides centralized pin definitions
- Consistent API for pin control across modules
- Support for both Teensy 4.1 and Little Dawn processors

## Error Handling and Diagnostics

### Event Logger System
- Multiple output targets: Serial, UDP syslog, WebSocket
- Configurable log levels and filtering
- Rate limiting to prevent log spam
- Source-based filtering for targeted debugging

### LED Status Indicators
- Four RGB LEDs show system state at a glance
- Color-coded for different subsystems
- Blue pulse overlays for real-time events
- FSM-based control ensures consistent behavior

### Serial Command Interface
- Interactive menu system (press '?')
- Event logger control (serial/UDP output)
- Log level adjustment
- Loop timing diagnostics
- Test message generation

## Performance Considerations

### Timing Critical Operations
- Autosteer control loop: 100Hz (10ms) - highest priority
- Sensor updates: 10Hz (100ms) 
- LED updates: 10Hz (100ms)
- Web server: Event-driven, non-blocking

### Memory Management
- Static allocation where possible
- Careful buffer management for network operations
- EEPROM wear leveling for configuration storage
- Efficient PGN message routing

### Network Optimization
- UDP for low-latency control messages
- WebSocket for efficient real-time updates
- Lightweight HTTP server reduces overhead
- Static IP recommended for consistent performance