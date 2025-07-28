// en_DeviceSettingsPage.h
// English version of Device Settings page

#ifndef EN_DEVICE_SETTINGS_PAGE_H
#define EN_DEVICE_SETTINGS_PAGE_H

#include <Arduino.h>

const char EN_DEVICE_SETTINGS_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name='viewport' content='width=device-width, initial-scale=1'>
    <title>Device Settings - AiO New Dawn</title>
    <style>%CSS_STYLES%</style>
    <script>
        let updateInterval = null;
        
        function updateEncoderCount() {
            fetch('/api/encoder/count')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('encoderCount').textContent = data.count || 0;
                    document.getElementById('encoderStatus').textContent = data.enabled ? 'Enabled' : 'Disabled';
                })
                .catch((error) => {
                    console.error('Error fetching encoder count:', error);
                });
        }
        
        function saveSettings() {
            const settings = {
                udpPassthrough: document.getElementById('udpPassthrough').checked,
                sensorFusion: document.getElementById('sensorFusion').checked,
                pwmBrakeMode: document.getElementById('pwmBrakeMode').checked,
                encoderType: parseInt(document.getElementById('encoderType').value)
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
                if (data.success) {
                    document.getElementById('status').innerHTML = 
                        '<span style="color: green;">Settings saved successfully!</span>';
                } else {
                    document.getElementById('status').innerHTML = 
                        '<span style="color: red;">Error saving settings: ' + data.error + '</span>';
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
                })
                .catch((error) => {
                    console.error('Error loading settings:', error);
                });
        }
        
        // Load settings when page loads
        window.onload = function() {
            loadSettings();
            // Start updating encoder count every 100ms
            updateEncoderCount();
            updateInterval = setInterval(updateEncoderCount, 100);
        };
        
        window.onbeforeunload = function() {
            // Stop updates when leaving page
            if (updateInterval) {
                clearInterval(updateInterval);
            }
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
                <div style='margin-top: 10px; padding: 10px; border: 1px solid #ccc; background-color: #f9f9f9;'>
                    <strong>Encoder Status:</strong> <span id='encoderStatus'>Loading...</span><br>
                    <strong>Current Count:</strong> <span id='encoderCount' style='font-size: 1.2em; font-weight: bold;'>0</span>
                    <div class='help-text' style='margin-top: 5px;'>
                        Rotate the encoder to see the count change. The count will reset when autosteer is engaged.
                    </div>
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

#endif // EN_DEVICE_SETTINGS_PAGE_H