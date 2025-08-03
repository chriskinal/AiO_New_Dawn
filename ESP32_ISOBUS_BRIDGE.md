# ESP32 ISOBUS VT Bridge for New Dawn

## Overview
The AiO board includes an ESP32 (XIAO ESP32C3) slot that can be used as a dedicated ISOBUS VT processor, solving the memory constraints of running AgIsoStack on the Teensy 4.1.

## Hardware Connections (from schematic)

### ESP32 Module: XIAO ESP32C3
- **Serial Communication**: 
  - ESP32TX → Teensy RX (via R152 pullup)
  - ESP32RX ← Teensy TX (via R153 pullup)
  - Level shifting handled by resistors

- **Shared Signals**:
  - GPS-TX/RX: Can receive GPS data from Teensy
  - SPEEDPULSE: Speed sensor input
  - T->PIEZO: Buzzer control output
  - T->DIR: Direction signal
  - T->PWM: PWM output
  - CURRENT_SENSE->T: Current monitoring

- **Status**: 
  - WIFI_LED: Status indication
  - LED_ESP_TX/RX: Communication activity

- **Power**: 
  - 3.3V and 5V available
  - GND reference

## Architecture

### ESP32 Responsibilities
1. Run AgIsoStack library for ISOBUS VT
2. Manage CAN2 interface (via external CAN transceiver)
3. Handle VT object pool and display updates
4. Process VT touch/button events
5. Bridge VT data to/from Teensy via serial

### Teensy Responsibilities  
1. Continue all existing New Dawn functions
2. Send relevant data to ESP32 for VT display
3. Receive VT input events from ESP32
4. Maintain primary control of machine

### CAN Hardware for ESP32
Since ESP32 doesn't have built-in CAN, options include:
1. **MCP2515 SPI CAN Controller** (recommended)
   - Connect to ESP32 SPI pins
   - Use existing CAN2 transceiver on board
2. **External CAN transceiver** 
   - If CAN2 pins are accessible

## Serial Protocol Design

### Message Format (Binary)
```
[SYNC_BYTE][LENGTH][MSG_TYPE][DATA...][CHECKSUM]
```

### Message Types

#### Teensy → ESP32
- `0x01`: Machine Status
  ```
  Speed(2), Heading(2), Roll(2), Pitch(2), SteerAngle(2)
  ```
- `0x02`: Work State
  ```
  WorkSwitch(1), Sections(2), Rate(2)
  ```
- `0x03`: GPS Data
  ```
  Lat(4), Lon(4), Altitude(2), Quality(1)
  ```

#### ESP32 → Teensy
- `0x10`: VT Connected Status
  ```
  Connected(1), VT_NAME(8)
  ```
- `0x11`: Soft Key Pressed
  ```
  ObjectID(2), KeyNumber(1), State(1)
  ```
- `0x12`: Numeric Value Changed
  ```
  ObjectID(2), Value(4)
  ```

### Communication Parameters
- Baud Rate: 115200 (can go higher if needed)
- 8N1 format
- Binary protocol for efficiency

## Implementation Plan

### Phase 1: ESP32 Setup
1. Create PlatformIO project for XIAO ESP32C3
2. Add AgIsoStack library 
3. Implement MCP2515 CAN interface
4. Create basic VT object pool

### Phase 2: Serial Bridge
1. Implement binary serial protocol
2. Create message handlers on both sides
3. Add error checking and recovery
4. Test communication reliability

### Phase 3: VT Integration
1. Design VT screens for New Dawn data
2. Map Teensy data to VT objects
3. Handle VT input events
4. Create status display on main screen

## Example ESP32 Code Structure

```cpp
// ESP32_ISOBUS_Bridge.ino
#include <AgIsoStack.hpp>
#include <MCP2515.h>
#include "SerialProtocol.h"
#include "VTObjectPool.h"

class ISOBUSBridge {
private:
    SerialProtocol teensy;
    MCP2515 can;
    VTClient vtClient;
    
public:
    void setup() {
        // Initialize serial to Teensy
        Serial.begin(115200);
        teensy.begin(&Serial);
        
        // Initialize CAN
        can.begin(250000);  // ISOBUS baud rate
        
        // Initialize VT client
        vtClient.begin(&can);
        vtClient.setObjectPool(VTObjectPool, sizeof(VTObjectPool));
    }
    
    void loop() {
        // Process Teensy messages
        if (teensy.available()) {
            auto msg = teensy.readMessage();
            handleTeensyMessage(msg);
        }
        
        // Update VT
        vtClient.update();
        
        // Handle VT events
        if (vtClient.hasEvent()) {
            auto event = vtClient.getEvent();
            sendEventToTeensy(event);
        }
    }
};
```

## Advantages
1. **No memory constraints**: ESP32 has plenty of RAM for AgIsoStack
2. **Dedicated processor**: VT updates don't impact Teensy performance
3. **Modular**: ESP32 module is optional - base system works without it
4. **WiFi bonus**: ESP32 could also provide WiFi features
5. **Future expansion**: ESP32 could handle other tasks too

## Development Steps
1. Order XIAO ESP32C3 and MCP2515 module
2. Create ESP32 firmware project
3. Implement minimal VT display
4. Add serial protocol to Teensy code
5. Test integrated system
6. Expand VT functionality

This approach solves the memory issue while keeping the system modular and maintainable.