# Technical Stack

## Core Platform

- **Hardware Platform:** Teensy 4.1 microcontroller (ARM Cortex-M7 @ 600MHz)
- **Development Framework:** Arduino framework with PlatformIO build system
- **Programming Language:** C++ with embedded system optimizations
- **Version Control:** Git with conventional commit standards
- **Current Version:** v1.0.51-beta (actively maintained)
- **Repository Structure:** Modular libraries in lib/ with clear separation of concerns

## Networking and Communication

- **Ethernet Stack:** QNEthernet library (lwIP-based) for Teensy 4.1 native Ethernet
- **Network Protocol:** UDP-based PGN (Parameter Group Number) messaging compatible with AgOpenGPS
- **Web Server:** Custom lightweight HTTP server without external dependencies
- **Real-time Communication:** WebSocket implementation for live telemetry and configuration
- **Serial Communication:** Hardware UART interfaces for GPS, IMU, and radio modules

## Hardware Interfaces

- **CAN Bus:** FlexCAN_T4 library for CAN3 interface supporting Keya motors and implements
- **I2C Management:** Custom I2CManager with thread-safe access and 1MHz bus speed
- **PWM Control:** Native Teensy PWM with PCA9685 I2C PWM controllers for section control
- **ADC Processing:** High-speed analog input processing for sensors and control feedback
- **GPIO Management:** Pin ownership tracking and shared resource coordination

## Data Processing and Storage

- **Configuration Storage:** EEPROM-based persistent configuration with wear leveling
- **JSON Processing:** ArduinoJson library for web API and configuration serialization
- **GPS/GNSS Parsing:** Custom NMEA and UBX protocol parsers for dual GPS support
- **Message Routing:** Event-driven PGN processor with callback registration system
- **Sensor Fusion:** Custom algorithms combining wheel angle sensors, encoders, and IMU data

## Motor Control and Automation

- **Motor Driver Interface:** Abstract base class supporting multiple motor driver types
- **PWM Motor Drivers:** Support for Cytron MD30C, IBT-2, DRV8701 with coast/brake modes
- **CAN Motor Control:** Native support for Keya CAN-based steering motors
- **Hydraulic Control:** Danfoss proportional valve support with automatic output protection
- **Safety Systems:** KickoutMonitor for threshold-based safety disengagement

## User Interface and Monitoring

- **Web Interface:** Responsive HTML5 interface with no external framework dependencies
- **CSS Framework:** Custom lightweight CSS with mobile-responsive design
- **JavaScript:** Vanilla JavaScript with WebSocket for real-time updates
- **Icon Library:** Custom SVG icons optimized for agricultural applications
- **Status Indication:** FSM-based RGB LED control with color-coded system states

## Development and Deployment

- **Build System:** PlatformIO with Teensy platform support
- **OTA Updates:** Web-based firmware upload with FlasherX integration
- **Code Repository:** Git-based version control with modular library organization
- **Testing Strategy:** Hardware-in-the-loop testing with serial command interface
- **Documentation:** Markdown-based documentation with architecture diagrams

## External Integrations

- **AgOpenGPS Compatibility:** Full PGN protocol implementation for seamless integration
- **GPS/RTK Support:** Multi-vendor GPS receiver support with RTK correction handling
- **IMU Integration:** Support for BNO08x, TM171, and UM981 IMU/INS systems
- **Radio Modules:** Serial-based RTCM correction data from radio transceivers
- **ESP32 Bridge:** Little Dawn compatibility with processor type detection

## Security and Reliability

- **Network Security:** Static IP configuration with optional DHCP support
- **Error Handling:** Comprehensive event logging with multiple output targets
- **Hardware Protection:** Pin ownership management preventing resource conflicts
- **Fault Recovery:** Automatic error recovery with graceful degradation
- **Diagnostics:** Real-time system monitoring with performance metrics

## Performance Characteristics

- **Control Loop Timing:** 100Hz autosteer control with deterministic timing
- **Network Performance:** UDP-based messaging with minimal latency
- **Memory Management:** Static allocation with careful buffer management
- **Real-time Constraints:** Interrupt-driven hardware interfaces with priority scheduling
- **Power Efficiency:** Optimized for 12V automotive electrical systems