// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

#ifndef PGNProcessor_H_
#define PGNProcessor_H_

#include "Arduino.h"
#include "QNetworkBase.h"
#include <QNEthernet.h>
#include <QNEthernetUDP.h>

// QNEthernet namespace
using namespace qindesign::network;

// Define callback function type for PGN handlers
// Parameters: PGN number, data buffer, data length
typedef void (*PGNCallback)(uint8_t pgn, const uint8_t* data, size_t len);

// Structure to hold PGN registration info
struct PGNRegistration {
    uint8_t pgn;
    PGNCallback callback;
    const char* name;  // For debugging
};

class PGNProcessor
{
public:
    static PGNProcessor *instance;
    
private:
    // Array to store registered callbacks (max 20 registrations)
    static constexpr size_t MAX_REGISTRATIONS = 20;
    PGNRegistration registrations[MAX_REGISTRATIONS];
    size_t registrationCount = 0;
    
    // Separate array for broadcast callbacks (PGN 200, 202)
    static constexpr size_t MAX_BROADCAST_CALLBACKS = 4; // GPS, IMU, Steer, Machine
    PGNCallback broadcastCallbacks[MAX_BROADCAST_CALLBACKS];
    const char* broadcastNames[MAX_BROADCAST_CALLBACKS];
    size_t broadcastCount = 0;
    
    // Track last time ANY PGN was received from AgIO
    uint32_t lastPGNReceivedTime = 0;

public:
    PGNProcessor();
    ~PGNProcessor();

    // Process incoming PGN data
    void processPGN(const uint8_t* data, size_t len, const IPAddress& remoteIP, uint16_t remotePort);

    // Utility methods
    void printPgnAnnouncement(uint8_t pgn, const char *pgnName, size_t dataLen);

    // PGN processing methods - REMOVED
    // PGNProcessor only routes to registered callbacks

    // Initialize the handler
    static void init();
    
    // Callback registration methods
    bool registerCallback(uint8_t pgn, PGNCallback callback, const char* name);
    bool unregisterCallback(uint8_t pgn);
    void listRegisteredCallbacks();  // For debugging
    
    // Broadcast callback registration (for PGN 200, 202)
    bool registerBroadcastCallback(PGNCallback callback, const char* name);
    
    // Get last time any PGN was received
    uint32_t getLastPGNReceivedTime() const { return lastPGNReceivedTime; }
    
    // Check if we're receiving PGNs from AgIO
    bool isReceivingFromAgIO() const { 
        return (millis() - lastPGNReceivedTime) < 5000; 
    }
};

#endif // PGNProcessor_H_