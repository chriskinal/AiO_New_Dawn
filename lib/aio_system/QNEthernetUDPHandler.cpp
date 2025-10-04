// QNEthernetUDPHandler.cpp
// Implementation of UDP handling using QNEthernet's native EthernetUDP

#include "QNEthernetUDPHandler.h"
#include <QNEthernet.h>
#include "QNetworkBase.h"
#include "PGNProcessor.h"
#include "RTCMProcessor.h"
#include "EventLogger.h"
#include "DHCPLite.h"
#include "ConfigManager.h"
#include "ESP32Interface.h"

using namespace qindesign::network;

// Static member definitions
EthernetUDP QNEthernetUDPHandler::udpPGN;
EthernetUDP QNEthernetUDPHandler::udpRTCM;
EthernetUDP QNEthernetUDPHandler::udpDHCP;
EthernetUDP QNEthernetUDPHandler::udpSend;
bool QNEthernetUDPHandler::dhcpServerEnabled = false;
uint8_t QNEthernetUDPHandler::packetBuffer[512];

// External ConfigManager
extern ConfigManager configManager;

void QNEthernetUDPHandler::init() {
    LOG_INFO(EventSource::NETWORK, "Initializing QNEthernet UDP handlers");
    
    // Check Ethernet link status first
    if (!Ethernet.linkState()) {
        LOG_ERROR(EventSource::NETWORK, "No Ethernet link detected!");
        return;
    }
    
    // Log network configuration
    IPAddress localIP = Ethernet.localIP();
    LOG_INFO(EventSource::NETWORK, "Local IP: %d.%d.%d.%d", 
             localIP[0], localIP[1], localIP[2], localIP[3]);
    uint8_t destIP[4];
    configManager.getDestIP(destIP);
    LOG_INFO(EventSource::NETWORK, "Broadcast IP: %d.%d.%d.%d", 
             destIP[0], destIP[1], destIP[2], destIP[3]);
    LOG_INFO(EventSource::NETWORK, "Link Speed: %d Mbps, Full Duplex: %s", 
             Ethernet.linkSpeed(), Ethernet.linkIsFullDuplex() ? "Yes" : "No");
    
    // Set up PGN listener on port 8888 (AgIO sends PGNs to this port)
    if (udpPGN.begin(8888)) {
        LOG_INFO(EventSource::NETWORK, "UDP listening on port 8888 for PGN from AgIO");
    } else {
        LOG_ERROR(EventSource::NETWORK, "Failed to start UDP on port 8888");
    }
    
    // Add delay between UDP listeners to avoid conflicts
    delay(100);
    
    // Set up RTCM listener on port 2233
    if (udpRTCM.begin(2233)) {
        LOG_INFO(EventSource::NETWORK, "UDP listening on port 2233 for RTCM");
    } else {
        LOG_ERROR(EventSource::NETWORK, "Failed to start UDP on port 2233");
    }
    
    // Add delay between UDP listeners
    delay(100);
    
    // Initialize send socket (no specific port binding needed)
    if (udpSend.begin(0)) {  // 0 = let system choose port
        LOG_INFO(EventSource::NETWORK, "UDP send socket initialized");
    } else {
        LOG_ERROR(EventSource::NETWORK, "Failed to initialize UDP send socket");
    }
    
    // Enable DHCP server by default
    enableDHCPServer(true);
    
    LOG_INFO(EventSource::NETWORK, "QNEthernet UDP initialization complete");
}

void QNEthernetUDPHandler::poll() {
    static uint32_t lastStatusCheck = 0;
    static bool lastLinkStatus = false;
    static uint8_t pollCounter = 0;

    // Skip every other poll to reduce overhead
    pollCounter++;
    if (pollCounter & 1) return;
    
    // Check for incoming PGN packets
    int packetSize = udpPGN.parsePacket();
    if (packetSize > 0 && packetSize <= sizeof(packetBuffer)) {
        int bytesRead = udpPGN.read(packetBuffer, packetSize);
        if (bytesRead > 0) {
            // Process the packet
            handlePGNPacket(packetBuffer, bytesRead, udpPGN.remoteIP(), udpPGN.remotePort());
        }
    }
    
    // Check for incoming RTCM packets
    packetSize = udpRTCM.parsePacket();
    if (packetSize > 0 && packetSize <= sizeof(packetBuffer)) {
        int bytesRead = udpRTCM.read(packetBuffer, packetSize);
        if (bytesRead > 0) {
            handleRTCMPacket(packetBuffer, bytesRead, udpRTCM.remoteIP(), udpRTCM.remotePort());
        }
    }
    
    // Check for incoming DHCP packets if server is enabled
    if (dhcpServerEnabled) {
        packetSize = udpDHCP.parsePacket();
        if (packetSize > 0 && packetSize <= sizeof(packetBuffer)) {
            int bytesRead = udpDHCP.read(packetBuffer, packetSize);
            if (bytesRead > 0) {
                handleDHCPPacket(packetBuffer, bytesRead, udpDHCP.remoteIP(), udpDHCP.remotePort());
            }
        }
    }
    
    // Check link status every 5 seconds
    if (millis() - lastStatusCheck > 5000) {
        lastStatusCheck = millis();
        
        bool currentLinkStatus = Ethernet.linkState();
        
        // Log if link status changed
        if (currentLinkStatus != lastLinkStatus) {
            lastLinkStatus = currentLinkStatus;
            
            if (currentLinkStatus) {
                IPAddress localIP = Ethernet.localIP();
                LOG_INFO(EventSource::NETWORK, "Ethernet link UP - IP: %d.%d.%d.%d, Speed: %d Mbps", 
                         localIP[0], localIP[1], localIP[2], localIP[3], Ethernet.linkSpeed());
            } else {
                LOG_ERROR(EventSource::NETWORK, "Ethernet link DOWN!");
            }
        }
        
        // If link is up, log periodic status
        if (currentLinkStatus) {
            static uint32_t statusCount = 0;
            statusCount++;
            
            // Every 12th check (60 seconds), log detailed status
            if (statusCount % 12 == 0) {
                IPAddress localIP = Ethernet.localIP();
                LOG_INFO(EventSource::NETWORK, "Network status: IP=%d.%d.%d.%d, Link=%d Mbps, FullDuplex=%s", 
                         localIP[0], localIP[1], localIP[2], localIP[3], 
                         Ethernet.linkSpeed(), 
                         Ethernet.linkIsFullDuplex() ? "Yes" : "No");
            }
        }
    }
}

