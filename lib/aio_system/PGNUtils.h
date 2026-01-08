// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

#ifndef PGN_UTILS_H
#define PGN_UTILS_H

#include <stdint.h>

// Utility function for PGN CRC calculation
// Calculates and sets the CRC byte for PGN messages
// Based on AOG NG-V6 implementation
inline void calculateAndSetCRC(uint8_t myMessage[], uint8_t myLen) 
{
    if (myLen <= 2) return;

    uint8_t crc = 0;
    for (uint8_t i = 2; i < myLen - 1; i++) 
    {
        crc = (crc + myMessage[i]);
    }
    myMessage[myLen - 1] = crc;
}

#endif // PGN_UTILS_H