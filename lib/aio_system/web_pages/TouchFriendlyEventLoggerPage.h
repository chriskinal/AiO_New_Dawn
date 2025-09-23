// TouchFriendlyEventLoggerPage.h
// Touch-optimized event logger configuration page

#ifndef TOUCH_FRIENDLY_EVENT_LOGGER_PAGE_H
#define TOUCH_FRIENDLY_EVENT_LOGGER_PAGE_H

#include <Arduino.h>

const char TOUCH_FRIENDLY_EVENT_LOGGER_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <meta name="apple-mobile-web-app-capable" content="yes">
    <title>Event Logger - AiO New Dawn</title>
    <link rel="stylesheet" href="/touch.css">
    <style>
        /* Additional styles specific to event logger */
        .config-section {
            padding: 15px 0;
            border-bottom: 1px solid #ecf0f1;
        }
        
        .config-section:last-child {
            border-bottom: none;
        }
        
        .toggle-row {
            display: flex;
            justify-content: space-between;
            align-items: center;
            padding: 10px 0;
        }
        
        .toggle-label {
            font-size: 18px;
            font-weight: 600;
            color: #2c3e50;
        }
        
        .level-select {
            width: 150px;
            padding: 12px;
            font-size: 18px;
            border: 2px solid #bdc3c7;
            border-radius: 8px;
            background: white;
            -webkit-appearance: none;
            appearance: none;
            background-image: url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='12' height='12' viewBox='0 0 12 12'%3E%3Cpath fill='%23333' d='M10.293 3.293L6 7.586 1.707 3.293A1 1 0 00.293 4.707l5 5a1 1 0 001.414 0l5-5a1 1 0 10-1.414-1.414z'/%3E%3C/svg%3E");
            background-repeat: no-repeat;
            background-position: right 12px center;
            padding-right: 40px;
        }
        
        .warning-text {
            color: #e74c3c;
            font-size: 14px;
            margin-top: 5px;
            font-style: italic;
        }
        
        .info-box {
            background: #e3f2fd;
            padding: 15px;
            border-radius: 8px;
            margin: 20px 0;
            text-align: center;
            color: #1976d2;
            font-size: 16px;
        }
        
        .nav-buttons {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 15px;
            margin-bottom: 20px;
        }
        
        @media (max-width: 600px) {
            .level-select {
                width: 120px;
            }
        }
    </style>
    <script>
        function loadConfig() {
            fetch('/api/eventlogger/config')
            .then(response => response.json())
            .then(data => {
                document.getElementById('serialEnabled').checked = data.serialEnabled;
                document.getElementById('serialLevel').value = data.serialLevel;
                document.getElementById('udpEnabled').checked = data.udpEnabled;
                document.getElementById('udpLevel').value = data.udpLevel;
                document.getElementById('rateLimitDisabled').checked = data.rateLimitDisabled;
            })
            .catch(error => {
                console.error('Error loading config:', error);
                document.getElementById('status').innerHTML = 
                    '<div class="status error">Error loading configuration</div>';
                setTimeout(() => {
                    document.getElementById('status').innerHTML = '';
                }, 5000);
            });
        }
        
        function saveConfig() {
            document.getElementById('status').innerHTML = 
                '<div class="status">Saving configuration...</div>';
                
            const data = {
                serialEnabled: document.getElementById('serialEnabled').checked,
                serialLevel: parseInt(document.getElementById('serialLevel').value),
                udpEnabled: document.getElementById('udpEnabled').checked,
                udpLevel: parseInt(document.getElementById('udpLevel').value),
                rateLimitDisabled: document.getElementById('rateLimitDisabled').checked
            };
            
            fetch('/api/eventlogger/config', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify(data)
            })
            .then(response => {
                if (response.ok) {
                    document.getElementById('status').innerHTML = 
                        '<div class="status success">Configuration saved!</div>';
                    setTimeout(() => {
                        document.getElementById('status').innerHTML = '';
                    }, 5000);
                } else {
                    document.getElementById('status').innerHTML = 
                        '<div class="status error">Error saving configuration</div>';
                    setTimeout(() => {
                        document.getElementById('status').innerHTML = '';
                    }, 5000);
                }
            })
            .catch(error => {
                document.getElementById('status').innerHTML = 
                    '<div class="status error">Error: ' + error + '</div>';
                setTimeout(() => {
                    document.getElementById('status').innerHTML = '';
                }, 5000);
            });
        }
        
        window.onload = loadConfig;
    </script>
</head>
<body>
    <div class="container">
        <h1>Event Logger Configuration</h1>
        
        <div class="nav-buttons">
            <button type="button" class="touch-button" style="background: #7f8c8d;" 
                    onclick="window.location.href='/'">
                Back to Home
            </button>
            <button type="button" class="touch-button" onclick="saveConfig()">
                Apply Changes
            </button>
        </div>
        
        <div id="status"></div>
        
        <div class="card">
            <div class="config-section">
                <div class="toggle-row">
                    <label for="serialEnabled" class="toggle-label">Serial Console</label>
                    <label class="toggle-switch">
                        <input type="checkbox" id="serialEnabled">
                        <span class="toggle-slider"></span>
                    </label>
                </div>
                <div class="toggle-row">
                    <label for="serialLevel" class="toggle-label">Serial Log Level</label>
                    <select id="serialLevel" class="level-select">
                        <option value="0">Emergency</option>
                        <option value="1">Alert</option>
                        <option value="2">Critical</option>
                        <option value="3">Error</option>
                        <option value="4">Warning</option>
                        <option value="5">Notice</option>
                        <option value="6">Info</option>
                        <option value="7">Debug</option>
                    </select>
                </div>
            </div>
            
            <div class="config-section">
                <div class="toggle-row">
                    <label for="udpEnabled" class="toggle-label">Remote Syslog (UDP)</label>
                    <label class="toggle-switch">
                        <input type="checkbox" id="udpEnabled">
                        <span class="toggle-slider"></span>
                    </label>
                </div>
                <div class="toggle-row">
                    <label for="udpLevel" class="toggle-label">UDP Log Level</label>
                    <select id="udpLevel" class="level-select">
                        <option value="0">Emergency</option>
                        <option value="1">Alert</option>
                        <option value="2">Critical</option>
                        <option value="3">Error</option>
                        <option value="4">Warning</option>
                        <option value="5">Notice</option>
                        <option value="6">Info</option>
                        <option value="7">Debug</option>
                    </select>
                </div>
            </div>
            
            <div class="config-section">
                <div class="toggle-row">
                    <label for="rateLimitDisabled" class="toggle-label">Disable Rate Limiting</label>
                    <label class="toggle-switch">
                        <input type="checkbox" id="rateLimitDisabled">
                        <span class="toggle-slider"></span>
                    </label>
                </div>
                <div class="warning-text">
                    Warning: May flood console with messages
                </div>
            </div>
            
            <div class="info-box" style="margin: 15px 0 0 0;">
                Syslog messages are sent to the server IP configured by your network admin on port 514
            </div>
        </div>
    </div>
</body>
</html>
)rawliteral";

#endif // TOUCH_FRIENDLY_EVENT_LOGGER_PAGE_H