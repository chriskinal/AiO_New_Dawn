# Communication and Networking

This document describes the communication interfaces and networking capabilities of AiO New Dawn.

## Overview

AiO New Dawn implements multiple communication protocols to interface with AgOpenGPS, sensors, actuators, and user interfaces. The system is built around reliable UDP communication with support for serial, CAN, I2C, and web interfaces.

## Network Communication

### Ethernet Configuration

The system uses QNEthernet library for robust network communication:

- **Default IP**: 192.168.5.126 (Steer module)
- **UDP Port**: 8888
- **Subnet**: 192.168.5.0/24
- **DHCP Support**: Available but static IP recommended

### UDP Communication

**QNEthernetUDPHandler** manages all UDP traffic:

```cpp
// Incoming PGN flow
AgOpenGPS → UDP Port 8888 → QNEthernetUDPHandler → PGNProcessor → Handlers

// Outgoing PGN flow  
Processors → PGN Messages → QNEthernetUDPHandler → UDP → AgOpenGPS
```

Key features:
- Non-blocking operation
- Automatic packet parsing
- PGN routing to registered handlers
- Configurable send rates

### PGN Protocol

Parameter Group Numbers (PGNs) define message types:

#### Incoming PGNs (from AgOpenGPS)
- **PGN 200**: Module scan request
- **PGN 201**: Subnet scan request  
- **PGN 211**: GPS configuration
- **PGN 239**: Section control commands
- **PGN 254**: Autosteer commands
- **PGN 192**: IMU configuration
- **PGN 197**: Navigation configuration

#### Outgoing PGNs (to AgOpenGPS)
- **PGN 202**: Module hello/identification
- **PGN 203**: IMU heading and roll
- **PGN 210**: GPS position data
- **PGN 250**: Machine sensor data (10Hz)
- **PGN 253**: Autosteer status (100Hz)

### Network Initialization

```cpp
// Startup sequence
1. Initialize Ethernet hardware
2. Configure IP (static or DHCP)
3. Start UDP listener on port 8888
4. Begin subnet scanning
5. Register PGN handlers
```

## Serial Communication

### SerialManager

Centralized management of multiple serial ports:

```cpp
SerialManager::getInstance()
    ├── Serial5 (GPS1)  - 460800 baud
    ├── Serial8 (GPS2)  - 460800 baud
    ├── Serial3 (Radio) - 115200 baud
    ├── Serial4 (IMU)   - 115200 baud
    ├── Serial7 (RS232) - 38400 baud
    ├── Serial2 (ESP32) - 460800 baud
    └── Serial1 (Keya)  - 115200 baud
```

### GPS/GNSS Communication

Supports multiple protocols:
- **NMEA 0183**: Standard GPS sentences
- **UBX**: u-blox binary protocol
- **RTCM**: RTK correction data

Features:
- Automatic protocol detection
- Dual GPS with failover
- Message filtering
- Configurable update rates

### IMU Communication

Supports multiple IMU types:

**BNO08x (RVC Mode)**:
- Binary protocol at 115200 baud
- 100Hz update rate
- Heading, roll, and pitch data

**TM171**:
- Alternative IMU protocol
- Similar data format

**UM981 INS**:
- Integrated into GPS module
- Combined position/attitude data

### RTCM Processing

RTK correction data routing:
```cpp
Network/Serial → RTCMProcessor → GPS Receivers
```

Features:
- Multiple input sources
- Automatic routing
- Message validation
- Visual feedback (blue LED pulse)

## CAN Communication

### CANManager

FlexCAN interface for motor control:

```cpp
CANManager::getInstance()
    └── CAN3
        ├── Keya Motor Detection
        ├── Command Messages (0x6000001)
        ├── Status Messages (0x7000001)
        └── Heartbeat Monitoring
```

### Keya Motor Protocol

CAN message structure:
- **Command ID**: 0x6000001
- **Status ID**: 0x7000001  
- **Heartbeat**: 500ms timeout
- **Baud Rate**: 250kbps

## I2C Communication

### I2CManager

Thread-safe I2C bus management:

```cpp
I2CManager::getInstance()
    ├── PCA9685 (0x44) - Section Control
    ├── PCA9685 (0x70) - LED Control
    └── BNO08x (0x4A/0x4B) - IMU (optional)
```

