# CAN Configuration Examples

This directory contains pre-configured CAN bus configuration files for specific tractor brands. These files can be uploaded to your AiO New Dawn device via the web interface to quickly configure CAN settings for your equipment.

## Overview

Each JSON file contains a complete, ready-to-use configuration for a specific tractor brand, including:
- CAN message filters and protocols
- Steering engage button configurations for specific models
- Work/hitch control settings
- Bus speed and timing parameters

## Available Configurations

### Tractor Brand Files

- **Case_IH_New_Holland.json** - Case IH and New Holland tractors (shared V-Bus protocol)
- **CAT_MT_Series.json** - Caterpillar MT series tractors (Early and Late models)
- **Claas.json** - Claas tractors (Pre-MR and Stage5 models)
- **Fendt_SCR_S4_Gen6.json** - Fendt SCR, S4, and Gen6 series tractors
- **Fendt_One.json** - Fendt One series tractors
- **JCB.json** - JCB tractors
- **Lindner.json** - Lindner tractors
- **Valtra_Massey_Ferguson.json** - Valtra and Massey Ferguson tractors (6000, S-Series, 7000-Series)

### Generic Configuration

- **Generic_Default.json** - Minimal configuration with only DISABLED and GENERIC brands
  - This is the firmware default configuration
  - Use Generic when mixing functions from different brands
  - Use Generic when using Keya CAN motor steering
  - Supports all bus types: V_Bus, K_Bus, ISO_Bus, and None (for Keya)

## How to Use

### Upload via Web Interface

1. Connect to your AiO New Dawn device at http://192.168.5.126/
2. Navigate to the CAN Configuration page
3. Click the "Upload Configuration" or drag-and-drop area
4. Select the appropriate JSON file for your tractor brand
5. The device will load the configuration and apply the settings
6. Select your specific model and button configuration from the dropdowns

### Customizing Configurations

You can customize these files to suit your needs:

1. **Combine multiple brands** - Edit a file to include configurations from multiple brands in the "brands" array
2. **Modify button mappings** - Change the CAN message IDs and bit masks to match your tractor's specific buttons
3. **Add custom models** - Add new model entries under the "models" array for your specific tractor variant
4. **Adjust bus speeds** - Modify the "defaultSpeed" values in "busTypes" if your equipment uses different speeds

### Example: Combining Configurations

To create a configuration with multiple brands, merge the "brands" arrays from multiple files:

```json
{
  "version": "2.0",
  "metadata": { ... },
  "functions": { ... },
  "busTypes": { ... },
  "brands": [
    { "id": 0, "name": "DISABLED", ... },
    { "id": 1, "name": "CASEIH_NH", ... },
    { "id": 4, "name": "FENDT", ... },
    { "id": 6, "name": "GENERIC", ... }
  ]
}
```

## File Structure

Each configuration file contains:

```json
{
  "version": "2.0",               // Configuration schema version
  "metadata": { ... },            // File metadata
  "functions": { ... },           // CAN function definitions (steering, buttons, etc.)
  "busTypes": { ... },           // CAN bus type definitions (V_Bus, K_Bus, ISO_Bus)
  "brands": [                    // Array of brand configurations
    {
      "id": 1,                   // Unique brand ID
      "name": "BRAND_NAME",      // Internal brand identifier
      "displayName": "...",      // User-friendly brand name
      "capabilities": { ... },   // Supported functions per bus
      "canConfig": { ... },      // CAN protocol settings
      "models": [                // Specific model configurations
        {
          "model": "...",        // Model name
          "vClaim": "...",       // V-Bus claim address
          "steer": [ ... ],      // Button configurations
          "work": [ ... ]        // Work/hitch configurations
        }
      ]
    }
  ]
}
```

### Required Brands

**IMPORTANT**: Every configuration file must include these two brands:

1. **DISABLED (id: 0)** - Must be first in the brands array
   - Allows users to disable CAN bus functionality
   - Has empty capabilities object

2. **GENERIC (id: 6)** - Should be last in the brands array
   - Provides fallback for mixed/custom configurations
   - Supports all bus types and functions including Keya steering

All brand-specific configuration files in this directory include both DISABLED and GENERIC brands alongside their specific brand configurations.

## Firmware Default

The firmware ships with the **Generic_Default.json** configuration, which includes:
- DISABLED brand (id: 0) - to disable CAN bus
- GENERIC brand (id: 6) - for custom setups and Keya motor steering

This minimal default keeps firmware size small while providing maximum flexibility.

## Support

For more information about:
- CAN configuration syntax, see `/knowledge/CAN_DRAG_DROP_INTERFACE_PLAN.md`
- Specific CAN protocols, see `/knowledge/CANInfo_v2.json`
- Hardware connections, see the AiO New Dawn hardware documentation

## Version History

- **v2.0** (2025-01-12) - Initial release of split configuration files
  - Separated each brand into individual files
  - Created Generic_Default.json for firmware default
  - Added this README
