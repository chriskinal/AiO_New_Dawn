#include "ESP32Interface.h"
#include "SerialManager.h"
#include "QNEthernetUDPHandler.h"
#include "EventLogger.h"

// Global instance
ESP32Interface esp32Interface;

// Initialize the interface
void ESP32Interface::init() {
    // SerialESP32 is already initialized by SerialManager at 460800 baud
    LOG_INFO(EventSource::SYSTEM, "ESP32 interface initialized on Serial2 (460800 baud)");
    // Already logged with LOG_INFO above
    
    // Clear receive buffer
    rxBufferIndex = 0;
    memset(rxBuffer, 0, RX_BUFFER_SIZE);
}

// Main processing loop - called from main.cpp
void ESP32Interface::process() {
    // Debug: Show we're running
    static uint32_t lastDebugTime = 0;
    if (millis() - lastDebugTime > 10000) {  // Every 10 seconds
        LOG_DEBUG(EventSource::SYSTEM, "ESP32Interface: Running, detected=%s, Serial2 available=%d", 
                      esp32Detected ? "YES" : "NO", SerialESP32.available());
        lastDebugTime = millis();
    }
    
    // Process any incoming serial data
    processIncomingData();
    
    // Check for timeout - if we haven't heard hello in a while
    if (esp32Detected && (millis() - lastHelloTime > HELLO_TIMEOUT_MS)) {
        esp32Detected = false;
        LOG_WARNING(EventSource::SYSTEM, "ESP32 connection lost (hello timeout)");
        LOG_DEBUG(EventSource::SYSTEM, "ESP32 hello timeout - last hello was %lu ms ago", 
                      millis() - lastHelloTime);
    }
}

// Send data to ESP32 (called by UDP handler)
void ESP32Interface::sendToESP32(const uint8_t* data, size_t length) {
    if (!esp32Detected) {
        return;  // Don't send if ESP32 not detected
    }
    
    // Log first few PGNs for debugging
    static int pgnCount = 0;
    if (pgnCount < 20 && length >= 5) {
        // Calculate CRC for debug logging
        uint8_t calcCrc = 0;
        if (length >= 6) {
            uint16_t crcSum = 0;
            for (size_t i = 2; i < length - 1; i++) {
                crcSum += data[i];
            }
            calcCrc = (uint8_t)(crcSum & 0xFF);
        }
        LOG_DEBUG(EventSource::SYSTEM, "ESP32 TX: PGN=%d, len=%zu, CRC: calc=%02X pkt=%02X", 
                  data[3], length, calcCrc, data[length-1]);
        pgnCount++;
    }
    
    // Send raw bytes to ESP32
    SerialESP32.write(data, length);
}

// Process incoming data from ESP32
void ESP32Interface::processIncomingData() {
    static bool firstByte = true;
    while (SerialESP32.available()) {
        uint8_t byte = SerialESP32.read();
        
        // Debug: Log first few bytes received
        if (firstByte) {
            LOG_DEBUG(EventSource::SYSTEM, "ESP32Interface: Receiving data from ESP32!");
            firstByte = false;
        }
        
        // Add to buffer
        if (rxBufferIndex < RX_BUFFER_SIZE) {
            rxBuffer[rxBufferIndex++] = byte;
        }
        
        // Check for hello message
        checkForHello();
        
        // Check for complete PGN message
        // PGN format: [0x80][0x81][Source][PGN][Length][Data...][CRC]
        if (rxBufferIndex >= 7) {  // Minimum PGN size (header + length + CRC)
            // Look for PGN header
            bool foundPGN = false;
            size_t pgnStart = 0;
            
            // Scan buffer for PGN header
            for (size_t i = 0; i <= rxBufferIndex - 7; i++) {
                if (rxBuffer[i] == 0x80 && rxBuffer[i + 1] == 0x81) {
                    // Found potential PGN header
                    pgnStart = i;
                    
                    // Extract message info
                    // uint8_t source = rxBuffer[i + 2];  // Source ID
                    // uint8_t pgnId = rxBuffer[i + 3];   // PGN ID
                    uint8_t dataLength = rxBuffer[i + 4];  // Data length
                    
                    // Calculate total message length: header(5) + dataLength + CRC(1)
                    size_t totalLength = 5 + dataLength + 1;
                    
                    // Check if we have the complete message
                    if ((pgnStart + totalLength) <= rxBufferIndex) {
                        foundPGN = true;
                        
                        // Send complete PGN to UDP9999 broadcast
                        uint8_t* pgnData = &rxBuffer[pgnStart];
                        
                        // Debug log received PGN
                        uint8_t source = rxBuffer[pgnStart + 2];
                        uint8_t pgn = rxBuffer[pgnStart + 3];
                        LOG_DEBUG(EventSource::SYSTEM, "ESP32 RX: PGN=%d, source=%d, len=%zu -> UDP9999", 
                                      pgn, source, totalLength);
                        
                        QNEthernetUDPHandler::sendUDP9999Packet(pgnData, totalLength);
                        
                        // Remove processed data from buffer
                        size_t remaining = rxBufferIndex - (pgnStart + totalLength);
                        if (remaining > 0) {
                            memmove(rxBuffer, &rxBuffer[pgnStart + totalLength], remaining);
                        }
                        rxBufferIndex = remaining;
                        
                        break;  // Process one PGN at a time
                    } else {
                        // Debug: Not enough data yet - but this is normal for serial data arriving in chunks
                        // Only log if the incomplete message persists (not just transient buffering)
                        static uint32_t incompleteStartTime = 0;
                        static size_t lastIncompleteSize = 0;
                        
                        if (rxBufferIndex != lastIncompleteSize) {
                            // Size changed, reset timer
                            incompleteStartTime = millis();
                            lastIncompleteSize = rxBufferIndex;
                        }
                        
                        // Only log if incomplete for more than 50ms (serial chunks should arrive faster)
                        if (millis() - incompleteStartTime > 50) {
                            static uint32_t lastIncompleteLog = 0;
                            if (millis() - lastIncompleteLog > 1000) {
                                LOG_DEBUG(EventSource::SYSTEM, "ESP32 RX: Incomplete PGN at %zu, need %zu bytes, have %zu", 
                                          pgnStart, totalLength, rxBufferIndex - pgnStart);
                                lastIncompleteLog = millis();
                            }
                        }
                    }
                }
            }
            
            // If no PGN found and buffer is getting full, clear old data
            if (!foundPGN && rxBufferIndex > RX_BUFFER_SIZE - 100) {
                LOG_WARNING(EventSource::SYSTEM, "ESP32 RX: Buffer full, clearing old data (had %zu bytes)", rxBufferIndex);
                // Keep last 100 bytes
                memmove(rxBuffer, &rxBuffer[rxBufferIndex - 100], 100);
                rxBufferIndex = 100;
            }
            
            // Reset partial message timer if we found a complete message
            static uint32_t partialMessageTime = 0;
            if (foundPGN) {
                partialMessageTime = 0;  // Reset timer after successful message
            }
            
            // Don't clear partial messages too quickly - serial data might arrive in chunks
            // Only clear if we've been stuck for a while
            if (!foundPGN && rxBufferIndex > 0 && rxBuffer[0] == 0x80) {
                if (partialMessageTime == 0) {
                    partialMessageTime = millis();
                }
                
                // Wait 100ms for rest of message before clearing
                if (millis() - partialMessageTime > 100) {
                    LOG_DEBUG(EventSource::SYSTEM, "ESP32 RX: Clearing partial message after timeout (%zu bytes)", rxBufferIndex);
                    rxBufferIndex = 0;
                    partialMessageTime = 0;
                }
            } else if (rxBufferIndex == 0 || rxBuffer[0] != 0x80) {
                // No partial message, reset timer
                partialMessageTime = 0;
            }
        }
    }
}

