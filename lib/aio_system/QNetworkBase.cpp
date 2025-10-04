// QNetworkBase.cpp
// Network implementation using QNEthernet

#include "QNetworkBase.h"
#include <QNEthernet.h>
#include <EEPROM.h>
#include "EEPROMLayout.h"
#include "EventLogger.h"
#include "PGNProcessor.h"
#include "ConfigManager.h"

using namespace qindesign::network;

// Global network configuration removed - now using ConfigManager

// Static member definition
volatile bool QNetworkBase::linkState = false;

// Save network configuration
void save_current_net() {
    // Save directly through ConfigManager
    ConfigManager* config = ConfigManager::getInstance();
    config->saveNetworkConfig();
    
    uint8_t ip[4];
    config->getIPAddress(ip);
    LOG_INFO(EventSource::CONFIG, "Network configuration saved - IP: %d.%d.%d.%d",
             ip[0], ip[1], ip[2], ip[3]);
}

// Load network configuration from EEPROM
bool load_network_config() {
    // Network config is now loaded automatically by ConfigManager
    ConfigManager* config = ConfigManager::getInstance();
    
    uint8_t ip[4];
    config->getIPAddress(ip);
    LOG_INFO(EventSource::CONFIG, "Network configuration loaded - IP: %d.%d.%d.%d",
             ip[0], ip[1], ip[2], ip[3]);
    return true;
}

// Initialize network stack
void QNetworkBase::init() {
    // Register link state callback BEFORE begin()
    Ethernet.onLinkState(onLinkStateChanged);
    
    // Set MAC address (required for Teensy)
    uint8_t mac[6];
    Ethernet.macAddress(mac);  // Get the built-in MAC
    LOG_INFO(EventSource::NETWORK, "MAC Address: %02X:%02X:%02X:%02X:%02X:%02X", 
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    
    // Load saved network config or use defaults
    load_network_config();
    
    // Configure static IP from ConfigManager
    ConfigManager* config = ConfigManager::getInstance();
    uint8_t ipBytes[4], subnetBytes[4], gatewayBytes[4];
    config->getIPAddress(ipBytes);
    config->getSubnet(subnetBytes);
    config->getGateway(gatewayBytes);
    
    IPAddress ip(ipBytes[0], ipBytes[1], ipBytes[2], ipBytes[3]);
    IPAddress subnet(subnetBytes[0], subnetBytes[1], subnetBytes[2], subnetBytes[3]);
    IPAddress gateway(gatewayBytes[0], gatewayBytes[1], gatewayBytes[2], gatewayBytes[3]);
    
    // Start Ethernet with static IP
    if (!Ethernet.begin(ip, subnet, gateway)) {
        LOG_ERROR(EventSource::NETWORK, "Failed to start Ethernet!");
        return;
    }
    
    // Wait for link
    LOG_INFO(EventSource::NETWORK, "Waiting for Ethernet link...");
    if (!Ethernet.waitForLink(5000)) {  // 5 second timeout
        LOG_WARNING(EventSource::NETWORK, "Link timeout, continuing anyway");
    }
    
    if (Ethernet.linkStatus()) {
        LOG_INFO(EventSource::NETWORK, "Link UP! IP: %d.%d.%d.%d, Speed: %d Mbps, Full Duplex: %s", 
                 ip[0], ip[1], ip[2], ip[3], 
                 Ethernet.linkSpeed(),
                 Ethernet.linkIsFullDuplex() ? "Yes" : "No");
    } else {
        LOG_ERROR(EventSource::NETWORK, "No Ethernet link detected!");
    }
    
    // Register PGN 201 handler for subnet changes
    if (PGNProcessor::instance) {
        PGNProcessor::instance->registerCallback(201, handlePGN201, "QNetworkBase");
        LOG_INFO(EventSource::NETWORK, "Registered PGN 201 handler for subnet changes");
    }
}

// Initialize UDP services - handled by AsyncUDPHandler
void QNetworkBase::udpSetup() {
    // UDP setup is now handled by AsyncUDPHandler::init()
}

// Handle PGN 201 - Subnet change request
void QNetworkBase::handlePGN201(uint8_t pgn, const uint8_t* data, size_t len) {
    // PGN 201 format after header removal:
    // [0] = 201 (magic byte)
    // [1] = 201 (magic byte)
    // [2] = new subnet octet 1
    // [3] = new subnet octet 2
    // [4] = new subnet octet 3
    
    if (pgn != 201) {
        return;
    }

    // Verify packet length (need at least 5 bytes after header)
    if (len < 5) {
        LOG_ERROR(EventSource::NETWORK, "PGN 201 packet too short: %d bytes", len);
        return;
    }
    
    // Check magic bytes for safety
    if (data[0] != 201 || data[1] != 201) {
        LOG_ERROR(EventSource::NETWORK, "PGN 201 invalid magic bytes: %d,%d", 
                  data[0], data[1]);
        return;
    }
    
    // Extract new subnet
    uint8_t newSubnet[3] = { data[2], data[3], data[4] };
    
    // Get current IP from ConfigManager
    ConfigManager* config = ConfigManager::getInstance();
    uint8_t currentIP[4];
    config->getIPAddress(currentIP);
    
    // Check if subnet actually changed
    if (currentIP[0] == newSubnet[0] && 
        currentIP[1] == newSubnet[1] && 
        currentIP[2] == newSubnet[2]) {
        LOG_INFO(EventSource::NETWORK, "Subnet unchanged (%d.%d.%d.x), ignoring PGN 201",
                 newSubnet[0], newSubnet[1], newSubnet[2]);
        return;
    }
    
    LOG_INFO(EventSource::NETWORK, "IP change requested via PGN 201: %d.%d.%d.%d -> %d.%d.%d.%d", 
             currentIP[0], currentIP[1], currentIP[2], currentIP[3],
             newSubnet[0], newSubnet[1], newSubnet[2], currentIP[3]);
    
    // Update IP address (keep last octet)
    uint8_t newIP[4] = {newSubnet[0], newSubnet[1], newSubnet[2], currentIP[3]};
    config->setIPAddress(newIP);
    
    // Update gateway to .1
    uint8_t newGateway[4] = {newSubnet[0], newSubnet[1], newSubnet[2], 1};
    config->setGateway(newGateway);
    
    // Update destination IP to .255
    uint8_t newDest[4] = {newSubnet[0], newSubnet[1], newSubnet[2], 255};
    config->setDestIP(newDest);
    
    LOG_WARNING(EventSource::NETWORK, "Saving network config to EEPROM and rebooting...");
    
    // Save to EEPROM
    config->saveNetworkConfig();
    delay(20);
    SCB_AIRCR = 0x05FA0004; // Teensy Reset
}