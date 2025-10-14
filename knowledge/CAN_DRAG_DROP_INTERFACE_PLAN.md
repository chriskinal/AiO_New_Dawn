# CAN Bus Drag & Drop Configuration Interface Plan

## Overview

This document outlines the design and implementation plan for a modern, touch-friendly drag-and-drop interface for configuring CAN bus functions in the AiO New Dawn system. The interface allows users to visually assign functions (steering, buttons, hitch, etc.) to different CAN buses based on their tractor brand.

## Requirements

### Functional Requirements
1. **Brand-Based Filtering**: Functions available for assignment should be filtered based on selected tractor brand
2. **Visual Assignment**: Users can drag functions from a pool and drop them onto CAN bus slots
3. **Validation Rules**: Prevent invalid configurations based on brand limitations and exclusive functions
4. **Touch Support**: Must work on tablets and touchscreen devices used in agricultural equipment
5. **Persistent Configuration**: Save and load configurations from the device

### Technical Constraints
- Must be lightweight (<50KB total) to fit in Teensy's limited flash memory
- No external library dependencies (pure HTML/CSS/JavaScript)
- Compatible with existing bitfield storage format
- Works offline (no CDN dependencies)

## User Interface Design

### Layout Structure
```
┌─────────────────────────────────────────────────────────────┐
│  CAN Bus Configuration                    Brand: [Dropdown▼] │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  Available Functions (Drag from here)                      │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐     │
│  │    🚗    │ │    🔘    │ │    🚜    │ │    ⚙️    │     │
│  │ Steering │ │ Buttons  │ │  Hitch   │ │   Keya   │     │
│  └──────────┘ └──────────┘ └──────────┘ └──────────┘     │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  CAN1 - V_Bus (250 kbps)                    [Clear All]    │
│  ┌─────────────────────────────────────────────────────┐  │
│  │ Drop functions here...                              │  │
│  └─────────────────────────────────────────────────────┘  │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  CAN2 - K_Bus (500 kbps)                    [Clear All]    │
│  ┌─────────────────────────────────────────────────────┐  │
│  │ Drop functions here...                              │  │
│  └─────────────────────────────────────────────────────┘  │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  CAN3 - ISO_Bus (250 kbps)                  [Clear All]    │
│  ┌─────────────────────────────────────────────────────┐  │
│  │ Drop functions here...                              │  │
│  └─────────────────────────────────────────────────────┘  │
├─────────────────────────────────────────────────────────────┤
│  [Save Configuration]  [Reset to Defaults]  [Cancel]       │
└─────────────────────────────────────────────────────────────┘
```

### Visual Elements

#### Function Cards
- Colorful icons representing each function
- Distinct colors for easy identification
- Hover effects for desktop users
- Touch-friendly size (minimum 44x44px for touch targets)

#### Drop Zones
- Clear visual boundaries for each CAN bus
- Visual feedback during drag operations:
  - Green highlight for valid drops
  - Red highlight for invalid drops
  - Dashed border in neutral state

#### Brand Selector
- Dropdown at the top of the interface
- Immediately filters available functions
- Shows brand-specific limitations

## Data Structures

### Configuration Data Source

**All brand capabilities, function definitions, and bus types are loaded from an external JSON file:**

- **File Location**: `/api/can/info` (served from CANInfo.json)
- **Format**: CANInfo v2.0 JSON format (see CANInfo_v2.json)
- **Benefits**:
  - Easy to update without recompiling firmware
  - Users can add new brands by editing JSON
  - Centralized configuration for UI and backend
  - Supports future expansion (new functions, brands, etc.)

### JSON Structure Overview

```javascript
{
  "version": "2.0",
  "functions": {
    "steering": { name, icon, color, description, exclusive, bitValue },
    "buttons": { ... },
    "hitch": { ... },
    "implement": { ... },
    "keya": { ... }
  },
  "busTypes": {
    "None": { id: 0, displayName: "None" },
    "Keya": { id: 1, displayName: "Keya" },
    "V_Bus": { id: 2, displayName: "V_Bus", defaultSpeed: 250 },
    "K_Bus": { id: 3, displayName: "K_Bus", defaultSpeed: 500 },
    "ISO_Bus": { id: 4, displayName: "ISO_Bus", defaultSpeed: 250 }
  },
  "brands": [
    {
      id: 0-9,
      name: "ENUM_NAME",
      displayName: "User-Friendly Name",
      capabilities: {
        "V_Bus": ["steering"],
        "K_Bus": ["buttons", "hitch"],
        ...
      },
      uiNotes: "Optional guidance text",
      canConfig: { /* CAN protocol details */ },
      models: [ /* Model-specific configurations */ ]
    }
  ]
}
```

