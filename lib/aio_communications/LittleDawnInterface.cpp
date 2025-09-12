#include "LittleDawnInterface.h"
#include "SerialManager.h"
#include "AutosteerProcessor.h"
#include "ADProcessor.h"
#include "IMUProcessor.h"

// Global instance
LittleDawnInterface littleDawnInterface;

// Initialize the interface
void LittleDawnInterface::init() {
    // SerialESP32 is already initialized by SerialManager at 460800 baud
    LOG_INFO(EventSource::SYSTEM, "Little Dawn interface initialized on Serial2 (460800 baud)");
    
    // Send initial handshake request
    sendHandshakeRequest();
    lastHandshakeTime = millis();
}

// Main processing loop - called from main.cpp
void LittleDawnInterface::process() {
    // Process any incoming data
    processIncomingData();
    
    // If not detected, send periodic handshake requests
    if (!littleDawnDetected) {
        if (millis() - lastHandshakeTime >= HANDSHAKE_INTERVAL_MS) {
            sendHandshakeRequest();
            lastHandshakeTime = millis();
        }
        return;  // Don't send data if not detected
    }
    
    // Check for timeout - if we haven't heard from Little Dawn in a while
    if (millis() - lastResponseTime > HANDSHAKE_TIMEOUT_MS) {
        littleDawnDetected = false;
        LOG_WARNING(EventSource::SYSTEM, "Little Dawn connection lost (timeout)");
        return;
    }
    
    // Send data every 100ms if detected
    if (millis() - lastTransmitTime >= TRANSMIT_INTERVAL_MS) {
        sendMachineStatus();
        lastTransmitTime = millis();
    }
}

// Calculate simple checksum
uint8_t LittleDawnInterface::calculateChecksum(const uint8_t* data, size_t len) {
    uint8_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum += data[i];
    }
    return ~sum;  // One's complement
}

// Send message to Little Dawn
void LittleDawnInterface::sendToLittleDawn(uint8_t id, const uint8_t* data, uint8_t length) {
    uint8_t buffer[68];  // id + length + data + checksum
    buffer[0] = id;
    buffer[1] = length;
    memcpy(&buffer[2], data, length);
    buffer[2 + length] = calculateChecksum(buffer, 2 + length);
    
    // Debug logging for first few messages
    static int msgCount = 0;
    if (msgCount < 5) {
        LOG_DEBUG(EventSource::SYSTEM, "Little Dawn TX: id=0x%02X len=%d checksum=0x%02X", 
                  id, length, buffer[2 + length]);
        msgCount++;
    }
    
    SerialESP32.write(buffer, 3 + length);
}

// Send machine status with current data
void LittleDawnInterface::sendMachineStatus() {
    MachineStatus status;
    
    // Get speed from AutosteerProcessor (km/h)
    float speedKmh = 0.0f;
    if (AutosteerProcessor::getInstance()) {
        speedKmh = AutosteerProcessor::getInstance()->getVehicleSpeed();
    } else {
        LOG_WARNING(EventSource::SYSTEM, "AutosteerProcessor instance is null!");
    }
    
    // Get WAS angle from ADProcessor
    float wasAngle = 0.0f;
    wasAngle = adProcessor.getWASAngle();
    
    // Get IMU data if available
    float heading = 0.0f;
    float roll = 0.0f;
    float pitch = 0.0f;
    
    if (imuProcessor.hasValidData()) {
        IMUData imuData = imuProcessor.getCurrentData();
        heading = imuData.heading;
        roll = imuData.roll;
        pitch = imuData.pitch;
    }
    
    // Convert to protocol format
    status.speed = (int16_t)(speedKmh * 100);        // Convert to 0.01 km/h units
    status.heading = (int16_t)(heading * 10);         // Convert to 0.1 degree units
    status.roll = (int16_t)(roll * 10);               // Convert to 0.1 degree units
    status.pitch = (int16_t)(pitch * 10);             // Convert to 0.1 degree units
    status.steerAngle = (int16_t)(wasAngle * 10);    // Convert to 0.1 degree units
    
    // Send the data
    static bool firstSend = true;
    if (firstSend) {
        LOG_INFO(EventSource::SYSTEM, "MachineStatus size: %d bytes", sizeof(status));
        firstSend = false;
    }
    sendToLittleDawn(MSG_MACHINE_STATUS, (uint8_t*)&status, sizeof(status));
}

// Check if interface is active
bool LittleDawnInterface::isActive() const {
    // Consider active if Little Dawn is detected and we've sent data recently
    return littleDawnDetected && ((millis() - lastTransmitTime) < (TRANSMIT_INTERVAL_MS * 2));
}

// Send handshake request
void LittleDawnInterface::sendHandshakeRequest() {
    uint8_t handshakeData[] = {'N', 'D', '2', 'L', 'D'};  // "ND2LD" - New Dawn to Little Dawn
    sendToLittleDawn(MSG_HANDSHAKE_REQUEST, handshakeData, sizeof(handshakeData));
}

