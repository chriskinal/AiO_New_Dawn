// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

// TouchFriendlyCANConfigPage.h
// CAN configuration page for touch-friendly interface

#ifndef TOUCH_FRIENDLY_CAN_CONFIG_PAGE_H
#define TOUCH_FRIENDLY_CAN_CONFIG_PAGE_H

#include <Arduino.h>

const char TOUCH_FRIENDLY_CAN_CONFIG_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <meta name="apple-mobile-web-app-capable" content="yes">
    <title>CAN Configuration</title>
    <link rel="stylesheet" href="/touch.css">
    <style>
        .status-box {
            margin: 20px 0;
            padding: 15px;
            background-color: #333;
            border-radius: 10px;
            text-align: center;
        }

        .status-connected {
            color: #4CAF50;
            font-weight: bold;
        }

        .status-disconnected {
            color: #f44336;
            font-weight: bold;
        }

        select {
            width: 100%;
            padding: 12px;
            margin: 5px 0;
            font-size: 16px;
            border: 2px solid #666;
            border-radius: 10px;
            background-color: #333;
            color: white;
            box-sizing: border-box;
        }

        .nav-buttons {
            display: grid;
            grid-template-columns: 1fr 1fr 1fr;
            gap: 15px;
            margin-bottom: 20px;
        }

        .brand-row {
            display: grid;
            grid-template-columns: 150px 1fr;
            gap: 10px;
            align-items: center;
            margin-bottom: 25px;
            padding: 15px;
            background-color: #2c2c2c;
            border-radius: 10px;
        }

        .brand-row label {
            font-weight: bold;
            font-size: 18px;
            color: white;
        }

        .can-config-grid {
            display: grid;
            gap: 15px;
            margin-bottom: 20px;
        }

        .can-row {
            display: grid;
            grid-template-columns: 80px 120px 140px 1fr;
            gap: 10px;
            align-items: center;
            padding: 15px;
            background-color: #2c2c2c;
            border-radius: 10px;
        }

        .function-checkboxes {
            display: flex;
            flex-wrap: wrap;
            gap: 15px;
            align-items: center;
        }

        .function-checkboxes label {
            display: flex;
            align-items: center;
            color: white;
            font-size: 14px;
            margin: 0;
        }

        .function-checkboxes input[type="checkbox"] {
            margin-right: 5px;
            width: 18px;
            height: 18px;
        }


        .can-label {
            font-weight: bold;
            font-size: 18px;
            color: white;
        }

        .info {
            margin-top: 20px;
            padding: 15px;
            background-color: #2c2c2c;
            border-radius: 10px;
            color: white;
        }

        .info h3 {
            margin-top: 0;
            color: white;
        }

        .info p {
            margin: 5px 0;
            color: white;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>CAN Configuration</h1>

        <div class="nav-buttons">
            <button type="button" class="touch-button" style="background: #7f8c8d;"
                    onclick="window.location.href='/'">
                Home
            </button>
            <button type="button" class="touch-button" style="background: #e74c3c;"
                    onclick="confirmRestart()">
                Restart
            </button>
            <button type="submit" class="touch-button" form="canConfigForm">
                Save
            </button>
        </div>

        <div id="statusMessage" class="status-box" style="display:none;"></div>

        <form id="canConfigForm">
            <div class="brand-row">
                <label for="brand">Tractor Brand</label>
                <select id="brand" name="brand">
                    <option value="0">Disabled</option>
                    <option value="1">Case IH/New Holland</option>
                    <option value="2">CAT MT Series</option>
                    <option value="3">Claas</option>
                    <option value="4">Fendt SCR/S4/Gen6</option>
                    <option value="5">Fendt One</option>
                    <option value="6">Generic</option>
                    <option value="7">JCB</option>
                    <option value="8">Lindner</option>
                    <option value="9">Valtra/Massey Ferguson</option>
                </select>
            </div>

            <div class="can-config-grid">
                <div class="can-row">
                    <div class="can-label">CAN1</div>
                    <select id="can1Speed" name="can1Speed">
                        <option value="0">250 kbps</option>
                        <option value="1">500 kbps</option>
                    </select>
                    <select id="can1Name" class="bus-name-select">
                        <option value="0">None</option>
                        <option value="1">Keya</option>
                        <option value="2">V_Bus</option>
                        <option value="3">K_Bus</option>
                        <option value="4">ISO_Bus</option>
                    </select>
                    <div class="function-checkboxes" id="can1Functions">
                        <!-- Dynamically populated based on brand and bus name -->
                    </div>
                    <select id="can1Function" name="can1Function" style="display:none;">
                        <!-- Values will be set dynamically by JavaScript -->
                        <option value="0">None</option>
                    </select>
                </div>

                <div class="can-row">
                    <div class="can-label">CAN2</div>
                    <select id="can2Speed" name="can2Speed">
                        <option value="0">250 kbps</option>
                        <option value="1">500 kbps</option>
                    </select>
                    <select id="can2Name" class="bus-name-select">
                        <option value="0">None</option>
                        <option value="1">Keya</option>
                        <option value="2">V_Bus</option>
                        <option value="3">K_Bus</option>
                        <option value="4">ISO_Bus</option>
                    </select>
                    <div class="function-checkboxes" id="can2Functions">
                        <!-- Dynamically populated based on brand and bus name -->
                    </div>
                    <select id="can2Function" name="can2Function" style="display:none;">
                        <!-- Values will be set dynamically by JavaScript -->
                        <option value="0">None</option>
                    </select>
                </div>

                <div class="can-row">
                    <div class="can-label">CAN3</div>
                    <select id="can3Speed" name="can3Speed">
                        <option value="0">250 kbps</option>
                        <option value="1">500 kbps</option>
                    </select>
                    <select id="can3Name" class="bus-name-select">
                        <option value="0">None</option>
                        <option value="1">Keya</option>
                        <option value="2">V_Bus</option>
                        <option value="3">K_Bus</option>
                        <option value="4">ISO_Bus</option>
                    </select>
                    <div class="function-checkboxes" id="can3Functions">
                        <!-- Dynamically populated based on brand and bus name -->
                    </div>
                    <select id="can3Function" name="can3Function" style="display:none;">
                        <!-- Values will be set dynamically by JavaScript -->
                        <option value="0">None</option>
                    </select>
                </div>
            </div>
        </form>

        <div class="info">
            <h3>Function Descriptions</h3>
            <p><strong>Keya:</strong> Keya motor control • <strong>V_Bus:</strong> Valve/steering commands • <strong>ISO_Bus:</strong> ISOBUS implement control</p>
            <p><strong>K_Bus:</strong> Tractor control bus • <strong>Generic Brand:</strong> Use when mixing functions from different brands</p>
        </div>
    </div>

    <script>
        // Load current configuration
        async function loadConfig() {
            try {
                const response = await fetch('/api/can/config');
                if (response.ok) {
                    const config = await response.json();
                    document.getElementById('brand').value = config.brand || 0;
                    document.getElementById('can1Speed').value = config.can1Speed || 0;

                    // Set hidden select values with proper options for bitfield values
                    [1, 2, 3].forEach(busNum => {
                        const speedSelect = document.getElementById(`can${busNum}Speed`);
                        const functionSelect = document.getElementById(`can${busNum}Function`);
                        const nameSelect = document.getElementById(`can${busNum}Name`);

                        const speedValue = config[`can${busNum}Speed`] || 0;
                        const functionValue = config[`can${busNum}Function`] || 0;
                        const nameValue = config[`can${busNum}Name`] || 0;

                        speedSelect.value = speedValue;
                        nameSelect.value = nameValue;

                        // For function select, ensure option exists for bitfield value
                        if (functionValue !== 0) {
                            // Remove all options except None
                            while (functionSelect.options.length > 1) {
                                functionSelect.remove(1);
                            }
                            // Add option for the saved bitfield value
                            const option = document.createElement('option');
                            option.value = functionValue;
                            option.text = 'Saved: ' + functionValue;
                            option.selected = true;
                            functionSelect.appendChild(option);
                            console.log(`CAN${busNum} loaded with function value: ${functionValue} (0x${functionValue.toString(16).padStart(2, '0')})`);
                        } else {
                            functionSelect.value = 0;
                        }
                    });
                }
            } catch (error) {
                console.error('Error loading config:', error);
                showStatus('Error loading configuration', 'error');
            }
        }

        // Save configuration
        document.getElementById('canConfigForm').addEventListener('submit', async (e) => {
            e.preventDefault();

            // Update all hidden selects from checkboxes before saving
            [1, 2, 3].forEach(busNum => updateHiddenSelect(busNum));

            const formData = new FormData(e.target);
            const config = {
                brand: parseInt(formData.get('brand')),
                can1Speed: parseInt(formData.get('can1Speed')),
                can1Function: parseInt(formData.get('can1Function')),
                can1Name: parseInt(document.getElementById('can1Name').value),
                can2Speed: parseInt(formData.get('can2Speed')),
                can2Function: parseInt(formData.get('can2Function')),
                can2Name: parseInt(document.getElementById('can2Name').value),
                can3Speed: parseInt(formData.get('can3Speed')),
                can3Function: parseInt(formData.get('can3Function')),
                can3Name: parseInt(document.getElementById('can3Name').value)
            };

            try {
                const response = await fetch('/api/can/config', {
                    method: 'POST',
                    headers: {
                        'Content-Type': 'application/json'
                    },
                    body: JSON.stringify(config)
                });

                if (response.ok) {
                    const result = await response.json();
                    showStatus(result.message || 'Configuration saved', 'success');
                } else {
                    showStatus('Error saving configuration', 'error');
                }
            } catch (error) {
                console.error('Error saving config:', error);
                showStatus('Network error', 'error');
            }
        });

        // Show status message
        function showStatus(message, type) {
            const statusDiv = document.getElementById('statusMessage');
            statusDiv.textContent = message;
            statusDiv.style.display = 'block';
            statusDiv.className = 'status-box ' + (type === 'success' ? 'status-connected' : 'status-disconnected');

            // Auto-hide after 5 seconds
            setTimeout(() => {
                statusDiv.style.display = 'none';
            }, 5000);
        }

        // Touch-friendly confirmation dialog
        function confirmRestart() {
            if (confirm('Are you sure you want to restart the system?')) {
                restartSystem();
            }
        }

        function restartSystem() {
            fetch('/api/restart', { method: 'POST' })
                .then(response => {
                    if (response.ok) {
                        showStatus('System is restarting...', 'success');
                    }
                })
                .catch(error => {
                    showStatus('Error restarting system', 'error');
                });
        }

        // Brand capabilities definition with bus-specific functions (matches alphabetized enum)
        const brandCapabilities = {
            0: { // Disabled
                name: 'Disabled',
                busTypes: {}
            },
            1: { // Case IH/NH
                name: 'Case IH/New Holland',
                busTypes: {
                    'V_Bus': ['steering'],
                    'K_Bus': ['buttons', 'hitch'],
                    'ISO_Bus': []
                }
            },
            2: { // CAT MT
                name: 'CAT MT Series',
                busTypes: {
                    'V_Bus': ['steering'],
                    'K_Bus': ['buttons'],
                    'ISO_Bus': []
                }
            },
            3: { // Claas
                name: 'Claas',
                busTypes: {
                    'V_Bus': ['steering'],
                    'K_Bus': ['buttons'],
                    'ISO_Bus': []
                }
            },
            4: { // Fendt SCR/S4/Gen6
                name: 'Fendt SCR/S4/Gen6',
                busTypes: {
                    'V_Bus': ['steering'],
                    'K_Bus': ['buttons', 'hitch'],
                    'ISO_Bus': []
                }
            },
            5: { // Fendt One
                name: 'Fendt One',
                busTypes: {
                    'V_Bus': ['steering'],
                    'K_Bus': ['buttons', 'hitch'],
                    'ISO_Bus': ['steering', 'implement']
                }
            },
            6: { // Generic
                name: 'Generic',
                busTypes: {
                    'V_Bus': ['steering'],
                    'K_Bus': ['buttons', 'hitch'],
                    'ISO_Bus': ['steering', 'implement']
                },
                // Special case: Generic brand with bus name "None" shows Keya option
                allowsKeya: true
            },
            7: { // JCB
                name: 'JCB',
                busTypes: {
                    'V_Bus': ['steering'],
                    'K_Bus': [],
                    'ISO_Bus': []
                }
            },
            8: { // Lindner
                name: 'Lindner',
                busTypes: {
                    'V_Bus': ['steering'],
                    'K_Bus': [],
                    'ISO_Bus': []
                }
            },
            9: { // Valtra/Massey Ferguson
                name: 'Valtra/Massey Ferguson',
                busTypes: {
                    'V_Bus': ['steering'],
                    'K_Bus': ['buttons', 'hitch'],
                    'ISO_Bus': []
                }
            }
        };

        // Function value mapping (bitfield values)
        const functionValues = {
            'NONE': 0x00,
            'STEERING': 0x01,
            'BUTTONS': 0x02,
            'HITCH': 0x04,
            'IMPLEMENT': 0x08,
            'KEYA': 0x10
        };

        // Function labels
        const functionLabels = {
            'steering': 'Steering',
            'buttons': 'Buttons',
            'hitch': 'Hitch',
            'implement': 'Implement',
            'keya': 'Keya Motor'
        };

        // Update function checkboxes based on brand AND bus name
        function updateFunctionOptions(busNum) {
            const brand = parseInt(document.getElementById('brand').value);
            const capabilities = brandCapabilities[brand] || brandCapabilities[9];
            const busNameSelect = document.getElementById(`can${busNum}Name`);
            const busName = busNameSelect.options[busNameSelect.selectedIndex].text;

            const container = document.getElementById(`can${busNum}Functions`);
            container.innerHTML = '';

            console.log(`updateFunctionOptions CAN${busNum}: brand=${brand}, busName="${busName}"`);

            // If bus name is "None", only show functions for brands that support it
            if (busName === 'None') {
                // Special case: Generic brand with "None" shows Keya option
                if (brand === 6 && capabilities.allowsKeya) {
                    const availableFunctions = ['keya'];
                    createFunctionCheckboxes(availableFunctions, container, busNum);
                }
                return;
            }

            // Get available functions for this brand and bus type
            const availableFunctions = capabilities.busTypes[busName] || [];
            console.log(`Available functions for ${busName}:`, availableFunctions);
            createFunctionCheckboxes(availableFunctions, container, busNum);
        }

        // Helper function to create checkboxes
        function createFunctionCheckboxes(availableFunctions, container, busNum) {
            availableFunctions.forEach(funcKey => {
                const label = document.createElement('label');
                const checkbox = document.createElement('input');
                checkbox.type = 'checkbox';
                checkbox.id = `can${busNum}_${funcKey}`;
                checkbox.dataset.function = funcKey;
                checkbox.dataset.busNum = busNum;
                console.log(`Created checkbox: ${checkbox.id}`);

                label.appendChild(checkbox);
                label.appendChild(document.createTextNode(` ${functionLabels[funcKey] || funcKey}`));

                checkbox.addEventListener('change', function() {
                    updateHiddenSelect(busNum);
                });

                container.appendChild(label);
            });
        }

        // Update all bus function options based on brand
        function updateAllBusFunctions() {
            const brand = parseInt(document.getElementById('brand').value);
            [1, 2, 3].forEach(busNum => updateFunctionOptions(busNum));
            updateInfoText(brand);
        }

        // Update hidden select based on checkboxes (supports multiple selections via bitfield)
        function updateHiddenSelect(busNum) {
            const hiddenSelect = document.getElementById(`can${busNum}Function`);
            const checkboxes = document.querySelectorAll(`#can${busNum}Functions input[type="checkbox"]`);
            const busNameSelect = document.getElementById(`can${busNum}Name`);
            const busNameValue = parseInt(busNameSelect.value);

            let selectedFunctions = 0x00; // Start with NONE

            checkboxes.forEach(cb => {
                if (cb.checked) {
                    const funcKey = cb.dataset.function;

                    if (funcKey === 'steering') {
                        selectedFunctions |= functionValues['STEERING'];
                    } else if (funcKey === 'buttons') {
                        selectedFunctions |= functionValues['BUTTONS'];
                    } else if (funcKey === 'hitch') {
                        selectedFunctions |= functionValues['HITCH'];
                    } else if (funcKey === 'implement') {
                        selectedFunctions |= functionValues['IMPLEMENT'];
                    } else if (funcKey === 'keya') {
                        selectedFunctions |= functionValues['KEYA'];
                    }
                }
            });

            // Ensure the select has an option with the combined value
            // Remove all options except the first (None)
            while (hiddenSelect.options.length > 1) {
                hiddenSelect.remove(1);
            }

            // Add option for the combined value if it's not 0
            if (selectedFunctions !== 0) {
                const option = document.createElement('option');
                option.value = selectedFunctions;
                option.text = 'Combined: ' + selectedFunctions;
                option.selected = true;
                hiddenSelect.appendChild(option);
            } else {
                hiddenSelect.value = 0; // Select None
            }

            console.log(`CAN${busNum} Function set to: ${selectedFunctions} (0x${selectedFunctions.toString(16).padStart(2, '0')})`);
        }

        // Update info text to show relevant functions only
        function updateInfoText(brand) {
            const infoDiv = document.querySelector('.info');
            const capabilities = brandCapabilities[brand] || brandCapabilities[9];

            let infoHtml = '<h3>Function Descriptions</h3>';

            if (brand === 0) { // Disabled
                infoHtml += '<p>CAN bus functions are disabled.</p>';
            } else {
                const descriptions = {
                    'steering': 'Control steering valve/motor',
                    'buttons': 'Read control buttons',
                    'hitch': 'Control 3-point hitch',
                    'implement': 'ISOBUS implement control',
                    'keya': 'Keya CAN motor control'
                };

                const functionDescParts = [];
                // Gather all unique functions from all bus types
                const allFunctions = new Set();
                Object.values(capabilities.busTypes).forEach(functions => {
                    functions.forEach(func => allFunctions.add(func));
                });

                // Special case: add keya if allowed
                if (capabilities.allowsKeya) {
                    allFunctions.add('keya');
                }

                // Build descriptions for available functions
                allFunctions.forEach(funcKey => {
                    if (descriptions[funcKey]) {
                        let desc = `<strong>${functionLabels[funcKey] || funcKey}:</strong> ${descriptions[funcKey]}`;
                        functionDescParts.push(desc);
                    }
                });

                if (functionDescParts.length > 0) {
                    infoHtml += `<p>${functionDescParts.join(' • ')}</p>`;
                }

                // Add brand-specific notes
                if (brand === 1) { // Case IH
                    infoHtml += '<p>V_Bus for steering • K_Bus for engage button and hitch control</p>';
                } else if (brand === 4) { // Fendt
                    infoHtml += '<p>K_Bus typically handles buttons and hitch • V_Bus for steering</p>';
                } else if (brand === 6) { // Generic
                    infoHtml += '<p><strong>Generic Brand:</strong> Use when mixing functions from different brands or using Keya steering</p>';
                } else if (brand === 9) { // Valtra
                    infoHtml += '<p>V_Bus steering only - requires continuous valve ready messages</p>';
                }
            }

            infoDiv.innerHTML = infoHtml;
        }

        // Load saved config into checkboxes (handles bitfield values)
        function loadConfigIntoUI() {
            // Use setTimeout to ensure DOM is fully ready
            setTimeout(() => {
                [1, 2, 3].forEach(busNum => {
                    const hiddenSelect = document.getElementById(`can${busNum}Function`);
                    const savedValue = parseInt(hiddenSelect.value);

                    console.log(`CAN${busNum}: savedValue=0x${savedValue.toString(16)}`);

                    if (savedValue !== 0x00) {
                        // Check each bit and set corresponding checkboxes
                        if (savedValue & functionValues['STEERING']) {
                            const steeringCb = document.getElementById(`can${busNum}_steering`);
                            if (steeringCb) {
                                steeringCb.checked = true;
                                console.log(`Checked steering for CAN${busNum}`);
                            }
                        }
                        if (savedValue & functionValues['BUTTONS']) {
                            const buttonsCb = document.getElementById(`can${busNum}_buttons`);
                            if (buttonsCb) {
                                buttonsCb.checked = true;
                                console.log(`Checked buttons for CAN${busNum}`);
                            }
                        }
                        if (savedValue & functionValues['HITCH']) {
                            const hitchCb = document.getElementById(`can${busNum}_hitch`);
                            if (hitchCb) {
                                hitchCb.checked = true;
                                console.log(`Checked hitch for CAN${busNum}`);
                            }
                        }
                        if (savedValue & functionValues['IMPLEMENT']) {
                            const implementCb = document.getElementById(`can${busNum}_implement`);
                            if (implementCb) {
                                implementCb.checked = true;
                                console.log(`Checked implement for CAN${busNum}`);
                            }
                        }
                        if (savedValue & functionValues['KEYA']) {
                            const keyaCb = document.getElementById(`can${busNum}_keya`);
                            if (keyaCb) {
                                keyaCb.checked = true;
                                console.log(`Checked keya for CAN${busNum}`);
                            }
                        }
                    }
                });
            }, 200); // Increased delay
        }

        // Load configuration on page load
        window.addEventListener('DOMContentLoaded', async function() {
            const brandSelect = document.getElementById('brand');
            brandSelect.addEventListener('change', () => {
                updateAllBusFunctions();
                // Don't restore checkboxes when brand changes manually
            });

            // Add event listeners for bus name changes
            [1, 2, 3].forEach(busNum => {
                const busNameSelect = document.getElementById(`can${busNum}Name`);
                busNameSelect.addEventListener('change', () => {
                    updateFunctionOptions(busNum);
                    // Update the hidden select when bus name changes
                    updateHiddenSelect(busNum);
                });
            });

            // Load configuration and update UI in proper sequence
            await loadConfig();
            // First create all the checkboxes based on brand and bus names
            updateAllBusFunctions();
            // Wait a bit for DOM to update after creating checkboxes
            setTimeout(() => {
                // Then restore the checkbox states
                loadConfigIntoUI();
            }, 100);
        });
    </script>
</body>
</html>
)rawliteral";

#endif // TOUCH_FRIENDLY_CAN_CONFIG_PAGE_H