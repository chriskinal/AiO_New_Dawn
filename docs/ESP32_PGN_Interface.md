# ESP32 Communication Using PGN Format

## Overview

Instead of creating a new protocol for ESP32-Teensy communication, we can reuse the existing AgOpenGPS PGN format. This provides consistency, reduces code complexity, and allows easy forwarding of PGNs between interfaces.

## PGN Format Structure

The existing PGN format used by AgOpenGPS:

```
[0x80][0x81][Source][PGN][Length][Data...][CRC]
```

- **Header**: `0x80 0x81` (128, 129) - Fixed header bytes
- **Source**: Source address (0-255)
- **PGN**: Message ID (0-255)
- **Length**: Data payload length (excluding header/CRC)
- **Data**: Variable length payload
- **CRC**: Simple additive checksum from Source byte to end of Data

### CRC Calculation
```cpp
uint8_t crc = 0;
for (int i = 2; i < totalLength - 1; i++) {
    crc += buffer[i];
}
```

## Proposed ESP32 Source Addresses

Reserve source addresses for ESP32 communication:
- `0xE0` (224): ESP32 Module
- `0xE1` (225): ESP32 Sensors
- `0xE2` (226): ESP32 WiFi/Network status
- `0xE3` (227): ESP32 Section Control

## Extended PGN Assignments for ESP32

### ESP32 → Teensy PGNs (50-69 range)
```cpp
#define PGN_ESP32_HELLO         50   // ESP32 announces presence
#define PGN_ESP32_SUBSCRIBE     51   // ESP32 requests data
#define PGN_ESP32_TO_AGIO       52   // Data to forward to AgIO
#define PGN_ESP32_STATUS        53   // ESP32 status report
#define PGN_ESP32_SENSOR_DATA   54   // Additional sensor data
#define PGN_ESP32_SECTION_CMD   55   // Section control commands
#define PGN_ESP32_WIFI_STATUS   56   // WiFi connection status
#define PGN_ESP32_REMOTE_CMD    57   // Remote control commands
#define PGN_ESP32_CONFIG_REQ    58   // Request configuration
#define PGN_ESP32_CONFIG_DATA   59   // Configuration data
// Reserve 60-69 for future ESP32 use
```

### Teensy → ESP32 PGNs
- Use existing PGNs (253, 254, etc.) - no changes needed
- ESP32 can subscribe to specific PGNs

## Implementation Examples

### ESP32 Hello Message
```cpp
struct ESP32HelloPGN {
    uint8_t version;        // Protocol version
    uint8_t capabilities;   // Capability flags
    uint16_t subscriptions; // Desired PGN mask
} __attribute__((packed));

void ESP32::announcePresence() {
    ESP32HelloPGN hello = {
        .version = 1,
        .capabilities = CAP_SECTION_CTRL | CAP_WIFI_STATUS,
        .subscriptions = SUB_MACHINE_STATUS | SUB_PGN_254
    };
    
    sendPGN(PGN_ESP32_HELLO, 0xE0, &hello, sizeof(hello));
}

void ESP32::sendPGN(uint8_t pgn, uint8_t source, const void* data, uint8_t len) {
    uint8_t buffer[256];
    buffer[0] = 0x80;
    buffer[1] = 0x81;
    buffer[2] = source;
    buffer[3] = pgn;
    buffer[4] = len;
    memcpy(&buffer[5], data, len);
    
    // Calculate CRC
    uint8_t crc = 0;
    for (int i = 2; i < 5 + len; i++) {
        crc += buffer[i];
    }
    buffer[5 + len] = crc;
    
    Serial.write(buffer, 6 + len);
}
```

### Teensy LittleDawnInterface Updates
```cpp
void LittleDawnInterface::processSerialData() {
    while (SerialESP32.available()) {
        uint8_t byte = SerialESP32.read();
        
        if (rxState == WAIT_HEADER1) {
            if (byte == 0x80) {
                rxState = WAIT_HEADER2;
            }
        }
        else if (rxState == WAIT_HEADER2) {
            if (byte == 0x81) {
                rxState = READ_SOURCE;
            } else {
                rxState = WAIT_HEADER1;
            }
        }
        else if (rxState == READ_SOURCE) {
            rxSource = byte;
            rxState = READ_PGN;
        }
        else if (rxState == READ_PGN) {
            rxPGN = byte;
            rxState = READ_LENGTH;
        }
        else if (rxState == READ_LENGTH) {
            rxLength = byte;
            rxIndex = 0;
            rxCRC = rxSource + rxPGN + rxLength;
            
            if (rxLength > 0) {
                rxState = READ_DATA;
            } else {
                rxState = READ_CRC;
            }
        }
        else if (rxState == READ_DATA) {
            rxBuffer[rxIndex++] = byte;
            rxCRC += byte;
            
            if (rxIndex >= rxLength) {
                rxState = READ_CRC;
            }
        }
        else if (rxState == READ_CRC) {
            if (byte == rxCRC) {
                processPGNFromESP32(rxPGN, rxSource, rxBuffer, rxLength);
            } else {
                LOG_WARNING("ESP32 PGN %d CRC error: calc=%02X recv=%02X", 
                           rxPGN, rxCRC, byte);
            }
            rxState = WAIT_HEADER1;
        }
    }
}

void LittleDawnInterface::processPGNFromESP32(uint8_t pgn, uint8_t source, 
                                              const uint8_t* data, uint8_t len) {
    switch (pgn) {
        case 50: // PGN_ESP32_HELLO
            ESP32HelloPGN* hello = (ESP32HelloPGN*)data;
            esp32Subscriptions = hello->subscriptions;
            esp32Capabilities = hello->capabilities;
            littleDawnDetected = true;
            lastResponseTime = millis();
            
            LOG_INFO("ESP32 connected: ver=%d cap=0x%02X sub=0x%04X",
                     hello->version, hello->capabilities, hello->subscriptions);
            break;
        }
        
        case 52: // PGN_ESP32_TO_AGIO
            // Forward raw data to AgIO (e.g., NMEA sentences)
            sendUDPbytes(data, len);
            break;
        
        case 55: // PGN_ESP32_SECTION_CMD
            // Handle section control commands
            if (sectionControlCallback) {
                sectionControlCallback(data, len);
            }
            break;
        }
    }
}
```

