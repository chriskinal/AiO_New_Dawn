// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

#ifndef MESSAGE_BUILDER_H
#define MESSAGE_BUILDER_H

#include <cstdint>
#include <cstring>

// Efficient NMEA message builder without format string parsing
class NMEAMessageBuilder {
private:
    char* buffer;
    char* ptr;
    
    // Fast integer to string conversion
    char* writeInt(int value) {
        if (value < 0) {
            *ptr++ = '-';
            value = -value;
        }
        
        // Count digits
        int temp = value;
        int digits = 1;
        while (temp >= 10) {
            digits++;
            temp /= 10;
        }
        
        // Write digits
        char* end = ptr + digits;
        ptr = end;
        do {
            *--end = '0' + (value % 10);
            value /= 10;
        } while (value > 0);
        
        return ptr;
    }
    
    // Fast float to string with fixed decimals
    char* writeFloat(float value, int decimals) {
        if (value < 0) {
            *ptr++ = '-';
            value = -value;
        }
        
        // Integer part
        int ipart = (int)value;
        ptr = writeInt(ipart);
        
        // Decimal point
        *ptr++ = '.';
        
        // Fractional part
        float fpart = value - ipart;
        for (int i = 0; i < decimals; i++) {
            fpart *= 10;
            int digit = (int)fpart;
            *ptr++ = '0' + digit;
            fpart -= digit;
        }
        
        return ptr;
    }
    
public:
    NMEAMessageBuilder(char* buf) : buffer(buf), ptr(buf) {}
    
    void addString(const char* str) {
        while (*str) {
            *ptr++ = *str++;
        }
    }
    
    void addChar(char c) {
        *ptr++ = c;
    }
    
    void addComma() {
        *ptr++ = ',';
    }
    
    void addInt(int value) {
        ptr = writeInt(value);
    }
    
    void addFloat(float value, int decimals) {
        ptr = writeFloat(value, decimals);
    }
    
    // Add latitude in NMEA format (DDMM.MMMMMM)
    void addLatitude(double lat) {
        int degrees = (int)(lat / 100);
        double minutes = lat - (degrees * 100);
        
        // Pad degrees to 2 digits
        if (degrees < 10) *ptr++ = '0';
        ptr = writeInt(degrees);
        
        // Pad minutes 
        if (minutes < 10) *ptr++ = '0';
        ptr = writeFloat(minutes, 6);
    }
    
    // Add longitude in NMEA format (DDDMM.MMMMMM)
    void addLongitude(double lon) {
        int degrees = (int)(lon / 100);
        double minutes = lon - (degrees * 100);
        
        // Pad degrees to 3 digits
        if (degrees < 100) *ptr++ = '0';
        if (degrees < 10) *ptr++ = '0';
        ptr = writeInt(degrees);
        
        // Pad minutes
        if (minutes < 10) *ptr++ = '0';
        ptr = writeFloat(minutes, 6);
    }
    
    int length() const {
        return ptr - buffer;
    }
    
    void terminate() {
        *ptr = '\0';
    }
    
    // Calculate NMEA checksum
    uint8_t calculateChecksum() const {
        uint8_t checksum = 0;
        // Skip $ and calculate up to * or end
        for (const char* p = buffer + 1; p < ptr; p++) {
            checksum ^= *p;
        }
        return checksum;
    }
    
    // Add checksum to message
    void addChecksum() {
        uint8_t checksum = calculateChecksum();
        *ptr++ = '*';
        
        // Convert to hex
        static const char hex[] = "0123456789ABCDEF";
        *ptr++ = hex[checksum >> 4];
        *ptr++ = hex[checksum & 0x0F];
        *ptr = '\0';
    }
};

#endif // MESSAGE_BUILDER_H