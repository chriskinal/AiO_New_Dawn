// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

// base64_simple.h
// Simple Base64 encoding for WebSocket handshake

#ifndef BASE64_SIMPLE_H
#define BASE64_SIMPLE_H

#include <Arduino.h>

namespace base64 {
    
const char b64_alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

inline String encode(const uint8_t* data, size_t length) {
    String encoded;
    encoded.reserve(((length + 2) / 3) * 4);
    
    for (size_t i = 0; i < length; i += 3) {
        uint32_t octet_a = i < length ? data[i] : 0;
        uint32_t octet_b = i + 1 < length ? data[i + 1] : 0;
        uint32_t octet_c = i + 2 < length ? data[i + 2] : 0;
        
        uint32_t triple = (octet_a << 16) + (octet_b << 8) + octet_c;
        
        encoded += b64_alphabet[(triple >> 18) & 0x3F];
        encoded += b64_alphabet[(triple >> 12) & 0x3F];
        encoded += (i + 1 < length) ? b64_alphabet[(triple >> 6) & 0x3F] : '=';
        encoded += (i + 2 < length) ? b64_alphabet[triple & 0x3F] : '=';
    }
    
    return encoded;
}

} // namespace base64

#endif // BASE64_SIMPLE_H