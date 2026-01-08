// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

// TelemetryWebSocket.h
// WebSocket server for high-frequency telemetry streaming

#ifndef TELEMETRY_WEBSOCKET_H
#define TELEMETRY_WEBSOCKET_H

#include <Arduino.h>
#include <QNEthernet.h>
#include "SimpleWebSocket.h"
#include <map>

using namespace qindesign::network;

// Binary telemetry packet structure (packed for efficiency)
struct __attribute__((packed)) TelemetryPacket {
    uint32_t timestamp;      // millis()
    float was_angle;         // Wheel angle sensor
    float was_angle_target;  // Target angle
    int16_t encoder_count;   // Encoder position
    float current_draw;      // Motor current
    float speed_kph;         // Vehicle speed
    float heading;           // Compass heading
    uint16_t status_flags;   // Various status bits
    uint8_t steer_switch;    // Steering switch state
    uint8_t work_switch;     // Work switch state (digital)
    uint8_t work_analog_percent; // Analog work switch percentage (0-100)
    uint8_t reserved[1];     // Padding to 32 bytes
};

class TelemetryWebSocket {
public:
    TelemetryWebSocket();
    ~TelemetryWebSocket();
    
    // Initialize and start the WebSocket server
    bool begin(uint16_t port = 8081);
    
    // Stop the server
    void stop();
    
    // Process client connections (call from main loop)
    void handleClient();
    
    // Broadcast telemetry to all connected clients
    void broadcastTelemetry(const TelemetryPacket& packet);
    
    // Get server status
    bool isRunning() const { return running; }
    uint16_t getPort() const { return serverPort; }
    size_t getClientCount() const;
    
private:
    static const uint8_t MAX_CLIENTS = 4;
    
    SimpleWebSocketServer wsServer;
    EthernetServer httpServer;  // For serving test page
    uint16_t serverPort;
    bool running;
    
    // Track client update rates
    uint32_t lastBroadcast;
    uint16_t broadcastRateHz;
    
    // Handle WebSocket messages
    void handleWebSocketMessage(const uint8_t* data, size_t length, bool binary);
    
    // Handle HTTP requests for test page
    void handleHttpRequest();
    
    // Send WebSocket test page
    void sendTestPage(EthernetClient& client);
};

#endif // TELEMETRY_WEBSOCKET_H