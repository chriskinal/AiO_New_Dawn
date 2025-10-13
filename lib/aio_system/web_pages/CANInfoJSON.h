// CANInfoJSON.h
// CAN configuration data in JSON format stored in PROGMEM

#ifndef CAN_INFO_JSON_H
#define CAN_INFO_JSON_H

#include <Arduino.h>

const char CAN_INFO_JSON[] PROGMEM = R"JSON({
  "version": "2.0",
  "metadata": {
    "description": "CAN bus configuration for AiO New Dawn - Unified format for UI and implementation",
    "lastUpdated": "2025-01-12",
    "schema": "Supports drag-and-drop UI configuration and detailed CAN protocol implementation"
  },

  "functions": {
    "steering": {
      "name": "Steering",
      "color": "#3498db",
      "description": "Valve/Motor steering control",
      "exclusive": true,
      "bitValue": 1
    },
    "buttons": {
      "name": "Buttons",
      "color": "#2ecc71",
      "description": "Control button inputs",
      "exclusive": false,
      "bitValue": 2
    },
    "hitch": {
      "name": "Hitch",
      "color": "#e74c3c",
      "description": "3-point hitch control",
      "exclusive": false,
      "bitValue": 4
    },
    "implement": {
      "name": "Implement",
      "color": "#f39c12",
      "description": "ISO implement control",
      "exclusive": false,
      "bitValue": 8
    },
    "keya": {
      "name": "Keya Motor",
      "color": "#9b59b6",
      "description": "Keya CAN motor control",
      "exclusive": true,
      "bitValue": 16
    }
  },

  "busTypes": {
    "None": {
      "id": 0,
      "displayName": "None",
      "description": "Bus not configured"
    },
    "V_Bus": {
      "id": 1,
      "displayName": "V_Bus",
      "description": "Valve bus for steering",
      "defaultSpeed": 250
    },
    "K_Bus": {
      "id": 2,
      "displayName": "K_Bus",
      "description": "Tractor control bus",
      "defaultSpeed": 500
    },
    "ISO_Bus": {
      "id": 3,
      "displayName": "ISO_Bus",
      "description": "ISOBUS implement control",
      "defaultSpeed": 250
    }
  },

  "brands": [
    {
      "id": 0,
      "name": "DISABLED",
      "displayName": "Disabled",
      "description": "CAN bus disabled",
      "capabilities": {},
      "uiNotes": "CAN bus functions are disabled."
    },
    {
      "id": 1,
      "name": "CASEIH_NH",
      "displayName": "Case IH/New Holland",
      "description": "Case IH and New Holland tractors (shared V-Bus protocol)",
      "capabilities": {
        "V_Bus": ["steering", "keya"],
        "K_Bus": ["buttons", "hitch", "keya"],
        "ISO_Bus": ["keya"]
      },
      "uiNotes": "V_Bus for steering • K_Bus for engage button and hitch control • Use Keya motor if no valve"
    },
    {
      "id": 2,
      "name": "CAT_MT",
      "displayName": "CAT MT Series",
      "description": "Caterpillar MT series tractors",
      "capabilities": {
        "V_Bus": ["steering", "keya"],
        "K_Bus": ["buttons", "keya"],
        "ISO_Bus": ["keya"]
      },
      "uiNotes": null
    },
    {
      "id": 3,
      "name": "CLAAS",
      "displayName": "Claas",
      "description": "Claas tractors",
      "capabilities": {
        "V_Bus": ["steering", "keya"],
        "K_Bus": ["buttons", "keya"],
        "ISO_Bus": ["keya"]
      },
      "uiNotes": null
    },
    {
      "id": 4,
      "name": "FENDT",
      "displayName": "Fendt SCR/S4/Gen6",
      "description": "Fendt SCR, S4, and Gen6 series tractors",
      "capabilities": {
        "V_Bus": ["steering", "keya"],
        "K_Bus": ["buttons", "hitch", "keya"],
        "ISO_Bus": ["keya"]
      },
      "uiNotes": "K_Bus typically handles buttons and hitch • V_Bus for steering • Use Keya motor if no valve"
    },
    {
      "id": 5,
      "name": "FENDT_ONE",
      "displayName": "Fendt One",
      "description": "Fendt One series tractors",
      "capabilities": {
        "V_Bus": ["steering", "keya"],
        "K_Bus": ["buttons", "hitch", "keya"],
        "ISO_Bus": ["steering", "implement", "keya"]
      },
      "uiNotes": null
    },
    {
      "id": 6,
      "name": "GENERIC",
      "displayName": "Generic",
      "description": "Generic/mixed configuration for custom setups",
      "capabilities": {
        "V_Bus": ["steering", "keya"],
        "K_Bus": ["buttons", "hitch", "keya"],
        "ISO_Bus": ["steering", "implement", "keya"]
      },
      "uiNotes": "Use when mixing functions from different brands or using Keya steering"
    },
    {
      "id": 7,
      "name": "JCB",
      "displayName": "JCB",
      "description": "JCB tractors",
      "capabilities": {
        "V_Bus": ["steering", "keya"],
        "K_Bus": ["keya"],
        "ISO_Bus": ["keya"]
      },
      "uiNotes": null
    },
    {
      "id": 8,
      "name": "LINDNER",
      "displayName": "Lindner",
      "description": "Lindner tractors",
      "capabilities": {
        "V_Bus": ["steering", "keya"],
        "K_Bus": ["keya"],
        "ISO_Bus": ["keya"]
      },
      "uiNotes": null
    },
    {
      "id": 9,
      "name": "VALTRA_MASSEY",
      "displayName": "Valtra/Massey Ferguson",
      "description": "Valtra and Massey Ferguson tractors (shared protocol)",
      "capabilities": {
        "V_Bus": ["steering", "keya"],
        "K_Bus": ["buttons", "hitch", "keya"],
        "ISO_Bus": ["keya"]
      },
      "uiNotes": "V_Bus steering only - requires continuous valve ready messages"
    }
  ]
})JSON";

#endif // CAN_INFO_JSON_H
