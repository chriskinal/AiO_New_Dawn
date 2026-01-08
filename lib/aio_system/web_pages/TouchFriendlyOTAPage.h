// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

// TouchFriendlyOTAPage.h
// Touch-optimized OTA firmware update page

#ifndef TOUCH_FRIENDLY_OTA_PAGE_H
#define TOUCH_FRIENDLY_OTA_PAGE_H

#include <Arduino.h>

const char TOUCH_FRIENDLY_OTA_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <meta name="apple-mobile-web-app-capable" content="yes">
    <title>System Update - AiO New Dawn</title>
    <link rel="stylesheet" href="/touch.css">
    <style>
        /* Additional styles specific to OTA update */
        .board-info {
            background: #e3f2fd;
            padding: 15px;
            border-radius: 8px;
            text-align: center;
            margin-bottom: 20px;
        }
        
        .board-info div {
            font-size: 18px;
            font-weight: 500;
            color: #1976d2;
            margin: 5px 0;
        }
        
        .warning-box {
            background: #ffebee;
            border: 2px solid #ef5350;
            padding: 15px;
            border-radius: 8px;
            margin-bottom: 20px;
            text-align: center;
            color: #c62828;
        }
        
        .warning-box strong {
            color: #c62828;
            font-size: 18px;
            display: block;
            margin-bottom: 5px;
        }
        
        
        .file-info {
            margin-top: 15px;
            padding: 15px;
            background: #e8f5e9;
            border-radius: 8px;
            font-family: monospace;
            word-break: break-all;
            display: none;
        }
        
        #feedback {
            font-family: monospace;
            background: #f5f5f5;
            padding: 15px;
            border-radius: 8px;
            margin: 20px 0;
            min-height: 120px;
            white-space: pre-wrap;
            font-size: 14px;
            border: 2px solid #bdc3c7;
        }
        
        .upload-button {
            background: #4caf50;
        }
        
        .button-grid .upload-button {
            margin-top: 0;
        }
        
        .upload-button:active {
            background: #388e3c;
        }
        
        .upload-button:disabled {
            background: #95a5a6;
            opacity: 0.6;
        }
        
        .nav-buttons {
            display: grid;
            grid-template-columns: 1fr;
            gap: 15px;
            margin-bottom: 20px;
        }
        
        .button-grid {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 15px;
            margin-bottom: 20px;
        }
        
        .button-grid .touch-button,
        .button-grid label.touch-button {
            display: flex;
            align-items: center;
            justify-content: center;
            cursor: pointer;
            height: 60px;
            min-height: 60px;
            box-sizing: border-box;
            padding: 20px 25px;
            margin: 0;
            line-height: 1;
        }
        
        @media (max-width: 600px) {
            #feedback {
                font-size: 12px;
            }
        }
    </style>
    <script>
        function loadVersion() {
            fetch('/api/status')
            .then(response => response.json())
            .then(data => {
                if (data.version) {
                    document.getElementById('currentVersion').textContent = data.version;
                } else {
                    document.getElementById('currentVersion').textContent = 'Unknown';
                }
            })
            .catch(error => {
                console.error('Error loading version:', error);
                document.getElementById('currentVersion').textContent = 'Error';
            });
        }
        
        window.onload = loadVersion;
        
        function displayFileName() {
            const fileInput = document.getElementById('file');
            const fileInfo = document.getElementById('fileInfo');
            const fileLabel = document.getElementById('fileLabel');
            const file = fileInput.files[0];
            
            if (file) {
                fileInfo.innerHTML = '<strong>Selected file:</strong> ' + file.name;
                fileInfo.style.display = 'block';
                fileLabel.textContent = 'File Selected: ' + file.name;
            } else {
                fileInfo.style.display = 'none';
                fileLabel.textContent = 'Choose Firmware File (.hex)';
            }
        }
        
        function uploadFile() {
            const fileInput = document.getElementById('file');
            const file = fileInput.files[0];
            
            if (!file) {
                alert('Please select a firmware file');
                return false;
            }
            
            if (!file.name.endsWith('.hex')) {
                alert('Please select a .hex firmware file');
                return false;
            }
        
            const feedback = document.getElementById('feedback');
            const uploadBtn = document.getElementById('uploadBtn');
        
            feedback.textContent = 'Reading file...';
            uploadBtn.disabled = true;
            
            // Read file content
            const reader = new FileReader();
            reader.onload = function(e) {
                const content = e.target.result;
                feedback.textContent += '\nFile loaded: ' + file.name + ' (' + content.length + ' bytes)\n';
                feedback.textContent += 'Uploading to device...\n';
                
                const xhr = new XMLHttpRequest();
                
                let blockCount = 0;
                let baseText = feedback.textContent;
                const updateInterval = setInterval(() => {
                    if (xhr.readyState !== 4) {
                        blockCount++;
                        feedback.textContent = baseText + 'Block ' + blockCount;
                    } else {
                        clearInterval(updateInterval);
                    }
                }, 500);
                
                xhr.addEventListener('load', function() {
                    clearInterval(updateInterval);
                    if (xhr.status === 200) {
                        feedback.textContent = baseText + 'Upload complete, rebooting...';
                        setTimeout(() => {
                            window.location.href = '/';
                        }, 5000);
                    } else {
                        feedback.textContent = baseText + 'Upload failed: ' + xhr.responseText;
                        uploadBtn.disabled = false;
                    }
                });
                
                xhr.addEventListener('error', function() {
                    clearInterval(updateInterval);
                    feedback.textContent = baseText + 'Connection lost (device may be rebooting)';
                    setTimeout(() => {
                        window.location.href = '/';
                    }, 5000);
                });
                
                xhr.open('POST', '/api/ota/upload');
                xhr.setRequestHeader('Content-Type', 'text/plain');
                xhr.send(content);
            };
            
            reader.readAsText(file);
            return false;
        }
    </script>
</head>
<body>
    <div class="container">
        <h1>System Update</h1>
        
        <div class="nav-buttons">
            <button type="button" class="touch-button" style="background: #7f8c8d;" 
                    onclick="window.location.href='/'">
                Back to Home
            </button>
        </div>
        
        <div class="card">
            <div class="button-grid">
                <label for="file" class="touch-button">
                    Choose Firmware
                </label>
                <button type="button" id="uploadBtn" class="touch-button upload-button" onclick="uploadFile()">
                    Upload Firmware
                </button>
            </div>
            
            <input type="file" id="file" name="firmware" accept=".hex" onchange="displayFileName()" style="display: none;">
            
            <div id="fileInfo" class="file-info"></div>
            
            <div id="feedback"></div>
        </div>
        
        <div class="board-info">
            <div>Board Type: Teensy 4.1</div>
            <div>Current Version: <span id="currentVersion">Loading...</span></div>
        </div>
        
        <div class="warning-box">
            <strong>⚠️ Warning</strong>
            Incorrect firmware can brick your device. Only upload firmware built for Teensy 4.1.
        </div>
        
    </div>
</body>
</html>
)rawliteral";

#endif // TOUCH_FRIENDLY_OTA_PAGE_H