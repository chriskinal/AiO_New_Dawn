// Firmware_Teensy_AiO-NG-v6 is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-NG-v6 is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-NG-v6 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Foobar. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

#ifndef SERIALGLOBALS_H_
#define SERIALGLOBALS_H_

#include "Arduino.h"
#include "HardwareSerial.h"
//#include <Streaming.h>

// Include necessary parser and statistics classes
#include "UBXParser.h"

// External variables that SerialManager needs access to
extern void sendUDPbytes(uint8_t *message, int msgLen);
extern UBX_Parser ubxParser;
extern bool nmeaDebug;
extern bool nmeaDebug2;
extern uint32_t GPS1BAUD;
extern uint32_t GPS2BAUD;
extern bool USB1DTR;
extern bool USB2DTR;

// USB Serial declarations for bridge mode
#if defined(USB_DUAL_SERIAL) || defined(USB_TRIPLE_SERIAL)
extern usb_serial_class SerialUSB1;
#endif

#if defined(USB_TRIPLE_SERIAL)
extern usb_serial_class SerialUSB2;
#endif

#endif // SERIALGLOBALS_H_