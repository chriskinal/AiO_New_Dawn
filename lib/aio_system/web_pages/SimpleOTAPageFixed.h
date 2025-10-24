// SimpleOTAPageFixed.h
// Fixed OTA update page without placeholders

#ifndef SIMPLE_OTA_PAGE_FIXED_H
#define SIMPLE_OTA_PAGE_FIXED_H

#include <Arduino.h>

const char SIMPLE_OTA_UPDATE_PAGE_FIXED[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name='viewport' content='width=device-width, initial-scale=1'>
    <title>OTA Update - AiO New Dawn</title>
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
        .help-text { 
            color: #666; 
            font-size: 0.9em; 
            margin-top: 5px;
        }
        .warning {
            background-color: #ffeeee;
            border: 1px solid #ff0000;
            padding: 10px;
            margin: 20px 0;
            border-radius: 5px;
        }
        .nav-buttons {
            margin-top: 20px;
        }
        #status {
            margin: 10px 0;
            padding: 10px;
            text-align: center;
            border-radius: 5px;
        }
        #feedback {
            font-family: monospace;
            background-color: #f5f5f5;
            padding: 10px;
            border-radius: 5px;
            margin: 10px 0;
            min-height: 100px;
            white-space: pre-wrap;
        }
        #file {
            width: 100%;
            max-width: 500px;
            padding: 8px;
            border: 1px solid #ccc;
            border-radius: 5px;
            background-color: #f9f9f9;
        }
        .file-info {
            margin-top: 10px;
            padding: 10px;
            background-color: #e3f2fd;
            border-radius: 5px;
            font-family: monospace;
            word-break: break-all;
        }
    </style>
    <script>
        function displayFileName() {
            const fileInput = document.getElementById('file');
            const fileInfo = document.getElementById('fileInfo');
            const file = fileInput.files[0];
            
            if (file) {
                fileInfo.innerHTML = '<strong>Selected file:</strong> ' + file.name;
                fileInfo.style.display = 'block';
            } else {
                fileInfo.style.display = 'none';
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
    <div class='container'>
        <h1>OTA Firmware Update</h1>
        
        <div class='status'>
            <p>Board Type: AiO v4.x - Teensy 4.1</p>
        </div>
        
        <div class='warning'>
            <strong>Warning:</strong> Incorrect firmware can brick your device. Only upload firmware built for Teensy 4.1.
        </div>
        
        <h2>Select Firmware File</h2>
        
        <div class='form-group'>
            <input type='file' id='file' name='firmware' accept='.hex' required onchange='displayFileName()'>
            <div class='help-text'>
                Select a .hex firmware file built for Teensy 4.1
            </div>
            <div id='fileInfo' class='file-info' style='display: none;'></div>
        </div>
        
        <div id='feedback'></div>
        
        <div class='nav-buttons'>
            <button type='button' class='btn btn-home' onclick='window.location.href="/"'>Home</button>
            <button type='button' id='uploadBtn' class='btn btn-primary' onclick='uploadFile()'>Upload Firmware</button>
        </div>
    </div>
</body>
</html>
)rawliteral";

#endif // SIMPLE_OTA_PAGE_FIXED_H