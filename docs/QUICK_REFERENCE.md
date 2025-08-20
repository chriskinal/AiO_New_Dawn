# AiO New Dawn Quick Reference

## System Information
- **Platform**: Teensy 4.1 / Little Dawn
- **Network**: 192.168.5.126:8888 (UDP)
- **Web Interface**: http://192.168.5.126
- **Version**: See `lib/aio_system/Version.h`

## Build Commands
```bash
# Build firmware
~/.platformio/penv/bin/pio run -e teensy41

# Upload to device
~/.platformio/penv/bin/pio run -e teensy41 --target upload

# Clean build
~/.platformio/penv/bin/pio run -e teensy41 --target clean
```

## Serial Menu
Press `?` for interactive menu:
- `1` - Toggle serial output
- `2` - Toggle UDP syslog
- `3/4` - Decrease/Increase serial level
- `5/6` - Decrease/Increase UDP level
- `7` - Toggle rate limiting
- `T` - Generate test messages
- `S` - Show statistics
- `R` - Reset event counter
- `L` - Toggle loop timing diagnostics

## LED Status Guide

| LED | Red | Yellow | Green | Blue Pulse |
|-----|-----|--------|-------|------------|
| PWR/ETH | No Ethernet | Connected, no AgIO | AgIO Active | - |
| GPS | No Data | GPS Fix | RTK Fixed | RTCM Data |
| STEER | HW Error | Ready | Engaged | Button Press |
| INS | No IMU | Aligning | Valid Data | - |

## PGN Reference

### Incoming (from AgOpenGPS)
- **200**: Module scan request
- **211**: GPS configuration  
- **239**: Section control
- **254**: Autosteer command
- **192**: IMU configuration

### Outgoing (to AgOpenGPS)
- **202**: Module identification
- **203**: IMU data (heading/roll)
- **210**: GPS position
- **250**: Machine sensors (10Hz)
- **253**: Autosteer status (100Hz)

## Pin Assignments (Teensy 4.1)

### Motor Control
- **PWM**: Pin 4
- **Direction**: Pin 5
- **Enable**: Pin 6
- **Sleep**: Pin 7

### Sensors
- **WAS**: A0 (pin 14)
- **Current**: A1 (pin 15)
- **Work Switch**: A2 (pin 16)
- **Pressure**: A3 (pin 17)
- **Remote**: Pin 36
- **Steer Button**: Pin 32

### Communication
- **GPS1 Serial**: Serial5
- **GPS2 Serial**: Serial8
- **Radio Serial**: Serial3
- **IMU Serial**: Serial4
- **RS232**: Serial7
- **ESP32**: Serial2
- **Keya Serial**: Serial1 (when not using CAN)
- **CAN**: CAN3

### I2C Devices
- **0x44**: PCA9685 (Sections)
- **0x70**: PCA9685 (LEDs)
- **0x4A/0x4B**: BNO08x IMU

## Motor Types
1. **Cytron MD30C** - PWM + Direction
2. **IBT-2** - PWM + Dir + Enable
3. **DRV8701** - PWM + Dir + Enable + Sleep
4. **Keya CAN** - CAN bus control
5. **Danfoss** - Hydraulic valve (PCA9685)

## Configuration Files
- **EEPROM Layout**: `lib/aio_config/EEPROMLayout.h`
- **Pin Definitions**: `lib/aio_config/HardwareManager.h`
- **Network Settings**: Via web interface
- **PGN Routing**: `lib/aio_system/PGNProcessor.cpp`

## Common Issues & Solutions

### No Ethernet Connection
- Check cable and LED status
- Verify IP configuration
- Try DHCP mode

### No GPS Data
- Check baud rate (460800)
- Verify antenna connection
- Monitor Serial2 output

### Autosteer Not Working
- Calibrate WAS sensor
- Check motor connections
- Verify PWM output
- Monitor current draw

### CAN Motor Not Detected
- Check termination resistors
- Verify CAN3 connections
- Monitor CAN traffic

## WebSocket Messages

### Subscribe to Telemetry
```json
{
  "type": "subscribe",
  "stream": "telemetry"
}
```

### Update Configuration
```json
{
  "type": "config",
  "data": {
    "parameter": "p_gain",
    "value": 120
  }
}
```

## Safety Features
- **Kickout Monitor**: Current/encoder limits
- **Watchdog Timer**: Communication timeout
- **Button Override**: Manual disengage
- **LED Feedback**: Visual status

## Performance Metrics
- **Autosteer Loop**: 100Hz (10ms)
- **Sensor Updates**: 10Hz (100ms)
- **Network Latency**: <5ms typical
- **Web Response**: <50ms

## Debugging Tools
- **Event Logger**: Serial/UDP/WebSocket
- **Packet Monitor**: PGN traffic log
- **Hardware Status**: Pin ownership map
- **CAN Monitor**: Message logging

## File Structure
```
AiO_New_Dawn/
├── lib/
│   ├── aio_autosteer/     # Steering control
│   ├── aio_communications/ # Serial/CAN/I2C
│   ├── aio_config/        # Configuration
│   ├── aio_navigation/    # GPS/IMU
│   └── aio_system/        # Core services
├── src/                   # Main application
├── docs/                  # Documentation
└── data/                  # Web resources
```

## Version History
- **1.0.0-beta**: Current release
- See CHANGELOG for details
- Git tags for releases

## Support Resources
- **Documentation**: `/docs` folder
- **Knowledge Base**: `/knowledge` folder  
- **Web Interface**: Built-in help
- **Serial Menu**: Interactive debugging