// Check buffer for ESP32 hello message
void ESP32Interface::checkForHello() {
    const char* helloMsg = "ESP32-hello";
    size_t helloLen = strlen(helloMsg);
    
    // Debug: Show buffer contents periodically (disabled - too noisy with constant PGN traffic)
    /*
    static uint32_t lastBufferDebug = 0;
    if (rxBufferIndex > 0 && millis() - lastBufferDebug > 2000) {
        Serial.printf("ESP32 RX Buffer (%d bytes): ", rxBufferIndex);
        for (size_t i = 0; i < rxBufferIndex && i < 20; i++) {
            if (rxBuffer[i] >= 32 && rxBuffer[i] <= 126) {
                Serial.printf("%c", rxBuffer[i]);
            } else {
                Serial.printf("[%02X]", rxBuffer[i]);
            }
        }
        if (rxBufferIndex > 20) Serial.print("...");
        Serial.println();
        lastBufferDebug = millis();
    }
    */
    
    // Look for hello message in buffer
    if (rxBufferIndex >= helloLen) {
        for (size_t i = 0; i <= rxBufferIndex - helloLen; i++) {
            if (memcmp(&rxBuffer[i], helloMsg, helloLen) == 0) {
                // Found hello message
                if (!esp32Detected) {
                    esp32Detected = true;
                    LOG_INFO(EventSource::SYSTEM, "ESP32 detected and connected");
                    LOG_INFO(EventSource::SYSTEM, "ESP32 will now receive PGNs from UDP port 8888");
                    // Already logged with LOG_INFO above
                } else {
                    // Already detected, just update the time
                    static uint32_t lastHelloLog = 0;
                    if (millis() - lastHelloLog > 30000) {  // Log every 30 seconds
                        LOG_DEBUG(EventSource::SYSTEM, "ESP32: Hello received, connection maintained");
                        lastHelloLog = millis();
                    }
                }
                lastHelloTime = millis();
                
                // Remove hello message from buffer
                size_t remaining = rxBufferIndex - (i + helloLen);
                if (remaining > 0) {
                    memmove(&rxBuffer[i], &rxBuffer[i + helloLen], remaining);
                }
                rxBufferIndex -= helloLen;
                
                break;
            }
        }
    }
}

// Print status information
void ESP32Interface::printStatus() {
    LOG_INFO(EventSource::SYSTEM, "ESP32 Interface Status: Detected=%s", esp32Detected ? "Yes" : "No");
    
    if (esp32Detected) {
        LOG_INFO(EventSource::SYSTEM, "  Last hello: %lu ms ago", millis() - lastHelloTime);
    }
    
    LOG_INFO(EventSource::SYSTEM, "  RX buffer: %zu bytes", rxBufferIndex);
}