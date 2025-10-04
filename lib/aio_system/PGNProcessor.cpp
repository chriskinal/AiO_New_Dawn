#include "PGNProcessor.h"
#include "ConfigManager.h" // Full definition needed for method calls
#include "EventLogger.h"

// External declaration of the global object (defined in main.cpp)
extern ConfigManager configManager;

// External UDP instances from QNetworkBase
extern EthernetUDP udpSend;
extern EthernetUDP udpRecv;

// QNEthernet namespace
using namespace qindesign::network;

// Static instance pointer
PGNProcessor *PGNProcessor::instance = nullptr;

PGNProcessor::PGNProcessor()
{
    instance = this;
}

PGNProcessor::~PGNProcessor()
{
    instance = nullptr;
}

void PGNProcessor::init()
{
    if (instance == nullptr)
    {
        new PGNProcessor();
    }
}

// Remove static callback - QNEthernet uses a different approach

void PGNProcessor::processPGN(const uint8_t* data, size_t len, const IPAddress& remoteIP, uint16_t remotePort)
{
    if (!QNetworkBase::isConnected())
        return;

    if (remotePort == 9999 && len >= 5)
    {
        // Verify first 3 PGN header bytes
        if (data[0] != 128 || data[1] != 129 || data[2] != 127)
        {
            return;
        }

        // Validate CRC before processing
        // CRC is sum of bytes from index 2 to len-2 (skip header[0,1] and CRC byte)
        if (len >= 6) // Need at least header(3) + pgn(1) + length(1) + crc(1)
        {
            uint8_t pgn = data[3];
            
            // Special handling for AgIO PGNs (200, 201, 202) which use fixed CRC 0x47
            if (pgn == 200 || pgn == 201 || pgn == 202) {
                uint8_t receivedCRC = data[len - 1];
                if (receivedCRC != 0x47) {
                    LOG_WARNING(EventSource::NETWORK, "AgIO PGN %d invalid fixed CRC: expected 0x47, got %02X", 
                               pgn, receivedCRC);
                    return; // Drop packet with bad CRC
                }
                // AgIO packets validated - continue processing
            } else {
                // Normal CRC calculation for AgOpenGPS PGNs
                uint16_t crcSum = 0;
                for (size_t i = 2; i < len - 1; i++)
                {
                    crcSum += data[i];
                }
                uint8_t calculatedCRC = (uint8_t)(crcSum & 0xFF);
                uint8_t receivedCRC = data[len - 1];
                
                if (calculatedCRC != receivedCRC)
                {
                    LOG_WARNING(EventSource::NETWORK, "PGN %d CRC mismatch: calc=%02X, recv=%02X", 
                               pgn, calculatedCRC, receivedCRC);
                    return; // Drop packet with bad CRC
                }
            }
        }

        uint8_t pgn = data[3];
        
        // Update last received time for ANY valid PGN
        lastPGNReceivedTime = millis();
        
        // Debug: show registered callbacks for this PGN
        if (pgn == 200) {
            for (size_t i = 0; i < registrationCount; i++) {
                if (registrations[i].pgn == 200) {
                    LOG_DEBUG(EventSource::NETWORK, "Found callback: %s", registrations[i].name);
                }
            }
        }
        
        // Check if this is a broadcast PGN (Hello or Scan Request)
        bool isBroadcast = (pgn == 200 || pgn == 202);
        
        // For broadcast PGNs, call only broadcast callbacks
        if (isBroadcast)
        {
            
            const uint8_t* pgnData = &data[5];
            size_t dataLen = len - 6; // Subtract header(3) + pgn(1) + len(1) + crc(1)
            
            for (size_t i = 0; i < broadcastCount; i++)
            {
                broadcastCallbacks[i](pgn, pgnData, dataLen);
            }
        }
        else
        {
            // For non-broadcast PGNs, only call matching callbacks
            for (size_t i = 0; i < registrationCount; i++)
            {
                if (registrations[i].pgn == pgn)
                {
                    // Found a registered handler - call it
                    
                    
                    // Pass the data starting after the 5-byte header
                    // PGN 254 data starts at position 5: speed(2), status(1), steerAngle(2), etc.
                    const uint8_t* pgnData = &data[5];
                    size_t dataLen = len - 6; // Subtract header(5) + crc(1)
                    
                    // Log when debug is enabled, but skip PGN 254 as it comes too frequently (10Hz)
                    if (pgn != 254) {
                        LOG_DEBUG(EventSource::NETWORK, "Calling %s for PGN %d", registrations[i].name, pgn);
                    }
                    registrations[i].callback(pgn, pgnData, dataLen);
                    break; // Only one handler per non-broadcast PGN
                }
            }
        }
        
        // PGNProcessor only routes - no built-in handlers
        // Unhandled PGNs are simply dropped

    }
    // No need to delete buffer - QNEthernet handles memory management
}

