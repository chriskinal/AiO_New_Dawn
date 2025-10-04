# CAN Message Reference - AiO New Dawn

This document contains all CAN message IDs and payloads implemented in the AiO New Dawn firmware.

## Table of Contents
- [Keya Motor](#keya-motor)
- [Valtra/Massey Ferguson](#valtramassey-ferguson)
- [Fendt](#fendt)
- [Case IH/New Holland](#case-ihnew-holland)
- [CAT MT Series](#cat-mt-series)
- [Claas](#claas)
- [Generic CAN Message Format](#generic-can-message-format)

---

## Keya Motor

### Receive Messages

#### Heartbeat/Status Message
- **ID:** `0x07000001` (Extended)
- **Direction:** Motor → Controller
- **Data Format:** Big-endian (MSB first)
  - Bytes 0-1: Position/Angle (uint16)
  - Bytes 2-3: Speed/RPM (int16)
  - Bytes 4-5: Current (int16)
  - Bytes 6-7: Error code (uint16)

### Transmit Messages

#### Enable/Disable Command
- **ID:** `0x06000001` (Extended)
- **Direction:** Controller → Motor
- **Enable Command:**
  ```
  Data: 23 0D 20 01 00 00 00 00
  ```
- **Disable Command:**
  ```
  Data: 23 0C 20 01 00 00 00 00
  ```

#### Speed Command
- **ID:** `0x06000001` (Extended)
- **Direction:** Controller → Motor
- **Data Format:**
  ```
  Byte 0: 0x23
  Byte 1: 0x00 (Speed command)
  Byte 2: 0x20
  Byte 3: 0x01
  Bytes 4-5: Speed value (little-endian, -1000 to +1000)
  Bytes 6-7: Speed value (big-endian, same value)
  ```

---

## Valtra/Massey Ferguson

### V_Bus (Steering Control)

#### Valve Status Message
- **ID:** `0x0CAC1C13` (Extended)
- **Direction:** Valve → Controller
- **Data Format:**
  - Bytes 0-1: Steering curve/position (int16, little-endian)
  - Byte 2: Valve ready state (0 = not ready, non-zero = ready)
  - Bytes 3-7: Reserved/Unknown

#### Steering Command
- **ID:** `0x0CAD131C` (Extended)
- **Direction:** Controller → Valve
- **Data Format:**
  - Bytes 0-1: Set curve value (int16, little-endian)
  - Byte 2: Intent flag (253 = steer intent, 252 = no intent)
  - Bytes 3-7: 0x00

#### Engage/Disengage Messages
- **IDs:** `0x18EF1C32`, `0x18EF1CFC`, `0x18EF1C00` (Extended)
- **Direction:** Tractor → Controller
- **Note:** These are informational messages about engagement state

### K_Bus (Button Control)

#### Button Status Message
- **ID:** `0x0CFF2621` (Extended)
- **Direction:** Bidirectional
- **Data Format:**
  - Byte 3, Bit 2 (0x04): Engage button state (toggles autosteer when pressed)
  - Byte 3, Bit 4 (0x10): F1 button (when sending)
  - Byte 3, Bit 5 (0x20): F2 button (when sending)
  - Byte 6: Rolling counter (increment when sending)
  - Other bytes: Copy from last received message

**Note:** The engage button (bit 2) now functions like the physical autosteer button - pressing it toggles between armed/disarmed states.

---

## Fendt SCR/S4/Gen6

### V_Bus (Steering Control)

#### Valve Status Message
- **ID:** `0x0CEF2CF0` (Extended)
- **Direction:** Valve → Controller
- **Data Format:**
  - Special valve ready detection:
    - If message length = 3 AND byte 2 = 0: Valve NOT ready
    - Otherwise: Valve is ready
  - Bytes 0-1: Steering curve/position (int16, big-endian)
  - Other bytes: Reserved/Unknown

#### Steering Command
- **ID:** `0x0CEFF02C` (Extended)
- **Direction:** Controller → Valve
- **Length:** 6 bytes (Fendt uses shorter messages)
- **Data Format:**
  - Byte 0: 0x05 (Fixed)
  - Byte 1: 0x09 (Fixed)
  - Byte 2: State (3 = steer active, 2 = inactive)
  - Byte 3: 0x0A (Fixed)
  - Bytes 4-5: Curve value (int16, big-endian)
  - **Note:** Curve = targetPWM - 32128 (special offset)

### K_Bus (Armrest Control)

#### Armrest Button Message
- **ID:** `0x613` (Standard - NOT extended!)
- **Direction:** Armrest → Controller
- **Data Format:**
  - Byte 1, Bit 7 (0x80): Button state (toggles autosteer when pressed)
  - Special state: When Buf[1] = 0x8A AND Buf[4] = 0x80: Auto steer active on tractor (disables valve)

**Note:**
- Unlike other brands, Fendt K_Bus uses standard CAN IDs (11-bit), not extended
- The armrest button functions like the physical autosteer button - pressing it toggles between armed/disarmed states

---

## Case IH/New Holland

### V_Bus (Steering Control)

#### Valve Status Message
- **ID:** `0x0CACAA08` (Extended)
- **Direction:** Valve → Controller
- **Data Format:**
  - Bytes 0-1: Steering curve/position (int16, little-endian)
  - Byte 2: Valve ready state (0 = not ready, non-zero = ready)
  - Bytes 3-7: Reserved/Unknown

#### Steering Command
- **ID:** `0x0CAD08AA` (Extended)
- **Direction:** Controller → Valve
- **Data Format:**
  - Bytes 0-1: Set curve value (int16, little-endian)
  - Byte 2: Intent flag (253 = steer intent, 252 = no intent)
  - Bytes 3-7: 0xFF

### K_Bus (Engage/Hitch Control)

#### Engage Message
- **ID:** `0x14FF7706` (Extended)
- **Direction:** Tractor → Controller
- **Data Format:**
  - Engage conditions (either):
    - Buf[0] = 130 AND Buf[1] = 1
    - Buf[0] = 178 AND Buf[4] = 1
  - **Function:** When engage goes from OFF to ON, toggles autosteer armed/disarmed state

#### Rear Hitch Information
- **ID:** `0x18FE4523` (Extended)
- **Direction:** Tractor → Controller
- **Data Format:**
  - Byte 0: Rear hitch pressure status

**Note:** Case IH/NH uses Module ID 0xAA in message addressing

---

## CAT MT Series

### V_Bus (Steering Control)

#### Curve Data Message
- **ID:** `0x0FFF9880` (Extended)
- **Direction:** Valve → Controller
- **Data Format:**
  - Bytes 4-5: Steering curve/position (int16, big-endian)
  - **Valve Ready:** Curve value between 15000-17000 indicates valve is ready
  - Other bytes: Reserved/Unknown

#### Steering Command
- **ID:** `0x0EF87F80` (Extended)
- **Direction:** Controller → Valve
- **Data Format:**
  - Bytes 0-1: 0x40, 0x01 (Fixed)
  - Bytes 2-3: Set curve value (int16, big-endian)
  - **Note:** Curve = targetPWM - 2048 (special calculation)
  - Bytes 4-7: 0xFF (Fixed)

### K_Bus (Engage Control)

#### Engage Message
- **ID:** `0x18F00400` (Extended)
- **Direction:** Tractor → Controller
- **Data Format:**
  - Engage condition: (Buf[0] & 0x0F) == 4
  - **Function:** When engage goes from OFF to ON, toggles autosteer armed/disarmed state

**Note:** CAT MT uses Module ID 0x1C and big-endian byte ordering for curve values

---

## Claas

### V_Bus (Steering Control)

#### Valve Status Message
- **ID:** `0x0CAC1E13` (Extended)
- **Direction:** Valve → Controller
- **Data Format:**
  - Bytes 0-1: Steering curve/position (int16, little-endian)
  - Byte 2: Valve ready state (0 = not ready, non-zero = ready)
  - Bytes 3-7: Reserved/Unknown

#### Steering Command
- **ID:** `0x0CAD131E` (Extended)
- **Direction:** Controller → Valve
- **Data Format:**
  - Bytes 0-1: Set curve value (int16, little-endian)
  - Byte 2: Intent flag (253 = steer intent, 252 = no intent)
  - Bytes 3-7: 0xFF

### K_Bus (Engage Control)

#### Engage Message
- **ID:** `0x18EF1CD2` (Extended)
- **Direction:** Tractor → Controller
- **Data Format:**
  - Engage conditions: Buf[1] == 0x81 OR Buf[1] == 0xF1
  - **Function:** When engage goes from OFF to ON, toggles autosteer armed/disarmed state

---

## Generic CAN Message Format

### Extended CAN IDs
All tractor CAN messages use 29-bit extended IDs. When using CAN sniffers or test tools:
- Enable extended ID mode
- Use 8-byte data length
- Most messages are sent cyclically (10-100ms intervals)

### Bus Assignment by Brand

| Brand | K_Bus (CAN1) | ISO_Bus (CAN2) | V_Bus (CAN3) |
|-------|--------------|----------------|--------------|
| Disabled | - | - | - |
| Fendt SCR/S4/Gen6 | Buttons, Hitch | - | Steering |
| Valtra/Massey | Buttons, Hitch | - | Steering |
| Case IH/NH | Hitch | - | Steering |
| Fendt One | Buttons, Hitch | Steering, Implement | Steering |
| Claas | Engage | - | Steering |
| JCB | Buttons | - | Steering |
| Lindner | Buttons | - | Steering |
| CAT MT | Engage | - | Steering |
| Generic | Keya* | Keya* | Keya* |

*Keya motor can be assigned to any bus when using Generic brand

### Testing with CAN Sniffer

1. **Configure sniffer:**
   - Baud rate: 250kbps (standard for agricultural CAN)
   - Extended IDs enabled
   - Connect to appropriate bus based on function

2. **Monitor traffic:**
   - Look for cyclic messages (valve status, button states)
   - Note message intervals and data patterns

3. **Send test messages:**
   - Use examples above to simulate tractor responses
   - Monitor system logs for confirmation

### Logging

When CAN messages are processed, the system logs events:
- `[INFO] AUTOSTEER: Keya motor detected and ready`
- `[INFO] AUTOSTEER: Valtra steering valve ready`
- `[INFO] AUTOSTEER: Massey K_Bus engage button pressed/released`
- `[INFO] AUTOSTEER: Massey F1/F2 button pressed`

---

## Notes

1. All multi-byte values follow the endianness specified in each message description
2. Reserved/unknown bytes should be set to 0x00 unless copying from received message
3. The system stores complete received messages for protocols that require message echo/modification (like Massey K_Bus)
4. Timeout for valve ready is 250ms - if no ready message received, steering is disabled

---

*Document Version: 1.0*
*Firmware Version: 1.0.63-beta*