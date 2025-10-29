// MessageBuilder.h - Utility for building AOG hardware popup messages
#ifndef MESSAGE_BUILDER_H
#define MESSAGE_BUILDER_H

#include <Arduino.h>

/**
 * MessageBuilder - Utility class for sending hardware popup messages to AgOpenGPS
 *
 * Handles construction and transmission of PGN 221 (Hardware Message) which displays
 * popup notifications in the AgOpenGPS interface.
 *
 * Message Format:
 * [0x80, 0x81, Src, PGN, Len, Duration, Color, ...Message..., CRC]
 *
 * PGN 221 (0xDD) - Hardware Message
 * Byte 5: Duration in seconds (how long to display)
 * Byte 6: Color (0=info/white, 1=warning/red)
 * Byte 7+: UTF-8 message string
 * Last byte: CRC (sum of bytes 2 through message end)
 */
class MessageBuilder {
public:
    /**
     * Send a hardware popup message to AgOpenGPS
     *
     * @param message Text to display (max ~110 chars)
     * @param durationSeconds How long to show popup (default 5 seconds)
     * @param color 0=info/white, 1=warning/red (default 0)
     */
    static void sendHardwarePopup(const char* message,
                                   uint8_t durationSeconds = 5,
                                   uint8_t color = 0);

private:
    // PGN and message constants
    static const uint8_t PGN_HARDWARE_MESSAGE = 221;  // 0xDD
    static const uint8_t SRC_STEER = 0x7E;            // Steer module source
    static const uint8_t PREAMBLE_1 = 0x80;
    static const uint8_t PREAMBLE_2 = 0x81;
    static const size_t MAX_MESSAGE_LENGTH = 110;     // Safe max for 128 byte buffer

    /**
     * Calculate checksum for AOG message
     * Sum of all bytes from index 2 (Src) through data end
     */
    static uint8_t calculateChecksum(const uint8_t* data, size_t start, size_t end);
};

#endif // MESSAGE_BUILDER_H