Features:
- 1MHz bus speed
- Mutex protection
- Error recovery
- Device scanning

### PCA9685 Control

16-channel PWM controllers:

**Address 0x44 (Sections)**:
- Outputs 1-16: Section control
- Outputs 5-6: Protected for Danfoss
- Outputs 7-8: Danfoss valve control

**Address 0x70 (LEDs)**:
- RGB LED control
- 120Hz PWM frequency
- 4096-step resolution

## Web Communication

### HTTP Server

Lightweight custom implementation:

```cpp
SimpleWebManager (Port 80)
    ├── Static Pages
    ├── API Endpoints
    ├── WebSocket Server
    └── OTA Updates
```

### WebSocket Protocol

Real-time bidirectional communication:

**TelemetryWebSocket**:
- JSON message format
- Event-based updates
- Configurable data streams
- Low latency

Message types:
```json
// Status update
{
  "type": "status",
  "gps": { "lat": 51.123, "lon": -114.456 },
  "steer": { "angle": 15.5, "enabled": true }
}

// Configuration change
{
  "type": "config",
  "param": "p_gain",
  "value": 120
}
```

### RESTful API

Configuration endpoints:
- `GET /api/status` - System status
- `GET /api/config` - Current configuration
- `POST /api/config` - Update settings
- `POST /api/reboot` - System restart

## Communication Patterns

### Event-Driven Architecture

```cpp
// PGN callback registration
PGNProcessor::registerCallback(254, [](uint8_t* data, uint8_t len) {
    // Handle autosteer command
});

// Broadcast callbacks for monitoring
PGNProcessor::registerBroadcastCallback([](uint16_t pgn, uint8_t* data) {
    // Log all PGN traffic
});
```

### Message Queuing

No explicit queuing - messages processed immediately:
- Reduces latency
- Simplifies architecture
- Prevents buffer overflow

### Error Handling

Robust error recovery:
- Automatic reconnection
- Timeout detection
- Invalid message filtering
- Error logging

## Data Flow Examples

### GPS Position to AgOpenGPS
```
GPS Module → NMEA/UBX → Serial2 → GNSSProcessor 
    → PGN 210 → QNEthernetUDPHandler → AgOpenGPS
```

### Autosteer Command from AgOpenGPS
```
AgOpenGPS → UDP → QNEthernetUDPHandler → PGN 254 
    → AutosteerProcessor → Motor Driver → Steering Motor
```

### Section Control
```
AgOpenGPS → PGN 239 → MachineProcessor 
    → I2CManager → PCA9685 → Section Outputs
```

## Performance Considerations

### Network Optimization
- Static IP reduces DHCP overhead
- UDP for low latency
- Minimal packet size
- Hardware checksum offload

### Serial Optimization
- DMA transfers where possible
- Ring buffers for data
- Selective message parsing
- Baud rate optimization

### Bus Utilization
- I2C at 1MHz for speed
- CAN message filtering
- Minimal polling overhead
- Interrupt-driven reception

## Debugging Tools

### Network Debugging
- Wireshark for packet capture
- Built-in packet logging
- UDP echo server mode
- Network status LEDs

### Serial Debugging
- Serial bridge mode
- Message logging
- Protocol analyzers
- Baud rate detection

### CAN Debugging
- CAN message logging
- Bus load monitoring
- Error frame detection
- Keya status display

## Best Practices

### Network Configuration
1. Use static IP for reliability
2. Ensure unique module IDs
3. Monitor connection status
4. Handle network interruptions

### Serial Communication
1. Use appropriate baud rates
2. Implement timeout handling
3. Validate message checksums
4. Buffer data appropriately

### Protocol Implementation
1. Follow PGN specifications exactly
2. Validate all incoming data
3. Use appropriate send rates
4. Log communication errors

## Troubleshooting

### Network Issues
- **No UDP packets**: Check IP configuration
- **Intermittent connection**: Check cable/switch
- **Wrong subnet**: Verify network settings

### Serial Issues
- **No GPS data**: Check baud rate and connections
- **Corrupted data**: Verify ground connection
- **Missing messages**: Check message filtering

### CAN Issues
- **No Keya detection**: Verify termination resistors
- **Bus errors**: Check cable quality
- **Timeout errors**: Monitor bus load