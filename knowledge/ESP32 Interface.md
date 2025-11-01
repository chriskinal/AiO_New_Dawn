⏺ Proposal: ESP32-Initiated Communication Protocol for AiO New Dawn

Background

Currently, the LittleDawnInterface uses a Teensy-initiated approach where the Teensy polls the ESP32 every second with handshake requests. Additionally, PR #54 proposes mixing raw serial data with the existing binary protocol, which could cause compatibility issues.

Proposed Solution

Redesign the Teensy-ESP32 communication to be ESP32-initiated using the existing binary protocol framework with extended message types.

Key Design Principles

ESP32-Initiated Communication
ESP32 announces its presence and capabilities on startup
Teensy only responds to ESP32 requests (no polling)
Similar to how GPS and AgOpenGPS interfaces work
Single Extensible Protocol
Format: [Message ID][Length][Data][Checksum]

Extended Message Types:
// Existing messages
#define MSG_MACHINE_STATUS 0x01 // Machine Status
#define MSG_HANDSHAKE_REQUEST 0x10 // Deprecated
#define MSG_HANDSHAKE_RESPONSE 0x11 // Repurposed as ACK

// New message types
#define MSG_PGN_DATA 0x20 // PGN from AgIO → ESP32
#define MSG_NMEA_DATA 0x21 // NMEA from Teensy → ESP32
#define MSG_ESP32_TO_AGIO 0x30 // Data from ESP32 → AgIO
#define MSG_ESP32_HELLO 0x50 // ESP32 announces capabilities
#define MSG_TEENSY_ACK 0x51 // Teensy acknowledge
#define MSG_ESP32_SUBSCRIBE 0x52 // ESP32 update subscriptions

Capability-Based Handshake
ESP32 Hello Message Structure:
struct ESP32HelloMessage {
uint8_t version; // Protocol version
uint8_t capabilities; // What ESP32 can provide
uint16_t subscriptions; // Bit flags for desired data
} attribute((packed));

// Subscription flags (what ESP32 wants to receive)
#define SUB_MACHINE_STATUS 0x0001 // Want machine status @ 10Hz
#define SUB_PGN_253 0x0002 // Want PGN 253 (steer data)
#define SUB_PGN_254 0x0004 // Want PGN 254 (GPS data)
#define SUB_PGN_255 0x0008 // Want PGN 255 (IMU data)
#define SUB_ALL_PGNS 0x0010 // Want all PGNs from port 9999
#define SUB_NMEA_GPS 0x0020 // Want NMEA sentences

// Capability flags (what ESP32 can send back)
#define CAP_SECTION_CTRL 0x01 // Can provide section control
#define CAP_SENSOR_DATA 0x02 // Has additional sensors
#define CAP_WIFI_STATUS 0x04 // Can provide WiFi status

Current vs Proposed Implementation

Current (Teensy-Initiated):

void LittleDawnInterface::process() {
// Teensy constantly polls ESP32
if (!littleDawnDetected) {
if (millis() - lastHandshakeTime >= 1000) { // Every second
sendHandshakeRequest(); // Wasted if no ESP32!
lastHandshakeTime = millis();
}
}
}

Proposed (ESP32-Initiated):

void LittleDawnInterface::process() {
// Just listen for incoming data
if (SerialESP32.available()) {
processIncomingData();
}

  // Only send if ESP32 has announced itself
  if (littleDawnDetected && shouldSendData()) {
      sendRequestedData();
  }
  // No polling! Resource efficient
}

Implementation Examples

ESP32 Side - Announcing Presence:

void ESP32::setup() {
// On startup, tell Teensy what we want
ESP32HelloMessage hello;
hello.version = 1;
hello.capabilities = CAP_SECTION_CTRL | CAP_WIFI_STATUS;
hello.subscriptions = SUB_MACHINE_STATUS | SUB_PGN_254;

  sendToTeensy(MSG_ESP32_HELLO, &hello, sizeof(hello));
}

void ESP32::sendToAgIO(const char* nmea) {
// Send data back to AgOpenGPS through Teensy
TextMessage msg;
msg.length = strlen(nmea);
memcpy(msg.text, nmea, msg.length);

  sendToTeensy(MSG_ESP32_TO_AGIO, (uint8_t*)&msg, msg.length + 1);
}

