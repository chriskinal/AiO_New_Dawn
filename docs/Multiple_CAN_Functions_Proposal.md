# Multiple CAN Functions Per Bus - Implementation Proposal

## Problem Statement

Current limitation: Each CAN bus can only be assigned one function, but real tractors often use:
- **K_Bus**: Buttons + Hitch + Diagnostics
- **ISO_Bus**: Implements + Steering messages
- **V_Bus**: Steering + Work switches

## Proposed Solution: Bitfield Approach

### 1. Update Configuration Structure

```cpp
// In ConfigManager.h

// CAN functions as bit flags (up to 8 functions)
enum class CANFunctionFlags : uint8_t {
    NONE      = 0x00,
    STEERING  = 0x01,  // Steering commands (Keya or brand-specific)
    BUTTONS   = 0x02,  // Button inputs
    HITCH     = 0x04,  // Hitch control
    IMPLEMENT = 0x08,  // ISO implement control
    DIAG      = 0x10,  // Diagnostics
    AUX1      = 0x20,  // Future use
    AUX2      = 0x40,  // Future use
    AUX3      = 0x80   // Future use
};

// Updated config structure
struct CANSteerConfig {
    uint8_t brand = 9;           // Tractor brand

    // Bus speeds
    uint8_t can1Speed = 0;       // 0=250k, 1=500k
    uint8_t can2Speed = 0;
    uint8_t can3Speed = 0;

    // Bus functions (bitmask)
    uint8_t can1Functions = 0;   // Bitmask of CANFunctionFlags
    uint8_t can2Functions = 0;
    uint8_t can3Functions = 0;

    // Steering type per bus (for when STEERING flag is set)
    uint8_t can1SteerType = 0;   // 0=None, 1=Keya, 2=V_Bus, 3=ISO_Bus
    uint8_t can2SteerType = 0;
    uint8_t can3SteerType = 0;

    uint8_t moduleID = 0x1C;
    uint8_t reserved[4];         // Future expansion
};
```

### 2. Update Bus Assignment Logic

```cpp
// In TractorCANDriver.cpp

void TractorCANDriver::assignCANBuses() {
    // Reset all buses
    steerBusNum = 0;
    steerCAN = nullptr;
    buttonBusNum = 0;
    buttonCAN = nullptr;
    hitchBusNum = 0;
    hitchCAN = nullptr;

    // Check each bus for functions
    for (uint8_t bus = 1; bus <= 3; bus++) {
        uint8_t functions = getBusFunctions(bus);

        // Steering function
        if (functions & CANFunctionFlags::STEERING) {
            if (!steerBusNum) {  // Use first bus with steering
                steerBusNum = bus;
                steerCAN = getBusPointer(bus);
                steerType = getBusSteerType(bus);
            }
        }

        // Button function
        if (functions & CANFunctionFlags::BUTTONS) {
            if (!buttonBusNum) {
                buttonBusNum = bus;
                buttonCAN = getBusPointer(bus);
            }
        }

        // Hitch function
        if (functions & CANFunctionFlags::HITCH) {
            if (!hitchBusNum) {
                hitchBusNum = bus;
                hitchCAN = getBusPointer(bus);
            }
        }
    }
}

uint8_t TractorCANDriver::getBusFunctions(uint8_t busNum) {
    switch (busNum) {
        case 1: return config.can1Functions;
        case 2: return config.can2Functions;
        case 3: return config.can3Functions;
        default: return 0;
    }
}
```

### 3. Web UI Updates with Brand Filtering

#### HTML Structure
```html
<div class="can-row">
    <div class="can-label">CAN1</div>
    <select id="can1Speed" name="can1Speed">
        <option value="0">250 kbps</option>
        <option value="1">500 kbps</option>
    </select>
    <div class="function-checks">
        <label><input type="checkbox" name="can1_steering"> Steering</label>
        <select id="can1SteerType" name="can1SteerType" style="display:none;">
            <option value="0">None</option>
            <option value="1">Keya</option>
            <option value="2">V_Bus</option>
            <option value="3">ISO_Bus</option>
        </select>
        <label><input type="checkbox" name="can1_buttons"> Buttons</label>
        <label><input type="checkbox" name="can1_hitch"> Hitch</label>
        <label><input type="checkbox" name="can1_implement"> Implement</label>
    </div>
</div>
```

