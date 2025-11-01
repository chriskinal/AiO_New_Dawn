# CAN Brand Implementation Guide

This document contains complete implementation details for all tractor brands based on the official AgOpenGPS CAN firmware.

## Brand Index

| Brand # | Name | Status | V_Bus | K_Bus | ISO_Bus |
|---------|------|--------|-------|-------|---------|
| 0 | Claas | ✅ Implemented | ✅ Steering | ✅ Engage | - |
| 1 | Valtra/Massey Ferguson | ✅ Implemented | ✅ Steering | ✅ Buttons/Hitch | - |
| 2 | Case IH/New Holland | ✅ Implemented | ✅ Steering | ✅ Engage/Hitch | - |
| 3 | Fendt SCR/S4/Gen6 | ✅ Implemented | ✅ Steering | ✅ Buttons | - |
| 4 | JCB | ❌ Not Implemented | ✅ Steering | ✅ Engage | - |
| 5 | Fendt One | ❌ Not Implemented | ✅ Steering | ✅ Engage (500k) | ✅ Implement |
| 6 | Lindner | ❌ Not Implemented | ✅ Steering | ✅ Engage | - |
| 7 | AgOpenGPS | ❌ Not Implemented | ✅ Steering | - | - |
| 8 | CAT MT Series | ✅ Implemented | ✅ Steering | ✅ Engage | - |
| 9 | Generic (Keya) | ✅ Implemented | - | - | - |

---

## Future Enhancements

### J1939 GPS Data Broadcasting
- **PGN 65267**: Broadcast pivot position (lat/lon) for smart implements
- **PGN 129029**: Broadcast comprehensive GPS data (position, altitude, fix, satellites, HDOP/PDOP)
- Use case: Implements with their own controllers that need position data

### Hitch/Implement Status
- **ISO PGN 65093**: Standard ISO 11783 rear hitch data
- **Hitch Pressure Work State**: Use pressure readings to detect implement up/down
  - Case IH: Message 0x18FE4523 (currently only logged)
  - Could provide automatic work state detection

### Additional Button Functionality
- **Fendt**: Add "Go" and "End" button support (currently only armrest button)
- **Claas**: Add CSM1/CSM2 button support
- **All Brands**: Expand headland management capabilities

### Implementation Priority
1. Complete all remaining tractor brands first
2. Add hitch status/work state detection
3. Implement J1939 GPS broadcasting (if needed for smart implements)
4. Expand button functionality for existing brands

---

## Brand 0: Claas

### V_Bus - Steering Control
**Valve Status Message (Receive)**
- ID: `0x0CAC1E13` (Extended)
- Data:
  - Bytes 0-1: Estimated curve (little-endian int16)
  - Byte 2: Valve ready status

**Steering Command (Send)**
- ID: `0x0CAD131E` (Extended)
- Length: 8 bytes
- Data:
  - Bytes 0-1: Set curve (little-endian int16)
  - Byte 2: Intent (253 = steer, 252 = no steer)
  - Bytes 3-7: 0xFF

### K_Bus - Engage Control
**Engage Message (Receive)**
- ID: `0x18EF1CD2` (Extended)
- Conditions for engage: (Buf[1] == 0x81) OR (Buf[1] == 0xF1)

---

## Brand 1: Valtra/Massey Ferguson (✅ IMPLEMENTED)

### V_Bus - Steering Control
**Valve Status Message (Receive)**
- ID: `0x0CAC1C13` (Extended)
- Data:
  - Bytes 0-1: Estimated curve (little-endian int16)
  - Byte 2: Valve ready status (non-zero = ready)

**Steering Command (Send)**
- ID: `0x0CAD131C` (Extended)
- Length: 8 bytes
- Data:
  - Bytes 0-1: Set curve (little-endian int16)
  - Byte 2: Intent (253 = steer, 252 = no steer)
  - Bytes 3-7: 0x00

### K_Bus - Button/Hitch Control
**Button Status Message**
- ID: `0x0CFF2621` (Extended)
- Data:
  - Byte 3, Bit 2 (0x04): Engage button
  - Byte 3, Bit 4 (0x10): F1 button
  - Byte 3, Bit 5 (0x20): F2 button
  - Byte 6: Rolling counter

**Engage Messages (Receive)**
- IDs: `0x18EF1C32`, `0x18EF1CFC`, `0x18EF1C00` (Extended)

---

## Brand 2: Case IH/New Holland

### Module ID: 0xAA

### V_Bus - Steering Control
**Valve Status Message (Receive)**
- ID: `0x0CACAA08` (Extended)
- Data:
  - Bytes 0-1: Estimated curve (little-endian int16)
  - Byte 2: Valve ready status

**Steering Command (Send)**
- ID: `0x0CAD08AA` (Extended)
- Length: 8 bytes
- Data:
  - Bytes 0-1: Set curve (little-endian int16)
  - Byte 2: Intent (253 = steer, 252 = no steer)
  - Bytes 3-7: 0xFF

### K_Bus - Engage/Hitch Control
**Engage Message (Receive)**
- ID: `0x14FF7706` (Extended)
- Engage conditions:
  - (Buf[0] == 130 && Buf[1] == 1) OR
  - (Buf[0] == 178 && Buf[4] == 1)

**Rear Hitch Information (Receive)**
- ID: `0x18FE4523` (Extended)
- Data:
  - Byte 0: Rear hitch pressure status

---

## Brand 3: Fendt SCR/S4/Gen6

### Module ID: 0x2C

