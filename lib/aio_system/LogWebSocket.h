// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

// LogWebSocket.h
// WebSocket server for real-time log streaming

#ifndef LOG_WEBSOCKET_H
#define LOG_WEBSOCKET_H

#include <Arduino.h>
#include <QNEthernet.h>
#include "SimpleWebSocket.h"
#include "EventLogger.h"

using namespace qindesign::network;

class LogWebSocket {
public:
    LogWebSocket();
    ~LogWebSocket();

    // Initialize and start the WebSocket server
    bool begin(uint16_t port = 8083);

    // Stop the server
    void stop();

    // Process client connections (call from main loop)
    void handleClient();

    // Broadcast a single log entry to all connected clients
    void broadcastLog(uint32_t timestamp, EventSeverity severity, EventSource source, const char* message);

    // Send initial log buffer history to a newly connected client
    void sendLogHistory(size_t clientIndex);

    // Get server status
    bool isRunning() const { return running; }
    uint16_t getPort() const { return serverPort; }
    size_t getClientCount() const;

private:
    static const uint8_t MAX_CLIENTS = 4;

    SimpleWebSocketServer wsServer;
    uint16_t serverPort;
    bool running;

    // Format log entry as JSON for WebSocket transmission
    String formatLogEntry(uint32_t timestamp, EventSeverity severity, EventSource source, const char* message);

    // Escape JSON special characters
    void appendEscapedString(String& json, const char* str);
};

#endif // LOG_WEBSOCKET_H
