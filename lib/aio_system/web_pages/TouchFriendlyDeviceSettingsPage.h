// TouchFriendlyDeviceSettingsPage.h
// Touch-optimized device settings page maintaining all existing functionality

#ifndef TOUCH_FRIENDLY_DEVICE_SETTINGS_PAGE_H
#define TOUCH_FRIENDLY_DEVICE_SETTINGS_PAGE_H

#include <Arduino.h>

const char TOUCH_FRIENDLY_DEVICE_SETTINGS_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <meta name="apple-mobile-web-app-capable" content="yes">
    <title>Device Settings - AiO New Dawn</title>
    <link rel="stylesheet" href="/touch.css">
    <style>
        /* Additional styles specific to device settings */
        .toggle-container {
            display: flex;
            align-items: center;
            justify-content: space-between;
            padding: 12px 0;
            border-bottom: 1px solid #ecf0f1;
        }

        .toggle-container:last-child {
            border-bottom: none;
        }

        .toggle-label {
            font-size: 18px;
            font-weight: 500;
            flex: 1;
            margin-right: 15px;
        }

        .toggle-info {
            flex: 1;
        }
        
        .help-text {
            font-size: 14px;
            color: #7f8c8d;
            margin-top: 5px;
            line-height: 1.3;
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
        
        .slider-container {
            margin: 10px 0;
        }
        
        .slider-value {
            font-size: 20px;
            font-weight: 600;
            color: #3498db;
            text-align: center;
            margin-bottom: 5px;
        }
        
        input[type="range"] {
            -webkit-appearance: none;
            appearance: none;
            width: 100%;
            height: 44px;
            background: transparent;
            margin: 10px 0;
            outline: none;
            cursor: pointer;
        }

        input[type="range"]::-webkit-slider-track {
            width: 100%;
            height: 12px;
            background: #e0e0e0;
            border-radius: 6px;
            border: 1px solid #bdc3c7;
            box-shadow: inset 0 1px 2px rgba(0,0,0,0.1);
        }

        input[type="range"]::-webkit-slider-runnable-track {
            width: 100%;
            height: 12px;
            background: #e0e0e0;
            border-radius: 6px;
            border: 1px solid #bdc3c7;
            box-shadow: inset 0 1px 2px rgba(0,0,0,0.1);
            cursor: pointer;
        }
        
        input[type="range"]::-webkit-slider-thumb {
            -webkit-appearance: none;
            appearance: none;
            width: 44px;
            height: 44px;
            background: #3498db;
            border-radius: 50%;
            cursor: pointer;
            margin-top: -16px;
            box-shadow: 0 2px 5px rgba(0,0,0,0.2);
            border: 2px solid #fff;
            position: relative;
        }

        input[type="range"]:focus {
            outline: none;
        }

        input[type="range"]:focus::-webkit-slider-thumb {
            box-shadow: 0 0 0 3px rgba(52, 152, 219, 0.3), 0 2px 5px rgba(0,0,0,0.2);
        }

        /* Firefox */
        input[type="range"]::-moz-range-track {
            width: 100%;
            height: 12px;
            background: #e0e0e0;
            border-radius: 6px;
            border: 1px solid #bdc3c7;
            box-shadow: inset 0 1px 2px rgba(0,0,0,0.1);
        }

        input[type="range"]::-moz-range-thumb {
            width: 44px;
            height: 44px;
            background: #3498db;
            border-radius: 50%;
            cursor: pointer;
            border: none;
            box-shadow: 0 2px 5px rgba(0,0,0,0.2);
        }

        /* IE/Edge */
        input[type="range"]::-ms-track {
            width: 100%;
            height: 12px;
            background: transparent;
            border-color: transparent;
            border-width: 16px 0;
            color: transparent;
        }

        input[type="range"]::-ms-fill-lower {
            background: #bdc3c7;
            border-radius: 6px;
        }

        input[type="range"]::-ms-fill-upper {
            background: #bdc3c7;
            border-radius: 6px;
        }

        input[type="range"]::-ms-thumb {
            width: 44px;
            height: 44px;
            background: #3498db;
            border-radius: 50%;
            cursor: pointer;
            margin-top: 0px;
            box-shadow: 0 2px 5px rgba(0,0,0,0.2);
        }
        
        .sensitivity-labels {
            display: flex;
            justify-content: space-between;
            font-size: 12px;
            color: #7f8c8d;
            margin-top: 3px;
        }
        
        #jdPWMSensitivityGroup {
            background: #f8f9fa;
            padding: 10px;
            border-radius: 8px;
            margin-top: 8px;
        }
        
        .nav-buttons {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 15px;
            margin-bottom: 20px;
        }
    </style>
    <script>
        function toggleJDPWMSensitivity() {
            const enabled = document.getElementById('jdPWMEnabled').checked;
            document.getElementById('jdPWMSensitivityGroup').style.display = enabled ? 'block' : 'none';
        }
        
        function updateSensitivityValue(value) {
            document.getElementById('sensitivityValue').textContent = value;
        }

        function updateSoftStartValue(value) {
            const val = parseInt(value);
            if (val === 0) {
                document.getElementById('softStartValue').textContent = 'OFF';
            } else {
                document.getElementById('softStartValue').textContent = val + ' ms';
            }
        }

        function saveSettings() {
            const settings = {
                udpPassthrough: document.getElementById('udpPassthrough').checked,
                sensorFusion: document.getElementById('sensorFusion').checked,
                pwmBrakeMode: document.getElementById('pwmBrakeMode').checked,
                softStartDuration: parseInt(document.getElementById('softStartDuration').value),
                encoderType: parseInt(document.getElementById('encoderType').value),
                serialRadioBaud: parseInt(document.getElementById('serialRadioBaud').value),
                jdPWMEnabled: document.getElementById('jdPWMEnabled').checked,
                jdPWMSensitivity: parseInt(document.getElementById('jdPWMSensitivity').value)
            };
            
            // Show saving status
            document.getElementById('status').innerHTML = 
                '<div class="status">Saving settings...</div>';
            
            fetch('/api/device/settings', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify(settings)
            })
            .then(response => response.json())
            .then(data => {
                if (data.status === 'saved') {
                    document.getElementById('status').innerHTML = 
                        '<div class="status success">Settings saved successfully!</div>';
                    setTimeout(() => {
                        document.getElementById('status').innerHTML = '';
                    }, 5000);
                } else {
                    document.getElementById('status').innerHTML = 
                        '<div class="status error">Error saving settings</div>';
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
        }
        
        function loadSettings() {
            fetch('/api/device/settings')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('udpPassthrough').checked = data.udpPassthrough || false;
                    document.getElementById('sensorFusion').checked = data.sensorFusion || false;
                    document.getElementById('pwmBrakeMode').checked = data.pwmBrakeMode || false;
                    document.getElementById('softStartDuration').value = data.softStartDuration || 500;
                    updateSoftStartValue(data.softStartDuration || 500);
                    document.getElementById('encoderType').value = data.encoderType || 1;
                    document.getElementById('serialRadioBaud').value = data.serialRadioBaud || 115200;
                    document.getElementById('jdPWMEnabled').checked = data.jdPWMEnabled || false;
                    document.getElementById('jdPWMSensitivity').value = data.jdPWMSensitivity || 5;
                    updateSensitivityValue(data.jdPWMSensitivity || 5);
                    toggleJDPWMSensitivity();
                })
                .catch((error) => {
                    console.error('Error loading settings:', error);
                });
        }
        
        window.onload = function() {
            loadSettings();
        };
    </script>