### V_Bus - Steering Control
**Valve Status Message (Receive)**
- ID: `0x0CEF2CF0` (Extended)
- Valve ready detection:
  - If message length == 3 && Buf[2] == 0: Valve NOT ready
  - Otherwise: Valve ready

**Steering Command (Send)**
- ID: `0x0CEFF02C` (Extended)
- Length: 6 bytes
- Data:
  - Byte 0: 0x05
  - Byte 1: 0x09
  - Byte 2: State (3 = steer active, 2 = inactive)
  - Byte 3: 0x0A
  - Bytes 4-5: Curve value (big-endian int16)
  - Note: Curve = setCurve - 32128

### K_Bus - Button Control
**Armrest Buttons (Receive)**
- ID: `0x613` (Standard - not extended!)
- Buttons:
  - Buf[1] & 0x80: Button state
  - Buf[1] == 0x8A && Buf[4] == 0x80: Auto steer active (sets valve not ready)

---

## Brand 4: JCB

### Module ID: 0xAB

### V_Bus - Steering Control
**Valve Status Message (Receive)**
- ID: `0x0CACAB13` (Extended)
- Data:
  - Bytes 0-1: Estimated curve (little-endian int16)
  - Byte 2: Valve ready status

**Steering Command (Send)**
- ID: `0x0CADAB13` (Extended)
- Length: 8 bytes
- Data:
  - Bytes 0-1: Set curve (little-endian int16)
  - Byte 2: Intent (253 = steer, 252 = no steer)
  - Bytes 3-7: 0xFF

### K_Bus - Engage Control
**Engage Message (Receive)**
- ID: `0x18EFAB27` (Extended)
- ID: `0x0CEFAB27` (Extended) - Secondary check

---

## Brand 5: Fendt One

### Module ID: 0x2C
### Special: K_Bus runs at 500kbps (not 250k!)

### V_Bus - Steering Control
**Valve Status Message (Receive)**
- ID: `0x0CEF2CF0` (Extended)
- Same as Fendt (Brand 3)

**Steering Command (Send)**
- ID: `0x0CEFF02C` (Extended)
- Same format as Fendt (Brand 3)

### K_Bus - Engage Control (500kbps!)
**Engage Message (Receive)**
- ID: `0x0CFFD899` (Extended)
- Conditions for engage:
  - (Buf[2] & 0x04) for button state

### ISO_Bus - Implement Control
**Note:** ISO_Bus is enabled for Fendt One

---

## Brand 6: Lindner

### Module ID: 0xF0

### V_Bus - Steering Control
**Valve Status Message (Receive)**
- ID: `0x0CACF013` (Extended)
- Data:
  - Bytes 0-1: Estimated curve (little-endian int16)
  - Byte 2: Valve ready status

**Steering Command (Send)**
- ID: `0x0CADF013` (Extended)
- Length: 8 bytes
- Data:
  - Bytes 0-1: Set curve (little-endian int16)
  - Byte 2: Intent (253 = steer, 252 = no steer)
  - Bytes 3-7: 0xFF

### K_Bus - Engage Control
**Engage Message (Receive)**
- ID: `0x0CEFF021` (Extended)

---

## Brand 7: AgOpenGPS

### Module ID: 0x1C

### V_Bus - Steering Control
**Valve Status Message (Receive)**
- ID: `0x0CAC1C13` (Extended)
- Same as Valtra/Massey

**Error Message (Receive)**
- ID: `0x19EF1C13` (Extended)

**Steering Command (Send)**
- ID: `0x0CAD131C` (Extended)
- Same as Valtra/Massey

---

## Brand 8: CAT MT Series

### Module ID: 0x1C

### V_Bus - Steering Control
**Curve Data Message (Receive)**
- ID: `0x0FFF9880` (Extended)
- Data:
  - Bytes 4-5: Estimated curve (big-endian int16)
  - Note: Values 15000-17000 indicate valve ready

**Steering Command (Send)**
- ID: `0x0EF87F80` (Extended)
- Length: 8 bytes
- Data:
  - Bytes 0-1: 0x40, 0x01
  - Bytes 2-3: Set curve (big-endian int16)
  - Note: Curve = setCurve - 2048 (with special handling for negatives)
  - Bytes 4-7: 0xFF

### K_Bus - Engage Control
**Engage Message (Receive)**
- ID: `0x18F00400` (Extended)
- Engage if: (Buf[0] & 0x0F) == 4

---

## Brand 9: Generic (Keya CAN Motor) (✅ IMPLEMENTED)

Uses Keya CAN protocol - not tractor-specific.

---

## Implementation Notes

### General Patterns
1. Most brands use little-endian for curve values (except Fendt/CAT which use big-endian)
2. Intent byte: 253 = steering active, 252 = inactive
3. Valve ready usually indicated by non-zero value in byte 2
4. Module IDs are used for ISO addressing

### Special Cases
1. **Fendt**: Curve offset of -32128, big-endian, 6-byte message
2. **Fendt One**: K_Bus at 500kbps instead of 250kbps
3. **CAT MT**: Curve offset of -2048, valve ready range 15000-17000
4. **Fendt K_Bus**: Uses standard CAN ID (0x613) not extended

### Common Message IDs
- Many brands share similar ID patterns (0x0CAC*, 0x0CAD*, 0x18EF*)
- The last byte often indicates the module ID

---

*Based on AgOpenGPS official CANBUS firmware*
*Source: https://github.com/AgOpenGPS-Official/Boards/tree/main/CANBUS/CANBUS%20Firmware*