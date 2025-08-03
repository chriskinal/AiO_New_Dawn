# ISOBUS Memory Optimization Strategy

## Current Memory Usage
- ITCM (RAM1) Code: 310KB / 512KB
- AgIsoStack requires: ~141KB additional
- Total needed: 451KB (within 512KB limit)

## Test Results
1. **With FLASHMEM web handlers**: 87KB overflow
2. **Without web server**: 35KB overflow  
3. **Without web server + serial menu**: 31KB overflow
4. **With custom CAN plugin (no duplicate FlexCAN)**: 23KB overflow
5. **Need to free**: Additional 23KB minimum

## Key Discovery: Duplicate FlexCAN
AgIsoStack includes its own FlexCAN_T4 plugin which duplicates the FlexCAN library already in New Dawn. By creating a custom CAN plugin (`NewDawnCANPlugin`) that uses New Dawn's existing CAN infrastructure, we saved ~8KB of ITCM.

## Option 1: Move Non-Critical Code to PROGMEM (Recommended)

### Add build flags to platformio.ini:
```ini
[env:teensy41]
build_flags = 
    -DENABLE_ISOBUS_VT
    -DCORE_FLASHMEM  ; Move more core code to flash
    
# Create new macros in a common header:
# define FLASHMEM_IFPOSSIBLE __attribute__((section(".flashmem")))
```

### Move these components out of ITCM:
1. **Web Server Functions** (~50KB potential savings)
   - All request handlers in WebSystem.cpp
   - JSON generation functions
   - String manipulation code
   
2. **Non-Critical Processors** (~30KB potential savings)
   - SpeedPulseProcessor (if not using speed sensor)
   - RemoteProcessor (if not using remote switches)
   - Some diagnostic functions

3. **Serial Menu System** (~20KB potential savings)
   - Interactive menu functions
   - String formatting code

4. **Error Handling and Logging** (~15KB potential savings)
   - Non-critical EventLogger functions
   - Verbose error messages

## Option 2: Conditional Compilation

### Create feature flags in platformio.ini:
```ini
[env:teensy41-isobus]
build_flags = 
    -DENABLE_ISOBUS_VT
    -DDISABLE_WEB_SERVER      ; Save ~80KB
    -DDISABLE_SERIAL_MENU     ; Save ~20KB
    -DDISABLE_REMOTE_SWITCH   ; Save ~10KB
    -DDISABLE_SPEED_PULSE     ; Save ~10KB
    -DMINIMAL_LOGGING         ; Save ~10KB
```

## Option 3: Dynamic Module Loading

### Split into two firmware versions:
1. **Standard Build**: All features except ISOBUS
2. **ISOBUS Build**: Core + ISOBUS, reduced web features

## Option 4: Optimize Existing Code

### Quick wins:
1. **Inline small functions** that are called frequently
2. **Combine similar string literals** to reduce duplication
3. **Use more efficient data structures** (e.g., bitfields)
4. **Remove debug code** in production builds

## Implementation Steps

### Step 1: Add FLASHMEM to large functions
```cpp
// In WebSystem.cpp
FLASHMEM void handleHomePage(AsyncWebServerRequest* request) {
    // Move out of ITCM
}

FLASHMEM void handleDeviceSettingsPage(AsyncWebServerRequest* request) {
    // Move out of ITCM
}
```

### Step 2: Create build configurations
```ini
[common]
lib_deps = 
    # Common dependencies

[env:teensy41]
platform = teensy
board = teensy41
framework = arduino
upload_speed = 921600
lib_deps = ${common.lib_deps}

[env:teensy41-isobus-minimal]
platform = teensy
board = teensy41
framework = arduino
upload_speed = 921600
build_flags = 
    -DENABLE_ISOBUS_VT
    -DDISABLE_WEB_SERVER
    -DMINIMAL_BUILD
lib_deps = ${common.lib_deps}

[env:teensy41-isobus-web]
platform = teensy
board = teensy41
framework = arduino
upload_speed = 921600
build_flags = 
    -DENABLE_ISOBUS_VT
    -DREDUCE_WEB_FEATURES
    -DFLASHMEM_WEBSYSTEM
lib_deps = ${common.lib_deps}
```

### Step 3: Modify main.cpp
```cpp
void setup() {
    // Core initialization always runs
    
    #ifndef DISABLE_WEB_SERVER
    webSystem.begin();
    #endif
    
    #ifdef ENABLE_ISOBUS_VT
    VTClient::getInstance()->init();
    #endif
}
```

## Testing Memory Savings

After each change, compile and check:
```bash
~/.platformio/penv/bin/pio run -e teensy41-isobus-minimal | grep "RAM1:"
```

Goal: RAM1 code ≤ 370KB to fit AgIsoStack

## Final Results

After extensive optimization attempts:

1. **Memory savings achieved**:
   - Disabled web server: ~52KB saved
   - Disabled serial menu: ~4KB saved  
   - Custom CAN plugin (no duplicate FlexCAN): ~8KB saved
   - Removed unused AgIsoStack modules: ~9KB saved
   - **Total saved**: ~73KB (from 141KB to 23KB overflow)

2. **Remaining issue**: 
   - Still need 23KB more ITCM space
   - AgIsoStack core is ~118KB even with minimal features
   - FastPacketProtocol is embedded in CANNetworkManager (can't remove)

## Conclusion

AgIsoStack is too large for New Dawn's current architecture. Even with aggressive optimization, the library requires more ITCM than available when combined with New Dawn's existing features.

## Recommended Solutions

1. **External ISOBUS Module** (Recommended)
   - Use dedicated ESP32 or STM32F4 for ISOBUS
   - Communicate via CAN bridge or serial protocol
   - Keeps all New Dawn features intact
   - Example: ESP32 + MCP2515 CAN controller

2. **Dual Firmware Approach**
   - Create separate "ISOBUS-only" firmware
   - Remove ALL non-essential features
   - Users choose between full features OR ISOBUS

3. **Alternative ISOBUS Library**
   - Find or create a lighter-weight VT client
   - Focus only on essential VT functions
   - Skip advanced features like Task Controller

## Build Instructions

For ISOBUS build:
```bash
~/.platformio/penv/bin/pio run -e teensy41-isobus-minimal
```

Note: This build has NO web interface or serial menu.