### Loading Configuration at Runtime

```javascript
// Load configuration on page load
let config = null;

async function loadCANConfiguration() {
  try {
    const response = await fetch('/api/can/info');
    config = await response.json();

    // Extract what we need for the UI
    const functions = config.functions;
    const busTypes = config.busTypes;
    const brands = config.brands;

    // Initialize UI with loaded data
    initializeBrandSelector(brands);
    initializeFunctionPool(functions);

  } catch (error) {
    console.error('Failed to load CAN configuration:', error);
    showNotification('Failed to load configuration data', 'error');
  }
}
```

### Example: Accessing Brand Data

```javascript
// After loading config from /api/can/info
const brand = config.brands.find(b => b.id === 1); // Case IH/NH

console.log(brand.displayName);  // "Case IH/New Holland"
console.log(brand.capabilities);
// {
//   "V_Bus": ["steering"],
//   "K_Bus": ["buttons", "hitch"],
//   "ISO_Bus": []
// }

console.log(brand.uiNotes);
// "V_Bus for steering • K_Bus for engage button and hitch control"
```

### Example: Accessing Function Data

```javascript
// Functions loaded from config.functions
const steeringFunc = config.functions.steering;

console.log(steeringFunc);
// {
//   name: "Steering",
//   icon: "🚗",
//   color: "#3498db",
//   description: "Valve/Motor steering control",
//   exclusive: true,
//   bitValue: 1
// }

// Use in UI to create function cards
Object.entries(config.functions).forEach(([key, func]) => {
  createFunctionCard(key, func);
});
```

### Example: Accessing Bus Type Data

```javascript
// Bus types loaded from config.busTypes
const vBus = config.busTypes.V_Bus;

console.log(vBus);
// {
//   id: 2,
//   displayName: "V_Bus",
//   description: "Valve bus for steering",
//   defaultSpeed: 250
// }
```

## Implementation Architecture

### Core Application State
```javascript
// Global configuration loaded from JSON
let canConfig = null;

// Application state
const state = {
    selectedBrand: 6, // Default to Generic
    busAssignments: {
        1: [],  // CAN1 assigned functions
        2: [],  // CAN2 assigned functions
        3: []   // CAN3 assigned functions
    },
    draggedElement: null,
    draggedFunction: null,
    touchOffset: { x: 0, y: 0 }
};
```

### Core Methods Structure
```javascript
// Initialization
async function loadCANConfiguration() { /* Load from /api/can/info */ }
function initializeBrandSelector(brands) { /* Populate brand dropdown */ }
function initializeFunctionPool(functions) { /* Create function cards */ }

// Brand and function management
function onBrandChange() { /* Update UI when brand changes */ }
function updateFunctionPool() { /* Refresh available functions */ }
function createFunctionCard(funcKey, func) { /* Create draggable card */ }

// Drag and drop
function setupDragAndDrop() { /* Setup mouse events */ }
function setupTouchEvents() { /* Setup touch events */ }
function handleDragStart(e) { }
function handleDragOver(e) { }
function handleDrop(e) { }
function handleTouchStart(e) { }
function handleTouchMove(e) { }
function handleTouchEnd(e) { }

// Validation
function canDropFunction(funcId, busNum) { /* Check if drop is valid */ }
function validateBrandCapabilities(funcId, busNum, busName) { }

// Assignment management
function assignFunction(funcId, busNum) { /* Add function to bus */ }
function removeFunction(funcId, busNum) { /* Remove function from bus */ }
function clearBus(busNum) { /* Clear all functions from bus */ }
function updateDropZone(busNum) { /* Refresh drop zone display */ }

// Configuration persistence
async function loadConfiguration() { /* Load from /api/can/config */ }
async function saveConfiguration() { /* Save to /api/can/config */ }
function bitfieldToFunctions(bitfield) { /* Convert backend format */ }
function functionsToBitfield(functions) { /* Convert to backend format */ }

// UI feedback
function showNotification(message, type) { /* Toast notification */ }
function updateInfoBox() { /* Update brand-specific info */ }
```

