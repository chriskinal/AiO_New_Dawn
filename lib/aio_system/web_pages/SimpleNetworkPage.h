// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

// SimpleNetworkPage.h
// Simplified Network Settings page

#ifndef SIMPLE_NETWORK_PAGE_H
#define SIMPLE_NETWORK_PAGE_H

#include <Arduino.h>

const char SIMPLE_NETWORK_SETTINGS_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name='viewport' content='width=device-width, initial-scale=1'>
    <title>Network Settings - AiO New Dawn</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            margin: 20px;
            background-color: #f0f0f0;
        }
        .container {
            max-width: 800px;
            margin: 0 auto;
            background-color: white;
            padding: 20px;
            border-radius: 10px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
        }
        h1 { color: #333; }
        h2 { color: #555; }
        .status {
            margin: 10px 0;
            padding: 10px;
            background-color: #e8f5e9;
            border-radius: 5px;
        }
        .form-group { margin: 15px 0; }
        label { font-weight: bold; }
        input[type="number"] {
            width: 60px;
            padding: 5px;
            margin: 0 5px;
        }
        .btn {
            padding: 10px 20px;
            border: none;
            border-radius: 5px;
            cursor: pointer;
            margin: 5px;
            color: white;
        }
        .btn-home { background-color: #6c757d; }
        .btn-primary { background-color: #007bff; }
        .btn-warning { background-color: #ff9800; }
        .help-text { 
            color: #666; 
            font-size: 0.9em; 
            margin-top: 5px;
        }
        .nav-buttons {
            margin-top: 20px;
        }
    </style>
    <script>
        function saveIPSettings() {
            const octet1 = parseInt(document.getElementById('octet1').value);
            const octet2 = parseInt(document.getElementById('octet2').value);
            const octet3 = parseInt(document.getElementById('octet3').value);
            
            if (octet1 < 0 || octet1 > 255 || octet2 < 0 || octet2 > 255 || octet3 < 0 || octet3 > 255) {
                alert('Invalid IP address. Each octet must be between 0 and 255.');
                return false;
            }
            
            const formData = {
                ip: [octet1, octet2, octet3, 126]
            };
            
            fetch('/api/network/config', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify(formData)
            })
            .then(response => response.json())
            .then(data => {
                if (data.status === 'ok') {
                    alert('Settings saved successfully! Click Reboot to apply new IP address.');
                } else {
                    alert('Error saving settings: ' + data.error);
                }
            })
            .catch((error) => {
                alert('Error: ' + error);
            });
            
            return false;
        }
        
        function rebootSystem() {
            if (confirm('Are you sure you want to reboot? The system will be unavailable for a moment.')) {
                fetch('/api/restart', {
                    method: 'POST'
                })
                .then(() => {
                    alert('System is rebooting...');
                })
                .catch((error) => {
                    alert('Error: ' + error);
                });
            }
        }
        
        function loadSettings() {
            fetch('/api/network/config')
            .then(response => response.json())
            .then(data => {
                if (data.ip && data.ip.length >= 3) {
                    document.getElementById('octet1').value = data.ip[0];
                    document.getElementById('octet2').value = data.ip[1];
                    document.getElementById('octet3').value = data.ip[2];
                }
            })
            .catch((error) => {
                console.error('Error loading settings:', error);
            });
        }
        
        window.onload = loadSettings;
    </script>
</head>
<body>
    <div class='container'>
        <h1>Network Settings</h1>
        
        <div class='status'>
            <p>Current IP: %IP_ADDRESS%</p>
            <p>Link Speed: %LINK_SPEED% Mbps</p>
        </div>
        
        <form onsubmit='return false;'>
            <h2>IP Address Configuration</h2>
            
            <div class='form-group'>
                <label>IP Address:</label>
                <div style='display: flex; align-items: center;'>
                    <input type='number' id='octet1' min='0' max='255' required>
                    <span>.</span>
                    <input type='number' id='octet2' min='0' max='255' required>
                    <span>.</span>
                    <input type='number' id='octet3' min='0' max='255' required>
                    <span>.</span>
                    <span style='font-weight: bold;'>126</span>
                </div>
                <div class='help-text'>
                    Configure the first three octets of the IP address. The last octet is fixed at 126.
                </div>
            </div>
            
            <div class='nav-buttons'>
                <button type='button' class='btn btn-home' onclick='window.location="/"'>Home</button>
                <button type='button' class='btn btn-primary' onclick='saveIPSettings()'>Apply Changes</button>
                <button type='button' class='btn btn-warning' onclick='rebootSystem()'>Reboot</button>
            </div>
            
            <p style='margin-top: 20px; color: #666;'>
                <strong>Note:</strong> After saving, you must click Reboot for the new IP address to take effect.
            </p>
        </form>
    </div>
</body>
</html>
)rawliteral";

#endif // SIMPLE_NETWORK_PAGE_H