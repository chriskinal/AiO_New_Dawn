// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

// SimpleHomePage.h
// Simplified home page for WebSocket-based architecture

#ifndef SIMPLE_HOME_PAGE_H
#define SIMPLE_HOME_PAGE_H

#include <Arduino.h>

const char SIMPLE_HOME_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name='viewport' content='width=device-width, initial-scale=1'>
    <title>AiO New Dawn</title>
    <style>%CSS_STYLES%</style>
</head>
<body>
    <div class='container'>
        <h1>AgOpenGPS AiO New Dawn</h1>
        <div class='status' id='status'>System Status: <span id='statusText'>Connecting...</span></div>
        
        <h2>Configuration</h2>
        <ul>
            <li><a href='/device'>Device Settings</a></li>
            <li><a href='/network'>Network Settings</a></li>
            <li><a href='/analogworkswitch'>Analog Work Switch</a></li>
            <li><a href='/eventlogger'>Event Logger Settings</a></li>
            <li><a href='/um98x-config'>UM98x GPS Configuration</a></li>
        </ul>
        
        <h2>System</h2>
        <ul>
            <li><a href='/api/status'>System Status (JSON)</a></li>
            <li><a href='/ota'>OTA Update</a></li>
            <li><a href='#' onclick='restartSystem()'>Restart System</a></li>
        </ul>
        
        <div class='info'>
            <p>Firmware Version: %FIRMWARE_VERSION%</p>
            <p>WebSocket Telemetry: <span id='wsStatus'>Disconnected</span> | Rate: <span id='telemetryRate'>0</span> Hz</p>
        </div>
    </div>
    
    <script>
        let ws;
        let lastPacketTime = 0;
        let packetCount = 0;
        let rateUpdateTime = 0;
        
        function connectWebSocket() {
            ws = new WebSocket('ws://' + window.location.hostname + ':8082');
            
            ws.onopen = function() {
                document.getElementById('wsStatus').textContent = 'Connected';
                document.getElementById('wsStatus').style.color = 'green';
            };
            
            ws.onclose = function() {
                document.getElementById('wsStatus').textContent = 'Disconnected';
                document.getElementById('wsStatus').style.color = 'red';
                document.getElementById('telemetryRate').textContent = '0';
                setTimeout(connectWebSocket, 2000);
            };
            
            ws.onmessage = function(event) {
                const now = Date.now();
                packetCount++;
                
                // Update rate every second
                if (now - rateUpdateTime >= 1000) {
                    const rate = packetCount;
                    document.getElementById('telemetryRate').textContent = rate;
                    packetCount = 0;
                    rateUpdateTime = now;
                }
                
                // Update system status
                document.getElementById('statusText').textContent = 'Running';
                document.getElementById('statusText').style.color = 'green';
            };
        }
        
        function restartSystem() {
            if (confirm('Are you sure you want to restart the system?')) {
                fetch('/api/restart', { method: 'POST' })
                    .then(response => response.json())
                    .then(data => {
                        alert('System is restarting...');
                    });
            }
            return false;
        }
        
        // Connect on page load
        connectWebSocket();
    </script>
</body>
</html>
)rawliteral";

#endif // SIMPLE_HOME_PAGE_H