### Forwarding PGNs to ESP32
```cpp
void LittleDawnInterface::forwardPGNToESP32(uint8_t pgn, const uint8_t* data, size_t len) {
    // Check if ESP32 subscribed to this PGN
    if (!shouldForwardPGN(pgn)) return;
    
    // Build complete PGN message
    uint8_t buffer[256];
    buffer[0] = 0x80;
    buffer[1] = 0x81;
    buffer[2] = data[0];  // Preserve original source
    buffer[3] = pgn;
    buffer[4] = len - 3;  // Subtract header bytes
    
    // Copy payload (skip the 3 header bytes from original)
    memcpy(&buffer[5], &data[3], len - 3);
    
    // Calculate CRC
    uint8_t crc = 0;
    for (int i = 2; i < 5 + len - 3; i++) {
        crc += buffer[i];
    }
    buffer[5 + len - 3] = crc;
    
    // Send to ESP32
    SerialESP32.write(buffer, 6 + len - 3);
}
```

## Benefits of Using PGN Format

1. **Consistency**: Same format throughout the system
2. **Code Reuse**: CRC calculation, parsing logic already exists
3. **Easy Forwarding**: PGNs can be forwarded without translation
4. **Extensible**: 256 PGN IDs available, plenty of room
5. **Debugging**: Same tools can decode ESP32 and AgIO traffic
6. **Checksum**: Built-in error detection

## Migration Path

1. Update LittleDawnInterface to parse PGN format
2. Add PGN forwarding based on subscriptions  
3. Implement ESP32 PGN handlers (50-69 range)
4. Remove old binary protocol code
5. Test with example ESP32 sketches

## PGN Range Summary

- **1-49**: Reserved for future use
- **50-69**: ESP32 ↔ Teensy communication
- **70-99**: Available for expansion
- **100-199**: Various AgOpenGPS PGNs
- **200-255**: AgOpenGPS primary PGNs

Using the 50-69 range keeps ESP32 communication clearly separated from AgOpenGPS traffic while leaving room for growth.

## Example ESP32 Sketch

```cpp
#include <Arduino.h>

// PGN definitions
#define PGN_ESP32_HELLO 50
#define PGN_STEER_DATA  253

// State machine
enum RxState {
    WAIT_HEADER1,
    WAIT_HEADER2,
    READ_SOURCE,
    READ_PGN,
    READ_LENGTH,
    READ_DATA,
    READ_CRC
};

RxState rxState = WAIT_HEADER1;
uint8_t rxBuffer[256];
uint8_t rxIndex;
uint8_t rxPGN, rxSource, rxLength, rxCRC;

void setup() {
    Serial.begin(115200);  // To Teensy
    
    // Announce presence
    struct {
        uint8_t version;
        uint8_t capabilities;
        uint16_t subscriptions;
    } hello = {1, 0x01, 0x0002};  // Want PGN 253
    
    sendPGN(PGN_ESP32_HELLO, 0xE0, &hello, sizeof(hello));
}

void loop() {
    // Process incoming PGNs
    while (Serial.available()) {
        processPGNByte(Serial.read());
    }
}

void sendPGN(uint8_t pgn, uint8_t source, const void* data, uint8_t len) {
    uint8_t buffer[256];
    buffer[0] = 0x80;
    buffer[1] = 0x81;
    buffer[2] = source;
    buffer[3] = pgn;
    buffer[4] = len;
    memcpy(&buffer[5], data, len);
    
    uint8_t crc = 0;
    for (int i = 2; i < 5 + len; i++) {
        crc += buffer[i];
    }
    buffer[5 + len] = crc;
    
    Serial.write(buffer, 6 + len);
}
```

## Conclusion

Using the existing PGN format for ESP32 communication provides a clean, consistent interface that integrates seamlessly with the existing AgOpenGPS ecosystem. The format is proven, simple, and efficient.