#### JavaScript with Brand Capabilities
```javascript
// Define what each brand supports
const brandCapabilities = {
    0: { // Disabled
        functions: [],
        steerTypes: []
    },
    1: { // Fendt SCR/S4/Gen6
        functions: ['steering', 'buttons', 'hitch'],
        steerTypes: ['V_Bus'],
        requiredFunctions: {
            'V_Bus': ['steering'],
            'K_Bus': ['buttons']
        }
    },
    2: { // Valtra/Massey
        functions: ['steering'],
        steerTypes: ['V_Bus']
    },
    3: { // Case IH/NH
        functions: ['steering', 'hitch'],
        steerTypes: ['V_Bus']
    },
    4: { // Fendt One
        functions: ['steering', 'buttons', 'hitch', 'implement'],
        steerTypes: ['V_Bus', 'ISO_Bus']
    },
    5: { // Claas
        functions: ['steering'],
        steerTypes: ['V_Bus']
    },
    6: { // JCB
        functions: ['steering'],
        steerTypes: ['V_Bus']
    },
    7: { // Lindner
        functions: ['steering'],
        steerTypes: ['V_Bus']
    },
    8: { // CAT MT
        functions: ['steering'],
        steerTypes: ['V_Bus']
    },
    9: { // Generic
        functions: ['steering', 'buttons', 'hitch', 'implement', 'diag'],
        steerTypes: ['Keya', 'V_Bus', 'ISO_Bus']
    }
};

// Update checkboxes when brand changes
function updateFunctionCheckboxes() {
    const brand = parseInt(document.getElementById('brand').value);
    const capabilities = brandCapabilities[brand];

    // For each CAN bus
    [1, 2, 3].forEach(busNum => {
        const container = document.getElementById(`can${busNum}Functions`);
        container.innerHTML = ''; // Clear existing

        // Add only supported functions as checkboxes
        capabilities.functions.forEach(func => {
            const label = document.createElement('label');
            const checkbox = document.createElement('input');
            checkbox.type = 'checkbox';
            checkbox.name = `can${busNum}_${func}`;
            checkbox.addEventListener('change', () => updateSteerType(busNum));

            label.appendChild(checkbox);
            label.appendChild(document.createTextNode(` ${capitalizeFirst(func)}`));
            container.appendChild(label);

            // Add steering type selector if steering function
            if (func === 'steering') {
                const select = document.createElement('select');
                select.id = `can${busNum}SteerType`;
                select.name = `can${busNum}SteerType`;
                select.style.display = 'none';

                // Add supported steering types
                const opt = document.createElement('option');
                opt.value = '0';
                opt.text = 'Select Type';
                select.appendChild(opt);

                capabilities.steerTypes.forEach((type, idx) => {
                    const opt = document.createElement('option');
                    opt.value = idx + 1;
                    opt.text = type;
                    select.appendChild(opt);
                });

                container.appendChild(select);
            }
        });
    });
}

// Show/hide steering type based on checkbox
function updateSteerType(busNum) {
    const steeringChecked = document.querySelector(`input[name="can${busNum}_steering"]`)?.checked;
    const steerSelect = document.getElementById(`can${busNum}SteerType`);

    if (steerSelect) {
        steerSelect.style.display = steeringChecked ? 'inline' : 'none';
        if (!steeringChecked) {
            steerSelect.value = '0';
        }
    }
}

// Validate brand-specific requirements
function validateBrandRequirements(config) {
    const brand = config.brand;
    const caps = brandCapabilities[brand];

    if (!caps.requiredFunctions) return true;

    // Check each required bus has required functions
    for (const [busType, requiredFuncs] of Object.entries(caps.requiredFunctions)) {
        // Find which bus is assigned this type
        let busFound = false;

        for (let i = 1; i <= 3; i++) {
            const funcs = config[`can${i}Functions`];
            const steerType = config[`can${i}SteerType`];

            // Check if this bus matches the required type
            if (busType === 'K_Bus' && hasFunction(funcs, 'buttons')) {
                busFound = true;
            } else if (busType === 'V_Bus' && steerType === 2) {
                busFound = true;
            }
            // ... etc
        }

        if (!busFound) {
            showError(`${caps.name} requires ${busType} with ${requiredFuncs.join(', ')}`);
            return false;
        }
    }

    return true;
}
// Dynamic steering type selector
document.querySelectorAll('input[name*="_steering"]').forEach(cb => {
    cb.addEventListener('change', function() {
        const busNum = this.name.match(/can(\d)_/)[1];
        const steerSelect = document.getElementById(`can${busNum}SteerType`);
        steerSelect.style.display = this.checked ? 'inline' : 'none';

        if (!this.checked) {
            steerSelect.value = '0';
        }
    });
});

// Convert checkboxes to bitmask
function getFunctionsBitmask(busNum) {
    let bitmask = 0;
    if (document.querySelector(`input[name="can${busNum}_steering"]`).checked)
        bitmask |= 0x01;
    if (document.querySelector(`input[name="can${busNum}_buttons"]`).checked)
        bitmask |= 0x02;
    if (document.querySelector(`input[name="can${busNum}_hitch"]`).checked)
        bitmask |= 0x04;
    if (document.querySelector(`input[name="can${busNum}_implement"]`).checked)
        bitmask |= 0x08;
    return bitmask;
}

// Save configuration
async function saveConfig() {
    const config = {
        brand: parseInt(document.getElementById('brand').value),
        can1Speed: parseInt(document.getElementById('can1Speed').value),
        can1Functions: getFunctionsBitmask(1),
        can1SteerType: parseInt(document.getElementById('can1SteerType').value),
        // ... repeat for CAN2 and CAN3
    };

    // Validate: Each function should only be on one bus
    if (!validateFunctions(config)) {
        showError('Each function can only be assigned to one bus');
        return;
    }

    // Send to server...
}
```

