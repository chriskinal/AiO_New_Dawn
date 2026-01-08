// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

// QNEthernetUDPHandler.h
// UDP handler using QNEthernet's native EthernetUDP
// Replaces AsyncUDPHandler with native QNEthernet implementation

#ifndef QNETHERNETUDPHANDLER_H
#define QNETHERNETUDPHANDLER_H

#include <stdint.h>
#include <QNEthernet.h>
#include <QNEthernetUDP.h>

class QNEthernetUDPHandler {
public:
    static void init();
    static void sendUDPPacket(uint8_t* data, int length);
    static void poll();  // Check for incoming packets and network status
    
    // DHCP Server control
    static void enableDHCPServer(bool enable);
    static bool isDHCPServerEnabled();
    
    // ESP32 bridge support
    static void sendUDP9999Packet(uint8_t* data, int length);
    
private:
    static qindesign::network::EthernetUDP udpPGN;   // For PGN traffic on port 8888
    static qindesign::network::EthernetUDP udpRTCM;  // For RTCM traffic on port 2233
    static qindesign::network::EthernetUDP udpDHCP;  // For DHCP server on port 67
    static qindesign::network::EthernetUDP udpSend;  // For sending packets
    
    static bool dhcpServerEnabled;
    static uint8_t packetBuffer[512];  // Buffer for receiving packets
    
    // Packet handlers
    static void handlePGNPacket(const uint8_t* data, size_t len, 
                               const IPAddress& remoteIP, uint16_t remotePort);
    static void handleRTCMPacket(const uint8_t* data, size_t len,
                                const IPAddress& remoteIP, uint16_t remotePort);
    static void handleDHCPPacket(const uint8_t* data, size_t len,
                                const IPAddress& remoteIP, uint16_t remotePort);
};

// Global function to maintain compatibility
void sendUDPbytes(uint8_t* data, int length);

#endif // QNETHERNETUDPHANDLER_H