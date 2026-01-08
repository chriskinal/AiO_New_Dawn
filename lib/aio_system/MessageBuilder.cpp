// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

// MessageBuilder.cpp - Implementation of AOG hardware popup message builder
#include "MessageBuilder.h"
#include "EventLogger.h"

// External UDP send function from main
extern void sendUDPbytes(uint8_t* data, int len);

void MessageBuilder::sendHardwarePopup(const char* message,
                                        uint8_t durationSeconds,
                                        uint8_t color) {
    if (!message) {
        LOG_ERROR(EventSource::SYSTEM, "MessageBuilder: null message pointer");
        return;
    }

    // Validate message length
    size_t msgLen = strlen(message);
    if (msgLen == 0) {
        LOG_ERROR(EventSource::SYSTEM, "MessageBuilder: empty message");
        return;
    }
    if (msgLen > MAX_MESSAGE_LENGTH) {
        LOG_WARNING(EventSource::SYSTEM, "MessageBuilder: message truncated from %d to %d chars",
                    msgLen, MAX_MESSAGE_LENGTH);
        msgLen = MAX_MESSAGE_LENGTH;
    }

    // Build message buffer
    // Format: [0x80, 0x81, Src, PGN, Len, Duration, Color, ...Message..., CRC]
    uint8_t buffer[128];
    size_t idx = 0;

    // Header
    buffer[idx++] = PREAMBLE_1;       // 0x80
    buffer[idx++] = PREAMBLE_2;       // 0x81
    buffer[idx++] = SRC_STEER;        // 0x7E (Steer module)
    buffer[idx++] = PGN_HARDWARE_MESSAGE;  // 0xDD (221)

    // Data length (duration + color + message bytes)
    buffer[idx++] = msgLen + 2;       // +2 for duration and color bytes

    // Payload
    buffer[idx++] = durationSeconds;  // How long to display
    buffer[idx++] = color;            // 0=info, 1=warning

    // Copy message bytes
    memcpy(&buffer[idx], message, msgLen);
    idx += msgLen;

    // Calculate checksum (sum of bytes 2 through message end)
    uint8_t checksum = calculateChecksum(buffer, 2, idx);
    buffer[idx++] = checksum;

    // Total message length
    size_t totalLength = idx;

    // Send via UDP
    sendUDPbytes(buffer, totalLength);

    LOG_DEBUG(EventSource::SYSTEM, "Hardware popup sent (%d bytes): %s", totalLength, message);

    // Optional: Dump message bytes for debugging
    #ifdef DEBUG_HARDWARE_MESSAGES
    Serial.println("Hardware Message Dump:");
    for (size_t i = 0; i < totalLength; i++) {
        if (i % 16 == 0) Serial.print("\n  ");
        Serial.printf("%02X ", buffer[i]);
    }
    Serial.println();
    #endif
}

uint8_t MessageBuilder::calculateChecksum(const uint8_t* data, size_t start, size_t end) {
    int16_t sum = 0;
    for (size_t i = start; i < end; i++) {
        sum += data[i];
    }
    return (uint8_t)(sum & 0xFF);
}