## Drag & Drop Implementation

### Desktop Drag Events
```javascript
// Drag start
document.addEventListener('dragstart', (e) => {
    if (e.target.classList.contains('function-card')) {
        this.draggedElement = e.target;
        this.draggedFunction = e.target.dataset.function;
        e.target.classList.add('dragging');
        e.dataTransfer.effectAllowed = 'copy';
    }
});

// Drag over
zone.addEventListener('dragover', (e) => {
    e.preventDefault();
    const validation = this.canDropFunction(draggedFunction, busId);
    zone.classList.add(validation.allowed ? 'drop-valid' : 'drop-invalid');
});

// Drop
zone.addEventListener('drop', (e) => {
    e.preventDefault();
    if (validation.allowed) {
        this.assignFunction(draggedFunction, busId);
    }
});
```

### Touch Events for Tablets
```javascript
// Touch start
document.addEventListener('touchstart', (e) => {
    const target = e.target.closest('.function-card');
    if (target) {
        this.draggedElement = target;
        const touch = e.touches[0];
        // Calculate offset for smooth dragging
        const rect = target.getBoundingClientRect();
        this.touchOffset.x = touch.clientX - rect.left;
        this.touchOffset.y = touch.clientY - rect.top;
    }
});

// Touch move
document.addEventListener('touchmove', (e) => {
    if (this.draggedElement) {
        e.preventDefault();
        const touch = e.touches[0];
        // Visual feedback
        this.draggedElement.style.position = 'fixed';
        this.draggedElement.style.left = (touch.clientX - this.touchOffset.x) + 'px';
        this.draggedElement.style.top = (touch.clientY - this.touchOffset.y) + 'px';

        // Highlight drop zone under touch point
        const dropTarget = document.elementFromPoint(touch.clientX, touch.clientY);
        const bus = dropTarget?.closest('.can-bus-drop-zone');
        this.highlightDropZone(bus);
    }
});

// Touch end
document.addEventListener('touchend', (e) => {
    if (this.draggedElement) {
        const touch = e.changedTouches[0];
        const dropTarget = document.elementFromPoint(touch.clientX, touch.clientY);
        const bus = dropTarget?.closest('.can-bus-drop-zone');

        if (bus) {
            const busId = bus.dataset.bus;
            const funcId = this.draggedElement.dataset.function;
            this.assignFunction(funcId, busId);
        }

        // Reset visual state
        this.resetDragState();
    }
});
```

## Validation Rules

### Function Assignment Rules
1. **Brand Compatibility**: Function must be in the brand's allowed list for that bus
2. **Exclusive Functions**: Steering and Keya can only be assigned to one bus
3. **Bus Type Matching**: Functions must match the bus type capabilities
4. **Keya Special Case**: Only available for Generic brand when bus name is "None"

### Validation Implementation
```javascript
function canDropFunction(funcKey, busNum) {
    // Get brand from loaded JSON config
    const brand = canConfig.brands.find(b => b.id === state.selectedBrand);
    if (!brand) {
        return { allowed: false, reason: 'Invalid brand selected' };
    }

    // Get bus name from dropdown
    const busNameValue = parseInt(document.getElementById(`can${busNum}Name`).value);
    const busName = Object.keys(canConfig.busTypes).find(
        key => canConfig.busTypes[key].id === busNameValue
    );

    // Get allowed functions for this bus from brand capabilities
    const allowedFunctions = brand.capabilities[busName] || [];

    // Check brand allows this function on this bus
    if (!allowedFunctions.includes(funcKey)) {
        const func = canConfig.functions[funcKey];
        return {
            allowed: false,
            reason: `${func.name} not supported on ${busName} for ${brand.displayName}`
        };
    }

    // Check exclusive functions (from JSON config)
    const func = canConfig.functions[funcKey];
    if (func.exclusive) {
        // Check if already assigned to a different bus
        for (const [otherBusNum, functions] of Object.entries(state.busAssignments)) {
            if (parseInt(otherBusNum) !== busNum && functions.includes(funcKey)) {
                return {
                    allowed: false,
                    reason: `${func.name} can only be assigned to one bus`
                };
            }
        }
    }

    return { allowed: true };
}
```

