// SimpleAnalogWorkSwitchPage.h
// Ultra-simplified version to completely avoid PROGMEM issues

#ifndef SIMPLE_ANALOG_WORK_SWITCH_PAGE_H
#define SIMPLE_ANALOG_WORK_SWITCH_PAGE_H

#include <Arduino.h>

const char SIMPLE_ANALOG_WORK_SWITCH_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name='viewport' content='width=device-width, initial-scale=1'>
<title>Analog Work Switch</title>
<style>
body {font-family: Arial, sans-serif; margin: 0; padding: 20px; background-color: #f5f5f5;}
.container {max-width: 600px; margin: 0 auto; background-color: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1);}
h1 {color: #333; margin-bottom: 20px;}
.bar-container {width: 100%; height: 60px; background-color: #f0f0f0; border: 2px solid #333; position: relative; margin: 20px 0 10px;}
.bar-fill {height: 100%; background-color: #4CAF50; width: 0%; transition: width 0.1s;}
.bar-marker {position: absolute; top: -5px; width: 3px; height: 70px; background-color: #000;}
.bar-zone {position: absolute; top: 0; height: 100%; opacity: 0.3;}
.reading {font-size: 24px; font-weight: bold; text-align: center; margin: 10px 0;}
.control-row {display: flex; align-items: center; justify-content: space-between; margin: 10px 0; flex-wrap: wrap;}
.control-group {display: flex; align-items: center; gap: 10px; margin: 5px 0;}
button {background-color: #4CAF50; color: white; padding: 10px 20px; border: none; border-radius: 4px; cursor: pointer;}
button:hover {background-color: #45a049;}
select {padding: 5px; font-size: 16px;}
.btn-home {background-color: #2196F3; color: white; padding: 10px 20px; border: none; border-radius: 5px; cursor: pointer; text-decoration: none; display: inline-block; margin: 5px;}
.btn-home:hover {background-color: #1976D2;}
#status {margin-top: 10px; padding: 10px; text-align: center;}
</style>
</head>
<body>
<div class='container'>
<h1>Analog Work Switch</h1>
<div class='reading' id='reading'>Loading...</div>
<div class='bar-container'>
<div class='bar-fill' id='bar'></div>
<div class='bar-zone' id='lowerZone' style='background-color: #4CAF50;'></div>
<div class='bar-zone' id='upperZone' style='background-color: #f44336;'></div>
<div class='bar-marker' id='setpointMarker'></div>
</div>
<div class='control-row'>
<div class='control-group'>
<button onclick='setSetpoint()'>Set Current as Setpoint</button>
<span id='setpoint'>SP: 50%</span>
</div>
<div class='control-group'>
<label>Hysteresis: 
<select id='hyst' onchange='updateHyst()'>
<option value='1'>1%</option>
<option value='2'>2%</option>
<option value='3'>3%</option>
<option value='5'>5%</option>
<option value='10'>10%</option>
<option value='15'>15%</option>
<option value='20' selected>20%</option>
<option value='25'>25%</option>
</select>
</label>
</div>
</div>
<div class='control-row'>
<div class='control-group'>
<label><input type='checkbox' id='enable' onchange='toggleEnable()'> Enable Analog Mode</label>
</div>
<div class='control-group'>
<label><input type='checkbox' id='invert' onchange='toggleInvert()'> Invert Logic</label>
</div>
</div>
<div id='status'></div>
<a href='/' class='btn-home'>Back to Home</a>
</div>
<script>
var ws = null;
var config = {enabled: false, setpoint: 50, hysteresis: 20, invert: false};

function connectWebSocket() {
    ws = new WebSocket('ws://' + window.location.hostname + ':8082');
    ws.binaryType = 'arraybuffer';
    
    ws.onopen = function() {
        document.getElementById('reading').textContent = 'Connected - Waiting for data...';
    };
    
    ws.onmessage = function(event) {
        if (event.data instanceof ArrayBuffer) {
            const view = new DataView(event.data);
            
            // Debug: log packet size
            console.log('Received packet size:', event.data.byteLength);
            
            // Parse telemetry packet
            const work_switch = view.getUint8(29);
            const work_analog_percent = view.getUint8(30);
            
            // Debug: log values
            console.log('work_switch:', work_switch, 'work_analog_percent:', work_analog_percent);
            
            // Update display
            var readingEl = document.getElementById('reading');
            var barEl = document.getElementById('bar');
            
            if (readingEl) {
                readingEl.textContent = 
                    'Reading: ' + work_analog_percent + '% (' + (work_switch ? 'ON' : 'OFF') + ')';
            } else {
                console.error('reading element not found');
            }
            
            if (barEl) {
                barEl.style.width = work_analog_percent + '%';
                barEl.style.backgroundColor = work_switch ? '#4CAF50' : '#f44336';
            } else {
                console.error('bar element not found');
            }
            
            updateHysteresisZones(config.setpoint, config.hysteresis);
        }
    };
    
    ws.onclose = function() {
        document.getElementById('reading').textContent = 'Disconnected - Reconnecting...';
        setTimeout(connectWebSocket, 2000);
    };
    
    ws.onerror = function(error) {
        console.error('WebSocket error:', error);
    };
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
}

function loadConfig() {
    var xhr = new XMLHttpRequest();
    xhr.onreadystatechange = function() {
        if (this.readyState == 4 && this.status == 200) {
            var data = JSON.parse(this.responseText);
            config.enabled = data.enabled;
            config.setpoint = data.setpoint;
            config.hysteresis = data.hysteresis;
            config.invert = data.invert;
            document.getElementById('enable').checked = data.enabled;
            document.getElementById('setpoint').textContent = 'SP: ' + data.setpoint + '%';
            document.getElementById('hyst').value = data.hysteresis;
            document.getElementById('invert').checked = data.invert;
            
            document.getElementById('reading').textContent = 
                'Reading: ' + Math.floor(data.percent) + '% (' + (data.state ? 'ON' : 'OFF') + ')';
            document.getElementById('bar').style.width = data.percent + '%';
            document.getElementById('bar').style.backgroundColor = 
                data.state ? '#4CAF50' : '#f44336';
            
            updateHysteresisZones(config.setpoint, config.hysteresis);
            
            // WebSocket provides real-time updates, no need for polling
        }
    };
    xhr.open('GET', '/api/analogworkswitch/status', true);
    xhr.send();
}

function toggleEnable() {
    var enabled = document.getElementById('enable').checked;
    saveConfig({enabled: enabled});
}

function setSetpoint() {
    var xhr = new XMLHttpRequest();
    xhr.onreadystatechange = function() {
        if (this.readyState == 4 && this.status == 200) {
            var data = JSON.parse(this.responseText);
            config.setpoint = data.newSetpoint;
            document.getElementById('setpoint').textContent = 'SP: ' + Math.floor(data.newSetpoint) + '%';
            document.getElementById('status').textContent = 'Setpoint saved!';
            updateHysteresisZones(config.setpoint, config.hysteresis);
        }
    };
    xhr.open('POST', '/api/analogworkswitch/setpoint', true);
    xhr.send();
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
    var xhr = new XMLHttpRequest();
    xhr.onreadystatechange = function() {
        if (this.readyState == 4 && this.status == 200) {
            document.getElementById('status').textContent = 'Settings saved!';
            setTimeout(loadConfig, 100);
        }
    };
    xhr.open('POST', '/api/analogworkswitch/config', true);
    xhr.setRequestHeader('Content-Type', 'application/json');
    xhr.send(JSON.stringify(data));
}

window.onload = function() {
    connectWebSocket();
    loadConfig();
};

window.onbeforeunload = function() {
    if (ws) ws.close();
};
</script>
</body>
</html>
)rawliteral";

#endif // SIMPLE_ANALOG_WORK_SWITCH_PAGE_H