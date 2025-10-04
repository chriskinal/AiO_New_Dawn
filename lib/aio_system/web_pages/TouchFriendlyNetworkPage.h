// TouchFriendlyNetworkPage.h
// Touch-optimized network settings page with compact layout

#ifndef TOUCH_FRIENDLY_NETWORK_PAGE_H
#define TOUCH_FRIENDLY_NETWORK_PAGE_H

#include <Arduino.h>

const char TOUCH_FRIENDLY_NETWORK_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <meta name="apple-mobile-web-app-capable" content="yes">
    <title>Network Settings - AiO New Dawn</title>
    <link rel="stylesheet" href="/touch.css">
    <style>
        /* Additional styles specific to network settings */
        .status-row {
            display: flex;
            justify-content: space-between;
            align-items: center;
            padding: 15px 0;
            border-bottom: 1px solid #ecf0f1;
            font-size: 18px;
        }
        
        .status-row span:first-child {
            font-weight: 500;
            color: #7f8c8d;
        }
        
        .status-row span:last-child {
            font-weight: 600;
            color: #2c3e50;
        }
        
        .ip-input-group {
            display: flex;
            align-items: center;
            justify-content: center;
            margin: 20px 0;
            font-size: 20px;
        }
        
        .ip-input-group input[type="number"] {
            width: 70px;
            padding: 15px;
            margin: 0 5px;
            font-size: 20px;
            text-align: center;
            border: 2px solid #bdc3c7;
            border-radius: 8px;
            -webkit-appearance: none;
            -moz-appearance: textfield;
        }
        
        .ip-input-group input[type="number"]::-webkit-inner-spin-button,
        .ip-input-group input[type="number"]::-webkit-outer-spin-button {
            -webkit-appearance: none;
            margin: 0;
        }
        
        .ip-input-group span {
            font-weight: 600;
            color: #34495e;
        }
        
        .fixed-octet {
            display: inline-block;
            min-width: 70px;
            text-align: center;
            font-weight: 600;
            color: #3498db;
        }
        
        .help-box {
            background: #ecf0f1;
            padding: 15px;
            border-radius: 8px;
            margin: 20px 0;
            text-align: center;
        }
        
        .help-box p {
            margin: 5px 0;
            font-size: 16px;
            color: #7f8c8d;
        }
        
        .nav-buttons {
            display: grid;
            grid-template-columns: 1fr 1fr 1fr;
            gap: 15px;
            margin-bottom: 20px;
        }
        
        @media (max-width: 600px) {
            .nav-buttons {
                grid-template-columns: 1fr;
            }
        }
        
        .reboot-btn {
            background: #e74c3c;
        }
        
        .reboot-btn:active {
            background: #c0392b;
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
            
            // Show saving status
            document.getElementById('status').innerHTML = 
                '<div class="status">Saving network settings...</div>';
            
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
                    document.getElementById('status').innerHTML = 
                        '<div class="status success">Settings saved! Click Reboot to apply.</div>';
                    setTimeout(() => {
                        document.getElementById('status').innerHTML = '';
                    }, 5000);
                } else {
                    document.getElementById('status').innerHTML = 
                        '<div class="status error">Error saving settings: ' + data.error + '</div>';
                    setTimeout(() => {
                        document.getElementById('status').innerHTML = '';
                    }, 5000);
                }
            })
            .catch((error) => {
                document.getElementById('status').innerHTML = 
                    '<div class="status error">Error: ' + error + '</div>';
                setTimeout(() => {
                    document.getElementById('status').innerHTML = '';
                }, 5000);
            });
            
            return false;
        }
        
        function rebootSystem() {
            if (confirm('Are you sure you want to reboot? The system will be unavailable for a moment.')) {
                document.getElementById('status').innerHTML = 
                    '<div class="status">System is rebooting...</div>';
                    
                fetch('/api/restart', {
                    method: 'POST'
                })
                .then(() => {
                    // Show countdown
                    let countdown = 10;
                    const interval = setInterval(() => {
                        document.getElementById('status').innerHTML = 
                            '<div class="status">Rebooting... Reconnect in ' + countdown + ' seconds</div>';
                        countdown--;
                        if (countdown < 0) {
                            clearInterval(interval);
                            document.getElementById('status').innerHTML = 
                                '<div class="status">You may need to refresh the page</div>';
                        }
                    }, 1000);
                })
                .catch((error) => {
                    document.getElementById('status').innerHTML = 
                        '<div class="status error">Error: ' + error + '</div>';
                    setTimeout(() => {
                        document.getElementById('status').innerHTML = '';
                    }, 5000);
                });
            }
        }
        
        function loadSettings() {
            // Load IP configuration
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
            
            // Load current status
            fetch('/api/status')
            .then(response => response.json())
            .then(data => {
                if (data.network) {
                    document.getElementById('currentIP').textContent = data.network.ip || 'Unknown';
                    document.getElementById('linkSpeed').textContent = data.network.linkSpeed || '--';
                }
            })
            .catch((error) => {
                console.error('Error loading status:', error);
                document.getElementById('currentIP').textContent = 'Error';
                document.getElementById('linkSpeed').textContent = '--';
            });
        }
        
        window.onload = loadSettings;
    </script>
</head>
<body>
    <div class="container">
        <h1>Network Settings</h1>
        
        <div class="nav-buttons">
            <button type="button" class="touch-button" style="background: #7f8c8d;" 
                    onclick="window.location.href='/'">
                Back to Home
            </button>
            <button type="button" class="touch-button" onclick="saveIPSettings()">
                Apply Changes
            </button>
            <button type="button" class="touch-button reboot-btn" onclick="rebootSystem()">
                Reboot
            </button>
        </div>
        
        <div id="status"></div>
        
        <div class="card">
            <div class="status-row">
                <span>IP: <span id="currentIP">Loading...</span></span>
                <span>Speed: <span id="linkSpeed">--</span> Mbps</span>
            </div>
            
            <form onsubmit="return false;">
                <div class="form-group">
                    <label style="text-align: center; display: block; margin-bottom: 10px;">
                        Configure IP Address
                    </label>
                    <div class="ip-input-group">
                        <input type="number" id="octet1" min="0" max="255" required>
                        <span>.</span>
                        <input type="number" id="octet2" min="0" max="255" required>
                        <span>.</span>
                        <input type="number" id="octet3" min="0" max="255" required>
                        <span>.</span>
                        <span class="fixed-octet">126</span>
                    </div>
                    <div class="help-text" style="text-align: center;">
                        The last octet is fixed at 126 for the steer module
                    </div>
                </div>
            </form>
        </div>
        
        <div class="help-box">
            <p><strong>Note:</strong> After saving, you must reboot for the new IP to take effect.</p>
            <p>The network will be briefly unavailable during reboot.</p>
        </div>
    </div>
</body>
</html>
)rawliteral";

#endif // TOUCH_FRIENDLY_NETWORK_PAGE_H