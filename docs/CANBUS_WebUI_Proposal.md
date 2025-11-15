# CAN Bus Web UI Configuration Proposal

## Overview

This proposal outlines a dynamic, brand-aware CAN configuration interface that shows only supported features for each tractor brand while maintaining flexibility for custom configurations.

## Proposed Structure: Single Dynamic Page

### Benefits of Single Page Approach
- Maintains consistency with current UI pattern
- Reduces navigation complexity
- Allows real-time updates based on brand selection
- Easier to maintain one smart page vs multiple pages

## Page Layout

```
┌─────────────────────────────────────────┐
│ CAN Configuration      [Home][Restart]  │
├─────────────────────────────────────────┤
│ Tractor Brand: [Dropdown]               │
├─────────────────────────────────────────┤
│ ┌── Bus Configuration ────────────────┐ │
│ │ CAN1: [Speed] [Name*] | [Function]  │ │
│ │ CAN2: [Speed] [Name*] | [Function]  │ │
│ │ CAN3: [Speed] [Name*] | [Function]  │ │
│ └─────────────────────────────────────┘ │
├─────────────────────────────────────────┤
│ ┌── Brand Features (Dynamic) ─────────┐ │
│ │ [Content changes based on brand]    │ │
│ └─────────────────────────────────────┘ │
├─────────────────────────────────────────┤
│ ┌── Advanced Settings (Optional) ─────┐ │
│ │ [Show only if features available]   │ │
│ └─────────────────────────────────────┘ │
└─────────────────────────────────────────┘
```
* Name is a drop down with V_Bus, K_Bus and IsoBus
* Functions for each bus are filtered based on brand and bus

## Dynamic Behavior

### 1. Brand-Aware Function Filtering

```javascript
const brandCapabilities = {
  0: { // Disabled
    functions: ['NONE'],
    features: []
  },
  1: { // Fendt SCR/S4/Gen6
    functions: ['NONE', 'V_BUS', 'K_BUS', 'ISO_BUS'],
    features: ['steering', 'buttons', 'engage'],
    requiredBuses: {
      'V_BUS': 'Steering Control',
      'K_BUS': 'Button Input (Optional)'
    }
  },
  2: { // Valtra/Massey
    functions: ['NONE', 'V_BUS'],
    features: ['steering', 'engage', 'workSwitch'],
    requiredBuses: {
      'V_BUS': 'Steering Control'
    }
  },
  9: { // Generic
    functions: ['NONE', 'KEYA', 'V_BUS', 'ISO_BUS', 'K_BUS'],
    features: ['custom'],
    requiredBuses: {}
  }
};
```

### 2. Dynamic Sections

#### A. Basic Configuration (Always Visible)
- Brand selection
- Bus speed and function assignment

#### B. Steering Configuration (Show if brand supports steering)
```html
<div id="steeringConfig" style="display:none">
  <h3>Steering Configuration</h3>
  <div class="config-row">
    <label>Steering Sensitivity</label>
    <input type="range" min="1" max="10" id="steerSensitivity">
  </div>
  <div class="config-row">
    <label>Engage Method</label>
    <select id="engageMethod">
      <option value="auto">Automatic</option>
      <option value="button">Button</option>
    </select>
  </div>
</div>
```

#### C. Button Configuration (Show if K_Bus assigned)
```html
<div id="buttonConfig" style="display:none">
  <h3>Button Configuration</h3>
  <div class="button-grid">
    <div class="button-map">
      <label>Button 1</label>
      <select>
        <option>Steer Engage</option>
        <option>Section Control</option>
        <option>Disabled</option>
      </select>
    </div>
    <!-- More buttons... -->
  </div>
</div>
```

#### D. Hitch Control (Show if brand supports and K_Bus assigned)
```html
<div id="hitchConfig" style="display:none">
  <h3>Hitch Control</h3>
  <div class="config-row">
    <label>Enable Hitch Control</label>
    <input type="checkbox" id="hitchEnabled">
  </div>
  <div class="config-row">
    <label>Lift Height Setpoint</label>
    <input type="number" min="0" max="100">
  </div>
</div>
```

## Implementation Strategy

### 1. JavaScript Controller

