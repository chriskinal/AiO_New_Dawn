# ESP32 to CAN2 Connection Options for ISOBUS VT

Since the AiO board has an internal header breaking out all 3 CAN buses, we have several options for connecting the ESP32 to CAN2 for ISOBUS communication.

## Option 1: MCP2515 SPI CAN Controller (Recommended)
Connect an MCP2515 module to the ESP32 via SPI, then wire to the CAN2 header.

### Connections:
```
ESP32 (XIAO)     MCP2515 Module
-----------      --------------
D8  (SCK)    →   SCK
D9  (MISO)   ←   SO (MISO)
D10 (MOSI)   →   SI (MOSI)
D2  (CS)     →   CS
D3  (INT)    ←   INT
3.3V         →   VCC
GND          →   GND

MCP2515 Module   CAN2 Header
--------------   -----------
CAN_H        →   CAN2_H
CAN_L        →   CAN2_L
```

### Advantages:
- Well-supported by AgIsoStack
- Proven reliable
- Cheap modules available (~$5)
- Isolated CAN transceivers available

### ESP32 Code Example:
```cpp
#include <MCP2515.h>
#include <AgIsoStack.hpp>

MCP2515 mcp2515(D2);  // CS pin
FlexCANT4Plugin canPlugin(&mcp2515);
```

## Option 2: Direct Serial Bridge Through Teensy
Instead of ESP32 accessing CAN directly, use the Teensy as a CAN bridge.

### Architecture:
```
ISOBUS VT ←→ CAN2 ←→ Teensy ←→ Serial ←→ ESP32
```

### Advantages:
- No additional hardware needed
- Teensy already manages CAN2
- Simpler wiring

### Disadvantages:
- Higher latency
- More complex software
- Teensy must handle CAN bridging

## Option 3: ESP32 with CAN Transceiver (ESP32-C3 doesn't support)
Note: The XIAO ESP32C3 doesn't have CAN controller. Would need ESP32 with TWAI.

## Option 4: Shared CAN Bus with Isolation
Use a CAN isolator to allow both Teensy and ESP32 to access CAN2.

### Connections:
```
CAN2 Header → CAN Isolator → Split to:
                              - Teensy CAN2 pins
                              - MCP2515 to ESP32
```

### Advantages:
- Both processors can access CAN2
- Electrical isolation prevents conflicts

### Disadvantages:
- More complex
- Additional cost

## Recommended Implementation: Option 1

### Shopping List:
1. MCP2515 CAN Module with TJA1050 transceiver
2. Dupont wires for connections
3. Optional: ISO1050 isolated CAN transceiver for robustness

### Wiring Steps:
1. Connect MCP2515 to ESP32 SPI pins
2. Run twisted pair from MCP2515 CAN_H/CAN_L to CAN2 header
3. Ensure common ground between ESP32 and CAN network
4. Add 120Ω termination if needed (usually at VT end)

### Software Configuration:
```cpp
// ESP32 setup
void setup() {
    // Initialize MCP2515
    mcp2515.reset();
    mcp2515.setBitrate(CAN_250KBPS, MCP_8MHZ);
    mcp2515.setNormalMode();
    
    // Initialize AgIsoStack with MCP2515
    CANHardwareInterface::set_number_of_can_channels(1);
    CANHardwareInterface::assign_can_channel_frame_handler(0, canPlugin);
    
    // Rest of ISOBUS setup...
}
```

This gives the ESP32 full access to the ISOBUS network while keeping it electrically isolated from the Teensy's CAN implementation.