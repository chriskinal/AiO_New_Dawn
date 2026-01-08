// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

#include "BNOAiOParser.h"
#include "EventLogger.h"

BNOAiOParser::BNOAiOParser() : 
    state(WAIT_HEADER1),
    bufferIndex(0),
    yawX10(0),
    pitchX10(0),
    rollX10(0),
    yawX100(0),
    prevYaw(0),
    angVel(0),
    angCounter(0),
    lastValidTime(0),
    dataValid(false),
    isSwapXY(false)
{
}

void BNOAiOParser::processByte(uint8_t byte)
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
            state = COLLECT_DATA;
        }
        else
        {
            // Not a valid header sequence, reset
            state = WAIT_HEADER1;
        }
        break;
        
    case COLLECT_DATA:
        buffer[bufferIndex++] = byte;
        
        // Check if we have all data bytes (18 total including headers)
        if (bufferIndex >= DATA_SIZE)
        {
            state = WAIT_CHECKSUM;
        }
        break;
        
    case WAIT_CHECKSUM:
        buffer[bufferIndex] = byte;
        
        // Validate checksum and parse if valid
        if (validateChecksum())
        {
            parsePacket();
            dataValid = true;
            lastValidTime = millis();
        }
        else
        {
            // Checksum failed - could be noise, don't log every failure
            // LOG_WARNING(EventSource::IMU, "BNO checksum failed");
        }
        
        // Reset for next packet
        resetParser();
        break;
    }
    
    // Safety check - reset if buffer overrun
    if (bufferIndex >= PACKET_SIZE)
    {
        resetParser();
    }
}

void BNOAiOParser::resetParser()
{
    state = WAIT_HEADER1;
    bufferIndex = 0;
}

bool BNOAiOParser::validateChecksum()
{
    // Calculate checksum on payload bytes (after headers, before checksum)
    // BNO RVC format: AA AA [15 bytes of data] [checksum]
    // We sum bytes 2 through 16 (0-indexed)
    uint8_t sum = 0;
    for (uint8_t i = 2; i < 18; i++)  // Sum 16 bytes starting from index 2
    {
        sum += buffer[i];
    }
    
    // Compare with received checksum at position 18
    return (sum == buffer[18]);
}

void BNOAiOParser::parsePacket()
{
    // Extract raw values from buffer
    // Buffer layout after headers:
    // [2] = Index (not used)
    // [3-4] = Yaw (little endian)
    // [5-6] = Pitch (little endian)
    // [7-8] = Roll (little endian)
    // Rest of packet not used for basic RVC mode
    
    int16_t rawYaw = buffer[3] | (buffer[4] << 8);
    int16_t rawPitch = buffer[5] | (buffer[6] << 8);
    int16_t rawRoll = buffer[7] | (buffer[8] << 8);
    
    // Store raw yaw for angular velocity calculation
    yawX100 = rawYaw;
    
    // Calculate angular velocity (same as original BNO_RVC)
    if (angCounter < 20)
    {
        angVel += (rawYaw - prevYaw);
        angCounter++;
        prevYaw = rawYaw;
    }
    else
    {
        angCounter = 0;
        prevYaw = 0;
        angVel = 0;
    }
    
    // Convert to degrees x 10 (original values are in 0.01 degree units)
    // So divide by 10 to get 0.1 degree units
    yawX10 = (int16_t)(rawYaw * 0.1f);
    if (yawX10 < 0) yawX10 += 3600;  // Ensure positive 0-3600 range
    
    pitchX10 = (int16_t)(rawPitch * 0.1f);
    rollX10 = (int16_t)(rawRoll * 0.1f);
    
    // Handle axis swap if configured
    if (isSwapXY)
    {
        int16_t temp = pitchX10;
        pitchX10 = rollX10;
        rollX10 = temp;
    }
}

void BNOAiOParser::printDebug()
{
    LOG_DEBUG(EventSource::IMU, "BNO: yaw=%.1f° pitch=%.1f° roll=%.1f° rate=%.1f°/s valid=%d",
              getYaw(), getPitch(), getRoll(), getYawRate(), isDataValid());
}