// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

// CANInfoJSON.h
// CAN configuration data in JSON format stored in PROGMEM
// Minimal Generic-only default - users upload brand-specific configs via web interface

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
      "id": 6,
      "name": "GENERIC",
      "displayName": "Generic",
      "description": "Generic/mixed configuration for custom setups",
      "capabilities": {
        "V_Bus": ["steering"],
        "K_Bus": ["buttons", "hitch"],
        "ISO_Bus": ["steering", "implement"],
        "None": ["keya"]
      },
      "allowsKeya": true,
      "uiNotes": "Use when mixing functions from different brands or using Keya steering"
    }
  ]
})JSON";

#endif // CAN_INFO_JSON_H
