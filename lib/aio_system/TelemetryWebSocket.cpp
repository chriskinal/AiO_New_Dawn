// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

// TelemetryWebSocket.cpp
// WebSocket server for high-frequency telemetry streaming

#include "TelemetryWebSocket.h"
#include "EventLogger.h"
#include <cstring>

// Static test page HTML stored in PROGMEM
const char wsTestPage[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html>
<head>
    <title>WebSocket Telemetry Test</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        .status { margin: 10px 0; padding: 10px; background: #f0f0f0; }
        .connected { background: #d4edda; }
        .disconnected { background: #f8d7da; }
        .data { font-family: monospace; margin: 10px 0; }
        button { margin: 5px; padding: 5px 10px; }
    </style>
</head>
<body>
    <h1>WebSocket Telemetry Test</h1>
    <div id="status" class="status disconnected">Disconnected</div>
    <button onclick="connect()">Connect</button>
    <button onclick="disconnect()">Disconnect</button>
    <button onclick="setRate(10)">10Hz</button>
    <button onclick="setRate(50)">50Hz</button>
    <button onclick="setRate(100)">100Hz</button>
    <div class="data">
        <h3>Latest Data:</h3>
        <pre id="data">No data received</pre>
    </div>
    <div class="data">
        <h3>Statistics:</h3>
        <pre id="stats">Messages: 0, Rate: 0 Hz</pre>
    </div>
    
    <script>
        let ws = null;
        let messageCount = 0;
        let lastMessageTime = Date.now();
        let rateTimer = null;
        
        function connect() {
            if (ws) return;
            
            ws = new WebSocket('ws://' + window.location.hostname + ':8082');
            ws.binaryType = 'arraybuffer';
            
            ws.onopen = () => {
                document.getElementById('status').className = 'status connected';
                document.getElementById('status').textContent = 'Connected';
                messageCount = 0;
                startRateTimer();
            };
            
            ws.onclose = () => {
                document.getElementById('status').className = 'status disconnected';
                document.getElementById('status').textContent = 'Disconnected';
                ws = null;
                stopRateTimer();
            };
            
            ws.onerror = (error) => {
                console.error('WebSocket error:', error);
            };
            
            ws.onmessage = (event) => {
                if (event.data instanceof ArrayBuffer) {
                    messageCount++;
                    const view = new DataView(event.data);
                    
                    // Parse binary telemetry packet (32 bytes)
                    const packet = {
                        timestamp: view.getUint32(0, true),
                        was_angle: view.getFloat32(4, true),
                        was_angle_target: view.getFloat32(8, true),
                        encoder_count: view.getInt16(12, true),
                        current_draw: view.getFloat32(14, true),
                        speed_kph: view.getFloat32(18, true),
                        heading: view.getFloat32(22, true),
                        status_flags: view.getUint16(26, true),
                        steer_switch: view.getUint8(28),
                        work_switch: view.getUint8(29),
                        work_analog_percent: view.getUint8(30)
                    };
                    
                    // Display data
                    document.getElementById('data').textContent = JSON.stringify(packet, null, 2);
                }
            };
        }
        
        function disconnect() {
            if (ws) {
                ws.close();
            }
        }
        
        function setRate(hz) {
            if (ws && ws.readyState === WebSocket.OPEN) {
                const cmd = new Uint8Array([hz]);
                ws.send(cmd);
            }
        }
        
        function startRateTimer() {
            rateTimer = setInterval(() => {
                const now = Date.now();
                const elapsed = (now - lastMessageTime) / 1000;
                const rate = messageCount / elapsed;
                document.getElementById('stats').textContent = 
                    'Messages: ' + messageCount + ', Rate: ' + rate.toFixed(1) + ' Hz';
            }, 1000);
        }
        
        function stopRateTimer() {
            if (rateTimer) {
                clearInterval(rateTimer);
                rateTimer = null;
            }
        }
    </script>
</body>
</html>
)HTML";

TelemetryWebSocket::TelemetryWebSocket() : 
    httpServer(8081),  // HTTP test page on 8081
    serverPort(8082),  // WebSocket on 8082
    running(false),
    lastBroadcast(0),
    broadcastRateHz(10) {
}

TelemetryWebSocket::~TelemetryWebSocket() {
    stop();
}

bool TelemetryWebSocket::begin(uint16_t port) {
    serverPort = port;
    
    // Start HTTP server for test page
    httpServer.begin();
    
    // Start WebSocket server
    wsServer.setMaxClients(MAX_CLIENTS);
    if (wsServer.begin(serverPort)) {
        running = true;
        LOG_INFO(EventSource::SYSTEM, "TelemetryWebSocket started - HTTP test page on port %d, WebSocket on port %d", 
                 8081, serverPort);
        return true;
    } else {
        LOG_ERROR(EventSource::SYSTEM, "TelemetryWebSocket failed to start on port %d", serverPort);
        return false;
    }
}

void TelemetryWebSocket::stop() {
    if (!running) return;
    
    // Stop servers
    wsServer.stop();
    httpServer.end();
    
    running = false;
    LOG_INFO(EventSource::SYSTEM, "TelemetryWebSocket stopped");
}

void TelemetryWebSocket::handleClient() {
    if (!running) return;
    
    // Handle HTTP requests for test page
    handleHttpRequest();
    
    // Handle WebSocket clients
    wsServer.handleClients();
}

void TelemetryWebSocket::broadcastTelemetry(const TelemetryPacket& packet) {
    if (!running) return;
    
    // No rate limiting here - let WebManager control the rate
    // Broadcast binary packet to all clients
    wsServer.broadcastBinary((const uint8_t*)&packet, sizeof(packet));
}

size_t TelemetryWebSocket::getClientCount() const {
    return wsServer.getClientCount();
}

void TelemetryWebSocket::handleHttpRequest() {
    EthernetClient httpClient = httpServer.available();
    
    if (httpClient) {
        char line[256];
        char method[16] = {0};
        char path[128] = {0};
        
        // Read request line
        int len = httpClient.readBytesUntil('\n', line, sizeof(line) - 1);
        if (len > 0) {
            line[len] = '\0';
            
            // Parse request
            sscanf(line, "%15s %127s", method, path);
            
            // Skip headers
            while (httpClient.available()) {
                len = httpClient.readBytesUntil('\n', line, sizeof(line) - 1);
                if (len <= 1) break;  // Empty line
            }
            
            // Handle request
            if (strcmp(method, "GET") == 0 && 
                (strcmp(path, "/") == 0 || strcmp(path, "/wstest") == 0)) {
                sendTestPage(httpClient);
            } else {
                // 404 Not Found
                httpClient.print("HTTP/1.1 404 Not Found\r\n");
                httpClient.print("Content-Type: text/plain\r\n");
                httpClient.print("Connection: close\r\n");
                httpClient.print("\r\n");
                httpClient.print("Not Found");
            }
        }
        
        httpClient.stop();
    }
}

void TelemetryWebSocket::sendTestPage(EthernetClient& client) {
    // Send HTTP headers
    client.print("HTTP/1.1 200 OK\r\n");
    client.print("Content-Type: text/html\r\n");
    client.printf("Content-Length: %d\r\n", strlen_P(wsTestPage));
    client.print("Connection: close\r\n");
    client.print("\r\n");
    
    // Send page content from PROGMEM
    client.print(wsTestPage);
    client.flush();
}