## Visual Styling

### Modern Touch-Friendly CSS
```css
.can-configurator {
    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
    max-width: 1200px;
    margin: 0 auto;
    padding: 20px;
}

.function-pool {
    min-height: 120px;
    background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
    border-radius: 15px;
    padding: 20px;
    display: flex;
    gap: 15px;
    flex-wrap: wrap;
}

.function-card {
    width: 100px;
    height: 100px;
    background: white;
    border-radius: 12px;
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    cursor: grab;
    transition: all 0.3s ease;
    box-shadow: 0 4px 6px rgba(0,0,0,0.1);
    border: 3px solid;
    user-select: none;
    -webkit-user-select: none;
}

.function-card:hover {
    transform: translateY(-5px);
    box-shadow: 0 8px 12px rgba(0,0,0,0.15);
}

.function-card.dragging {
    opacity: 0.5;
    cursor: grabbing;
}

.can-bus-drop-zone {
    min-height: 100px;
    border: 3px dashed #dee2e6;
    border-radius: 10px;
    padding: 15px;
    display: flex;
    gap: 10px;
    flex-wrap: wrap;
    transition: all 0.3s ease;
    background: white;
}

.can-bus-drop-zone.drop-valid {
    background: #d4edda;
    border-color: #28a745;
    box-shadow: 0 0 20px rgba(40, 167, 69, 0.3);
}

.can-bus-drop-zone.drop-invalid {
    background: #f8d7da;
    border-color: #dc3545;
    animation: shake 0.5s;
}

@keyframes shake {
    0%, 100% { transform: translateX(0); }
    25% { transform: translateX(-5px); }
    75% { transform: translateX(5px); }
}

/* Touch-specific enhancements */
@media (pointer: coarse) {
    .function-card {
        width: 110px;
        height: 110px;
    }

    .can-bus-drop-zone {
        min-height: 120px;
    }

    button {
        min-height: 44px;
        font-size: 16px;
    }
}

/* Responsive design for mobile */
@media (max-width: 768px) {
    .can-configurator {
        padding: 10px;
    }

    .function-pool {
        padding: 15px;
    }

    .function-card {
        width: 80px;
        height: 80px;
        font-size: 14px;
    }
}
```

## Backend Integration

### Data Format Conversion
```javascript
// Convert bus name string to enum value
busNameToEnum(busName) {
    const mapping = {
        'None': 0,
        'Keya': 1,
        'V_Bus': 2,
        'K_Bus': 3,
        'ISO_Bus': 4
    };
    return mapping[busName] || 0;
}

// Convert UI state to backend bitfield format
saveConfiguration() {
    const config = {
        brand: this.selectedBrand,
        can1Speed: 0, // 0=250kbps, 1=500kbps
        can2Speed: 1,
        can3Speed: 0,
        can1Name: this.busNameToEnum(this.busNames.can1),
        can2Name: this.busNameToEnum(this.busNames.can2),
        can3Name: this.busNameToEnum(this.busNames.can3),
        can1Function: this.functionsToBitfield(this.busAssignments.can1),
        can2Function: this.functionsToBitfield(this.busAssignments.can2),
        can3Function: this.functionsToBitfield(this.busAssignments.can3)
    };

    return fetch('/api/can/config', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(config)
    });
}

// Convert bitfield to function array for UI
bitfieldToFunctions(bitfield) {
    const functions = [];
    if (bitfield & 0x01) functions.push('steering');
    if (bitfield & 0x02) functions.push('buttons');
    if (bitfield & 0x04) functions.push('hitch');
    if (bitfield & 0x08) functions.push('implement');
    if (bitfield & 0x10) functions.push('keya');
    return functions;
}

// Convert function array to bitfield for backend
function functionsToBitfield(functions) {
    let bitfield = 0;
    functions.forEach(funcKey => {
        // Get bitValue from loaded JSON config
        const func = canConfig.functions[funcKey];
        if (func) {
            bitfield |= func.bitValue;
        }
    });
    return bitfield;
}

// Convert bitfield to function array for UI
function bitfieldToFunctions(bitfield) {
    const functions = [];
    // Iterate through all functions in JSON config
    Object.entries(canConfig.functions).forEach(([key, func]) => {
        if (bitfield & func.bitValue) {
            functions.push(key);
        }
    });
    return functions;
}
```

