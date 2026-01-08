// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

// CANGlobals.h - Global CAN bus instances to avoid ownership conflicts
#ifndef CAN_GLOBALS_H
#define CAN_GLOBALS_H

#include <FlexCAN_T4.h>

// Global CAN instances - defined here, instantiated in CANGlobals.cpp
// Reduced buffer sizes with hardware mailbox filtering protection
extern FlexCAN_T4<CAN1, RX_SIZE_16, TX_SIZE_16> globalCAN1;  // 256->16 saves ~12KB
extern FlexCAN_T4<CAN2, RX_SIZE_16, TX_SIZE_16> globalCAN2;  // 256->16 saves ~12KB
extern FlexCAN_T4<CAN3, RX_SIZE_32, TX_SIZE_64> globalCAN3;  // 256->32 saves ~11KB

// Initialize all CAN buses
void initializeGlobalCANBuses();

// Set CAN bus speed (must be called before any CAN usage)
void setCAN1Speed(uint32_t speed);
void setCAN2Speed(uint32_t speed);
void setCAN3Speed(uint32_t speed);

#endif // CAN_GLOBALS_H