void QNEthernetUDPHandler::handlePGNPacket(const uint8_t* data, size_t len, 
                                           const IPAddress& remoteIP, uint16_t remotePort) {
    // Process PGN packet
    
    // Forward to ESP32 if detected
    if (esp32Interface.isDetected()) {
        esp32Interface.sendToESP32(data, len);
    }
    
    // Process the packet normally
    if (len > 0 && PGNProcessor::instance) {
        PGNProcessor::instance->processPGN(data, len, remoteIP, remotePort);
    }
}

void QNEthernetUDPHandler::handleRTCMPacket(const uint8_t* data, size_t len,
                                            const IPAddress& remoteIP, uint16_t remotePort) {
    // Process RTCM packet
    if (len > 0 && RTCMProcessor::instance) {
        RTCMProcessor::instance->processRTCM(data, len, remoteIP, remotePort);
    }
}

void QNEthernetUDPHandler::sendUDPPacket(uint8_t* data, int length) {
    // Check Ethernet link status
    if (!Ethernet.linkState()) {
        LOG_ERROR(EventSource::NETWORK, "Cannot send UDP - no Ethernet link");
        return;
    }
    
    // Use the broadcast address from ConfigManager
    uint8_t destIP[4];
    configManager.getDestIP(destIP);
    IPAddress broadcastIP(destIP[0], destIP[1], destIP[2], destIP[3]);
    
    // Send packet
    udpSend.beginPacket(broadcastIP, configManager.getDestPort());
    udpSend.write(data, length);
    if (!udpSend.endPacket()) {
        LOG_ERROR(EventSource::NETWORK, "Failed to send UDP packet");
    }
}

// Global function to replace sendUDPbytes
void sendUDPbytes(uint8_t* data, int length) {
    QNEthernetUDPHandler::sendUDPPacket(data, length);
}

// Send packet on port 9999 (for ESP32 bridge)
void QNEthernetUDPHandler::sendUDP9999Packet(uint8_t* data, int length) {
    // Check Ethernet link status
    if (!Ethernet.linkState()) {
        LOG_ERROR(EventSource::NETWORK, "Cannot send UDP9999 - no Ethernet link");
        return;
    }
    
    // Use the broadcast address from ConfigManager
    uint8_t destIP[4];
    configManager.getDestIP(destIP);
    IPAddress broadcastIP(destIP[0], destIP[1], destIP[2], destIP[3]);
    
    // Send packet on port 9999
    udpSend.beginPacket(broadcastIP, 9999);
    udpSend.write(data, length);
    if (!udpSend.endPacket()) {
        LOG_ERROR(EventSource::NETWORK, "Failed to send UDP9999 packet");
    }
}

void QNEthernetUDPHandler::enableDHCPServer(bool enable) {
    if (enable && !dhcpServerEnabled) {
        // Start DHCP server on port 67
        if (udpDHCP.begin(DHCP_SERVER_PORT)) {
            LOG_INFO(EventSource::NETWORK, "DHCP server started on port 67");
            LOG_INFO(EventSource::NETWORK, "DHCP range: 192.168.5.1 - 192.168.5.125");
            dhcpServerEnabled = true;
        } else {
            LOG_ERROR(EventSource::NETWORK, "Failed to start DHCP server on port 67");
        }
    } else if (!enable && dhcpServerEnabled) {
        // Stop DHCP server
        udpDHCP.stop();
        dhcpServerEnabled = false;
        LOG_INFO(EventSource::NETWORK, "DHCP server stopped");
    }
}

bool QNEthernetUDPHandler::isDHCPServerEnabled() {
    return dhcpServerEnabled;
}

void QNEthernetUDPHandler::handleDHCPPacket(const uint8_t* data, size_t len,
                                            const IPAddress& remoteIP, uint16_t remotePort) {
    if (len < sizeof(RIP_MSG)) {
        return;  // Packet too small
    }
    
    // Get our server IP
    IPAddress serverIP = Ethernet.localIP();
    byte serverIPBytes[4] = {serverIP[0], serverIP[1], serverIP[2], serverIP[3]};
    
    // Process DHCP request
    RIP_MSG* dhcpMsg = (RIP_MSG*)data;
    int replySize = DHCPreply(dhcpMsg, len, serverIPBytes, NULL);
    
    if (replySize > 0) {
        // Send DHCP reply to broadcast address on client port
        IPAddress broadcastIP(255, 255, 255, 255);
        udpDHCP.beginPacket(broadcastIP, DHCP_CLIENT_PORT);
        udpDHCP.write((uint8_t*)dhcpMsg, replySize);
        udpDHCP.endPacket();
        
        // Log DHCP activity
        static uint32_t lastDHCPLog = 0;
        if (millis() - lastDHCPLog > 1000) {  // Rate limit logging
            lastDHCPLog = millis();
            LOG_DEBUG(EventSource::NETWORK, "DHCP request processed from %d.%d.%d.%d", 
                      remoteIP[0], remoteIP[1], remoteIP[2], remoteIP[3]);
        }
    }
}