## User Feedback Mechanisms

### Visual Notifications
```javascript
showNotification(message, type = 'info') {
    const notification = document.createElement('div');
    notification.className = `notification notification-${type}`;
    notification.textContent = message;

    // Icons based on type
    const icons = {
        'success': '✅',
        'error': '❌',
        'warning': '⚠️',
        'info': 'ℹ️'
    };

    notification.innerHTML = `${icons[type]} ${message}`;
    document.body.appendChild(notification);

    // Auto-remove after 3 seconds
    setTimeout(() => {
        notification.classList.add('fade-out');
        setTimeout(() => notification.remove(), 300);
    }, 3000);
}
```

### Validation Tooltips
```javascript
showValidationTooltip(element, message) {
    const tooltip = document.createElement('div');
    tooltip.className = 'validation-tooltip';
    tooltip.textContent = message;

    const rect = element.getBoundingClientRect();
    tooltip.style.left = rect.left + 'px';
    tooltip.style.top = (rect.bottom + 5) + 'px';

    document.body.appendChild(tooltip);

    // Remove after 2 seconds
    setTimeout(() => tooltip.remove(), 2000);
}
```

## Future Enhancements

### Planned Features
1. **Undo/Redo System**: Track state changes and allow reverting
2. **Configuration Templates**: Save and load preset configurations
3. **Export/Import**: JSON file export for backup and sharing
4. **Advanced Mode**: Show raw bitfield values for debugging
5. **Conflict Resolution**: Automatic suggestions when conflicts occur
6. **Animation Effects**: Smooth transitions when functions move
7. **Keyboard Shortcuts**: Support for keyboard navigation
8. **Multi-language Support**: Internationalization ready

### Performance Optimizations
1. **Virtual DOM**: Implement lightweight virtual DOM for updates
2. **Lazy Loading**: Load function definitions on demand
3. **Canvas Rendering**: Use HTML5 Canvas for complex visualizations
4. **Web Workers**: Offload validation logic to background thread

## Testing Considerations

### Browser Compatibility
- Chrome/Edge 88+
- Firefox 85+
- Safari 14+
- Chrome Android 88+
- Safari iOS 14+

### Touch Device Testing
- iPad (9.7" and 12.9")
- Android tablets (7" and 10")
- Windows touch devices
- Agricultural equipment displays

### Validation Test Cases
1. Assign exclusive function to multiple buses → Should fail
2. Assign function not allowed by brand → Should fail
3. Change brand with existing assignments → Should filter invalid
4. Save and reload configuration → Should persist correctly
5. Touch drag across screen boundaries → Should handle gracefully

## File Size Budget

| Component | Target Size | Notes |
|-----------|------------|-------|
| HTML Structure | 5 KB | Minified |
| CSS Styles | 8 KB | Minified, no frameworks |
| JavaScript Logic | 15 KB | Minified, no libraries |
| Icons/Assets | 2 KB | Unicode emojis |
| **Total** | **30 KB** | Well within Teensy limits |

## Implementation Notes

### Current Backend Implementation
- API Endpoint: `/api/can/config` (GET/POST)
- Brand enum values: 0-9 (DISABLED, CASEIH_NH, CAT_MT, CLAAS, FENDT, FENDT_ONE, GENERIC, JCB, LINDNER, VALTRA_MASSEY)
- Bus name enum values: 0-4 (NONE, KEYA, V_BUS, K_BUS, ISO_BUS)
- Function bitfield values: 0x01 (STEERING), 0x02 (BUTTONS), 0x04 (HITCH), 0x08 (IMPLEMENT), 0x10 (KEYA)
- Bus speeds: 0 = 250kbps, 1 = 500kbps

### Replacing Existing Interface
This drag-and-drop interface will replace the current checkbox-based interface in `TouchFriendlyCANConfigPage.h`. The new implementation should:
- Use the same API endpoints
- Maintain backward compatibility with existing configurations
- Provide a more intuitive visual interface
- Support the same brand/function/bus relationships

## Implementation Timeline

### Phase 1: JSON Configuration Support
- Create `/api/can/info` endpoint to serve CANInfo.json
- Create `/api/can/info/upload` endpoint for JSON updates
- Test JSON loading and parsing in frontend
- Validate JSON structure and error handling