// REMOVED ALL BUILT-IN HANDLERS
// PGNProcessor now ONLY routes to registered callbacks
// The following functions were removed:
// - processSubnetChange
// - processScanRequest  
// - processSteerConfig
// - processSteerSettings
// - processSteerData

void PGNProcessor::printPgnAnnouncement(uint8_t pgn, const char *pgnName, size_t dataLen)
{
    LOG_DEBUG(EventSource::NETWORK, "PGN 0x%02X(%d)-%s Length:%d", pgn, pgn, pgnName, dataLen);
}

bool PGNProcessor::registerCallback(uint8_t pgn, PGNCallback callback, const char* name)
{
    // Check if we have room for more registrations
    if (registrationCount >= MAX_REGISTRATIONS)
    {
        LOG_ERROR(EventSource::NETWORK, "Registration failed - max callbacks reached (%d)", MAX_REGISTRATIONS);
        return false;
    }
    
    // Check if this PGN is already registered
    for (size_t i = 0; i < registrationCount; i++)
    {
        if (registrations[i].pgn == pgn)
        {
            LOG_WARNING(EventSource::NETWORK, "PGN %d already registered to %s", pgn, registrations[i].name);
            return false;
        }
    }
    
    // Add the new registration
    registrations[registrationCount].pgn = pgn;
    registrations[registrationCount].callback = callback;
    registrations[registrationCount].name = name;
    registrationCount++;
    
    LOG_INFO(EventSource::NETWORK, "Registered callback for PGN %d (%s)", pgn, name);
    return true;
}

bool PGNProcessor::unregisterCallback(uint8_t pgn)
{
    // Find the registration
    for (size_t i = 0; i < registrationCount; i++)
    {
        if (registrations[i].pgn == pgn)
        {
            // Found it - remove by shifting remaining entries
            LOG_INFO(EventSource::NETWORK, "Unregistering callback for PGN %d (%s)", pgn, registrations[i].name);
            
            for (size_t j = i; j < registrationCount - 1; j++)
            {
                registrations[j] = registrations[j + 1];
            }
            registrationCount--;
            return true;
        }
    }
    
    LOG_WARNING(EventSource::SYSTEM, "PGN %d not found for unregistration", pgn);
    return false;
}

void PGNProcessor::listRegisteredCallbacks()
{
    LOG_INFO(EventSource::SYSTEM, "Registered callbacks (%d):", registrationCount);
    for (size_t i = 0; i < registrationCount; i++)
    {
        LOG_INFO(EventSource::SYSTEM, "  - PGN %d: %s", registrations[i].pgn, registrations[i].name);
    }
}

bool PGNProcessor::registerBroadcastCallback(PGNCallback callback, const char* name)
{
    // Check if we have room for more broadcast callbacks
    if (broadcastCount >= MAX_BROADCAST_CALLBACKS)
    {
        LOG_ERROR(EventSource::NETWORK, "Broadcast registration failed - max callbacks reached (%d)", MAX_BROADCAST_CALLBACKS);
        return false;
    }
    
    // Add the new broadcast callback
    broadcastCallbacks[broadcastCount] = callback;
    broadcastNames[broadcastCount] = name;
    broadcastCount++;
    
    LOG_INFO(EventSource::NETWORK, "Registered broadcast callback for %s (total: %d/%d)", name, broadcastCount, MAX_BROADCAST_CALLBACKS);
    return true;
}