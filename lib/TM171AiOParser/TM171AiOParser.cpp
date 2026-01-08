// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

#include "TM171AiOParser.h"
#include "EventLogger.h"

TM171AiOParser::TM171AiOParser()
    : state(WAIT_HEADER1), bufferIndex(0), expectedSize(0), payloadInfoBytes(0),
      roll(0), pitch(0), yaw(0), timestamp(0), dataValid(false), lastValidTime(0),
      negateRoll(true) // Default to negating roll based on previous issue
{
}

void TM171AiOParser::processByte(uint8_t byte)
{
    switch (state)
    {
    case WAIT_HEADER1:
        if (byte == HEADER1)
        {
            buffer[0] = byte;
            bufferIndex = 1;
            state = WAIT_HEADER2;
        }
        break;

    case WAIT_HEADER2:
        if (byte == HEADER2)
        {
            buffer[1] = byte;
            bufferIndex = 2;
            state = WAIT_SIZE;
        }
        else
        {
            resetParser();
        }
        break;

    case WAIT_SIZE:
        buffer[bufferIndex++] = byte;
        expectedSize = byte;

        // We're only interested in RPY packets (20 byte payload)
        if (expectedSize == RPY_PAYLOAD_SIZE)
        {
            payloadInfoBytes = 0;
            state = WAIT_PAYLOAD_INFO;
        }
        else
        {
            // Not RPY packet size, but collect it anyway for stats
            state = COLLECT_DATA;
        }
        break;

    case WAIT_PAYLOAD_INFO:
        buffer[bufferIndex++] = byte;
        payloadInfoBytes++;

        if (payloadInfoBytes >= 4)
        {
            // Got all 4 payload info bytes, now collect the rest
            state = COLLECT_DATA;
        }
        break;

    case COLLECT_DATA:
        buffer[bufferIndex++] = byte;

        // Check if we have complete packet (header + size + payload + crc)
        if (bufferIndex >= 3 + expectedSize + 2)
        {
            state = PROCESS_PACKET;
            // Don't break, fall through to process
        }
        else
        {
            break;
        }

    case PROCESS_PACKET:
        // Packet received

        // Validate CRC first
        if (!validateCRC())
        {
            LOG_WARNING(EventSource::IMU, "TM171 CRC error");
            resetParser();
            break;
        }

        // Check object ID
        uint8_t objectID = extractObjectID();

        if (objectID == RPY_OBJECT_ID && expectedSize == RPY_PAYLOAD_SIZE)
        {
            // This is an RPY packet, parse it
            parseRPYPacket();
            // RPY packet processed
        }
        else
        {
            // Some other packet type
            // Non-RPY packet ignored
        }

        resetParser();
        break;
    }
}

uint16_t TM171AiOParser::calculateCRC(const uint8_t *data, uint8_t length)
{
    // Modbus-style 16-bit CRC as specified in TM171 documentation
    uint16_t crc = 0xFFFF;

    for (uint8_t i = 0; i < length; i++)
    {
        crc ^= (uint16_t)data[i];

        for (uint8_t j = 0; j < 8; j++)
        {
            if (crc & 0x0001)
            {
                crc >>= 1;
                crc ^= 0xA001;
            }
            else
            {
                crc >>= 1;
            }
        }
    }

    return crc;
}

bool TM171AiOParser::validateCRC()
{
    if (bufferIndex < 5)
        return false; // Minimum packet size

    // CRC is calculated over all bytes EXCEPT:
    // - The 2-byte header (0xAA 0x55)
    // - The 2-byte CRC at the end
    // So we start from index 2 (after header) and go to bufferIndex-2
    uint8_t crcStart = 2;                // Skip header
    uint8_t crcLength = bufferIndex - 4; // Total length minus header(2) and CRC(2)

    uint16_t calculatedCRC = calculateCRC(&buffer[crcStart], crcLength);

    // Extract received CRC (little-endian)
    uint16_t receivedCRC = buffer[bufferIndex - 2] | (buffer[bufferIndex - 1] << 8);

    return (calculatedCRC == receivedCRC);
}

uint8_t TM171AiOParser::extractObjectID()
{
    // Object ID is in the first 7 bits of the payload info field
    // Payload info starts at buffer[3]
    return buffer[3] & 0x7F;
}

void TM171AiOParser::parseRPYPacket()
{
    // RPY packet structure after header and size:
    // [PayloadInfo(4)][Timestamp(4)][Roll(4)][Pitch(4)][Yaw(4)][CRC(2)]

    // Timestamp starts at index 7 (after header[2] + size[1] + payload_info[4])
    memcpy(&timestamp, &buffer[7], sizeof(uint32_t));

    // Roll at index 11
    float rollValue;
    memcpy(&rollValue, &buffer[11], sizeof(float));

    // Pitch at index 15
    float pitchValue;
    memcpy(&pitchValue, &buffer[15], sizeof(float));

    // Yaw at index 19
    float yawValue;
    memcpy(&yawValue, &buffer[19], sizeof(float));

    // Update values
    roll = rollValue;
    pitch = pitchValue;
    yaw = yawValue;

    dataValid = true;
    lastValidTime = millis();
}

void TM171AiOParser::resetParser()
{
    state = WAIT_HEADER1;
    bufferIndex = 0;
    expectedSize = 0;
    payloadInfoBytes = 0;
}

void TM171AiOParser::printStats()
{
    // No statistics - event-based logging only
    LOG_INFO(EventSource::IMU, "TM171 Parser - No statistics available (event-based logging)");
}

void TM171AiOParser::printDebug()
{
    LOG_DEBUG(EventSource::IMU, "=== TM171 Parser Debug ===");
    LOG_DEBUG(EventSource::IMU, "State: %d", state);
    LOG_DEBUG(EventSource::IMU, "Buffer Index: %d", bufferIndex);
    LOG_DEBUG(EventSource::IMU, "Expected Size: %d", expectedSize);

    // No statistics to print

    if (dataValid)
    {
        LOG_DEBUG(EventSource::IMU, "Last Valid Data:");
        LOG_DEBUG(EventSource::IMU, "  Timestamp: %lu µs", timestamp);
        LOG_DEBUG(EventSource::IMU, "  Roll: %.2f°%s", getRoll(), negateRoll ? " (negated)" : "");
        LOG_DEBUG(EventSource::IMU, "  Pitch: %.2f°", pitch);
        LOG_DEBUG(EventSource::IMU, "  Yaw: %.2f°", yaw);
        LOG_DEBUG(EventSource::IMU, "  Age: %lu ms", getTimeSinceLastValid());
    }
    else
    {
        LOG_DEBUG(EventSource::IMU, "No valid data");
    }

    LOG_DEBUG(EventSource::IMU, "========================");
}