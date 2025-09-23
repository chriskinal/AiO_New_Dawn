# ESP32 Serial-to-WiFi Bridge Interface Specification

## Overview
The ESP32 bridge provides a transparent serial-to-WiFi relay for AgOpenGPS PGN messages between the Teensy 4.1 (New Dawn) and the AgOpenGPS network. The ESP32 connects to the Teensy via Serial2 at 460800 baud.

## Hardware Connection
- **Serial Interface**: UART Serial2 on Teensy (TX2/RX2)
- **Baud Rate**: 460800
- **Logic Level**: 3.3V (compatible with ESP32)
- **ESP32 Module**: XIAO ESP32C3 or similar in the AiO board slot

## Communication Protocol

### 1. ESP32 Announcement
The ESP32 must announce its presence by sending:
```
ESP32-hello
```
- Send this message periodically (every 1-2 seconds) until acknowledged
- Once the Teensy detects this message, it will start forwarding PGNs
- Continue sending hello messages every 5 seconds to maintain connection
- The message is plain ASCII text, no line ending required

### 2. PGN Message Format
All PGN messages use the standard AgOpenGPS format:
```
[0x80][0x81][Source][PGN][Length][Data...][CRC]
```

Where:
- `0x80, 0x81` - Fixed header bytes (decimal 128, 129)
- `Source` - Source address (1 byte)
- `PGN` - Parameter Group Number (1 byte)
- `Length` - Number of data bytes (1 byte)
- `Data` - Variable length data (Length bytes)
- `CRC` - Checksum (1 byte) - XOR of all bytes from 0x80 to last data byte

### 3. Data Flow

#### Teensy → ESP32 (Network to Serial)
- Teensy receives PGN messages on UDP port 8888
- Raw PGN bytes are forwarded to ESP32 via serial
- No modification or wrapping - exact bytes as received from network
- ESP32 should parse and process these PGNs as needed

#### ESP32 → Teensy (Serial to Network)
- ESP32 creates complete PGN messages with proper format and CRC
- Send raw PGN bytes to Teensy via serial
- Teensy will broadcast these on UDP port 9999
- Multiple PGNs can be sent back-to-back

## Example PGN Messages

### Example 1: Steer Module Heartbeat (PGN 239)
```
80 81 7F EF 05 00 00 00 00 00 5B
```
- Header: 0x80, 0x81
- Source: 0x7F (127)
- PGN: 0xEF (239)
- Length: 0x05 (5 bytes)
- Data: 0x00, 0x00, 0x00, 0x00, 0x00
- CRC: 0x5B

### Example 2: GPS Data (PGN 211)
```
80 81 79 D3 08 4C 04 E7 03 00 00 00 00 1A
```
- Header: 0x80, 0x81
- Source: 0x79 (121)
- PGN: 0xD3 (211)
- Length: 0x08 (8 bytes)
- Data: 8 bytes of GPS/IMU data
- CRC: 0x1A

## ESP32 Implementation Requirements

### Serial Setup
```cpp
void setup() {
    Serial.begin(460800);  // Communication with Teensy
    // Send hello message
    Serial.print("ESP32-hello");
}
```

### Main Loop Structure
```cpp
void loop() {
    // Send periodic hello
    static unsigned long lastHello = 0;
    if (millis() - lastHello > 5000) {
        Serial.print("ESP32-hello");
        lastHello = millis();
    }
    
    // Process incoming serial data (PGNs from network)
    if (Serial.available()) {
        // Parse PGN messages
        // Process based on PGN ID
    }
    
    // Send outgoing PGNs
    if (havePGNToSend) {
        // Build complete PGN with CRC
        // Serial.write(pgnBuffer, pgnLength);
    }
}
```

### CRC Calculation
```cpp
uint8_t calculateCRC(uint8_t* buffer, uint8_t length) {
    uint8_t crc = 0;
    for (int i = 0; i < length; i++) {
        crc ^= buffer[i];
    }
    return crc;
}
```

### Building a PGN Message
```cpp
void sendPGN(uint8_t source, uint8_t pgn, uint8_t* data, uint8_t dataLen) {
    uint8_t buffer[256];
    buffer[0] = 0x80;
    buffer[1] = 0x81;
    buffer[2] = source;
    buffer[3] = pgn;
    buffer[4] = dataLen;
    
    memcpy(&buffer[5], data, dataLen);
    
    uint8_t crc = calculateCRC(buffer, 5 + dataLen);
    buffer[5 + dataLen] = crc;
    
    Serial.write(buffer, 6 + dataLen);
}
```

## Common PGN IDs

### Incoming (from AgOpenGPS)
- **PGN 252 (0xFC)** - Steer Settings
- **PGN 253 (0xFD)** - Steer Data
- **PGN 254 (0xFE)** - GPS Data from AgIO
- **PGN 239 (0xEF)** - Machine Data
- **PGN 238 (0xEE)** - Machine Config
- **PGN 236 (0xEC)** - Machine Customization

### Outgoing (to AgOpenGPS)
- **PGN 253 (0xFD)** - Steer Module Status
- **PGN 250 (0xFA)** - Steer Module Sensors
- **PGN 211 (0xD3)** - IMU Data
- **PGN 203 (0xCB)** - Subnet Response

## Testing

1. **Basic Connection Test**:
   - ESP32 sends "ESP32-hello" every second
   - Monitor Teensy serial output for "ESP32 detected and connected"

2. **PGN Relay Test**:
   - Send a test PGN from ESP32: `80 81 50 FA 02 01 02 58`
   - This should appear on UDP port 9999

3. **Network Tools**:
   - Use Wireshark to monitor UDP traffic on ports 8888 and 9999
   - AgIO's "Serial Monitor" can show PGN traffic

## Important Notes

1. **No Line Endings**: Don't add CR/LF to messages - send raw bytes only
2. **Binary Data**: This is a binary protocol, not text-based
3. **Timing**: The Teensy processes serial data in its main loop, so there's no strict timing requirement
4. **Buffer Size**: Keep individual PGN messages under 256 bytes total
5. **Error Handling**: Invalid PGNs are silently dropped by the Teensy

## Network Configuration
- The Teensy uses its configured broadcast address (typically 192.168.x.255)
- ESP32 PGNs are broadcast to all devices on the network via UDP 9999
- The ESP32 doesn't need to know network details - the Teensy handles all UDP communication