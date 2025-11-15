# CAN Bus Testing Guide

This guide shows how to test the CAN implementations using a CAN sniffer/analyzer that can send and receive messages.

## Hardware Setup

1. Connect your CAN analyzer to the appropriate CAN bus pins on the Teensy
2. Ensure proper termination (120Ω resistors)
3. Match the baud rate (250k or 500k as configured)

## Testing Keya Motor

### 1. Configure AiO
- Brand: Generic
- Assign "Keya" function to your test bus
- Speed: 250 kbps (typical)

### 2. Send Heartbeat (Keya → AiO)
Send this message every 100ms to simulate Keya motor:
```
ID: 0x07000001 (Extended)
DLC: 8
Data: [POS_H, POS_L, SPD_H, SPD_L, CUR_H, CUR_L, ERR_H, ERR_L]

Example (motor at position 1000, speed 0, no current, no error):
0x07000001 EXT [03, E8, 00, 00, 00, 00, 00, 00]
```

### 3. Verify AiO Sends Commands
You should see these messages from AiO:

**Enable Command:**
```
ID: 0x06000001 (Extended)
Data: [23, 0D, 20, 01, 00, 00, 00, 00]
```

**Speed Command (alternating with enable):**
```
ID: 0x06000001 (Extended)
Data: [23, 00, 20, 01, XX, XX, XX, XX]
Where XX is the speed value
```

### 4. Test Steering
- Enable autosteer in AgOpenGPS
- You should see speed values change based on steering commands
- Speed = (PWM * 10), so full right (+255 PWM) = +1000 speed units

## Testing Valtra/Massey Ferguson

### 1. Configure AiO
- Brand: Valtra/Massey Ferguson
- Assign "V_Bus" function to your test bus
- Speed: 250 kbps

### 2. Send Steering Ready (Tractor → AiO)
Send this CONTINUOUSLY to indicate steering valve is ready:
```
ID: 0x0CAC1C13 (Extended)
DLC: 8
Data: [CRV_L, CRV_H, VALVE_STATE, XX, XX, XX, XX, XX]

Example (curve=0, valve ready):
0x0CAC1C13 EXT [00, 00, 01, 00, 00, 00, 00, 00]

IMPORTANT: Must be sent at least every 200ms or connection will timeout!
```

### 3. Verify AiO Sends Steering Commands
You should see continuous messages:
```
ID: 0x0CAD131C (Extended)
DLC: 8
Data: [CRV_L, CRV_H, INTENT, 00, 00, 00, 00, 00]

Where:
- CRV = Desired curve value (little-endian)
- INTENT = 252 (no steer intent) initially
- INTENT = 253 (steer intent) after valve ready received

Note: Messages are sent continuously regardless of valve ready state
```

### 4. Simulate Steering Feedback
Send curve position updates:
```
ID: 0x0CAC1C13 EXT [E8, 03, 01, 00, 00, 00, 00, 00]  # Curve = 1000
```

## Testing Other Brands

### Fendt (When Implemented)
```
Receive: 0x0CF02300 (Steer ready)
Send: 0x0CEFF02C (6-byte steering command)
```

### Generic Testing Script

For automated testing, you can create a script that:

1. **Sends periodic heartbeat/status messages**
```python
# Example for Valtra continuous valve ready
import can
import time

bus = can.interface.Bus(bustype='socketcan', channel='can0', bitrate=250000)

# Send valve ready every 100ms (must be < 200ms)
while True:
    msg = can.Message(
        arbitration_id=0x0CAC1C13,
        is_extended_id=True,
        data=[0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00]
    )
    bus.send(msg)
    time.sleep(0.1)  # 100ms
```

```python
# Example for Keya
import can
import time

bus = can.interface.Bus(bustype='socketcan', channel='can0', bitrate=250000)

# Send heartbeat every 100ms
while True:
    msg = can.Message(
        arbitration_id=0x07000001,
        is_extended_id=True,
        data=[0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00]
    )
    bus.send(msg)
    time.sleep(0.1)
```

2. **Monitors responses from AiO**
```python
# Listen for commands
for msg in bus:
    if msg.arbitration_id == 0x06000001:  # Keya command
        print(f"Received command: {msg.data.hex()}")
```

## Important Notes

### Kickout Behavior
- TRACTOR_CAN type (including Valtra/Massey) is treated like Keya - it handles its own kickout internally
- External sensors (encoder/pressure/current) are NOT monitored for CAN-based tractors
- If you experience unexpected kickouts, check:
  1. Turn sensor settings are disabled
  2. No external kickout sensors are enabled for CAN tractors

## Debugging Tips

### 1. Monitor CAN Traffic
- Use filters to show only relevant IDs
- Log timestamps to verify timing
- Check extended vs standard frame types

### 2. Common Issues
- **No heartbeat response**: Check bus assignment and speed
- **No steering commands**: Verify AgOpenGPS is sending data
- **Wrong curve scaling**: Adjust scaling factors in TractorCANDriver.cpp

### 3. Testing Without AgOpenGPS
You can manually enable steering by:
1. Send UDP packet to port 8888 with PGN 254 (steer enable)
2. Send PGN 253 with steering angle data

## CAN Message Reference

### Standard IDs (11-bit)
- Most standard IDs are in range 0x000-0x7FF

### Extended IDs (29-bit)
- Keya: 0x06000001, 0x07000001
- Valtra: 0x0CAC1C13, 0x0CAD131C
- Fendt: 0x0CF02300, 0x0CEFF02C
- Various engage messages: 0x18XXXXXX

### Message Timing
- Keya heartbeat: 100ms max gap
- Steering commands: Continuous when active
- Timeout detection: 200ms typical

## Useful CAN Analyzer Features

1. **Message Generator**: Create periodic messages
2. **Trigger/Response**: Auto-reply to specific messages
3. **Logging**: Record all traffic for analysis
4. **Filtering**: Focus on specific IDs
5. **Error Injection**: Test timeout handling

## Example Test Sequence

```
# 1. Start CAN logger
# 2. Power up AiO with Keya configured
# 3. Send Keya heartbeat
→ 0x07000001 EXT [00, 00, 00, 00, 00, 00, 00, 00]
# 4. Verify enable command received
← 0x06000001 EXT [23, 0D, 20, 01, 00, 00, 00, 00]
# 5. Continue heartbeat
→ 0x07000001 EXT [00, 00, 00, 00, 00, 00, 00, 00]
# 6. Verify speed command
← 0x06000001 EXT [23, 00, 20, 01, 00, 00, 00, 00]
```

This allows you to verify the CAN implementation without needing actual tractor hardware.