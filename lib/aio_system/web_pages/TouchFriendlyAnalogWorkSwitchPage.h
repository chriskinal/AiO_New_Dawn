// TouchFriendlyAnalogWorkSwitchPage.h
// Touch-optimized analog work switch page with WebSocket support

#ifndef TOUCH_FRIENDLY_ANALOG_WORK_SWITCH_PAGE_H
#define TOUCH_FRIENDLY_ANALOG_WORK_SWITCH_PAGE_H

#include <Arduino.h>

const char TOUCH_FRIENDLY_ANALOG_WORK_SWITCH_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <meta name="apple-mobile-web-app-capable" content="yes">
    <title>Analog Work Switch - AiO New Dawn</title>
    <link rel="stylesheet" href="/touch.css">
    <style>
        /* Additional styles specific to analog work switch */
        .reading-display {
            font-size: 48px;
            font-weight: 600;
            text-align: center;
            margin: 20px 0;
            color: #2c3e50;
        }
        
        .status-indicator {
            display: inline-block;
            padding: 8px 16px;
            border-radius: 20px;
            font-size: 18px;
            font-weight: 500;
            margin-left: 15px;
        }
        
        .status-on {
            background: #2ecc71;
            color: white;
        }
        
        .status-off {
            background: #e74c3c;
            color: white;
        }
        
        .bar-container {
            width: 100%;
            height: 80px;
            background-color: #ecf0f1;
            border: 3px solid #34495e;
            border-radius: 10px;
            position: relative;
            margin: 30px 0;
            overflow: hidden;
        }
        
        .bar-fill {
            height: 100%;
            background-color: #3498db;
            width: 0%;
            transition: width 0.2s, background-color 0.3s;
        }
        
        .bar-zone {
            position: absolute;
            top: 0;
            height: 100%;
            opacity: 0.3;
        }
        
        .bar-marker {
            position: absolute;
            top: -5px;
            width: 4px;
            height: 90px;
            background-color: #2c3e50;
            border-radius: 2px;
            box-shadow: 0 0 4px rgba(0,0,0,0.3);
        }
        
        .zone-label {
            position: absolute;
            top: -25px;
            font-size: 12px;
            color: #7f8c8d;
            transform: translateX(-50%);
        }
        
        .control-grid {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 15px;
            margin: 20px 0;
        }
        
        .toggle-container {
            display: flex;
            align-items: center;
            justify-content: space-between;
            padding: 15px;
            background: #f8f9fa;
            border-radius: 8px;
        }
        
        .toggle-label {
            font-size: 18px;
            font-weight: 500;
        }
        
        .setpoint-row {
            display: flex;
            gap: 15px;
            align-items: center;
            margin: 10px 0;
        }
        
        .setpoint-display {
            background: #3498db;
            color: white;
            padding: 15px 20px;
            border-radius: 8px;
            font-size: 20px;
            font-weight: 500;
            text-align: center;
            flex: 1;
            display: flex;
            align-items: center;
            justify-content: center;
            min-height: 60px;
        }
        
        .setpoint-row .touch-button {
            flex: 1;
            margin: 0;
            padding: 15px 20px;
            min-height: 60px;
            font-size: 20px;
        }
        
        select {
            width: 100%;
            padding: 15px;
            font-size: 18px;
            border: 2px solid #bdc3c7;
            border-radius: 8px;
            background: white;
            -webkit-appearance: none;
            appearance: none;
            background-image: url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='12' height='12' viewBox='0 0 12 12'%3E%3Cpath fill='%23333' d='M10.293 3.293L6 7.586 1.707 3.293A1 1 0 00.293 4.707l5 5a1 1 0 001.414 0l5-5a1 1 0 10-1.414-1.414z'/%3E%3C/svg%3E");
            background-repeat: no-repeat;
            background-position: right 15px center;
            padding-right: 45px;
        }
        
        .nav-buttons {
            display: grid;
            grid-template-columns: 1fr;
            gap: 15px;
            margin-bottom: 20px;
        }
        
        .ws-status {
            text-align: center;
            font-size: 14px;
            color: #7f8c8d;
            margin-top: 10px;
        }
        
        @media (max-width: 600px) {
            .control-grid {
                grid-template-columns: 1fr;
            }
            
            .reading-display {
                font-size: 36px;
            }
        }
    </style>
    <script>
        var ws = null;
        var config = {enabled: false, setpoint: 50, hysteresis: 20, invert: false};
        
        function connectWebSocket() {
            ws = new WebSocket('ws://' + window.location.hostname + ':8082');
            ws.binaryType = 'arraybuffer';
            
            ws.onopen = function() {
                document.getElementById('wsStatus').textContent = 'WebSocket Connected';
                document.getElementById('wsStatus').style.color = '#2ecc71';
            };
            
            ws.onmessage = function(event) {
                if (event.data instanceof ArrayBuffer) {
                    const view = new DataView(event.data);
                    
                    // Parse telemetry packet - work switch data at bytes 29-30
                    const work_switch = view.getUint8(29);
                    const work_analog_percent = view.getUint8(30);
                    
                    // Update display
                    updateDisplay(work_analog_percent, work_switch);
                }
            };
            
            ws.onclose = function() {
                document.getElementById('wsStatus').textContent = 'WebSocket Disconnected - Reconnecting...';
                document.getElementById('wsStatus').style.color = '#e74c3c';
                setTimeout(connectWebSocket, 2000);
            };
            
            ws.onerror = function(error) {
                console.error('WebSocket error:', error);
            };
        }
        
        function updateDisplay(percent, state) {
            // Update reading
            document.getElementById('percentValue').textContent = percent;
            
            // Update status indicator
            var statusEl = document.getElementById('statusIndicator');
            statusEl.textContent = state ? 'ON' : 'OFF';
            statusEl.className = 'status-indicator ' + (state ? 'status-on' : 'status-off');
            
            // Update bar
            var barEl = document.getElementById('bar');
            barEl.style.width = percent + '%';
            barEl.style.backgroundColor = state ? '#2ecc71' : '#e74c3c';
            
            // Update hysteresis zones
            updateHysteresisZones(config.setpoint, config.hysteresis);
        }
        
        function updateHysteresisZones(setpoint, hysteresis) {
            var halfHyst = hysteresis / 2;
            var lowerBound = Math.max(0, setpoint - halfHyst);
            var upperBound = Math.min(100, setpoint + halfHyst);
            
            var lowerZone = document.getElementById('lowerZone');
            lowerZone.style.left = lowerBound + '%';
            lowerZone.style.width = (setpoint - lowerBound) + '%';
            
            var upperZone = document.getElementById('upperZone');
            upperZone.style.left = setpoint + '%';
            upperZone.style.width = (upperBound - setpoint) + '%';
            
            var marker = document.getElementById('setpointMarker');
            marker.style.left = setpoint + '%';
            
            // Update label
            document.getElementById('setpointLabel').style.left = setpoint + '%';
            document.getElementById('setpointLabel').textContent = setpoint + '%';
        }
        
        function loadConfig() {
            fetch('/api/analogworkswitch/status')
            .then(response => response.json())
            .then(data => {
                config.enabled = data.enabled;
                config.setpoint = data.setpoint;
                config.hysteresis = data.hysteresis;
                config.invert = data.invert;
                
                document.getElementById('enable').checked = data.enabled;
                document.getElementById('setpointValue').textContent = data.setpoint + '%';
                document.getElementById('hyst').value = data.hysteresis;
                document.getElementById('invert').checked = data.invert;
                
                updateDisplay(Math.floor(data.percent), data.state);
            })
            .catch(error => {
                console.error('Error loading config:', error);
                document.getElementById('status').innerHTML = 
                    '<div class="status error">Error loading configuration</div>';
            });
        }
        
        function toggleEnable() {
            var enabled = document.getElementById('enable').checked;
            saveConfig({enabled: enabled});
        }
        
        function setSetpoint() {
            document.getElementById('status').innerHTML = 
                '<div class="status">Setting current value as setpoint...</div>';
                
            fetch('/api/analogworkswitch/setpoint', { method: 'POST' })
            .then(response => response.json())
            .then(data => {
                config.setpoint = data.newSetpoint;
                document.getElementById('setpointValue').textContent = data.newSetpoint + '%';
                document.getElementById('status').innerHTML = 
                    '<div class="status success">Setpoint saved!</div>';
                updateHysteresisZones(config.setpoint, config.hysteresis);
            })
            .catch(error => {
                document.getElementById('status').innerHTML = 
                    '<div class="status error">Error setting setpoint</div>';
            });
        }
        
        function updateHyst() {
            var h = document.getElementById('hyst').value;
            config.hysteresis = parseInt(h);
            saveConfig({hysteresis: config.hysteresis});
            updateHysteresisZones(config.setpoint, config.hysteresis);
        }
        
        function toggleInvert() {
            config.invert = document.getElementById('invert').checked;
            saveConfig({invert: config.invert});
        }
        
        function saveConfig(data) {
            fetch('/api/analogworkswitch/config', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(data)
            })
            .then(response => response.json())
            .then(result => {
                document.getElementById('status').innerHTML = 
                    '<div class="status success">Settings saved!</div>';
                setTimeout(() => {
                    document.getElementById('status').innerHTML = '';
                }, 5000);
                setTimeout(loadConfig, 100);
            })
            .catch(error => {
                document.getElementById('status').innerHTML = 
                    '<div class="status error">Error saving settings</div>';
                setTimeout(() => {
                    document.getElementById('status').innerHTML = '';
                }, 5000);
            });
        }
        
        window.onload = function() {
            connectWebSocket();
            loadConfig();
        };
        
        window.onbeforeunload = function() {
            if (ws) ws.close();
        };
    </script>