### 4. UI Examples by Brand

#### Valtra/Massey Ferguson (Simple - Steering Only)
```
CAN1 [250k] ☑ Steering [V_Bus ▼]
CAN2 [250k] ☐ Steering
CAN3 [250k] ☐ Steering
```

#### Fendt SCR (Multiple Functions)
```
CAN1 [250k] ☑ Steering [V_Bus ▼] ☐ Buttons ☐ Hitch
CAN2 [250k] ☐ Steering ☑ Buttons ☑ Hitch
CAN3 [250k] ☐ Steering ☐ Buttons ☐ Hitch
```

#### Generic (All Options Available)
```
CAN1 [250k] ☑ Steering [Keya ▼] ☐ Buttons ☐ Hitch ☐ Implement ☐ Diag
CAN2 [250k] ☐ Steering ☑ Buttons ☑ Hitch ☐ Implement ☐ Diag
CAN3 [250k] ☐ Steering ☐ Buttons ☐ Hitch ☑ Implement ☐ Diag
```

### 5. Validation Rules

1. **Steering**: Can be on multiple buses but only first is used
2. **Buttons/Hitch**: Should only be on one bus each
3. **Brand-specific rules**:
   - Fendt requires K_Bus to have BUTTONS flag
   - Valtra requires at least one bus with STEERING + V_Bus type

### 5. Migration Path

For existing configs:
```cpp
// In ConfigManager::loadCANSteerConfig()
if (marker != 0xCA) {  // Old format
    // Convert old single-function to new bitmask
    CANSteerConfig newConfig;
    newConfig.brand = oldConfig.brand;

    // Convert function enums to bitmasks
    if (oldConfig.can1Function == CANFunction::KEYA) {
        newConfig.can1Functions = CANFunctionFlags::STEERING;
        newConfig.can1SteerType = 1;  // Keya
    } else if (oldConfig.can1Function == CANFunction::V_BUS) {
        newConfig.can1Functions = CANFunctionFlags::STEERING |
                                 CANFunctionFlags::BUTTONS |
                                 CANFunctionFlags::HITCH;
        newConfig.can1SteerType = 2;  // V_Bus
    }
    // ... etc
}
```

### 6. Benefits

1. **Flexibility**: Supports real-world bus usage patterns
2. **Backward Compatible**: Can migrate old configs
3. **Extensible**: 8 function flags available
4. **Clear UI**: Checkboxes show exactly what each bus does
5. **Validation**: Prevents conflicting assignments

### 7. Implementation Phases

**Phase 1**: Backend changes
- Update ConfigManager structures
- Implement bus assignment logic
- Add migration code

**Phase 2**: UI updates
- Replace dropdowns with checkboxes
- Add validation logic
- Update info text dynamically

**Phase 3**: Testing
- Test migration from old format
- Verify multi-function buses work
- Test each brand configuration

This approach maintains the current architecture while adding the flexibility to assign multiple functions per bus, matching real tractor implementations.