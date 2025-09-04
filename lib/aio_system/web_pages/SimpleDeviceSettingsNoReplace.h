// SimpleDeviceSettingsNoReplace.h
// Device Settings page with CSS already embedded - no replacements needed

#ifndef SIMPLE_DEVICE_SETTINGS_NO_REPLACE_H
#define SIMPLE_DEVICE_SETTINGS_NO_REPLACE_H

#include <Arduino.h>

const char SIMPLE_DEVICE_SETTINGS_NO_REPLACE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name='viewport' content='width=device-width, initial-scale=1'>
    <title>Device Settings - AiO New Dawn</title>
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
        .form-group { margin-bottom: 20px; }
        .btn { padding: 10px 20px; margin: 5px; border: none; border-radius: 5px; cursor: pointer; }
        .btn-primary { background-color: #007bff; color: white; }
        .btn-home { background-color: #6c757d; color: white; }
        .help-text { font-size: 0.9em; color: #666; }
    </style>
    <script>
        function saveSettings() {
            const settings = {
                udpPassthrough: document.getElementById('udpPassthrough').checked,
                sensorFusion: document.getElementById('sensorFusion').checked,
                pwmBrakeMode: document.getElementById('pwmBrakeMode').checked,
                encoderType: parseInt(document.getElementById('encoderType').value),
                jdPWMEnabled: document.getElementById('jdPWMEnabled').checked,
                jdPWMThreshold: parseInt(document.getElementById('jdPWMThreshold').value)
            };
            
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
                        '<span style="color: green;">Settings saved successfully!</span>';
                } else {
                    document.getElementById('status').innerHTML = 
                        '<span style="color: red;">Error saving settings</span>';
                }
            })
            .catch((error) => {
                document.getElementById('status').innerHTML = 
                    '<span style="color: red;">Error: ' + error + '</span>';
            });
        }
        
        function loadSettings() {
            fetch('/api/device/settings')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('udpPassthrough').checked = data.udpPassthrough || false;
                    document.getElementById('sensorFusion').checked = data.sensorFusion || false;
                    document.getElementById('pwmBrakeMode').checked = data.pwmBrakeMode || false;
                    document.getElementById('encoderType').value = data.encoderType || 1;
                    document.getElementById('jdPWMEnabled').checked = data.jdPWMEnabled || false;
                    document.getElementById('jdPWMThreshold').value = data.jdPWMThreshold || 20;
                    toggleJDPWMThreshold();
                })
                .catch((error) => {
                    console.error('Error loading settings:', error);
                });
        }
        
        function toggleJDPWMThreshold() {
            const enabled = document.getElementById('jdPWMEnabled').checked;
            document.getElementById('jdPWMThresholdGroup').style.display = enabled ? 'block' : 'none';
        }
        
        window.onload = function() {
            loadSettings();
        };
    </script>
</head>
<body>
    <div class='container'>
        <h1>Device Settings</h1>
        
        <form onsubmit='saveSettings(); return false;'>
            <h2>GPS Configuration</h2>
            
            <div class='form-group'>
                <label class='checkbox-container' style='display: inline-flex; align-items: center;'>
                    <input type='checkbox' id='udpPassthrough' name='udpPassthrough' style='margin-right: 10px;'>
                    <span class='checkbox-label' style='white-space: nowrap;'>GPS-UDP Passthrough</span>
                </label>
                <div class='help-text' style='margin-left: 25px; margin-top: 5px;'>
                    Enable direct UDP passthrough of NMEA sentences from GPS1 to AgIO.
                </div>
            </div>
            
            <h2>Steering Configuration</h2>
            
            <div class='form-group'>
                <label class='checkbox-container' style='display: inline-flex; align-items: center;'>
                    <input type='checkbox' id='sensorFusion' name='sensorFusion' style='margin-right: 10px;'>
                    <span class='checkbox-label' style='white-space: nowrap;'>Enable Virtual WAS (VWAS)</span>
                </label>
                <div class='help-text' style='margin-left: 25px; margin-top: 5px;'>
                    Use Keya motor encoder and GPS/IMU to create a virtual wheel angle sensor. Requires Keya CAN motor and vehicle movement.
                </div>
            </div>
            
            <h2>Motor Configuration</h2>
            
            <div class='form-group'>
                <label class='checkbox-container' style='display: inline-flex; align-items: center;'>
                    <input type='checkbox' id='pwmBrakeMode' name='pwmBrakeMode' style='margin-right: 10px;'>
                    <span class='checkbox-label' style='white-space: nowrap;'>PWM Motor Brake Mode</span>
                </label>
                <div class='help-text' style='margin-left: 25px; margin-top: 5px;'>
                    When enabled, PWM motors use brake mode (active braking). When disabled, motors use coast mode (free-wheeling). Only affects PWM-based motor drivers.
                </div>
            </div>
            
            <h2>Turn Sensor Configuration</h2>
            
            <div class='form-group'>
                <label for='encoderType'>Encoder Type:</label>
                <select id='encoderType' name='encoderType' style='width: 100%; padding: 5px;'>
                    <option value='1'>Single Channel</option>
                    <option value='2'>Quadrature (Dual Channel)</option>
                </select>
                <div class='help-text' style='margin-top: 5px;'>
                    Single channel encoders use only the Kickout-D pin. Quadrature encoders use both Kickout-A and Kickout-D pins for direction sensing and higher resolution.
                </div>
            </div>
            
            <div class='form-group'>
                <label class='checkbox-container' style='display: inline-flex; align-items: center;'>
                    <input type='checkbox' id='jdPWMEnabled' name='jdPWMEnabled' onchange='toggleJDPWMThreshold()' style='margin-right: 10px;'>
                    <span class='checkbox-label' style='white-space: nowrap;'>John Deere PWM Encoder Mode</span>
                </label>
                <div class='help-text' style='margin-left: 25px; margin-top: 5px;'>
                    Enable John Deere Autotrac PWM encoder support. This uses the pressure sensor input (Kickout-A pin) to measure PWM duty cycle changes for steering wheel motion detection.
                </div>
            </div>
            
            <div class='form-group' id='jdPWMThresholdGroup' style='display: none; margin-left: 25px;'>
                <label for='jdPWMThreshold'>JD PWM Motion Threshold:</label>
                <input type='number' id='jdPWMThreshold' name='jdPWMThreshold' min='5' max='100' value='20' style='width: 80px; padding: 5px;'>
                <span style='margin-left: 10px;'>(5-100)</span>
                <div class='help-text' style='margin-top: 5px;'>
                    Motion detection threshold for JD PWM encoder. Lower values = more sensitive. Default is 20.
                </div>
            </div>
            
            <div id='status' style='margin: 10px 0;'></div>
            
            <div class='nav-buttons'>
                <button type='button' class='btn btn-home' onclick='window.location="/"'>Home</button>
                <button type='submit' class='btn btn-primary'>Apply Changes</button>
            </div>
        </form>
    </div>
</body>
</html>
)rawliteral";

#endif // SIMPLE_DEVICE_SETTINGS_NO_REPLACE_H