Teensy Side - Handling Hello:

void LittleDawnInterface::processMessage(uint8_t id, const uint8_t* data, uint8_t len) {
switch(id) {
case MSG_ESP32_HELLO: {
ESP32HelloMessage* hello = (ESP32HelloMessage*)data;

          // Store what ESP32 wants
          esp32Subscriptions = hello->subscriptions;
          esp32Capabilities = hello->capabilities;

          // Mark as detected
          littleDawnDetected = true;
          lastResponseTime = millis();

          LOG_INFO("ESP32 connected: cap=0x%02X sub=0x%04X",
                   hello->capabilities, hello->subscriptions);

          // Send acknowledgment
          sendToLittleDawn(MSG_TEENSY_ACK, nullptr, 0);

          // Register for PGNs if requested
          if (hello->subscriptions & SUB_ALL_PGNS) {
              registerForPGNForwarding();
          }
          break;
      }

      case MSG_ESP32_TO_AGIO:
          // Forward data from ESP32 to AgIO
          sendUDPbytes(data, len);
          break;
  }
}

PGN Forwarding (Clean Protocol):

// Instead of raw serial write (PR #54 approach):
void PGNProcessor::handlePGN(uint8_t pgn, const uint8_t* data, size_t len) {
if (destPort == 9999) {
SerialESP32.write(data, len); // ❌ Mixes protocols!
}
}

// Use structured protocol:
void LittleDawnInterface::forwardPGN(uint8_t pgn, const uint8_t* data, size_t len) {
if (esp32Subscriptions & SUB_ALL_PGNS) {
PGNMessage msg;
msg.pgn = pgn;
msg.sourceAddress = data[0];
msg.dataLength = len - 1;
memcpy(msg.data, &data[1], msg.dataLength);

      sendToLittleDawn(MSG_PGN_DATA, (uint8_t*)&msg, sizeof(msg));  // ✅ Clean!
  }
}

Selective Data Sending:

void LittleDawnInterface::sendRequestedData() {
uint32_t now = millis();

  // Only send what ESP32 subscribed to
  if (esp32Subscriptions & SUB_MACHINE_STATUS) {
      if (now - lastStatusTime >= 100) {  // 10Hz
          sendMachineStatus();
          lastStatusTime = now;
      }
  }

  // GPS data at different rate
  if (esp32Subscriptions & SUB_PGN_254) {
      if (now - lastGPSTime >= 1000) {  // 1Hz
          sendGPSData();
          lastGPSTime = now;
      }
  }
}

Message Flow Example

ESP32 Startup:
ESP32 → [0x50][4][version=1, cap=0x05, sub=0x0006][checksum]

Teensy Acknowledges:
Teensy → [0x51][0][checksum]

Normal Operation (only subscribed data):
Teensy → [0x01][10][speed, heading, roll, etc.][checksum] @ 10Hz
Teensy → [0x20][8][PGN 254 data][checksum] @ 1Hz

ESP32 Sends Data:
ESP32 → [0x30][20]["$PASHR,1,2,3..."][checksum]
Teensy forwards to AgIO via UDP

Benefits

Resource Efficient:
// No more of this when ESP32 absent:
if (!detected) {
sendHandshake(); // Wasted cycles
}
Protocol Integrity:
// All messages validated with checksum
if (calculateChecksum(buffer, len) == buffer[len]) {
processMessage(); // Safe to process
}
Flexible Subscriptions:
// ESP32 can change what it wants at runtime
if (needMoreData) {
hello.subscriptions |= SUB_PGN_255; // Add IMU data
sendToTeensy(MSG_ESP32_SUBSCRIBE, &hello, sizeof(hello));
}
Recommendation

Implement this ESP32-initiated protocol instead of PR #54's approach. This provides:

Clean separation of protocols (no mixing)
Resource efficient (no polling)
Extensible for future needs
Compatible with existing infrastructure
The code examples show how this can be implemented with minimal changes while providing maximum flexibility.