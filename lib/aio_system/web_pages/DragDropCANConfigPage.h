// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

// DragDropCANConfigPage.h
// Modern drag-and-drop CAN configuration interface

#ifndef DRAG_DROP_CAN_CONFIG_PAGE_H
#define DRAG_DROP_CAN_CONFIG_PAGE_H

#include <Arduino.h>

const char DRAG_DROP_CAN_CONFIG_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <meta name="apple-mobile-web-app-capable" content="yes">
    <title>CAN Configuration</title>
    <style>
        * {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
        }

        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: linear-gradient(135deg, #1e3c72 0%, #2a5298 100%);
            color: #333;
            min-height: 100vh;
            padding: 20px;
        }

        .container {
            max-width: 1200px;
            margin: 0 auto;
        }

        h1 {
            color: white;
            text-align: center;
            margin-bottom: 20px;
            font-size: 28px;
        }

        .nav-buttons {
            display: grid;
            grid-template-columns: 1fr 1fr 1fr 1fr;
            gap: 15px;
            margin-bottom: 20px;
        }

        .touch-button {
            padding: 15px;
            border: none;
            border-radius: 10px;
            font-size: 16px;
            font-weight: bold;
            color: white;
            cursor: pointer;
            transition: all 0.3s ease;
            min-height: 50px;
        }

        .touch-button:active {
            transform: scale(0.95);
        }

        .btn-home { background: #7f8c8d; }
        .btn-upload { background: #3498db; }
        .btn-restart { background: #e74c3c; }
        .btn-save { background: #27ae60; }

        .status-message {
            padding: 15px;
            border-radius: 10px;
            text-align: center;
            margin-bottom: 20px;
            display: none;
            font-weight: bold;
        }

        .status-success {
            background: #d4edda;
            color: #155724;
        }

        .status-error {
            background: #f8d7da;
            color: #721c24;
        }

        .brand-selector {
            background: white;
            padding: 15px;
            border-radius: 10px;
            margin-bottom: 15px;
            box-shadow: 0 4px 6px rgba(0,0,0,0.1);
            display: flex;
            align-items: center;
            gap: 15px;
        }

        .brand-selector label {
            font-weight: bold;
            font-size: 16px;
            white-space: nowrap;
        }

        .brand-selector select {
            flex: 1;
            padding: 10px;
            font-size: 16px;
            border: 2px solid #ddd;
            border-radius: 8px;
            background: white;
        }

        .function-pool {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            border-radius: 10px;
            padding: 15px;
            margin-bottom: 15px;
            height: 80px;
            display: flex;
            align-items: center;
            gap: 20px;
        }

        .function-pool-header {
            display: flex;
            flex-direction: column;
            gap: 3px;
            flex-shrink: 0;
        }

        .function-pool-title {
            color: white;
            font-weight: bold;
            font-size: 16px;
            white-space: nowrap;
        }

        .function-pool-subtitle {
            color: rgba(255, 255, 255, 0.85);
            font-size: 13px;
            font-weight: normal;
            white-space: nowrap;
        }

        .function-cards {
            display: flex;
            gap: 10px;
            flex-wrap: wrap;
            flex: 1;
            align-content: center;
        }

        .function-card {
            min-width: 90px;
            padding: 10px 15px;
            background: white;
            border-radius: 8px;
            display: flex;
            align-items: center;
            justify-content: center;
            cursor: grab;
            transition: all 0.3s ease;
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
            border: 2px solid;
            user-select: none;
            -webkit-user-select: none;
            touch-action: none;
        }

        .function-card:hover {
            transform: translateY(-2px);
            box-shadow: 0 4px 8px rgba(0,0,0,0.2);
        }

        .function-card.dragging {
            opacity: 0.5;
            cursor: grabbing;
        }

        .function-card.hidden {
            display: none;
        }

        .function-name {
            font-size: 14px;
            font-weight: bold;
            text-align: center;
        }

        .can-bus-section {
            background: white;
            border-radius: 10px;
            padding: 15px;
            margin-bottom: 10px;
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
        }

        .bus-header {
            display: flex;
            align-items: center;
            gap: 10px;
            margin-bottom: 10px;
        }

        .bus-title {
            font-size: 16px;
            font-weight: bold;
            color: #2c3e50;
        }

        .drop-zone {
            min-height: 60px;
            border: 2px dashed #dee2e6;
            border-radius: 8px;
            padding: 10px;
            display: flex;
            gap: 8px;
            flex-wrap: wrap;
            align-content: flex-start;
            transition: all 0.3s ease;
            background: #f8f9fa;
            margin-bottom: 10px;
        }

        .bus-controls {
            display: flex;
            gap: 10px;
            align-items: center;
        }

        .bus-config {
            display: flex;
            gap: 10px;
            align-items: center;
            flex: 1;
        }

        .bus-config label {
            font-size: 13px;
            font-weight: 600;
            color: #555;
        }

        .bus-config select {
            padding: 6px 10px;
            border: 2px solid #ddd;
            border-radius: 6px;
            font-size: 13px;
        }

        .clear-button {
            padding: 6px 15px;
            background: #e74c3c;
            color: white;
            border: none;
            border-radius: 6px;
            cursor: pointer;
            font-size: 13px;
            font-weight: bold;
        }

        .clear-button:hover {
            background: #c0392b;
        }

        .drop-zone.empty::before {
            content: 'Drop functions here...';
            color: #adb5bd;
            font-style: italic;
            width: 100%;
            text-align: center;
            line-height: 40px;
            font-size: 13px;
        }

        .drop-zone.drag-over {
            background: #d4edda;
            border-color: #28a745;
            box-shadow: 0 0 20px rgba(40, 167, 69, 0.3);
        }

        .drop-zone.drag-invalid {
            background: #f8d7da;
            border-color: #dc3545;
            animation: shake 0.5s;
        }

        @keyframes shake {
            0%, 100% { transform: translateX(0); }
            25% { transform: translateX(-5px); }
            75% { transform: translateX(5px); }
        }

        .info-box {
            background: white;
            padding: 20px;
            border-radius: 15px;
            margin-top: 20px;
            box-shadow: 0 4px 6px rgba(0,0,0,0.1);
        }

        .info-box h3 {
            margin-bottom: 10px;
            color: #2c3e50;
        }

        .info-box p {
            color: #555;
            line-height: 1.6;
        }

        /* Touch-specific enhancements */
        @media (pointer: coarse) {
            .function-pool {
                height: 90px;
            }

            .function-card {
                padding: 12px 18px;
                min-width: 100px;
            }

            .drop-zone {
                min-height: 70px;
            }

            .touch-button {
                min-height: 55px;
                font-size: 18px;
            }
        }

        /* Responsive design for mobile */
        @media (max-width: 768px) {
            body {
                padding: 10px;
            }

            h1 {
                font-size: 22px;
            }

            .function-pool {
                height: auto;
                min-height: 100px;
                flex-direction: column;
                align-items: flex-start;
                gap: 10px;
            }

            .function-cards {
                justify-content: center;
            }

            .bus-controls {
                flex-wrap: wrap;
            }

            .bus-config {
                flex-wrap: wrap;
            }

            .bus-config select {
                flex: 1;
                min-width: 100px;
            }

            .function-card {
                padding: 10px 12px;
                min-width: 80px;
            }

            .function-name {
                font-size: 13px;
            }
        }

        /* Notification styles */
        .notification {
            position: fixed;
            top: 20px;
            right: 20px;
            background: white;
            padding: 15px 20px;
            border-radius: 10px;
            box-shadow: 0 4px 12px rgba(0,0,0,0.3);
            z-index: 1000;
            animation: slideIn 0.3s ease;
        }

        .notification.fade-out {
            animation: fadeOut 0.3s ease;
        }

        @keyframes slideIn {
            from {
                transform: translateX(400px);
                opacity: 0;
            }
            to {
                transform: translateX(0);
                opacity: 1;
            }
        }

        @keyframes fadeOut {
            to {
                opacity: 0;
                transform: translateX(400px);
            }
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>CAN Bus Configuration</h1>

        <div class="nav-buttons">
            <button type="button" class="touch-button btn-home" onclick="window.location.href='/'">
                Home
            </button>
            <button type="button" class="touch-button btn-upload" onclick="window.location.href='/can/upload'">
                Upload JSON
            </button>
            <button type="button" class="touch-button btn-restart" onclick="confirmRestart()">
                Restart
            </button>
            <button type="button" class="touch-button btn-save" onclick="saveConfiguration()">
                Save
            </button>
        </div>

        <div id="statusMessage" class="status-message"></div>

        <div class="brand-selector">
            <label for="brandSelect">Tractor Brand</label>
            <select id="brandSelect" onchange="onBrandChange()">
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

        <div class="function-pool">
            <div class="function-pool-header">
                <div class="function-pool-title">Available Functions</div>
                <div class="function-pool-subtitle">(Drag to CAN Bus)</div>
            </div>
            <div class="function-cards" id="functionPool">
                <!-- Function cards will be dynamically generated -->
            </div>
        </div>

        <div class="can-bus-section">
            <div class="bus-header">
                <div class="bus-title">CAN1</div>
            </div>
            <div class="drop-zone empty" id="can1DropZone" data-bus="1"></div>
            <div class="bus-controls">
                <div class="bus-config">
                    <label>Speed:</label>
                    <select id="can1Speed">
                        <option value="0">250 kbps</option>
                        <option value="1">500 kbps</option>
                    </select>
                    <label>Bus:</label>
                    <select id="can1Name" onchange="onBusNameChange(1)">
                        <option value="0">None</option>
                        <option value="1">V_Bus</option>
                        <option value="2">K_Bus</option>
                        <option value="3">ISO_Bus</option>
                    </select>
                </div>
                <button class="clear-button" onclick="clearBus(1)">Clear</button>
            </div>
        </div>

        <div class="can-bus-section">
            <div class="bus-header">
                <div class="bus-title">CAN2</div>
            </div>
            <div class="drop-zone empty" id="can2DropZone" data-bus="2"></div>
            <div class="bus-controls">
                <div class="bus-config">
                    <label>Speed:</label>
                    <select id="can2Speed">
                        <option value="0">250 kbps</option>
                        <option value="1">500 kbps</option>
                    </select>
                    <label>Bus:</label>
                    <select id="can2Name" onchange="onBusNameChange(2)">
                        <option value="0">None</option>
                        <option value="1">V_Bus</option>
                        <option value="2">K_Bus</option>
                        <option value="3">ISO_Bus</option>
                    </select>
                </div>
                <button class="clear-button" onclick="clearBus(2)">Clear</button>
            </div>
        </div>

        <div class="can-bus-section">
            <div class="bus-header">
                <div class="bus-title">CAN3</div>
            </div>
            <div class="drop-zone empty" id="can3DropZone" data-bus="3"></div>
            <div class="bus-controls">
                <div class="bus-config">
                    <label>Speed:</label>
                    <select id="can3Speed">
                        <option value="0">250 kbps</option>
                        <option value="1">500 kbps</option>
                    </select>
                    <label>Bus:</label>
                    <select id="can3Name" onchange="onBusNameChange(3)">
                        <option value="0">None</option>
                        <option value="1">V_Bus</option>
                        <option value="2">K_Bus</option>
                        <option value="3">ISO_Bus</option>
                    </select>
                </div>
                <button class="clear-button" onclick="clearBus(3)">Clear</button>
            </div>
        </div>

        <div class="info-box" id="infoBox">
            <h3>Function Descriptions</h3>
            <p>Select a tractor brand to see available functions.</p>
        </div>
    </div>

    <script>
        // Configuration state
        const state = {
            selectedBrand: 6,
            busAssignments: {
                1: [],
                2: [],
                3: []
            },
            draggedElement: null,
            draggedFunction: null,
            touchOffset: { x: 0, y: 0 }
        };

        // Configuration data loaded from JSON
        let canInfo = null;
        let brandCapabilities = {};
        let functionDefinitions = {};
        let busNameLabels = {};

        // Legacy hardcoded brand capabilities (fallback)
        const legacyBrandCapabilities = {
            0: { name: 'Disabled', busTypes: {} },
            1: {
                name: 'Case IH/New Holland',
                busTypes: {
                    'V_Bus': ['steering'],
                    'K_Bus': ['buttons', 'hitch'],
                    'ISO_Bus': []
                }
            },
            2: {
                name: 'CAT MT Series',
                busTypes: {
                    'V_Bus': ['steering'],
                    'K_Bus': ['buttons'],
                    'ISO_Bus': []
                }
            },
            3: {
                name: 'Claas',
                busTypes: {
                    'V_Bus': ['steering'],
                    'K_Bus': ['buttons'],
                    'ISO_Bus': []
                }
            },
            4: {
                name: 'Fendt SCR/S4/Gen6',
                busTypes: {
                    'V_Bus': ['steering'],
                    'K_Bus': ['buttons', 'hitch'],
                    'ISO_Bus': []
                }
            },
            5: {
                name: 'Fendt One',
                busTypes: {
                    'V_Bus': ['steering'],
                    'K_Bus': ['buttons', 'hitch'],
                    'ISO_Bus': ['steering', 'implement']
                }
            },
            6: {
                name: 'Generic',
                busTypes: {
                    'V_Bus': ['steering'],
                    'K_Bus': ['buttons', 'hitch'],
                    'ISO_Bus': ['steering', 'implement']
                },
                allowsKeya: true
            },
            7: {
                name: 'JCB',
                busTypes: {
                    'V_Bus': ['steering'],
                    'K_Bus': [],
                    'ISO_Bus': []
                }
            },
            8: {
                name: 'Lindner',
                busTypes: {
                    'V_Bus': ['steering'],
                    'K_Bus': [],
                    'ISO_Bus': []
                }
            },
            9: {
                name: 'Valtra/Massey Ferguson',
                busTypes: {
                    'V_Bus': ['steering'],
                    'K_Bus': ['buttons', 'hitch'],
                    'ISO_Bus': []
                }
            }
        };

        // Legacy function definitions (fallback)
        const legacyFunctionDefinitions = {
            'steering': {
                name: 'Steering',
                color: '#3498db',
                description: 'Valve/Motor steering control',
                exclusive: true,
                bitValue: 0x01
            },
            'buttons': {
                name: 'Buttons',
                color: '#2ecc71',
                description: 'Control button inputs',
                exclusive: false,
                bitValue: 0x02
            },
            'hitch': {
                name: 'Hitch',
                color: '#e74c3c',
                description: '3-point hitch control',
                exclusive: false,
                bitValue: 0x04
            },
            'implement': {
                name: 'Implement',
                color: '#f39c12',
                description: 'ISO implement control',
                exclusive: false,
                bitValue: 0x08
            },
            'keya': {
                name: 'Keya Motor',
                color: '#9b59b6',
                description: 'Keya CAN motor control',
                exclusive: true,
                bitValue: 0x10
            }
        };

        const legacyBusNameLabels = {
            0: 'None',
            1: 'V_Bus',
            2: 'K_Bus',
            3: 'ISO_Bus'
        };

        // Load CAN info from JSON endpoint
        async function loadCANInfo() {
            try {
                const response = await fetch('/api/can/info');
                if (response.ok) {
                    canInfo = await response.json();

                    // Build functionDefinitions from JSON
                    functionDefinitions = {};
                    for (const [key, func] of Object.entries(canInfo.functions)) {
                        functionDefinitions[key] = {
                            name: func.name,
                            color: func.color,
                            description: func.description,
                            exclusive: func.exclusive,
                            bitValue: func.bitValue
                        };
                    }

                    // Build busNameLabels from JSON
                    busNameLabels = {};
                    for (const [key, bus] of Object.entries(canInfo.busTypes)) {
                        busNameLabels[bus.id] = key;
                    }

                    // Build brandCapabilities from JSON brands
                    brandCapabilities = {};
                    for (const brand of canInfo.brands) {
                        brandCapabilities[brand.id] = {
                            name: brand.displayName,
                            busTypes: brand.capabilities,
                            allowsKeya: brand.allowsKeya || false,
                            uiNotes: brand.uiNotes
                        };
                    }

                    // Update brand selector options from JSON
                    const brandSelect = document.getElementById('brandSelect');
                    brandSelect.innerHTML = '';
                    for (const brand of canInfo.brands) {
                        const option = document.createElement('option');
                        option.value = brand.id;
                        option.textContent = brand.displayName;
                        brandSelect.appendChild(option);
                    }

                    console.log('Loaded CAN info from JSON');
                    return true;
                } else {
                    throw new Error('Failed to fetch CAN info');
                }
            } catch (error) {
                console.error('Error loading CAN info, using fallback:', error);
                // Use legacy hardcoded data
                brandCapabilities = legacyBrandCapabilities;
                functionDefinitions = legacyFunctionDefinitions;
                busNameLabels = legacyBusNameLabels;
                return false;
            }
        }

        // Initialize
        document.addEventListener('DOMContentLoaded', async function() {
            await loadCANInfo();
            await loadConfiguration();
            setupDragAndDrop();
        });

        // Load configuration from backend
        async function loadConfiguration() {
            try {
                const response = await fetch('/api/can/config');
                if (response.ok) {
                    const config = await response.json();

                    // Set brand
                    state.selectedBrand = config.brand || 6;
                    document.getElementById('brandSelect').value = state.selectedBrand;

                    // Set bus speeds and names
                    for (let i = 1; i <= 3; i++) {
                        document.getElementById(`can${i}Speed`).value = config[`can${i}Speed`] || 0;
                        document.getElementById(`can${i}Name`).value = config[`can${i}Name`] || 0;

                        // Parse functions from bitfield
                        const functions = bitfieldToFunctions(config[`can${i}Function`] || 0);
                        state.busAssignments[i] = functions;
                    }

                    updateFunctionPool();
                    updateAllDropZones();
                    updateInfoBox();
                }
            } catch (error) {
                console.error('Error loading config:', error);
                showNotification('Error loading configuration', 'error');
            }
        }

        // Save configuration to backend
        async function saveConfiguration() {
            const config = {
                brand: state.selectedBrand,
                can1Speed: parseInt(document.getElementById('can1Speed').value),
                can1Name: parseInt(document.getElementById('can1Name').value),
                can1Function: functionsToBitfield(state.busAssignments[1]),
                can2Speed: parseInt(document.getElementById('can2Speed').value),
                can2Name: parseInt(document.getElementById('can2Name').value),
                can2Function: functionsToBitfield(state.busAssignments[2]),
                can3Speed: parseInt(document.getElementById('can3Speed').value),
                can3Name: parseInt(document.getElementById('can3Name').value),
                can3Function: functionsToBitfield(state.busAssignments[3])
            };

            try {
                const response = await fetch('/api/can/config', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify(config)
                });

                if (response.ok) {
                    const result = await response.json();
                    showNotification(result.message || 'Configuration saved successfully', 'success');
                } else {
                    showNotification('Error saving configuration', 'error');
                }
            } catch (error) {
                console.error('Error saving config:', error);
                showNotification('Network error', 'error');
            }
        }

        // Convert bitfield to function array
        function bitfieldToFunctions(bitfield) {
            const functions = [];
            if (bitfield & 0x01) functions.push('steering');
            if (bitfield & 0x02) functions.push('buttons');
            if (bitfield & 0x04) functions.push('hitch');
            if (bitfield & 0x08) functions.push('implement');
            if (bitfield & 0x10) functions.push('keya');
            return functions;
        }

        // Convert function array to bitfield
        function functionsToBitfield(functions) {
            let bitfield = 0;
            functions.forEach(func => {
                bitfield |= functionDefinitions[func].bitValue;
            });
            return bitfield;
        }

        // Brand change handler
        function onBrandChange() {
            state.selectedBrand = parseInt(document.getElementById('brandSelect').value);
            updateFunctionPool();
            updateInfoBox();
        }

        // Bus name change handler
        function onBusNameChange(busNum) {
            // Check for duplicate bus names
            const selectedValue = parseInt(document.getElementById(`can${busNum}Name`).value);

            if (selectedValue !== 0) { // If not "None"
                // Check other buses for the same selection
                for (let i = 1; i <= 3; i++) {
                    if (i !== busNum) {
                        const otherValue = parseInt(document.getElementById(`can${i}Name`).value);
                        if (otherValue === selectedValue) {
                            // Duplicate found - revert this selection and show error
                            document.getElementById(`can${busNum}Name`).value = 0;
                            const busName = busNameLabels[selectedValue];
                            showNotification(`${busName} is already assigned to CAN${i}. Each bus name can only be used once.`, 'error');
                            updateFunctionPool();
                            return;
                        }
                    }
                }
            }

            updateFunctionPool();
        }

        // Update function pool based on brand and current assignments
        function updateFunctionPool() {
            const pool = document.getElementById('functionPool');
            pool.innerHTML = '';

            const brand = brandCapabilities[state.selectedBrand];
            if (!brand || state.selectedBrand === 0) {
                return;
            }

            // Collect all available functions for this brand across ALL bus types
            const availableFunctions = new Set();

            // Add all functions from all bus types defined for this brand
            Object.values(brand.busTypes).forEach(busTypeFunctions => {
                busTypeFunctions.forEach(func => availableFunctions.add(func));
            });

            // Create cards for available functions
            availableFunctions.forEach(funcKey => {
                const func = functionDefinitions[funcKey];
                const card = createFunctionCard(funcKey, func);

                // Hide if already assigned
                const isAssigned = Object.values(state.busAssignments).some(
                    assigned => assigned.includes(funcKey)
                );
                if (isAssigned) {
                    card.classList.add('hidden');
                }

                pool.appendChild(card);
            });
        }

        // Create a function card element
        function createFunctionCard(funcKey, func) {
            const card = document.createElement('div');
            card.className = 'function-card';
            card.draggable = true;
            card.dataset.function = funcKey;
            card.style.borderColor = func.color;

            const name = document.createElement('div');
            name.className = 'function-name';
            name.textContent = func.name;
            name.style.color = func.color;

            card.appendChild(name);

            return card;
        }

        // Update all drop zones with assigned functions
        function updateAllDropZones() {
            for (let busNum = 1; busNum <= 3; busNum++) {
                updateDropZone(busNum);
            }
        }

        // Update a specific drop zone
        function updateDropZone(busNum) {
            const zone = document.getElementById(`can${busNum}DropZone`);
            zone.innerHTML = '';

            const assigned = state.busAssignments[busNum];
            if (assigned.length === 0) {
                zone.classList.add('empty');
            } else {
                zone.classList.remove('empty');
                assigned.forEach(funcKey => {
                    const func = functionDefinitions[funcKey];
                    const card = createFunctionCard(funcKey, func);
                    card.onclick = () => removeFunction(funcKey, busNum);
                    zone.appendChild(card);
                });
            }
        }

        // Setup drag and drop events
        function setupDragAndDrop() {
            // Desktop drag events
            document.addEventListener('dragstart', handleDragStart);
            document.addEventListener('dragend', handleDragEnd);

            // Touch events
            document.addEventListener('touchstart', handleTouchStart, { passive: false });
            document.addEventListener('touchmove', handleTouchMove, { passive: false });
            document.addEventListener('touchend', handleTouchEnd);

            // Setup drop zones
            for (let busNum = 1; busNum <= 3; busNum++) {
                const zone = document.getElementById(`can${busNum}DropZone`);
                zone.addEventListener('dragover', handleDragOver);
                zone.addEventListener('dragleave', handleDragLeave);
                zone.addEventListener('drop', handleDrop);
            }
        }

        function handleDragStart(e) {
            if (e.target.classList.contains('function-card') &&
                e.target.parentElement.id === 'functionPool') {
                state.draggedElement = e.target;
                state.draggedFunction = e.target.dataset.function;
                e.target.classList.add('dragging');
                e.dataTransfer.effectAllowed = 'copy';
            }
        }

        function handleDragEnd(e) {
            if (state.draggedElement) {
                state.draggedElement.classList.remove('dragging');
                state.draggedElement = null;
                state.draggedFunction = null;
            }
            // Clear all drag states
            document.querySelectorAll('.drop-zone').forEach(zone => {
                zone.classList.remove('drag-over', 'drag-invalid');
            });
        }

        function handleDragOver(e) {
            e.preventDefault();
            if (!state.draggedFunction) return;

            const zone = e.currentTarget;
            const busNum = parseInt(zone.dataset.bus);
            const validation = canDropFunction(state.draggedFunction, busNum);

            zone.classList.remove('drag-over', 'drag-invalid');
            if (validation.allowed) {
                zone.classList.add('drag-over');
            } else {
                zone.classList.add('drag-invalid');
            }
        }

        function handleDragLeave(e) {
            const zone = e.currentTarget;
            zone.classList.remove('drag-over', 'drag-invalid');
        }

        function handleDrop(e) {
            e.preventDefault();
            if (!state.draggedFunction) return;

            const zone = e.currentTarget;
            const busNum = parseInt(zone.dataset.bus);
            const validation = canDropFunction(state.draggedFunction, busNum);

            zone.classList.remove('drag-over', 'drag-invalid');

            if (validation.allowed) {
                assignFunction(state.draggedFunction, busNum);
            } else {
                showNotification(validation.reason, 'error');
            }
        }

        // Touch event handlers
        function handleTouchStart(e) {
            const target = e.target.closest('.function-card');
            if (target && target.parentElement.id === 'functionPool') {
                state.draggedElement = target;
                state.draggedFunction = target.dataset.function;

                const touch = e.touches[0];
                const rect = target.getBoundingClientRect();
                state.touchOffset.x = touch.clientX - rect.left;
                state.touchOffset.y = touch.clientY - rect.top;

                target.classList.add('dragging');
            }
        }

        function handleTouchMove(e) {
            if (!state.draggedElement) return;
            e.preventDefault();

            const touch = e.touches[0];

            // Find drop zone under touch
            const elements = document.elementsFromPoint(touch.clientX, touch.clientY);
            const dropZone = elements.find(el => el.classList.contains('drop-zone'));

            // Update drop zone highlights
            document.querySelectorAll('.drop-zone').forEach(zone => {
                zone.classList.remove('drag-over', 'drag-invalid');
            });

            if (dropZone) {
                const busNum = parseInt(dropZone.dataset.bus);
                const validation = canDropFunction(state.draggedFunction, busNum);
                if (validation.allowed) {
                    dropZone.classList.add('drag-over');
                } else {
                    dropZone.classList.add('drag-invalid');
                }
            }
        }

        function handleTouchEnd(e) {
            if (!state.draggedElement) return;

            const touch = e.changedTouches[0];
            const elements = document.elementsFromPoint(touch.clientX, touch.clientY);
            const dropZone = elements.find(el => el.classList.contains('drop-zone'));

            if (dropZone) {
                const busNum = parseInt(dropZone.dataset.bus);
                const validation = canDropFunction(state.draggedFunction, busNum);

                if (validation.allowed) {
                    assignFunction(state.draggedFunction, busNum);
                } else {
                    showNotification(validation.reason, 'error');
                }
            }

            state.draggedElement.classList.remove('dragging');
            state.draggedElement = null;
            state.draggedFunction = null;

            // Clear all drag states
            document.querySelectorAll('.drop-zone').forEach(zone => {
                zone.classList.remove('drag-over', 'drag-invalid');
            });
        }

        // Validate if function can be dropped on bus
        function canDropFunction(funcKey, busNum) {
            const brand = brandCapabilities[state.selectedBrand];
            const busNameValue = parseInt(document.getElementById(`can${busNum}Name`).value);
            const busName = busNameLabels[busNameValue];

            // Check if function is allowed for this bus type
            const allowedFunctions = brand.busTypes[busName] || [];
            if (!allowedFunctions.includes(funcKey)) {
                return {
                    allowed: false,
                    reason: `${functionDefinitions[funcKey].name} not supported on ${busName} for ${brand.name}`
                };
            }

            // Check exclusive functions (steering and keya can only be on one bus)
            const func = functionDefinitions[funcKey];
            if (func.exclusive) {
                for (const [otherBusNum, functions] of Object.entries(state.busAssignments)) {
                    if (parseInt(otherBusNum) !== busNum && functions.includes(funcKey)) {
                        return {
                            allowed: false,
                            reason: `${func.name} can only be assigned to one bus`
                        };
                    }
                }
            }

            // Special rule: steering and keya are mutually exclusive across all buses
            if (funcKey === 'steering') {
                // Check if keya is already assigned to any bus
                for (const [otherBusNum, functions] of Object.entries(state.busAssignments)) {
                    if (functions.includes('keya')) {
                        return {
                            allowed: false,
                            reason: `Cannot use Steering and Keya Motor together - they are mutually exclusive`
                        };
                    }
                }
            } else if (funcKey === 'keya') {
                // Check if steering is already assigned to any bus
                for (const [otherBusNum, functions] of Object.entries(state.busAssignments)) {
                    if (functions.includes('steering')) {
                        return {
                            allowed: false,
                            reason: `Cannot use Keya Motor and Steering together - they are mutually exclusive`
                        };
                    }
                }
            }

            return { allowed: true };
        }

        // Assign function to bus
        function assignFunction(funcKey, busNum) {
            if (!state.busAssignments[busNum].includes(funcKey)) {
                state.busAssignments[busNum].push(funcKey);
                updateDropZone(busNum);
                updateFunctionPool();
            }
        }

        // Remove function from bus
        function removeFunction(funcKey, busNum) {
            const index = state.busAssignments[busNum].indexOf(funcKey);
            if (index > -1) {
                state.busAssignments[busNum].splice(index, 1);
                updateDropZone(busNum);
                updateFunctionPool();
            }
        }

        // Clear all functions from bus
        function clearBus(busNum) {
            state.busAssignments[busNum] = [];
            updateDropZone(busNum);
            updateFunctionPool();
        }

        // Update info box with brand-specific information
        function updateInfoBox() {
            const infoBox = document.getElementById('infoBox');
            const brand = brandCapabilities[state.selectedBrand];

            let html = '<h3>Function Descriptions</h3>';

            if (state.selectedBrand === 0) {
                html += '<p>CAN bus functions are disabled.</p>';
            } else {
                const descriptions = {
                    'steering': 'Control steering valve/motor',
                    'buttons': 'Read control buttons',
                    'hitch': 'Control 3-point hitch',
                    'implement': 'ISOBUS implement control',
                    'keya': 'Keya CAN motor control'
                };

                // Gather all unique functions
                const allFunctions = new Set();
                Object.values(brand.busTypes).forEach(functions => {
                    functions.forEach(func => allFunctions.add(func));
                });

                const funcDescs = [];
                allFunctions.forEach(funcKey => {
                    if (descriptions[funcKey]) {
                        funcDescs.push(`<strong>${functionDefinitions[funcKey].name}:</strong> ${descriptions[funcKey]}`);
                    }
                });

                if (funcDescs.length > 0) {
                    html += `<p>${funcDescs.join(' • ')}</p>`;
                }

                // Brand-specific notes from JSON or fallback
                if (brand.uiNotes) {
                    html += `<p>${brand.uiNotes}</p>`;
                }
            }

            infoBox.innerHTML = html;
        }

        // Show notification
        function showNotification(message, type) {
            const notification = document.createElement('div');
            notification.className = `notification notification-${type}`;

            const icons = {
                'success': '✅',
                'error': '❌',
                'warning': '⚠️',
                'info': 'ℹ️'
            };

            notification.innerHTML = `${icons[type] || ''} ${message}`;
            document.body.appendChild(notification);

            setTimeout(() => {
                notification.classList.add('fade-out');
                setTimeout(() => notification.remove(), 300);
            }, 3000);
        }

        // Restart confirmation
        function confirmRestart() {
            if (confirm('Are you sure you want to restart the system?')) {
                fetch('/api/restart', { method: 'POST' })
                    .then(response => {
                        if (response.ok) {
                            showNotification('System is restarting...', 'success');
                        }
                    })
                    .catch(error => {
                        showNotification('Error restarting system', 'error');
                    });
            }
        }
    </script>
</body>
</html>
)rawliteral";

#endif // DRAG_DROP_CAN_CONFIG_PAGE_H