</head>
<body>
    <div class="container">
        <h1>Device Settings</h1>
        
        <div class="nav-buttons">
            <button type="button" class="touch-button" style="background: #7f8c8d;" 
                    onclick="window.location.href='/'">
                Back to Home
            </button>
            <button type="submit" class="touch-button" form="settingsForm">
                Apply Changes
            </button>
        </div>
        
        <div id="status"></div>
        
        <form id="settingsForm" onsubmit="saveSettings(); return false;">
            <div class="card">
                <div class="toggle-container">
                    <div class="toggle-info">
                        <label for="udpPassthrough" class="toggle-label">GPS-UDP Passthrough</label>
                        <div class="help-text">
                            Enable direct UDP passthrough of NMEA sentences from GPS1 to AgIO.
                        </div>
                    </div>
                    <label class="toggle-switch">
                        <input type="checkbox" id="udpPassthrough" name="udpPassthrough">
                        <span class="toggle-slider"></span>
                    </label>
                </div>
                
                <div class="toggle-container">
                    <div class="toggle-info">
                        <label for="sensorFusion" class="toggle-label">Enable Virtual WAS (VWAS)</label>
                        <div class="help-text">
                            Use Keya motor encoder and GPS/IMU to create a virtual wheel angle sensor. Requires Keya CAN motor and vehicle movement.
                        </div>
                    </div>
                    <label class="toggle-switch">
                        <input type="checkbox" id="sensorFusion" name="sensorFusion">
                        <span class="toggle-slider"></span>
                    </label>
                </div>
                
                <div class="toggle-container">
                    <div class="toggle-info">
                        <label for="pwmBrakeMode" class="toggle-label">PWM Motor Brake Mode</label>
                        <div class="help-text">
                            When enabled, PWM motors use brake mode (active braking). When disabled, motors use coast mode (free-wheeling). Only affects PWM-based motor drivers.
                        </div>
                    </div>
                    <label class="toggle-switch">
                        <input type="checkbox" id="pwmBrakeMode" name="pwmBrakeMode">
                        <span class="toggle-slider"></span>
                    </label>
                </div>

                <div style="margin: 15px 0; padding: 15px 0; border-top: 1px solid #ecf0f1; border-bottom: none;">
                    <div class="slider-container">
                        <label for="softStartDuration">Motor Soft Start Duration</label>
                        <div class="slider-value" id="softStartValue">500 ms</div>
                        <input type="range" id="softStartDuration" name="softStartDuration"
                               min="0" max="1000" value="500" step="50"
                               oninput="updateSoftStartValue(this.value)">
                        <div class="sensitivity-labels">
                            <span>OFF (0)</span>
                            <span>1000ms</span>
                        </div>
                        <div class="help-text" style="text-align: center; margin-top: 5px;">
                            Ramps motor power on startup and direction changes. 0 = disabled. Direction changes use half this duration automatically.
                        </div>
                    </div>
                </div>

                <div class="form-group" style="margin-top: 0; padding-top: 0;">
                    <label for="encoderType">Encoder Type:</label>
                    <select id="encoderType" name="encoderType">
                        <option value="1">Single Channel</option>
                        <option value="2">Quadrature (Dual Channel)</option>
                    </select>
                    <div class="help-text" style="margin-top: 5px;">
                        Single channel encoders use only the Kickout-D pin. Quadrature encoders use both Kickout-A and Kickout-D pins for direction sensing and higher resolution.
                    </div>
                </div>

                <div class="form-group" style="margin-top: 15px;">
                    <label for="serialRadioBaud">RTK Radio Baud Rate:</label>
                    <select id="serialRadioBaud" name="serialRadioBaud">
                        <option value="4800">4800</option>
                        <option value="9600">9600</option>
                        <option value="14400">14400</option>
                        <option value="19200">19200</option>
                        <option value="38400">38400</option>
                        <option value="57600">57600</option>
                        <option value="115200">115200 (Default)</option>
                        <option value="230400">230400</option>
                        <option value="460800">460800</option>
                        <option value="921600">921600</option>
                    </select>
                    <div class="help-text" style="margin-top: 5px;">
                        Set the baud rate for the serial radio (XBee) used for RTK corrections. Some radios cannot easily change their baud rate, so match this setting to your radio's configuration.
                    </div>
                </div>
                
                <div class="toggle-container" style="margin-top: 15px;">
                    <div class="toggle-info">
                        <label for="jdPWMEnabled" class="toggle-label">John Deere PWM Encoder Mode</label>
                        <div class="help-text">
                            Enable John Deere Autotrac PWM encoder support. This uses the digital kickout input (Kickout-D pin 3) to measure PWM duty cycle changes for steering wheel motion detection.<br>
                            <strong>Note:</strong> In AgOpenGPS, you must enable "Pressure Sensor" kickout mode and use the pressure set point in AgOpenGPS to set the kickout point.
                        </div>
                    </div>
                    <label class="toggle-switch">
                        <input type="checkbox" id="jdPWMEnabled" name="jdPWMEnabled" onchange="toggleJDPWMSensitivity()">
                        <span class="toggle-slider"></span>
                    </label>
                </div>
                
                <div id="jdPWMSensitivityGroup" style="display: none;">
                    <div class="slider-container">
                        <label for="jdPWMSensitivity">JD PWM Sensitivity</label>
                        <div class="slider-value" id="sensitivityValue">5</div>
                        <input type="range" id="jdPWMSensitivity" name="jdPWMSensitivity" 
                               min="1" max="10" value="5" 
                               oninput="updateSensitivityValue(this.value)">
                        <div class="sensitivity-labels">
                            <span>1 - Less sensitive</span>
                            <span>10 - More sensitive</span>
                        </div>
                        <div class="help-text" style="text-align: center; margin-top: 5px;">
                            Higher values require less wheel movement to trigger
                        </div>
                    </div>
                </div>
            </div>
        </form>
    </div>
</body>
</html>
)rawliteral";

#endif // TOUCH_FRIENDLY_DEVICE_SETTINGS_PAGE_H