// Process incoming data from Little Dawn
void LittleDawnInterface::processIncomingData() {
    static uint8_t rxBuffer[64], rxBufferForUDP[64];
    static uint8_t rxIndex = 0, rxIndexForUDP = 0;
    static enum { WAIT_ID, WAIT_LEN, WAIT_DATA, WAIT_CHECKSUM } rxState = WAIT_ID;
    static uint8_t msgId = 0;
    static uint8_t msgLen = 0;
    
    while (SerialESP32.available()) {
        uint8_t byte = SerialESP32.read();
        //get the complete sentence to foreward it
        rxBufferForUDP[rxIndexForUDP] = byte;
        rxIndexForUDP++;

        // Check for CRLF termination
        if (rxIndexForUDP >= 2 &&
            rxBufferForUDP[rxIndexForUDP - 2] == 13 &&
            rxBufferForUDP[rxIndexForUDP - 1] == 10)
        {
            //foreward sentence to AgIO via UDP, no checking = every data everywhere
            sendUDPbytes(rxBufferForUDP, rxIndexForUDP - 2); //-2 to not send CRLF
            rxIndexForUDP = 0;
            //when sentence ended with CRLF, then no LittleDawn message can be in it
            rxIndex = 0; //reset also the other buffer
            rxState = WAIT_ID; //reset state machine
        }

        // Prevent buffer overflow
        if (rxIndexForUDP >= sizeof(rxBufferForUDP))
        {
            rxIndexForUDP = 0;
        }        
        
        switch (rxState) {
            case WAIT_ID:
                msgId = byte;
                rxIndex = 0;
                rxState = WAIT_LEN;
                break;
                
            case WAIT_LEN:
                msgLen = byte;
                if (msgLen > sizeof(rxBuffer) || ((rxBufferForUDP[0] == 128 && rxBufferForUDP[1] == 129))) {
                    // Invalid length or PGN for AgIO, reset
                    rxState = WAIT_ID;
                } else {
                    rxState = (msgLen > 0) ? WAIT_DATA : WAIT_CHECKSUM;
                }
                break;
                
            case WAIT_DATA:
                rxBuffer[rxIndex++] = byte;
                if (rxIndex >= msgLen) {
                    rxState = WAIT_CHECKSUM;
                }
                break;
                
            case WAIT_CHECKSUM:
                {
                    // Verify checksum
                    uint8_t calcChecksum = msgId + msgLen;
                    for (uint8_t i = 0; i < msgLen; i++) {
                        calcChecksum += rxBuffer[i];
                    }
                    calcChecksum = ~calcChecksum;
                    
                    if (calcChecksum == byte) {
                        // Valid message received
                        if (processMessage(msgId, rxBuffer, msgLen)) {
                            lastResponseTime = millis();
                        }
                    } else {
                        // Log checksum mismatch for debugging
                        LOG_WARNING(EventSource::SYSTEM, "Little Dawn checksum mismatch: expected 0x%02X, got 0x%02X (id=0x%02X, len=%d)", 
                                   calcChecksum, byte, msgId, msgLen);
                    }
                    
                    rxState = WAIT_ID;
                }
                break;
        }
    }
}

// Process a received message
bool LittleDawnInterface::processMessage(uint8_t id, const uint8_t* data, uint8_t length) {
    switch (id) {
        case MSG_HANDSHAKE_RESPONSE:
            if (length >= 5 && data[0] == 'L' && data[1] == 'D' && data[2] == '2' && 
                data[3] == 'N' && data[4] == 'D') {  // "LD2ND" - Little Dawn to New Dawn
                if (!littleDawnDetected) {
                    littleDawnDetected = true;
                    LOG_INFO(EventSource::SYSTEM, "Little Dawn detected and connected");
                }
                // Always update response time when we get a valid handshake
                lastResponseTime = millis();
                return true;
            }
            break;
            
        default:
            // Any other valid message also serves as a keepalive
            if (littleDawnDetected) {
                // Consider any message from Little Dawn as a sign it's still alive
                return true;
            }
            break;
    }
    
    return false;
}

// Print status information
void LittleDawnInterface::printStatus() {
    Serial.println("\n=== Little Dawn Interface Status ===");
    Serial.printf("Detected: %s\n", littleDawnDetected ? "Yes" : "No");
    Serial.printf("Active: %s\n", isActive() ? "Yes" : "No");
    
    if (littleDawnDetected) {
        Serial.printf("Last response: %lu ms ago\n", millis() - lastResponseTime);
        Serial.printf("Last transmit: %lu ms ago\n", millis() - lastTransmitTime);
    } else {
        Serial.printf("Last handshake attempt: %lu ms ago\n", millis() - lastHandshakeTime);
    }
    
    // Print current data being sent
    if (isActive()) {
        float speedKmh = AutosteerProcessor::getInstance() ? 
                         AutosteerProcessor::getInstance()->getVehicleSpeed() : 0.0f;
        float wasAngle = adProcessor.getWASAngle();
        
        Serial.printf("Current data:\n");
        Serial.printf("  Speed: %.2f km/h\n", speedKmh);
        Serial.printf("  WAS angle: %.1f deg\n", wasAngle);
        
        if (imuProcessor.hasValidData()) {
            IMUData imuData = imuProcessor.getCurrentData();
            Serial.printf("  Heading: %.1f deg\n", imuData.heading);
            Serial.printf("  Roll: %.1f deg\n", imuData.roll);
            Serial.printf("  Pitch: %.1f deg\n", imuData.pitch);
        } else {
            Serial.println("  IMU: No valid data");
        }
    }
}