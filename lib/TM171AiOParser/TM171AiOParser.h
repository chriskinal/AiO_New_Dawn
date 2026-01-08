// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

#ifndef TM171AIOPARSER_H_
#define TM171AIOPARSER_H_

#include <Arduino.h>

// TM171 parser specifically for AgOpenGPS - only processes RPY packets (0x23)
class TM171AiOParser
{
private:
    // State machine for packet parsing
    enum State
    {
        WAIT_HEADER1,
        WAIT_HEADER2,
        WAIT_SIZE,
        WAIT_PAYLOAD_INFO,
        COLLECT_DATA,
        PROCESS_PACKET
    };

    // Constants
    static const uint8_t HEADER1 = 0xAA;
    static const uint8_t HEADER2 = 0x55;
    static const uint8_t RPY_OBJECT_ID = 0x23;    // Roll/Pitch/Yaw object
    static const uint8_t RPY_PAYLOAD_SIZE = 0x14; // 20 bytes
    static const uint8_t MAX_PACKET_SIZE = 128;

    // Parser state
    State state;
    uint8_t buffer[MAX_PACKET_SIZE];
    uint8_t bufferIndex;
    uint8_t expectedSize;
    uint8_t payloadInfoBytes;

    // Parsed data
    float roll;
    float pitch;
    float yaw;
    uint32_t timestamp;
    bool dataValid;
    uint32_t lastValidTime;

    // Configuration
    bool negateRoll; // Fix for inverted roll axis

    // Private methods
    uint16_t calculateCRC(const uint8_t *data, uint8_t length);
    bool validateCRC();
    uint8_t extractObjectID();
    void parseRPYPacket();
    void resetParser();

public:
    TM171AiOParser();

    // No statistics - event-based logging only

    // Main interface
    void processByte(uint8_t byte);

    // Data access
    float getRoll() const { return negateRoll ? -roll : roll; }
    float getPitch() const { return pitch; }
    float getYaw() const { return yaw; }
    uint32_t getTimestamp() const { return timestamp; }
    bool isDataValid() const { return dataValid && (millis() - lastValidTime < 500); }
    uint32_t getTimeSinceLastValid() const { return millis() - lastValidTime; }

    // Configuration
    void setNegateRoll(bool negate) { negateRoll = negate; }

    // Debug
    void printStats();
    void printDebug();
};

#endif // TM171AIOPARSER_H_