```javascript
class CANConfigController {
  constructor() {
    this.brandSelect = document.getElementById('brand');
    this.brandSelect.addEventListener('change', () => this.onBrandChange());
  }

  onBrandChange() {
    const brand = parseInt(this.brandSelect.value);
    const capabilities = brandCapabilities[brand];

    // Update function dropdowns
    this.updateFunctionOptions(capabilities.functions);

    // Show/hide feature sections
    this.toggleSection('steeringConfig', capabilities.features.includes('steering'));
    this.toggleSection('buttonConfig', this.hasKBus() && capabilities.features.includes('buttons'));
    this.toggleSection('hitchConfig', this.hasKBus() && capabilities.features.includes('hitch'));

    // Update help text
    this.updateHelpText(capabilities);

    // Validate bus assignments
    this.validateBusAssignments(capabilities.requiredBuses);
  }

  updateFunctionOptions(allowedFunctions) {
    ['can1Function', 'can2Function', 'can3Function'].forEach(id => {
      const select = document.getElementById(id);
      const currentValue = select.value;

      // Clear and rebuild options
      select.innerHTML = '';

      const allFunctions = {
        0: 'None',
        1: 'Keya',
        2: 'V_Bus',
        3: 'ISO_Bus',
        4: 'K_Bus'
      };

      Object.entries(allFunctions).forEach(([value, text]) => {
        if (allowedFunctions.includes(text.toUpperCase().replace('_', '_'))) {
          const option = document.createElement('option');
          option.value = value;
          option.text = text;
          select.appendChild(option);
        }
      });

      // Restore value if still valid
      if ([...select.options].some(opt => opt.value === currentValue)) {
        select.value = currentValue;
      }
    });
  }
}
```

### 2. Visual Feedback

```css
/* Highlight required buses */
.bus-required {
  border: 2px solid #27ae60;
}

/* Dim unavailable options */
.feature-disabled {
  opacity: 0.5;
  pointer-events: none;
}

/* Show configuration status */
.config-status {
  padding: 10px;
  border-radius: 5px;
  margin-top: 10px;
}

.config-valid {
  background-color: #27ae60;
  color: white;
}

.config-warning {
  background-color: #f39c12;
  color: white;
}
```

### 3. Configuration Validation

```javascript
validateConfiguration() {
  const brand = parseInt(this.brandSelect.value);
  const capabilities = brandCapabilities[brand];
  const warnings = [];

  // Check required buses
  for (const [func, desc] of Object.entries(capabilities.requiredBuses)) {
    if (!this.hasBusFunction(func)) {
      warnings.push(`${desc} requires ${func} to be assigned to a bus`);
    }
  }

  // Check for conflicts
  if (this.hasMultipleSameFunctions()) {
    warnings.push('Same function assigned to multiple buses');
  }

  // Display status
  this.showConfigStatus(warnings);
}
```

## Alternative: Multi-Page Approach (Not Recommended)

If a multi-page approach is preferred:

```
/can/
├── index.html          # Main selection page
├── basic.html          # Generic/Keya configuration
├── fendt.html          # Fendt-specific features
├── valtra.html         # Valtra-specific features
└── advanced.html       # Expert mode (all options)
```

### Drawbacks:
- Navigation complexity
- Duplicate code maintenance
- Harder to switch between brands
- Inconsistent with current UI pattern

## Recommended Approach: Single Dynamic Page

### Advantages:
1. **User-Friendly**: No navigation required
2. **Context-Aware**: Shows only relevant options
3. **Validation**: Real-time feedback on configuration
4. **Maintainable**: Single codebase to update
5. **Flexible**: Easy to add new brands/features

### Implementation Priority:
1. **Phase 1**: Basic dynamic filtering of functions
2. **Phase 2**: Add steering configuration section
3. **Phase 3**: Add button mapping interface
4. **Phase 4**: Add hitch control settings
5. **Phase 5**: Add diagnostics/monitoring

## Example User Flow

1. User selects "Fendt SCR" brand
2. V_Bus and K_Bus options appear in function dropdowns
3. Steering configuration section appears
4. User assigns V_Bus to CAN1
5. Button configuration section appears
6. System validates and shows "Configuration Valid"

## Technical Requirements

1. **Backend API Endpoints**:
   - GET /api/can/capabilities/{brand}
   - GET /api/can/validation
   - POST /api/can/config (existing)

2. **Frontend Requirements**:
   - Dynamic form generation
   - Real-time validation
   - Responsive design
   - Touch-friendly controls

3. **Storage**:
   - Save brand-specific settings in EEPROM
   - Separate config blocks for each feature

This approach provides a clean, intuitive interface that grows with the user's needs while hiding complexity when not required.