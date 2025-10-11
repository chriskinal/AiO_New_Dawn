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

### Brand Capabilities Mapping
```javascript
const BrandCapabilities = {
    0: { // Disabled
        name: 'Disabled',
        functions: {}
    },
    1: { // Case IH/NH
        name: 'Case IH/New Holland',
        functions: {
            'V_Bus': ['steering'],
            'K_Bus': ['buttons', 'hitch'],
            'ISO_Bus': []
        }
    },
    2: { // CAT MT
        name: 'CAT MT Series',
        functions: {
            'V_Bus': ['steering'],
            'K_Bus': ['buttons'],
            'ISO_Bus': []
        }
    },
    3: { // Claas
        name: 'Claas',
        functions: {
            'V_Bus': ['steering'],
            'K_Bus': ['buttons'],
            'ISO_Bus': []
        }
    },
    4: { // Fendt SCR/S4/Gen6
        name: 'Fendt SCR/S4/Gen6',
        functions: {
            'V_Bus': ['steering'],
            'K_Bus': ['buttons', 'hitch'],
            'ISO_Bus': []
        }
    },
    5: { // Fendt One
        name: 'Fendt One',
        functions: {
            'V_Bus': ['steering'],
            'K_Bus': ['buttons', 'hitch'],
            'ISO_Bus': ['steering', 'implement']
        }
    },
    6: { // Generic
        name: 'Generic',
        functions: {
            'V_Bus': ['steering'],
            'K_Bus': ['buttons', 'hitch'],
            'ISO_Bus': ['steering', 'implement'],
            'None': ['keya']  // Special case for Keya motor
        }
    },
    7: { // JCB
        name: 'JCB',
        functions: {
            'V_Bus': ['steering'],
            'K_Bus': [],
            'ISO_Bus': []
        }
    },
    8: { // Lindner
        name: 'Lindner',
        functions: {
            'V_Bus': ['steering'],
            'K_Bus': [],
            'ISO_Bus': []
        }
    },
    9: { // Valtra/Massey Ferguson
        name: 'Valtra/Massey Ferguson',
        functions: {
            'V_Bus': ['steering'],
            'K_Bus': ['buttons', 'hitch'],
            'ISO_Bus': []
        }
    }
};
```

### Function Definitions
```javascript
const FunctionDefinitions = {
    'steering': {
        name: 'Steering',
        icon: '🚗',
        color: '#3498db',
        description: 'Valve/Motor steering control',
        exclusive: true,  // Only one instance allowed across all buses
        bitValue: 0x01
    },
    'buttons': {
        name: 'Buttons',
        icon: '🔘',
        color: '#2ecc71',
        description: 'Control button inputs',
        exclusive: false,
        bitValue: 0x02
    },
    'hitch': {
        name: 'Hitch',
        icon: '🚜',
        color: '#e74c3c',
        description: '3-point hitch control',
        exclusive: false,
        bitValue: 0x04
    },
    'implement': {
        name: 'Implement',
        icon: '🌾',
        color: '#f39c12',
        description: 'ISO implement control',
        exclusive: false,
        bitValue: 0x08
    },
    'keya': {
        name: 'Keya Motor',
        icon: '⚙️',
        color: '#9b59b6',
        description: 'Keya CAN motor control',
        exclusive: true,  // Only one instance allowed
        bitValue: 0x10
    }
};
```

## Implementation Architecture

### Core Class Structure
```javascript
class CANBusConfigurator {
    constructor() {
        this.selectedBrand = 6; // Default to Generic
        this.busAssignments = {
            can1: [],
            can2: [],
            can3: []
        };
        this.busNames = {
            can1: 'V_Bus',
            can2: 'K_Bus',
            can3: 'ISO_Bus'
        };
        this.draggedElement = null;
        this.touchOffset = { x: 0, y: 0 };
    }

    // Core methods
    init() { }
    createBrandSelector() { }
    onBrandChange(brandId) { }
    updateFunctionPool() { }
    createFunctionCard(funcId, func) { }
    validateCurrentAssignments() { }
    canDropFunction(funcId, busId) { }
    assignFunction(funcId, busId) { }
    removeFunction(funcId, busId) { }
    setupDragAndDrop() { }
    setupTouchEvents() { }
    saveConfiguration() { }
    loadCurrentConfig() { }
    showNotification(message, type) { }
    bitfieldToFunctions(bitfield) { }
    functionsToBitfield(functions) { }
}
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
canDropFunction(funcId, busId) {
    const brand = BrandCapabilities[this.selectedBrand];
    const busName = this.busNames[busId];
    const allowedFunctions = brand.functions[busName] || [];

    // Check brand allows this function on this bus
    if (!allowedFunctions.includes(funcId)) {
        return {
            allowed: false,
            reason: `${FunctionDefinitions[funcId].name} not supported on ${busName} for ${brand.name}`
        };
    }

    // Check exclusive functions
    const func = FunctionDefinitions[funcId];
    if (func.exclusive) {
        for (const [otherBusId, functions] of Object.entries(this.busAssignments)) {
            if (functions.includes(funcId)) {
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
// Convert UI state to backend bitfield format
saveConfiguration() {
    const config = {
        brand: this.selectedBrand,
        can1Speed: 0, // 0=250kbps, 1=500kbps
        can2Speed: 1,
        can3Speed: 0,
        can1Name: this.busNameToId('V_Bus'),    // Convert to enum
        can2Name: this.busNameToId('K_Bus'),
        can3Name: this.busNameToId('ISO_Bus'),
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
functionsToBitfield(functions) {
    let bitfield = 0;
    functions.forEach(func => {
        bitfield |= FunctionDefinitions[func].bitValue;
    });
    return bitfield;
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

## Implementation Timeline

### Phase 1: Core Framework (Week 1)
- Basic HTML structure
- Brand selector functionality
- Function pool generation
- Basic drag and drop

### Phase 2: Validation & Rules (Week 2)
- Brand-based filtering
- Exclusive function rules
- Drop zone validation
- Error messaging

### Phase 3: Polish & Testing (Week 3)
- Touch event handling
- Visual animations
- Cross-browser testing
- Performance optimization

### Phase 4: Integration (Week 4)
- Backend API connection
- Configuration persistence
- Testing with actual hardware
- Documentation

## Conclusion

This drag-and-drop interface will significantly improve the user experience for CAN bus configuration while maintaining full compatibility with the existing backend systems. The visual approach reduces configuration errors and makes the system more accessible to users who may not understand the technical details of CAN bus protocols.