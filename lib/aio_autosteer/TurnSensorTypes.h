// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

#ifndef TURN_SENSOR_TYPES_H
#define TURN_SENSOR_TYPES_H

#include <Arduino.h>

// Turn sensor types matching AgOpenGPS
enum class TurnSensorType : uint8_t {
    NONE = 0,
    ENCODER = 1,      // Rotary encoder (single or quadrature)
    PRESSURE = 2,     // Hydraulic pressure sensor
    CURRENT = 3,      // Motor current sensor
    JD_PWM = 4        // John Deere PWM encoder
};

// Encoder types
enum class EncoderType : uint8_t {
    SINGLE = 1,       // Single channel encoder
    QUADRATURE = 2    // Quadrature (dual channel) encoder
};

#endif // TURN_SENSOR_TYPES_H