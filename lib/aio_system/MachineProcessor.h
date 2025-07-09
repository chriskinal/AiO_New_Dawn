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
    
    // Machine configuration from PGN 238
    struct MachineConfig {
        uint8_t raiseTime;           // Hydraulic raise time (seconds)
        uint8_t lowerTime;           // Hydraulic lower time (seconds)
        uint8_t hydEnable;           // Hydraulic functions enabled
        uint8_t isPinActiveHigh;     // 0=active low (default), 1=active high
        uint8_t user1;               // User defined values
        uint8_t user2;
        uint8_t user3;
        uint8_t user4;
        bool configReceived;         // Track if config has been received
    } machineConfig;
    
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
    
    // Section control helpers
    bool initializeSectionOutputs();
    void setPinHigh(uint8_t pin);
    void setPinLow(uint8_t pin);
    bool checkPCA9685();
};

extern MachineProcessor machineProcessor;

constexpr uint8_t MACHINE_SOURCE_ID = 0x7C;
constexpr uint8_t MACHINE_PGN_PIN_CONFIG = 0xEC; // 236 - Pin Config from AOG
constexpr uint8_t MACHINE_PGN_CONFIG = 0xEE;     // 238 - Machine Config from AOG
constexpr uint8_t MACHINE_PGN_DATA = 0xEF;       // 239 - Machine Data from AOG
constexpr uint8_t MACHINE_PGN_REPLY = 0xED;      // 237 - From Machine to AOG
constexpr uint8_t MACHINE_HELLO_REPLY = 0x7B;