### Phase 2: Core Framework
- Update HTML structure to load from JSON
- Dynamic brand selector from JSON data
- Dynamic function pool from JSON definitions
- Basic drag and drop event handling

### Phase 3: Validation & Rules
- Brand-based filtering using JSON capabilities
- Exclusive function rules from JSON config
- Drop zone validation with JSON data
- Error messaging and visual feedback

### Phase 4: Polish & Testing
- Touch event handling for tablets
- Visual animations and transitions
- Cross-browser testing
- Performance optimization
- Test with various JSON configurations

### Phase 5: Integration & Documentation
- Replace existing checkbox interface
- Configuration persistence verification
- Create documentation for JSON format
- Provide example for adding new brands
- Testing with actual hardware
- Update version number

## JSON Configuration Management

### File Storage Location
- **Development**: `/Users/chris/Documents/Code/AiO_New_Dawn/knowledge/CANInfo_v2.json`
- **Production**: Stored in Teensy 4.1 flash memory (LittleFS or similar)
- **Backup**: Users can download/upload via web interface

### API Endpoints

#### GET `/api/can/info`
Returns the complete CANInfo JSON for the UI to consume.

**Response:**
```json
{
  "version": "2.0",
  "functions": {...},
  "busTypes": {...},
  "brands": [...]
}
```

#### POST `/api/can/info/upload`
Allows users to upload a new CANInfo JSON file.

**Request:**
- Content-Type: `application/json`
- Body: Complete CANInfo JSON

**Response:**
```json
{
  "status": "success",
  "message": "Configuration updated successfully",
  "version": "2.0"
}
```

**Validation:**
- Check JSON structure is valid
- Verify required fields exist
- Validate brand IDs are sequential (0-N)
- Verify function bitValues are unique powers of 2
- Check bus type IDs match enums

#### GET `/api/can/info/download`
Allows users to download current configuration for backup.

**Response:**
- Content-Type: `application/json`
- Content-Disposition: `attachment; filename="CANInfo.json"`
- Body: Current CANInfo JSON

### Adding New Brands

Users can add new brands by editing the JSON file:

1. **Download** current configuration via `/api/can/info/download`
2. **Edit** JSON file:
   ```json
   {
     "id": 10,  // Next sequential ID
     "name": "DEUTZ_FAHR",
     "displayName": "Deutz-Fahr",
     "description": "Deutz-Fahr tractors",
     "capabilities": {
       "V_Bus": ["steering"],
       "K_Bus": ["buttons", "hitch"],
       "ISO_Bus": []
     },
     "uiNotes": "Standard V-Bus steering configuration",
     "canConfig": {
       "VFilter": "0x...",
       // Add CAN protocol details
     }
   }
   ```
3. **Upload** via `/api/can/info/upload`
4. **Restart** device if backend code needs to recognize new brand

### Adding New Functions

To add a new function (e.g., "work_switch"):

1. Add to `functions` section:
   ```json
   "work_switch": {
     "name": "Work Switch",
     "icon": "🔄",
     "color": "#16a085",
     "description": "Automatic work switch detection",
     "exclusive": false,
     "bitValue": 32  // Next power of 2
   }
   ```

2. Add to brand capabilities where applicable:
   ```json
   "capabilities": {
     "V_Bus": ["steering"],
     "K_Bus": ["buttons", "hitch", "work_switch"],
     "ISO_Bus": []
   }
   ```

3. Backend code must be updated to handle new bitValue (0x20)

### Version Management

- Update `version` field when making significant changes
- Include `lastUpdated` timestamp in metadata
- Consider semantic versioning (2.0 → 2.1 for additions, 3.0 for breaking changes)

## Conclusion

This drag-and-drop interface will significantly improve the user experience for CAN bus configuration while maintaining full compatibility with the existing backend systems. The visual approach reduces configuration errors and makes the system more accessible to users who may not understand the technical details of CAN bus protocols.

### Key Benefits of JSON-Based Configuration

1. **User Extensibility**: Users can add new brands without firmware updates
2. **Easy Updates**: Configuration changes don't require recompilation
3. **Centralized Data**: Single source of truth for UI and backend
4. **Team Collaboration**: JSON file can be version controlled and shared
5. **Backward Compatible**: Existing configurations remain functional
6. **Future-Proof**: Easy to extend with new features and functions