# AiO New Dawn Architecture Overview

This document provides a high-level overview of the AiO New Dawn system architecture and its key components.

## System Overview

AiO New Dawn is a modular agricultural control system built on the Teensy 4.1 platform. It integrates GPS/GNSS navigation, automated steering, section control, and network communication to work with AgOpenGPS.

## Core Architecture Principles

- **Modular Design**: Each subsystem is self-contained with clear interfaces
- **Event-Driven Communication**: Uses PGN (Parameter Group Number) messages for inter-module communication
- **Real-Time Processing**: Critical control loops run at consistent intervals
- **Network-First**: Built around QNEthernet for reliable UDP communication

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

**SubnetManager** (`lib/aio_system/`)
- Handles AgOpenGPS subnet scanning and module identification (PGN 201)
- Manages "Hello" broadcasts and scan responses

**WebManager** (`lib/aio_system/`)
- Provides web interface for configuration and status monitoring
- Supports OTA firmware updates and multi-language pages

**SerialManager** (`lib/aio_communications/`)
- Manages multiple hardware serial ports for GPS, RTK, IMU, and RS232
- Provides bridging functionality between serial devices and UDP

### Navigation & Positioning

**GNSSProcessor** (`lib/aio_navigation/`)
- Parses NMEA and UBX messages from GPS/GNSS receivers
- Manages dual GPS inputs with automatic failover

**RTCMProcessor** (`lib/aio_system/`)
- Handles RTK correction data (RTCM messages)
- Routes corrections between network/serial sources and GPS receivers

**IMUProcessor** (`lib/aio_navigation/`)
- Interfaces with BNO08x & TM172 IMU's for heading and roll data
- Processes IMU data and forwards to AgOpenGPS via PGN

**NAVProcessor** (`lib/aio_navigation/`)
- Combines GPS and IMU data for complete navigation solution
- Manages coordinate transformations and heading calculations

### Vehicle Control

**AutosteerProcessor** (`lib/aio_autosteer/`)
- Implements automated steering control with multiple motor driver support
- Handles steering angle sensing, motor control, and safety switches

**MachineProcessor** (`lib/aio_system/`)
- Manages section control for implements (sprayers, seeders, etc.)
- Controls up to 16 sections via PCA9685 PWM controller

**MotorDriverInterface** (`lib/aio_autosteer/`)
- Abstract interface for different motor driver types
- Implementations include PWM (cytron/IBT2) and CAN-based (Keya) drivers

**PWMProcessor** (`lib/aio_system/`)
- Generates speed pulse output for rate controllers
- Configurable frequency and duty cycle based on ground speed

### Hardware Interfaces

**HardwareManager** (`lib/aio_config/`)
- Manages hardware initialization and pin assignments
- Provides buzzer control and hardware status reporting

**I2CManager** (`lib/aio_config/`)
- Manages I2C bus for PCA9685 PWM controller and other devices
- Handles bus initialization and error recovery

**CANManager** (`lib/aio_communications/`)
- Manages FlexCAN interfaces for motor control and implement communication
- Auto-detects connected CAN devices like Keya steering motors

**LEDManager** (`lib/aio_system/`)
- Controls PCA9685-RGB LEDs for front panel status indication
- Shows GPS fix quality, steering status, and network state

**ADProcessor** (`lib/aio_system/`)
- Reads analog inputs for steering angle sensor (WAS)
- Provides filtering and calibration for analog signals

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

**TM172AiOParser** (`lib/aio_navigation/`)
- Interfaces with TM171 IMU
- Simplified binary protocol for heading and motion data

## Data Flow

1. **GPS/IMU Data** → GNSSProcessor/IMUProcessor → NAVProcessor → PGN messages → UDP to AgOpenGPS
2. **Steering Commands** ← UDP from AgOpenGPS → PGN messages → AutosteerProcessor → Motor Driver
3. **Section Control** ← UDP from AgOpenGPS → PGN messages → MachineProcessor → PCA9685 → Sections
4. **Configuration** ← Web Interface/Serial Menu → ConfigManager → EEPROM

## Key Design Patterns

- **Singleton Pattern**: Used for managers and processors that need global access
- **Factory Pattern**: Motor drivers created based on detected hardware
- **Observer Pattern**: PGN callback system for message routing
- **State Machine**: Network connection and protocol handling

## Building and Development

The project uses PlatformIO with the Teensy 4.1 platform. Key directories:
- `lib/` - All modular components
- `src/` - Main application entry point
- `docs/` - Documentation
- `testing/` - Test utilities and examples

## Network Protocol

Uses AgOpenGPS PGN protocol over UDP port 8888:
- Receives control commands from AgOpenGPS
- Sends position, heading, and status data
- Supports module discovery via subnet scanning
