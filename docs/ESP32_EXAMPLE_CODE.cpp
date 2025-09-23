// ESP32_BRIDGE_EXAMPLE.cpp
// Example ESP32 code for New Dawn Serial-to-WiFi Bridge
// This demonstrates the basic structure needed to communicate with New Dawn

#include <Arduino.h>

// PGN Message structure
struct PGNMessage {
    uint8_t header1 = 0x80;
    uint8_t header2 = 0x81;
    uint8_t source;
    uint8_t pgn;
    uint8_t length;
    uint8_t data[250];  // Max data size
    uint8_t crc;
};

// Receive buffer
uint8_t rxBuffer[512];
size_t rxIndex = 0;

// Timing
unsigned long lastHelloTime = 0;
unsigned long lastStatusTime = 0;

// Connection state
bool teensynDetected = false;

// Calculate CRC for PGN message
uint8_t calculateCRC(uint8_t* buffer, uint8_t length) {
    uint8_t crc = 0;
    for (int i = 0; i < length; i++) {
        crc ^= buffer[i];
    }
    return crc;
}

// Send a complete PGN message
void sendPGN(uint8_t source, uint8_t pgn, uint8_t* data, uint8_t dataLen) {
    uint8_t buffer[256];
    buffer[0] = 0x80;
    buffer[1] = 0x81;
    buffer[2] = source;
    buffer[3] = pgn;
    buffer[4] = dataLen;
    
    if (dataLen > 0) {
        memcpy(&buffer[5], data, dataLen);
    }
    
    uint8_t crc = calculateCRC(buffer, 5 + dataLen);
    buffer[5 + dataLen] = crc;
    
    Serial.write(buffer, 6 + dataLen);
}

// Parse incoming PGN messages
void processIncomingData() {
    while (Serial.available()) {
        uint8_t byte = Serial.read();
        
        if (rxIndex < sizeof(rxBuffer)) {
            rxBuffer[rxIndex++] = byte;
        }
        
        // Look for PGN header
        if (rxIndex >= 7) {  // Minimum PGN size
            for (size_t i = 0; i <= rxIndex - 7; i++) {
                if (rxBuffer[i] == 0x80 && rxBuffer[i + 1] == 0x81) {
                    // Found header
                    uint8_t source = rxBuffer[i + 2];
                    uint8_t pgn = rxBuffer[i + 3];
                    uint8_t dataLen = rxBuffer[i + 4];
                    
                    // Check if we have complete message
                    size_t totalLen = 5 + dataLen + 1;  // header + data + crc
                    if (i + totalLen <= rxIndex) {
                        // Verify CRC
                        uint8_t calcCrc = calculateCRC(&rxBuffer[i], 5 + dataLen);
                        uint8_t rxCrc = rxBuffer[i + 5 + dataLen];
                        
                        if (calcCrc == rxCrc) {
                            // Valid PGN received
                            handlePGN(source, pgn, &rxBuffer[i + 5], dataLen);
                            teensynDetected = true;
                        }
                        
                        // Remove processed message from buffer
                        size_t remaining = rxIndex - (i + totalLen);
                        if (remaining > 0) {
                            memmove(rxBuffer, &rxBuffer[i + totalLen], remaining);
                        }
                        rxIndex = remaining;
                        break;
                    }
                }
            }
        }
        
        // Prevent buffer overflow
        if (rxIndex > sizeof(rxBuffer) - 100) {
            rxIndex = 0;  // Reset buffer
        }
    }
}

// Handle received PGN
void handlePGN(uint8_t source, uint8_t pgn, uint8_t* data, uint8_t length) {
    switch (pgn) {
        case 0xFC:  // 252 - Steer Settings
            // Process steer settings
            break;
            
        case 0xFD:  // 253 - Steer Data  
            // Process steer data
            break;
            
        case 0xFE:  // 254 - GPS Data from AgIO
            // Process GPS data
            break;
            
        case 0xEF:  // 239 - Machine Data
            // Process machine data
            break;
            
        case 0xEE:  // 238 - Machine Config
            // Process machine config
            break;
            
        default:
            // Unknown PGN
            break;
    }
}

// Example: Send a status PGN
void sendStatusPGN() {
    uint8_t statusData[8];
    
    // Example status data
    statusData[0] = 0x01;  // Status flags
    statusData[1] = 0x00;  // Reserved
    statusData[2] = 0x00;  // Reserved
    statusData[3] = 0x00;  // Reserved
    statusData[4] = 0x00;  // Reserved
    statusData[5] = 0x00;  // Reserved
    statusData[6] = 0x00;  // Reserved
    statusData[7] = 0x00;  // Reserved
    
    sendPGN(0x50, 0xFA, statusData, 8);  // Source 0x50, PGN 250
}

void setup() {
    // Initialize serial for Teensy communication
    Serial.begin(460800);
    
    // Small delay for serial to initialize
    delay(100);
    
    // Send initial hello
    Serial.print("ESP32-hello");
    lastHelloTime = millis();
}

void loop() {
    // Send periodic hello message
    if (millis() - lastHelloTime > 5000) {  // Every 5 seconds
        Serial.print("ESP32-hello");
        lastHelloTime = millis();
    }
    
    // Process incoming serial data
    processIncomingData();
    
    // Send periodic status if connected
    if (teensynDetected && millis() - lastStatusTime > 1000) {  // Every second
        sendStatusPGN();
        lastStatusTime = millis();
    }
    
    // Add your custom functionality here
    // For example:
    // - Read sensors and send as PGNs
    // - Control outputs based on received PGNs
    // - Bridge to other protocols (CAN, etc.)
}