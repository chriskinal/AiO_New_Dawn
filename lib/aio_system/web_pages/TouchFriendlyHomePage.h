// TouchFriendlyHomePage.h
// Home page optimized for touchscreen tablets

#ifndef TOUCH_FRIENDLY_HOME_PAGE_H
#define TOUCH_FRIENDLY_HOME_PAGE_H

#include <Arduino.h>

const char TOUCH_FRIENDLY_HOME_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <meta name="apple-mobile-web-app-capable" content="yes">
    <title>AiO New Dawn</title>
    <link rel="stylesheet" href="/touch.css">
</head>
<body>
    <div class="container">
        <h1>AgOpenGPS AiO New Dawn</h1>
        
        <h2>Configuration</h2>
        <nav class="nav-menu">
            <li><a href="/device">
                <svg width="24" height="24" viewBox="0 0 24 24" fill="white" style="margin-right: 10px;">
                    <path d="M12 15.5A3.5 3.5 0 0 1 8.5 12A3.5 3.5 0 0 1 12 8.5a3.5 3.5 0 0 1 3.5 3.5a3.5 3.5 0 0 1-3.5 3.5m7.43-2.53c.04-.32.07-.64.07-.97c0-.33-.03-.66-.07-1l2.11-1.63c.19-.15.24-.42.12-.64l-2-3.46c-.12-.22-.39-.31-.61-.22l-2.49 1c-.52-.39-1.06-.73-1.69-.98l-.37-2.65A.506.506 0 0 0 14 2h-4c-.25 0-.46.18-.5.42l-.37 2.65c-.63.25-1.17.59-1.69.98l-2.49-1c-.22-.09-.49 0-.61.22l-2 3.46c-.13.22-.07.49.12.64L4.57 11c-.04.34-.07.67-.07 1c0 .33.03.65.07.97l-2.11 1.66c-.19.15-.25.42-.12.64l2 3.46c.12.22.39.3.61.22l2.49-1.01c.52.4 1.06.74 1.69.99l.37 2.65c.04.24.25.42.5.42h4c.25 0 .46-.18.5-.42l.37-2.65c.63-.26 1.17-.59 1.69-.99l2.49 1.01c.22.08.49 0 .61-.22l2-3.46c.12-.22.07-.49-.12-.64l-2.11-1.66Z"/>
                </svg>
                Device Settings
            </a></li>
            <li><a href="/network">
                <svg width="24" height="24" viewBox="0 0 24 24" fill="white" style="margin-right: 10px;">
                    <path d="M4.93 4.93C3.12 6.74 2 9.24 2 12c0 2.76 1.12 5.26 2.93 7.07l1.41-1.41A7.94 7.94 0 0 1 4 12c0-2.21.9-4.21 2.34-5.66l-1.41-1.41M19.07 4.93l-1.41 1.41A7.94 7.94 0 0 1 20 12c0 2.21-.9 4.21-2.34 5.66l1.41 1.41C20.88 17.26 22 14.76 22 12c0-2.76-1.12-5.26-2.93-7.07M7.76 7.76A4.5 4.5 0 0 0 6.5 12c0 1.25.51 2.38 1.26 3.24l1.41-1.41A2.47 2.47 0 0 1 8.5 12c0-.69.28-1.31.73-1.76L7.76 7.76M16.24 7.76l-1.41 1.41c.44.45.73 1.07.73 1.76c0 .69-.28 1.31-.73 1.76l1.41 1.41A4.5 4.5 0 0 0 17.5 12c0-1.25-.51-2.38-1.26-3.24M12 10a2 2 0 0 0-2 2a2 2 0 0 0 2 2a2 2 0 0 0 2-2a2 2 0 0 0-2-2Z"/>
                </svg>
                Network Settings
            </a></li>
            <li><a href="/analogworkswitch">
                <svg width="24" height="24" viewBox="0 0 24 24" fill="white" style="margin-right: 10px;">
                    <path d="M7 2v11h3v9l7-12h-4l4-8H7Z"/>
                </svg>
                Work Switch
            </a></li>
            <li><a href="/can">
                <svg width="24" height="24" viewBox="0 0 24 24" fill="white" style="margin-right: 10px;">
                    <path d="M17,12V6C17,5.45 16.55,5 16,5H13V7H16V11H12V13H16V17H13V19H16A1,1 0 0,0 17,18V12M3,5H11A1,1 0 0,1 12,6V18A1,1 0 0,1 11,19H3A1,1 0 0,1 2,18V6A1,1 0 0,1 3,5M4,7V11H10V7H4M4,13V17H10V13H4Z"/>
                </svg>
                CAN Steering
            </a></li>
            <li><a href="/eventlogger">
                <svg width="24" height="24" viewBox="0 0 24 24" fill="white" style="margin-right: 10px;">
                    <path d="M19 3H5c-1.1 0-2 .9-2 2v14c0 1.1.9 2 2 2h14c1.1 0 2-.9 2-2V5c0-1.1-.9-2-2-2m-5 14H7v-2h7v2m3-4H7v-2h10v2m0-4H7V7h10v2Z"/>
                </svg>
                Event Logger Config
            </a></li>
            <li><a href="/logs">
                <svg width="24" height="24" viewBox="0 0 24 24" fill="white" style="margin-right: 10px;">
                    <path d="M21 16v-2l-8-5V3.5c0-.83-.67-1.5-1.5-1.5S10 2.67 10 3.5V9l-8 5v2l8-2.5V19l-2 1.5V22l3.5-1 3.5 1v-1.5L13 19v-5.5l8 2.5Z"/>
                </svg>
                Live Log Viewer
            </a></li>
            <li><a href="/um98x-config">
                <svg width="24" height="24" viewBox="0 0 24 24" fill="white" style="margin-right: 10px;">
                    <path d="M12 2C8.13 2 5 5.13 5 9c0 5.25 7 13 7 13s7-7.75 7-13c0-3.87-3.13-7-7-7zm0 9.5c-1.38 0-2.5-1.12-2.5-2.5s1.12-2.5 2.5-2.5 2.5 1.12 2.5 2.5-1.12 2.5-2.5 2.5z"/>
                </svg>
                GPS Config
            </a></li>
        </nav>
        
        <h2>System</h2>
        <div class="grid">
            <button class="touch-button" onclick="location.href='/ota'">
                System Update
            </button>
            <button class="touch-button" onclick="confirmRestart()">
                Restart System
            </button>
        </div>
        
        <div class="info">
            <p>Firmware: <span id="firmwareVersion">Loading...</span></p>
            <p>WebSocket: <span id="wsStatus">Disconnected</span> | <span id="telemetryRate">0</span> Hz</p>
        </div>
    </div>
    
    <script>
        let ws;
        let lastPacketTime = 0;
        let packetCount = 0;
        let rateUpdateTime = 0;
        
        // Touch-friendly confirmation dialog
        function confirmRestart() {
            if (confirm('Are you sure you want to restart the system?')) {
                restartSystem();
            }
        }
        
        function restartSystem() {
            fetch('/api/restart', { method: 'POST' })
                .then(response => {
                    if (response.ok) {
                        alert('System is restarting...');
                    }
                });
        }
        
        function connectWebSocket() {
            const wsUrl = 'ws://' + window.location.hostname + ':8082';
            console.log('Attempting WebSocket connection to:', wsUrl);
            
            try {
                ws = new WebSocket(wsUrl);
                
                ws.onopen = function() {
                    console.log('WebSocket connected');
                    document.getElementById('wsStatus').textContent = 'Connected';
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
                };
            
                ws.onclose = function() {
                    console.log('WebSocket disconnected');
                    document.getElementById('wsStatus').textContent = 'Disconnected';
                    setTimeout(connectWebSocket, 3000);
                };
                
                ws.onerror = function(error) {
                    console.error('WebSocket error:', error);
                };
            } catch (e) {
                console.error('Failed to create WebSocket:', e);
                document.getElementById('wsStatus').textContent = 'Error';
            }
        }
        
        function updateDisplay(data) {
            // WebSocket data received - could be used for future updates
        }
        
        // Connect on page load - same as old page
        connectWebSocket();
        
        // Fetch firmware version
        fetch('/api/status')
            .then(response => response.json())
            .then(data => {
                if (data.version) {
                    document.getElementById('firmwareVersion').textContent = data.version;
                }
            })
            .catch(err => {
                document.getElementById('firmwareVersion').textContent = 'Unknown';
            });
        
        // Prevent zoom on double tap
        document.addEventListener('touchstart', function(event) {
            if (event.touches.length > 1) {
                event.preventDefault();
            }
        });
    </script>
</body>
</html>
)rawliteral";

#endif // TOUCH_FRIENDLY_HOME_PAGE_H