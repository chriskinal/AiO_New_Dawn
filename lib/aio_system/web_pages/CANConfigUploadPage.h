// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

// CANConfigUploadPage.h
// Touch-optimized CAN configuration JSON upload page

#ifndef CAN_CONFIG_UPLOAD_PAGE_H
#define CAN_CONFIG_UPLOAD_PAGE_H

#include <Arduino.h>

const char CAN_CONFIG_UPLOAD_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <meta name="apple-mobile-web-app-capable" content="yes">
    <title>CAN Config Upload - AiO New Dawn</title>
    <link rel="stylesheet" href="/touch.css">
    <style>
        .info-box {
            background: #e3f2fd;
            padding: 15px;
            border-radius: 8px;
            margin-bottom: 20px;
        }

        .info-box h3 {
            color: #1976d2;
            margin: 0 0 10px 0;
            font-size: 18px;
        }

        .info-box p {
            margin: 5px 0;
            line-height: 1.5;
            color: #1565c0;
            font-weight: 500;
        }

        .info-box strong {
            color: #0d47a1;
            font-weight: 600;
        }

        .warning-box {
            background: #fff3e0;
            border: 2px solid #ff9800;
            padding: 15px;
            border-radius: 8px;
            margin-bottom: 20px;
        }

        .warning-box strong {
            color: #e65100;
            font-size: 16px;
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
            color: #1b5e20;
            font-weight: 500;
        }

        .file-info strong {
            color: #2e7d32;
            font-weight: 600;
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
            color: #37474f;
            font-weight: 500;
        }

        .upload-button {
            background: #4caf50;
        }

        .upload-button:active {
            background: #388e3c;
        }

        .upload-button:disabled {
            background: #95a5a6;
            opacity: 0.6;
        }

        .restore-button {
            background: #ff9800;
        }

        .restore-button:active {
            background: #f57c00;
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
        function displayFileName() {
            const fileInput = document.getElementById('file');
            const fileInfo = document.getElementById('fileInfo');
            const feedback = document.getElementById('feedback');
            const uploadBtn = document.getElementById('uploadBtn');
            const file = fileInput.files[0];

            if (file) {
                fileInfo.innerHTML = '<strong>Selected file:</strong> ' + file.name + ' (' + file.size + ' bytes)';
                fileInfo.style.display = 'block';

                // Automatically validate on file selection
                feedback.textContent = 'Validating JSON file...';
                uploadBtn.disabled = true;

                const reader = new FileReader();
                reader.onload = function(e) {
                    const content = e.target.result;
                    const validation = validateJSON(content);

                    if (!validation.valid) {
                        feedback.textContent = 'VALIDATION FAILED:\n' + validation.error + '\n\nPlease select a valid JSON file.';
                        feedback.style.background = '#ffebee';
                        feedback.style.border = '2px solid #c62828';
                        uploadBtn.disabled = true;
                    } else {
                        feedback.textContent = 'VALIDATION PASSED!\n' +
                            '  Version: ' + validation.json.version + '\n' +
                            '  Brands: ' + validation.json.brands.length + '\n' +
                            '  Functions: ' + Object.keys(validation.json.functions).length + '\n\n' +
                            'Ready to upload.';
                        feedback.style.background = '#e8f5e9';
                        feedback.style.border = '2px solid #4caf50';
                        uploadBtn.disabled = false;
                    }
                };

                reader.onerror = function() {
                    feedback.textContent = 'ERROR: Failed to read file';
                    feedback.style.background = '#ffebee';
                    feedback.style.border = '2px solid #c62828';
                    uploadBtn.disabled = true;
                };

                reader.readAsText(file);
            } else {
                fileInfo.style.display = 'none';
                feedback.textContent = '';
                feedback.style.background = '#f5f5f5';
                feedback.style.border = '2px solid #bdc3c7';
                uploadBtn.disabled = true;
            }
        }

        function validateJSON(jsonString) {
            try {
                const json = JSON.parse(jsonString);

                // Validate required fields
                if (!json.version) {
                    return { valid: false, error: 'Missing "version" field' };
                }
                if (!json.functions) {
                    return { valid: false, error: 'Missing "functions" field' };
                }
                if (!json.busTypes) {
                    return { valid: false, error: 'Missing "busTypes" field' };
                }
                if (!json.brands || !Array.isArray(json.brands)) {
                    return { valid: false, error: 'Missing or invalid "brands" field' };
                }

                // Validate bus types exist
                if (!json.busTypes.None || !json.busTypes.V_Bus ||
                    !json.busTypes.K_Bus || !json.busTypes.ISO_Bus) {
                    return { valid: false, error: 'Missing required bus types (None, V_Bus, K_Bus, ISO_Bus)' };
                }

                // Validate at least one brand
                if (json.brands.length === 0) {
                    return { valid: false, error: 'No brands defined' };
                }

                return { valid: true, json: json };
            } catch (e) {
                return { valid: false, error: 'Invalid JSON: ' + e.message };
            }
        }

        function uploadFile() {
            const fileInput = document.getElementById('file');
            const file = fileInput.files[0];

            if (!file) {
                alert('Please select a JSON file');
                return false;
            }

            const feedback = document.getElementById('feedback');
            const uploadBtn = document.getElementById('uploadBtn');

            feedback.textContent = 'Uploading to device...';
            feedback.style.background = '#e3f2fd';
            feedback.style.border = '2px solid #1976d2';
            uploadBtn.disabled = true;

            const reader = new FileReader();
            reader.onload = function(e) {
                const content = e.target.result;

                const xhr = new XMLHttpRequest();

                xhr.addEventListener('load', function() {
                    if (xhr.status === 200) {
                        feedback.textContent = 'Upload successful!\n\n' +
                            'Custom CAN configuration is now active.\n' +
                            'Redirecting to CAN config page...';
                        feedback.style.background = '#e8f5e9';
                        feedback.style.border = '2px solid #4caf50';
                        setTimeout(() => {
                            window.location.href = '/can';
                        }, 2000);
                    } else {
                        feedback.textContent = 'Upload failed:\n' + xhr.responseText;
                        feedback.style.background = '#ffebee';
                        feedback.style.border = '2px solid #c62828';
                        uploadBtn.disabled = false;
                    }
                });

                xhr.addEventListener('error', function() {
                    feedback.textContent = 'Connection error during upload';
                    feedback.style.background = '#ffebee';
                    feedback.style.border = '2px solid #c62828';
                    uploadBtn.disabled = false;
                });

                xhr.open('POST', '/api/can/config/upload');
                xhr.setRequestHeader('Content-Type', 'application/json');
                xhr.send(content);
            };

            reader.readAsText(file);
            return false;
        }

        function restoreDefault() {
            if (!confirm('Restore default CAN configuration?\n\nThis will delete your custom configuration and revert to the built-in defaults.')) {
                return;
            }

            const feedback = document.getElementById('feedback');
            feedback.textContent = 'Restoring default configuration...\n';

            const xhr = new XMLHttpRequest();

            xhr.addEventListener('load', function() {
                if (xhr.status === 200) {
                    feedback.textContent += 'Default configuration restored!\n';
                    feedback.textContent += 'Redirecting to CAN config page...';
                    setTimeout(() => {
                        window.location.href = '/can';
                    }, 2000);
                } else {
                    feedback.textContent += 'Failed to restore: ' + xhr.responseText;
                }
            });

            xhr.addEventListener('error', function() {
                feedback.textContent += 'Connection error';
            });

            xhr.open('POST', '/api/can/config/restore');
            xhr.send();
        }

        function checkCurrentConfig() {
            fetch('/api/can/config/status')
            .then(response => response.json())
            .then(data => {
                const statusDiv = document.getElementById('configStatus');
                if (data.custom) {
                    statusDiv.innerHTML = '<strong style="color: #4caf50;">Status:</strong> Using custom configuration<br>' +
                                        '<strong>Version:</strong> ' + data.version + '<br>' +
                                        '<strong>Size:</strong> ' + data.size + ' bytes';
                } else {
                    statusDiv.innerHTML = '<strong style="color: #1976d2;">Status:</strong> Using built-in default configuration<br>' +
                                        '<strong>Version:</strong> ' + data.version;
                }
            })
            .catch(error => {
                console.error('Error checking config status:', error);
            });
        }

        window.onload = checkCurrentConfig;
    </script>
</head>
<body>
    <div class="container">
        <h1>CAN Config Upload</h1>

        <div class="nav-buttons">
            <button type="button" class="touch-button" style="background: #7f8c8d;"
                    onclick="window.location.href='/can'">
                Back to CAN Config
            </button>
        </div>

        <div class="info-box">
            <h3>Current Configuration</h3>
            <p id="configStatus">Loading...</p>
        </div>

        <div class="card">
            <h2>Upload Custom JSON</h2>

            <div class="button-grid">
                <label for="file" class="touch-button">
                    Choose JSON File
                </label>
                <button type="button" id="uploadBtn" class="touch-button upload-button" onclick="uploadFile()" disabled>
                    Upload Configuration
                </button>
            </div>

            <input type="file" id="file" name="config" accept=".json" onchange="displayFileName()" style="display: none;">

            <div id="fileInfo" class="file-info"></div>

            <div id="feedback"></div>

            <button type="button" class="touch-button restore-button" onclick="restoreDefault()" style="width: 100%;">
                Restore Default Configuration
            </button>
        </div>

        <div class="info-box">
            <h3>JSON Format Requirements</h3>
            <p><strong>Required fields:</strong></p>
            <p>• version - Configuration version string</p>
            <p>• functions - Object defining available functions (steering, buttons, hitch, implement, keya)</p>
            <p>• busTypes - Object defining bus types (None, V_Bus, K_Bus, ISO_Bus)</p>
            <p>• brands - Array of brand configurations with capabilities</p>
        </div>

        <div class="warning-box">
            <strong>⚠️ Important</strong>
            Invalid JSON will be rejected. The file is validated before upload to prevent configuration errors.
        </div>

    </div>
</body>
</html>
)rawliteral";

#endif // CAN_CONFIG_UPLOAD_PAGE_H
