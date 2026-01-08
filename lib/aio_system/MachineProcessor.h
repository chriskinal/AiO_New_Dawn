// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

#pragma once

// Re-enabling MachineProcessor - memory corruption was AgOpenGPS v6.7.1 bug
// #define MACHINE_PROCESSOR_DISABLED 1

#include <Arduino.h>
#include <stdint.h>

// Function assignments for machine outputs
// Functions 1-16: Section control
// Function 17: Hydraulic Up
// Function 18: Hydraulic Down  
// Function 19: Tramline Right
// Function 20: Tramline Left
// Function 21: Geo Stop
constexpr uint8_t MAX_FUNCTIONS = 21;
constexpr uint8_t MAX_MACHINE_OUTPUTS = 6;  // Physical outputs available
constexpr uint8_t MAX_PIN_CONFIG = 24;      // Max pins configurable in AOG

class MachineProcessor {
private:
    static MachineProcessor* instance;
    
    MachineProcessor();
    
    // Machine state - tracks all functions from PGN 239
    struct MachineState {
        // Section control (existing)
        uint16_t sectionStates;      // Section states (bytes 11 & 12 from PGN 239)
        uint32_t lastPGN239Time;     // For watchdog timeout
        
        // Machine control data from PGN 239
        uint8_t hydLift;             // Byte 7: 0=off, 1=down, 2=up
        uint8_t tramline;            // Byte 8: bit0=right, bit1=left
        uint8_t geoStop;             // Byte 9: 0=inside boundary, 1=outside
        
        // Function states array [0-21], index 0 unused
        // Functions 1-16: sections, 17-18: hyd up/down, 19-20: tram R/L, 21: geo
        bool functions[MAX_FUNCTIONS + 1];  // Index 0 not used
        
        // Track changes
        bool functionsChanged;       // Flag when any function state changes
        
        // Hydraulic timing
        uint32_t hydStartTime;       // When hydraulic movement started
        uint8_t lastHydLift;         // Previous hydraulic state
    } machineState;
    
    // Machine configuration from PGN 238 is stored directly in ConfigManager
    // configReceived tracks if we've received config from AgOpenGPS
    bool configReceived = false;
    
    // Pin configuration from PGN 236
    struct PinConfig {
        // Pin function assignments [1-24], value 0-21
        // 0=unassigned, 1-21=function number
        uint8_t pinFunction[MAX_PIN_CONFIG + 1];  // Index 0 not used
        bool configReceived;         // Track if config has been received
    } pinConfig;
    
public:
    static MachineProcessor* getInstance();
    static bool init();
    
    bool initialize();
    void process();
    
    static void handleBroadcastPGN(uint8_t pgn, const uint8_t* data, size_t len);
    static void handlePGN236(uint8_t pgn, const uint8_t* data, size_t len);  // Pin config
    static void handlePGN238(uint8_t pgn, const uint8_t* data, size_t len);  // Machine config
    static void handlePGN239(uint8_t pgn, const uint8_t* data, size_t len);  // Machine data
    
    void updateSectionOutputs();
    void updateMachineOutputs();     // New unified output handler
    void updateFunctionStates();     // Update functions array from PGN data
    
    // Debug helpers
    const char* getFunctionName(uint8_t functionNum);
    
    // EEPROM persistence
    void savePinConfig();
    void loadPinConfig();
    void saveMachineConfig();
    void loadMachineConfig();
    
    // Section control helpers
    bool initializeSectionOutputs();
    void setPinHigh(uint8_t pin);
    void setPinLow(uint8_t pin);
    void setPinPWM(uint8_t pin, uint16_t pwmValue);  // For Danfoss valve control
    bool checkPCA9685();

    // Section control sleep mode detection
    bool isOnboardSectionControlActive() const;  // Returns true if onboard SC should respond
};

extern MachineProcessor machineProcessor;

constexpr uint8_t MACHINE_SOURCE_ID = 0x7C;
constexpr uint8_t MACHINE_PGN_PIN_CONFIG = 0xEC; // 236 - Pin Config from AOG
constexpr uint8_t MACHINE_PGN_CONFIG = 0xEE;     // 238 - Machine Config from AOG
constexpr uint8_t MACHINE_PGN_DATA = 0xEF;       // 239 - Machine Data from AOG
constexpr uint8_t MACHINE_PGN_REPLY = 0xED;      // 237 - From Machine to AOG
constexpr uint8_t MACHINE_HELLO_REPLY = 0x7B;