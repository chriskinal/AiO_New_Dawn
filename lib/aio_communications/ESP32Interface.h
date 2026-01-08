// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

#ifndef ESP32INTERFACE_H
#define ESP32INTERFACE_H

#include <Arduino.h>
#include "EventLogger.h"

/**
 * ESP32Interface - Transparent serial-to-WiFi bridge for ESP32 module
 * 
 * Relays AOG PGN messages between UDP network and ESP32 via serial:
 * - UDP8888 packets are forwarded to ESP32 via serial
 * - ESP32 serial data is broadcast on UDP9999
 * - ESP32 announces presence with "ESP32-hello"
 */
class ESP32Interface {
private:    
    // Detection state
    bool esp32Detected = false;
    uint32_t lastHelloTime = 0;
    static constexpr uint32_t HELLO_TIMEOUT_MS = 10000;   // Consider disconnected after 10s (ESP32 sends every 5s)

    // Serial configuration handled by SerialManager (BAUD_ESP32 = 460800)

    // Receive buffer for serial data
    static constexpr size_t RX_BUFFER_SIZE = 512;
    uint8_t rxBuffer[RX_BUFFER_SIZE];
    size_t rxBufferIndex = 0;
    
    // Helper methods
    void processIncomingData();
    void checkForHello();
    
public:
    ESP32Interface() = default;
    ~ESP32Interface() = default;
    
    // Main interface
    void init();
    void process();  // Called from main loop
    
    // Send data to ESP32
    void sendToESP32(const uint8_t* data, size_t length);
    
    // Status
    bool isDetected() const { return esp32Detected; }
    void printStatus();
};

// Global instance
extern ESP32Interface esp32Interface;

#endif // ESP32INTERFACE_H