</head>
<body>
    <div class="container">
        <h1>Analog Work Switch</h1>
        
        <div class="nav-buttons">
            <button type="button" class="touch-button" style="background: #7f8c8d;" 
                    onclick="window.location.href='/'">
                Back to Home
            </button>
        </div>
        
        <div id="status"></div>
        
        <div class="card">
            <div class="reading-display">
                <span id="percentValue">--</span>%
                <span id="statusIndicator" class="status-indicator status-off">OFF</span>
            </div>
            
            <div class="bar-container">
                <div class="bar-fill" id="bar"></div>
                <div class="bar-zone" id="lowerZone" style="background-color: #2ecc71;"></div>
                <div class="bar-zone" id="upperZone" style="background-color: #e74c3c;"></div>
                <div class="bar-marker" id="setpointMarker"></div>
                <div class="zone-label" id="setpointLabel">50%</div>
            </div>
            
            <div class="setpoint-row">
                <div class="setpoint-display">
                    Setpoint: <span id="setpointValue">50%</span>
                </div>
                
                <button class="touch-button" onclick="setSetpoint()">
                    Set Current as Setpoint
                </button>
            </div>
        </div>
        
        <div class="card">
            <h2>Configuration</h2>
            
            <div class="control-grid">
                <div class="toggle-container">
                    <label for="enable" class="toggle-label">Analog Mode</label>
                    <label class="toggle-switch">
                        <input type="checkbox" id="enable" onchange="toggleEnable()">
                        <span class="toggle-slider"></span>
                    </label>
                </div>
                
                <div class="toggle-container">
                    <label for="invert" class="toggle-label">Invert Logic</label>
                    <label class="toggle-switch">
                        <input type="checkbox" id="invert" onchange="toggleInvert()">
                        <span class="toggle-slider"></span>
                    </label>
                </div>
            </div>
            
            <div class="form-group">
                <label for="hyst">Hysteresis:</label>
                <select id="hyst" onchange="updateHyst()">
                    <option value="1">1%</option>
                    <option value="5">5%</option>
                    <option value="10">10%</option>
                    <option value="15">15%</option>
                    <option value="20" selected>20%</option>
                    <option value="25">25%</option>
                    <option value="30">30%</option>
                </select>
            </div>
        </div>
        
        <div class="ws-status" id="wsStatus">WebSocket Disconnected</div>
    </div>
</body>
</html>
)rawliteral";

#endif // TOUCH_FRIENDLY_ANALOG_WORK_SWITCH_PAGE_H