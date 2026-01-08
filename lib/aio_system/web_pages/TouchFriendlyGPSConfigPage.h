// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

// TouchFriendlyGPSConfigPage.h
// Touch-optimized GPS configuration page

#ifndef TOUCH_FRIENDLY_GPS_CONFIG_PAGE_H
#define TOUCH_FRIENDLY_GPS_CONFIG_PAGE_H

#include <Arduino.h>

const char TOUCH_FRIENDLY_GPS_CONFIG_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <meta name="apple-mobile-web-app-capable" content="yes">
    <title>GPS Configuration - AiO New Dawn</title>
    <link rel="stylesheet" href="/touch.css">
    <style>
        /* Additional styles specific to GPS config */
        textarea {
            width: 100%;
            padding: 12px;
            border: 2px solid #bdc3c7;
            border-radius: 8px;
            font-family: 'Courier New', monospace;
            font-size: 14px;
            box-sizing: border-box;
            resize: vertical;
            background: #f8f9fa;
        }
        
        input[type="text"] {
            width: 100%;
            padding: 12px;
            border: 2px solid #bdc3c7;
            border-radius: 8px;
            font-family: 'Courier New', monospace;
            font-size: 16px;
            box-sizing: border-box;
            background: #f8f9fa;
        }
        
        .config-group {
            margin-bottom: 20px;
        }
        
        .config-group label {
            display: block;
            font-weight: 600;
            margin-bottom: 8px;
            color: #2c3e50;
            font-size: 18px;
        }
        
        .button-grid {
            display: grid;
            grid-template-columns: 1fr 1fr 1fr 1fr;
            gap: 15px;
            margin-bottom: 20px;
        }
        
        .nav-buttons {
            display: grid;
            grid-template-columns: 1fr;
            gap: 15px;
            margin-bottom: 20px;
        }
        
        .btn-danger {
            background: #e74c3c;
        }
        
        .btn-danger:active {
            background: #c0392b;
        }
        
        .btn-secondary {
            background: #95a5a6;
        }
        
        .btn-secondary:active {
            background: #7f8c8d;
        }
        
        .loading {
            opacity: 0.6;
            pointer-events: none;
        }
        
        @media (max-width: 600px) {
            .button-grid {
                grid-template-columns: 1fr 1fr;
            }
            
            textarea {
                font-size: 12px;
            }
        }
    </style>
    <script>
        function showStatus(message, type = 'info') {
            const statusDiv = document.getElementById('status');
            const classes = {
                'success': 'status success',
                'error': 'status error',
                'info': 'status'
            };
            statusDiv.className = classes[type] || 'status';
            statusDiv.textContent = message;
            statusDiv.style.display = 'block';
            
            if (type === 'success') {
                setTimeout(() => {
                    statusDiv.style.display = 'none';
                }, 5000);
            }
        }
        
        function setLoading(loading) {
            document.body.classList.toggle('loading', loading);
            const buttons = document.querySelectorAll('button');
            buttons.forEach(btn => btn.disabled = loading);
        }
        
        async function readConfig() {
            setLoading(true);
            showStatus('Reading configuration from GPS...', 'info');
            
            try {
                const response = await fetch('/api/um98x/read');
                const data = await response.json();
                
                if (data.success) {
                    document.getElementById('config').value = data.config || '';
                    document.getElementById('mode').value = data.mode || '';
                    document.getElementById('messages').value = data.messages || '';
                    showStatus('Configuration read successfully', 'success');
                } else {
                    showStatus('Error: ' + (data.error || 'Failed to read configuration'), 'error');
                }
            } catch (error) {
                showStatus('Error: ' + error.message, 'error');
            } finally {
                setLoading(false);
            }
        }
        
        async function writeConfig() {
            setLoading(true);
            showStatus('Writing configuration to GPS...', 'info');
            
            const config = {
                config: document.getElementById('config').value.trim(),
                mode: document.getElementById('mode').value.trim(),
                messages: document.getElementById('messages').value.trim()
            };
            
            try {
                const response = await fetch('/api/um98x/write', {
                    method: 'POST',
                    headers: {
                        'Content-Type': 'application/json'
                    },
                    body: JSON.stringify(config)
                });
                
                const data = await response.json();
                
                if (data.success) {
                    showStatus('Configuration saved to GPS EEPROM successfully', 'success');
                } else {
                    showStatus('Error: ' + (data.error || 'Failed to write configuration'), 'error');
                }
            } catch (error) {
                showStatus('Error: ' + error.message, 'error');
            } finally {
                setLoading(false);
            }
        }
        
        function clearAll() {
            if (confirm('Clear all configuration fields?')) {
                document.getElementById('config').value = '';
                document.getElementById('mode').value = '';
                document.getElementById('messages').value = '';
                showStatus('All fields cleared', 'info');
            }
        }
        
        function loadUM981Defaults() {
            const defaultConfig = `CONFIG INS ANGLE 0 0 0
CONFIG IMUTOANT OFFSET 0.00 0.00 0.00 0.01 0.01 0.01
CONFIG INSSOL OFFSET 0.00 0.00 0.00
CONFIG INS TIMEOUT 60
CONFIG INS ALIGNMENTVEL 1.0
CONFIG INSDIRECTION AUTO
CONFIG ANTENNA POWERON
CONFIG NMEAVERSION V410
CONFIG RTK TIMEOUT 120
CONFIG RTK RELIABILITY 3 1
CONFIG BESTPARA 16.00 10.00
CONFIG DGPS TIMEOUT 300
CONFIG RTCMB1CB2A ENABLE
CONFIG ANTENNADELTAHEN 0.0000 0.0000 0.0000
CONFIG PPS ENABLE GPS POSITIVE 500000 1000 0 0
CONFIG SIGNALGROUP 1
CONFIG ANTIJAM AUTO
CONFIG AGNSS DISABLE
CONFIG COM1 460800
CONFIG COM2 460800
CONFIG COM3 460800`;
            
            document.getElementById('config').value = defaultConfig;
            document.getElementById('mode').value = 'MODE ROVER SURVEY';
            document.getElementById('messages').value = 'INSPVAXA COM3 0.1';
            showStatus('UM981 default configuration loaded', 'success');
        }
    </script>
</head>
<body>
    <div class="container">
        <h1>GPS Configuration</h1>
        
        <div class="nav-buttons">
            <button type="button" class="touch-button" style="background: #7f8c8d;" 
                    onclick="window.location.href='/'">
                Back to Home
            </button>
        </div>
        
        <div class="button-grid">
            <button class="touch-button" onclick="readConfig()">Read</button>
            <button class="touch-button" onclick="writeConfig()">Save</button>
            <button class="touch-button btn-secondary" onclick="loadUM981Defaults()">UM981 Defaults</button>
            <button class="touch-button btn-danger" onclick="clearAll()">Clear</button>
        </div>
        
        <div class="card">
            <div class="config-group">
                <label for="config">Configuration Commands</label>
                <textarea id="config" rows="15" placeholder="Click 'Read from GPS' to load current configuration..."></textarea>
            </div>
            
            <div class="config-group">
                <label for="mode">Mode Setting</label>
                <input type="text" id="mode" placeholder="Click 'Read from GPS' to load current mode...">
            </div>
            
            <div class="config-group">
                <label for="messages">Message Output</label>
                <textarea id="messages" rows="4" placeholder="Click 'Read from GPS' to load current message outputs..."></textarea>
            </div>
        </div>
        
        <div id="status" style="display: none;"></div>
    </div>
</body>
</html>
)rawliteral";

#endif // TOUCH_FRIENDLY_GPS_CONFIG_PAGE_H