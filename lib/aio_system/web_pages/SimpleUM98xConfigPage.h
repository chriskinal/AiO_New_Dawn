// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

// SimpleUM98xConfigPage.h - Web page for UM98x GPS configuration
#ifndef SIMPLE_UM98X_CONFIG_PAGE_H
#define SIMPLE_UM98X_CONFIG_PAGE_H

#include "Arduino.h"

const char UM98X_CONFIG_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>UM98x GPS Configuration</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            margin: 0;
            padding: 20px;
            background-color: #f0f0f0;
        }
        .container {
            max-width: 800px;
            margin: 0 auto;
            background-color: white;
            padding: 20px;
            border-radius: 8px;
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
        }
        h1 {
            color: #333;
            margin-bottom: 20px;
        }
        .config-section {
            margin-bottom: 20px;
        }
        label {
            display: block;
            font-weight: bold;
            margin-bottom: 5px;
            color: #555;
        }
        textarea {
            width: 100%;
            padding: 8px;
            border: 1px solid #ddd;
            border-radius: 4px;
            font-family: 'Courier New', monospace;
            font-size: 12px;
            box-sizing: border-box;
            resize: vertical;
        }
        input[type="text"] {
            width: 100%;
            padding: 8px;
            border: 1px solid #ddd;
            border-radius: 4px;
            font-family: 'Courier New', monospace;
            font-size: 12px;
            box-sizing: border-box;
        }
        .button-group {
            margin-top: 20px;
            display: flex;
            gap: 10px;
        }
        button {
            padding: 10px 20px;
            border: none;
            border-radius: 4px;
            cursor: pointer;
            font-size: 14px;
            transition: background-color 0.3s;
        }
        .btn-primary {
            background-color: #007bff;
            color: white;
        }
        .btn-primary:hover {
            background-color: #0056b3;
        }
        .btn-secondary {
            background-color: #6c757d;
            color: white;
        }
        .btn-secondary:hover {
            background-color: #545b62;
        }
        .btn-danger {
            background-color: #dc3545;
            color: white;
        }
        .btn-danger:hover {
            background-color: #c82333;
        }
        #status {
            margin-top: 20px;
            padding: 10px;
            border-radius: 4px;
            min-height: 20px;
        }
        .status-success {
            background-color: #d4edda;
            color: #155724;
            border: 1px solid #c3e6cb;
        }
        .status-error {
            background-color: #f8d7da;
            color: #721c24;
            border: 1px solid #f5c6cb;
        }
        .status-info {
            background-color: #d1ecf1;
            color: #0c5460;
            border: 1px solid #bee5eb;
        }
        .loading {
            opacity: 0.6;
            pointer-events: none;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>UM98x GPS Configuration</h1>
        
        <div class="config-section">
            <label for="config">Configuration Commands:</label>
            <textarea id="config" rows="20" placeholder="Click 'Read from GPS' to load current configuration..."></textarea>
        </div>
        
        <div class="config-section">
            <label for="mode">Mode Setting:</label>
            <input type="text" id="mode" placeholder="Click 'Read from GPS' to load current mode...">
        </div>
        
        <div class="config-section">
            <label for="messages">Message Output:</label>
            <textarea id="messages" rows="6" placeholder="Click 'Read from GPS' to load current message outputs..."></textarea>
        </div>
        
        <div id="status"></div>
        
        <div class="button-group">
            <button class="btn-secondary" onclick="window.location.href='/'">Home</button>
            <button class="btn-primary" onclick="readConfig()">Read from GPS</button>
            <button class="btn-primary" onclick="writeConfig()">Save to GPS</button>
            <button class="btn-secondary" onclick="loadUM981Defaults()">Load UM981 Defaults</button>
            <button class="btn-secondary" onclick="clearAll()">Clear All</button>
        </div>
    </div>
    
    <script>
        (function() {
            function showStatus(message, type = 'info') {
                const statusDiv = document.getElementById('status');
                statusDiv.textContent = message;
                statusDiv.className = 'status-' + type;
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
            
            window.readConfig = async function() {
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
            };
            
            window.writeConfig = async function() {
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
            };
            
            window.clearAll = function() {
                if (confirm('Clear all configuration fields?')) {
                    document.getElementById('config').value = '';
                    document.getElementById('mode').value = '';
                    document.getElementById('messages').value = '';
                    showStatus('All fields cleared', 'info');
                }
            };
            
            window.loadUM981Defaults = function() {
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
            };
        })();
    </script>
</body>
</html>
)rawliteral";

#endif // SIMPLE_UM98X_CONFIG_PAGE_H