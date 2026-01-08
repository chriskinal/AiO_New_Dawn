// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

// sha1_simple.h
// Simple SHA1 implementation for WebSocket handshake

#ifndef SHA1_SIMPLE_H
#define SHA1_SIMPLE_H

#include <stdint.h>
#include <string.h>

// SHA1 produces a 160-bit (20 byte) hash
#define SHA1_HASH_SIZE 20

// Simple SHA1 implementation
class SHA1 {
public:
    SHA1() { reset(); }
    
    void reset() {
        h[0] = 0x67452301;
        h[1] = 0xEFCDAB89;
        h[2] = 0x98BADCFE;
        h[3] = 0x10325476;
        h[4] = 0xC3D2E1F0;
        length = 0;
        bufferOffset = 0;
    }
    
    void update(const uint8_t* data, size_t len) {
        for (size_t i = 0; i < len; i++) {
            buffer[bufferOffset++] = data[i];
            length++;
            
            if (bufferOffset == 64) {
                processBlock();
                bufferOffset = 0;
            }
        }
    }
    
    void finalize(uint8_t* hash) {
        // Pad the message
        buffer[bufferOffset++] = 0x80;
        
        if (bufferOffset > 56) {
            while (bufferOffset < 64) {
                buffer[bufferOffset++] = 0;
            }
            processBlock();
            bufferOffset = 0;
        }
        
        while (bufferOffset < 56) {
            buffer[bufferOffset++] = 0;
        }
        
        // Append length in bits
        uint64_t bitLength = length * 8;
        for (int i = 7; i >= 0; i--) {
            buffer[bufferOffset++] = (bitLength >> (i * 8)) & 0xFF;
        }
        
        processBlock();
        
        // Copy hash to output
        for (int i = 0; i < 5; i++) {
            hash[i * 4] = (h[i] >> 24) & 0xFF;
            hash[i * 4 + 1] = (h[i] >> 16) & 0xFF;
            hash[i * 4 + 2] = (h[i] >> 8) & 0xFF;
            hash[i * 4 + 3] = h[i] & 0xFF;
        }
    }
    
private:
    uint32_t h[5];
    uint8_t buffer[64];
    size_t bufferOffset;
    uint64_t length;
    
    uint32_t rotl(uint32_t n, unsigned int b) {
        return ((n << b) | (n >> (32 - b)));
    }
    
    void processBlock() {
        uint32_t w[80];
        
        // Copy block to w[0..15]
        for (int i = 0; i < 16; i++) {
            w[i] = (buffer[i * 4] << 24) | (buffer[i * 4 + 1] << 16) |
                   (buffer[i * 4 + 2] << 8) | buffer[i * 4 + 3];
        }
        
        // Extend w[16..79]
        for (int i = 16; i < 80; i++) {
            w[i] = rotl(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);
        }
        
        uint32_t a = h[0];
        uint32_t b = h[1];
        uint32_t c = h[2];
        uint32_t d = h[3];
        uint32_t e = h[4];
        
        for (int i = 0; i < 80; i++) {
            uint32_t f, k;
            
            if (i < 20) {
                f = (b & c) | ((~b) & d);
                k = 0x5A827999;
            } else if (i < 40) {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1;
            } else if (i < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDC;
            } else {
                f = b ^ c ^ d;
                k = 0xCA62C1D6;
            }
            
            uint32_t temp = rotl(a, 5) + f + e + k + w[i];
            e = d;
            d = c;
            c = rotl(b, 30);
            b = a;
            a = temp;
        }
        
        h[0] += a;
        h[1] += b;
        h[2] += c;
        h[3] += d;
        h[4] += e;
    }
};

// Helper function for WebSocket handshake
inline void sha1(const uint8_t* data, size_t len, uint8_t* hash) {
    SHA1 sha;
    sha.update(data, len);
    sha.finalize(hash);
}

#endif // SHA1_SIMPLE_H