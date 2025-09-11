#include "GNSSProcessor.h"
#include "UBXParser.h"
#include "calc_crc32.h"
#include "PGNUtils.h"
#include "EventLogger.h"
#include "QNetworkBase.h"
#include "ConfigManager.h"
#include <string.h>
#include <math.h>


GNSSProcessor::GNSSProcessor() : bufferIndex(0),
                                 state(WAIT_START),
                                 calculatedChecksum(0),
                                 receivedChecksum(0),
                                 checksumIndex(0),
                                 fieldCount(0),
                                 enableNoiseFilter(true),
                                 enableDebug(false),
                                 ubxParser(nullptr),
                                 udpPassthroughEnabled(false),
                                 processingPaused(false),
                                 lastGGALatitude(0.0),
                                 lastGGALongitude(0.0)
{

    // Initialize data structures
    memset(&gpsData, 0, sizeof(gpsData));
    // Initialize data

    gpsData.hdop = 99.9f;
    gpsData.fixTimeFractional = 0.0f;
    
    // Initialize NMEA coordinate cache with valid defaults
    gpsData.latitudeNMEA = 0.0;
    gpsData.longitudeNMEA = 0.0;
    gpsData.latDir = 'N';  // Default to North
    gpsData.lonDir = 'W';  // Default to West
    resetParser();
    
    // Initialize UBX parser
    ubxParser = new UBX_Parser();
}

GNSSProcessor::~GNSSProcessor()
{
    // Cleanup UBX parser
    if (ubxParser) {
        delete ubxParser;
        ubxParser = nullptr;
    }
}

bool GNSSProcessor::init()
{
    resetParser();
    
    // Load UDP passthrough setting from ConfigManager
    extern ConfigManager configManager;
    udpPassthroughEnabled = configManager.getGPSPassThrough();
    LOG_DEBUG(EventSource::GNSS, "UDP Passthrough %s (from EEPROM)", 
              udpPassthroughEnabled ? "enabled" : "disabled");
    
    // Register with PGNProcessor to receive broadcast messages
    if (PGNProcessor::instance)
    {
        // Register for broadcast PGNs (200 and 202)
        bool success = PGNProcessor::instance->registerBroadcastCallback(handleBroadcastPGN, "GPS Handler");
        if (!success)
        {
            LOG_ERROR(EventSource::GNSS, "Failed to register PGN callback");
            return false;
        }
        LOG_DEBUG(EventSource::GNSS, "Successfully registered for broadcast PGNs");
    }
    else
    {
        LOG_ERROR(EventSource::GNSS, "PGNProcessor not initialized");
        return false;
    }
    
    return true;
}

bool GNSSProcessor::setup(bool enableDebug, bool enableNoiseFilter)
{
    // Configure processor
    this->enableDebug = enableDebug;
    this->enableNoiseFilter = enableNoiseFilter;

    // Initialize
    if (!init())
    {
        if (enableDebug)
        {
            LOG_ERROR(EventSource::GNSS, "GNSS Processor init failed");
        }
        return false;
    }

    if (enableDebug)
    {
        LOG_INFO(EventSource::GNSS, "GNSS Processor initialized successfully");
    }

    return true;
}

bool GNSSProcessor::processNMEAChar(char c)
{
    // Skip processing if paused
    if (processingPaused) {
        return false;
    }
    
    // Periodic status logging
    static uint32_t lastStatusLog = 0;
    if (millis() - lastStatusLog > 60000) {  // Every minute
        lastStatusLog = millis();
        LOG_INFO(EventSource::GNSS, "GNSSProcessor status: passthrough=%d", udpPassthroughEnabled);
    }
    
    switch (state)
    {
    case WAIT_START:
        if (c == '$' || c == '#')
        {
            resetParser();
            state = READ_DATA;
            calculatedChecksum = 0;
            isUnicoreMessage = (c == '#');
            parseBuffer[bufferIndex++] = c;
        }
        break;

    case READ_DATA:
        if (c == '*')
        {
            // Store the asterisk but don't include it in CRC calculation
            if (bufferIndex < sizeof(parseBuffer) - 1) {
                parseBuffer[bufferIndex++] = c;
            }
            state = READ_CHECKSUM;
            receivedChecksum = 0;
            receivedChecksum32 = 0;
            checksumIndex = 0;
        }
        else if (c == '\r' || c == '\n')
        {
            // Message without checksum - shouldn't happen for valid NMEA
            parseBuffer[bufferIndex] = '\0';
            return processMessage();
        }
        else
        {
            if (bufferIndex < sizeof(parseBuffer) - 1)
            {
                parseBuffer[bufferIndex++] = c;
                if (!isUnicoreMessage)
                {
                    calculatedChecksum ^= c;
                }
            }
        }
        break;

    case READ_CHECKSUM:
        if (isHex(c))
        {
            // Store checksum characters
            if (bufferIndex < sizeof(parseBuffer) - 1) {
                parseBuffer[bufferIndex++] = c;
            }
            
            if (isUnicoreMessage)
            {
                // Unicore uses 32-bit CRC (8 hex digits)
                if (checksumIndex < 8)
                {
                    receivedChecksum32 = (receivedChecksum32 << 4) | hexToInt(c);
                    checksumIndex++;
                    if (checksumIndex == 8)
                    {
                        // Complete sentence received
                        // Find the asterisk position for CRC calculation
                        int asteriskPos = -1;
                        for (int i = 0; i < bufferIndex; i++) {
                            if (parseBuffer[i] == '*') {
                                asteriskPos = i;
                                break;
                            }
                        }
                        
                        // Log INSPVAA/INSPVAXA messages for debugging (only occasionally)
                        if (parseBuffer[1] == 'I') {
                            if (strstr(parseBuffer, "INSPVAXA") != nullptr) {
                                static uint32_t inspvaxaLogCount = 0;
                                if (++inspvaxaLogCount % 100 == 1) {  // Log every 100th message
                                    LOG_DEBUG(EventSource::GNSS, "INSPVAXA complete: %.80s...", parseBuffer);
                                }
                            }
                            else if (strstr(parseBuffer, "INSPVAA") != nullptr) {
                                static uint32_t inspvaaLogCount = 0;
                                if (++inspvaaLogCount % 100 == 1) {  // Log every 100th message
                                    LOG_DEBUG(EventSource::GNSS, "INSPVAA complete: %.80s...", parseBuffer);
                                }
                            }
                        }
                        
                        parseBuffer[bufferIndex] = '\0'; // Null terminate
                        
                        if (udpPassthroughEnabled) {
                            // Just send via UDP and done
                            if (parseBuffer[1] == 'I' && strstr(parseBuffer, "INSPVAA") != nullptr) {
                                LOG_INFO(EventSource::GNSS, "INSPVAA UDP passthrough enabled - not processing");
                            }
                            sendCompleteNMEA();
                            resetParser();
                            return true;
                        }
                        
                        // Otherwise validate and process
                        bool checksumOK = validateChecksum();
                        if (parseBuffer[1] == 'I') {
                            if (strstr(parseBuffer, "INSPVAXA") != nullptr) {
                                static uint32_t msgCountXA = 0;
                                msgCountXA++;
                                // Show every 100th message in debug mode
                                if (enableDebug && msgCountXA % 100 == 0) {
                                    LOG_DEBUG(EventSource::GNSS, "INSPVAXA #%u checksum %s", 
                                             msgCountXA, checksumOK ? "PASSED" : "FAILED");
                                }
                            }
                            else if (strstr(parseBuffer, "INSPVAA") != nullptr) {
                                static uint32_t msgCount = 0;
                                msgCount++;
                                // Show every 100th message in debug mode
                                if (enableDebug && msgCount % 100 == 0) {
                                    LOG_DEBUG(EventSource::GNSS, "INSPVAA #%u checksum %s", 
                                             msgCount, checksumOK ? "PASSED" : "FAILED");
                                }
                            }
                        }
                        
                        if (checksumOK) {
                            return processMessage();
                        } else {
                            resetParser();
                        }
                    }
                }
            }
            else
            {
                // Standard NMEA uses 8-bit XOR (2 hex digits)
                if (checksumIndex == 0)
                {
                    receivedChecksum = hexToInt(c) << 4;
                    checksumIndex = 1;
                }
                else
                {
                    receivedChecksum |= hexToInt(c);
                    
                    // Complete sentence received
                    parseBuffer[bufferIndex] = '\0'; // Null terminate
                    
                    if (udpPassthroughEnabled) {
                        // Send complete NMEA sentence via UDP
                        sendCompleteNMEA();
                        resetParser();
                        return true;
                    }
                    
                    // Otherwise validate and process
                    if (validateChecksum()) {
                        return processMessage();
                    } else {
                        resetParser();
                    }
                }
            }
        }
        else if (c == '\r' || c == '\n')
        {
            // End of sentence - ignore trailing CR/LF
        }
        break;
    }

    return false;
}

uint16_t GNSSProcessor::processNMEAStream(const char *data, uint16_t length)
{
    // Skip processing if paused
    if (processingPaused) {
        return 0;
    }
    
    uint16_t processed = 0;

    for (uint16_t i = 0; i < length; i++)
    {
        if (processNMEAChar(data[i]))
        {
            processed++;
        }
    }

    return processed;
}

void GNSSProcessor::resetParser()
{
    bufferIndex = 0;
    state = WAIT_START;
    fieldCount = 0;
    checksumIndex = 0;
    isUnicoreMessage = false;
    memset(parseBuffer, 0, sizeof(parseBuffer));
}

bool GNSSProcessor::validateChecksum()
{
    if (isUnicoreMessage)
    {
        // For Unicore messages, CRC32 is calculated from the character after # up to (but not including) *
        // Find the asterisk position
        int asteriskPos = -1;
        for (int i = 0; i < bufferIndex; i++) {
            if (parseBuffer[i] == '*') {
                asteriskPos = i;
                break;
            }
        }
        
        if (asteriskPos < 0) {
            return false; // No asterisk found
        }
        
        // Calculate CRC from position 1 (after #) to asteriskPos (not including *)
        // Length = asteriskPos - 1 (since we start at position 1, not 0)
        unsigned long calculatedCRC = CalculateCRC32(parseBuffer + 1, asteriskPos - 1);
        
        // CRC debug enabled for testing - always show for INSPVAA
        bool isINSPVAA = (parseBuffer[1] == 'I' && strstr(parseBuffer, "INSPVAA") != nullptr);
        
        // Debug: show CRC info for INSPVAA (only when debug enabled)
        if (isINSPVAA && enableDebug) {
            static uint32_t crcDebugCount = 0;
            if (++crcDebugCount % 100 == 1) {  // Show every 100th message
                LOG_DEBUG(EventSource::GNSS, "INSPVAA CRC: calc=%08lX recv=%08lX (len=%d)", 
                         calculatedCRC, receivedChecksum32, asteriskPos - 1);
            }
        }
        
        if (enableDebug)
        {
            LOG_DEBUG(EventSource::GNSS, "Unicore CRC: calc=%08lX recv=%08lX (len=%d, asterisk@%d)", 
                     calculatedCRC, receivedChecksum32, asteriskPos - 1, asteriskPos);
        }
        
        return calculatedCRC == receivedChecksum32;
    }
    else
    {
        // Standard NMEA XOR checksum
        return calculatedChecksum == receivedChecksum;
    }
}

bool GNSSProcessor::processMessage()
{
    // Use zero-copy parsing
    parseFieldsZeroCopy();

    if (fieldCount < 1)
    {
        resetParser();
        return false;
    }

    // Determine message type and process
    // Extract message type from first field for detection
    char msgTypeBuffer[16];
    uint8_t msgTypeLen = min(fieldRefs[0].length, (uint8_t)15);
    memcpy(msgTypeBuffer, fieldRefs[0].start, msgTypeLen);
    msgTypeBuffer[msgTypeLen] = '\0';
    const char *msgType = msgTypeBuffer;
    bool processed = false;
    
    // Debug enabled for testing
    if (enableDebug)
    {
        LOG_DEBUG(EventSource::GNSS, "Message type: %s, fields: %d", msgType, fieldCount);
    }
    
    // Use fast message type detection
    MessageType type = detectMessageType(msgType);
    
    // Special logging and handling for INSPVAA/INSPVAXA
    if (msgTypeBuffer[0] == 'I') {
        if (strstr(msgTypeBuffer, "INSPVAA") != nullptr && strstr(msgTypeBuffer, "INSPVAXA") == nullptr) {
            if (enableDebug) {
                LOG_DEBUG(EventSource::GNSS, "INSPVAA processMessage: msgType='%s', fieldCount=%d, detected type=%d", 
                         msgTypeBuffer, fieldCount, type);
            }
            // Force INSPVAA type if detection failed
            if (type != MSG_INSPVAA) {
                if (enableDebug) {
                    LOG_DEBUG(EventSource::GNSS, "Forcing INSPVAA type (was %d)", type);
                }
                type = MSG_INSPVAA;
            }
        }
        else if (strstr(msgTypeBuffer, "INSPVAXA") != nullptr) {
            if (enableDebug) {
                LOG_DEBUG(EventSource::GNSS, "INSPVAXA processMessage: msgType='%s', fieldCount=%d, detected type=%d", 
                         msgTypeBuffer, fieldCount, type);
            }
            // Force INSPVAXA type if detection failed
            if (type != MSG_INSPVAXA) {
                if (enableDebug) {
                    LOG_DEBUG(EventSource::GNSS, "Forcing INSPVAXA type (was %d)", type);
                }
                type = MSG_INSPVAXA;
            }
        }
    }
    
    // Debug log for message types
    if (enableDebug) {
        static uint32_t lastMsgLog = 0;
        if (millis() - lastMsgLog > 10000) {  // Every 10 seconds
            lastMsgLog = millis();
            LOG_DEBUG(EventSource::GNSS, "GPS messages: %s (type=%d, fields=%d)", 
                      msgType, type, fieldCount);
        }
    }
    
    switch(type) {
        case MSG_GGA:
            processed = parseGGAZeroCopy();  // Use zero-copy version
            break;
            
        case MSG_GNS:
            processed = parseGNSZeroCopy();  // Use zero-copy version
            break;
            
        case MSG_VTG:
            processed = parseVTGZeroCopy();  // Use zero-copy version
            break;
            
        case MSG_HPR:
            processed = parseHPRZeroCopy();  // Use zero-copy version
            break;
            
        case MSG_KSXT:
            LOG_DEBUG(EventSource::GNSS, "Processing KSXT message");
            processed = parseKSXT();
            break;
            
        case MSG_INSPVAA:
            processed = parseINSPVAA();
            if (enableDebug && processed) {
                static uint32_t inspvaaProcessCount = 0;
                if (++inspvaaProcessCount % 100 == 1) {  // Log every 100th successful parse
                    LOG_DEBUG(EventSource::GNSS, "INSPVAA parsed - hasINS=%d, hasDualHeading=%d, lat=%.8f", 
                             gpsData.hasINS, gpsData.hasDualHeading, gpsData.latitude);
                }
            }
            break;
            
        case MSG_INSPVAXA:
            if (enableDebug) {
                LOG_DEBUG(EventSource::GNSS, "INSPVAXA detected, fieldCount=%d, bufferIndex=%d", fieldCount, bufferIndex);
            }
            processed = parseINSPVAXA();
            if (processed && enableDebug) {
                static uint32_t inspvaxaProcessCount = 0;
                if (++inspvaxaProcessCount % 100 == 1) {  // Log every 100th successful parse
                    LOG_DEBUG(EventSource::GNSS, "INSPVAXA parsed - hasINS=%d, hasDualHeading=%d, lat=%.8f, fixQuality=%d", 
                             gpsData.hasINS, gpsData.hasDualHeading, gpsData.latitude, gpsData.fixQuality);
                }
            } else if (!processed && enableDebug) {
                LOG_DEBUG(EventSource::GNSS, "INSPVAXA parse failed");
            }
            break;
            
        case MSG_BESTGNSSPOS:
        case MSG_RMC:
        case MSG_AVR:
        case MSG_UNKNOWN:
        default:
            // Messages we don't currently parse
            processed = false;
            break;
    }

    if (processed)
    {
        // Valid GPS message received
        // Note: lastUpdateTime is now set by individual message parsers
        
        // Debug log to track hasDualHeading status after each message
        static uint32_t lastTraceTime = 0;
        if (millis() - lastTraceTime > 5000) {  // Log every 5 seconds
            lastTraceTime = millis();
            LOG_DEBUG(EventSource::GNSS, "GPS State: hasDualHeading=%d, hasINS=%d, hasPosition=%d, fixQual=%d, msgMask=0x%02X", 
                      gpsData.hasDualHeading, gpsData.hasINS, gpsData.hasPosition, 
                      gpsData.fixQuality, gpsData.messageTypeMask);
        }
    }

    resetParser();
    return processed;
}






bool GNSSProcessor::parseKSXT()
{
    if (fieldCount < 10)
        return false;

    // Field 1: Timestamp (YYYYMMDDHHMMSS.SS format)
    if (fieldRefs[1].length >= 14)
    {
        // Extract just HHMMSS.SS portion (skip YYYYMMDD)
        const char* timeStr = fieldRefs[1].start + 8; // Skip first 8 chars (YYYYMMDD)
        
        // Parse HHMMSS part manually
        int hours = (timeStr[0] - '0') * 10 + (timeStr[1] - '0');
        int mins = (timeStr[2] - '0') * 10 + (timeStr[3] - '0');
        int secs = (timeStr[4] - '0') * 10 + (timeStr[5] - '0');
        gpsData.fixTime = hours * 10000 + mins * 100 + secs;
        
        // Parse fractional seconds if present
        if (timeStr[6] == '.' && fieldRefs[1].length >= 16)
        {
            gpsData.fixTimeFractional = (timeStr[7] - '0') * 0.1f + (timeStr[8] - '0') * 0.01f;
        }
        else
        {
            gpsData.fixTimeFractional = 0.0f;
        }
    }

    // Field 2: Longitude (decimal degrees)
    if (fieldRefs[2].length > 0)
    {
        gpsData.longitude = parseDoubleZeroCopy(fieldRefs[2]);
    }

    // Field 3: Latitude (decimal degrees)
    if (fieldRefs[3].length > 0)
    {
        gpsData.latitude = parseDoubleZeroCopy(fieldRefs[3]);
    }
    
    // Cache NMEA format coordinates
    cacheNMEACoordinates(gpsData.latitude, gpsData.longitude);

    // Field 4: Altitude
    if (fieldRefs[4].length > 0)
    {
        gpsData.altitude = parseFloatZeroCopy(fieldRefs[4]);
    }

    // Field 5: Heading
    if (fieldRefs[5].length > 0)
    {
        gpsData.dualHeading = parseFloatZeroCopy(fieldRefs[5]);
    }

    // Field 6: Pitch (used as roll in AOG)
    if (fieldRefs[6].length > 0)
    {
        gpsData.dualRoll = parseFloatZeroCopy(fieldRefs[6]);
    }

    // Field 10: Position quality (convert to GGA scheme)
    if (fieldRefs[10].length > 0)
    {
        uint8_t ksxtQual = parseIntZeroCopy(fieldRefs[10]);
        if (ksxtQual == 2)
            ksxtQual = 5; // FLOAT
        if (ksxtQual == 3)
            ksxtQual = 4; // RTK FIX

        gpsData.fixQuality = ksxtQual;
        gpsData.hasPosition = (ksxtQual > 0);
        gpsData.isValid = gpsData.hasPosition;
        gpsData.headingQuality = ksxtQual; // Use same quality for heading
    }

    // Field 8: Speed in km/h - convert to knots
    if (fieldRefs[8].length > 0)
    {
        float speedKmh = parseFloatZeroCopy(fieldRefs[8]);
        gpsData.speedKnots = speedKmh * 0.539957f; // Convert km/h to knots
    }

    // Field 13: Number of satellites
    if (fieldCount > 13 && fieldRefs[13].length > 0)
    {
        gpsData.numSatellites = parseIntZeroCopy(fieldRefs[13]);
    }

    // Note: HDOP not directly available in KSXT, using default
    gpsData.hdop = 0.0f;

    gpsData.hasDualHeading = true;
    gpsData.hasPosition = true;
    gpsData.lastUpdateTime = millis();
    gpsData.messageTypeMask |= (1 << 6);  // Set KSXT bit
    
    if (enableDebug)
    {
        logDebug("KSXT processed");
    }

    return true;
}

// Parsing utilities
double GNSSProcessor::parseLatitude(const char *lat, const char *ns)
{
    if (!lat || !ns || strlen(lat) < 4)
        return 0.0;

    // Cache the original NMEA value and direction
    gpsData.latitudeNMEA = atof(lat);
    gpsData.latDir = ns[0];

    double degrees = gpsData.latitudeNMEA / 100.0;
    int wholeDegrees = (int)degrees;
    double minutes = (degrees - wholeDegrees) * 100.0;

    double result = wholeDegrees + (minutes / 60.0);

    if (ns[0] == 'S')
        result = -result;

    return result;
}

double GNSSProcessor::parseLongitude(const char *lon, const char *ew)
{
    if (!lon || !ew || strlen(lon) < 5)
        return 0.0;

    // Cache the original NMEA value and direction
    gpsData.longitudeNMEA = atof(lon);
    gpsData.lonDir = ew[0];

    double degrees = gpsData.longitudeNMEA / 100.0;
    int wholeDegrees = (int)degrees;
    double minutes = (degrees - wholeDegrees) * 100.0;

    double result = wholeDegrees + (minutes / 60.0);

    if (ew[0] == 'W')
        result = -result;

    return result;
}

float GNSSProcessor::parseFloat(const char *str)
{
    return (str && strlen(str) > 0) ? atof(str) : 0.0f;
}

uint32_t GNSSProcessor::parseTime(const char *str)
{
    return (str && strlen(str) > 0) ? atol(str) : 0;
}

uint8_t GNSSProcessor::parseFixQuality(const char *str, bool isGNS)
{
    if (!str || strlen(str) == 0)
        return 0;

    if (isGNS)
    {
        // GNS mode indicator conversion
        switch (str[0])
        {
        case 'A':
            return 1; // Autonomous
        case 'D':
            return 2; // Differential
        case 'F':
            return 5; // Float RTK
        case 'R':
            return 4; // RTK Fixed
        case 'E':
            return 6; // Dead reckoning
        case 'S':
            return 4; // Simulator
        default:
            return 0; // Invalid
        }
    }
    else
    {
        // Standard GGA fix quality
        return atoi(str);
    }
}

uint8_t GNSSProcessor::hexToInt(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return 0;
}

bool GNSSProcessor::isHex(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

void GNSSProcessor::logDebug(const char *msg)
{
    if (enableDebug)
    {
        LOG_DEBUG(EventSource::GNSS, "%s", msg);
    }
}

uint32_t GNSSProcessor::getDataAge() const
{
    return millis() - gpsData.lastUpdateTime;
}

bool GNSSProcessor::isDataFresh(uint32_t maxAgeMs) const
{
    return getDataAge() <= maxAgeMs;
}


void GNSSProcessor::printData() const
{
    LOG_INFO(EventSource::GNSS, "=== GNSS Data ===");
    LOG_INFO(EventSource::GNSS, "Position: %.6f, %.6f (Alt: %.1fm)",
             gpsData.latitude, gpsData.longitude, gpsData.altitude);
    LOG_INFO(EventSource::GNSS, "Fix: Quality=%d Sats=%d HDOP=%.1f",
             gpsData.fixQuality, gpsData.numSatellites, gpsData.hdop);
    LOG_INFO(EventSource::GNSS, "Speed: %.3f knots, Heading: %.1f°",
             gpsData.speedKnots, gpsData.headingTrue);

    if (gpsData.hasDualHeading)
    {
        LOG_INFO(EventSource::GNSS, "Dual: Heading=%.2f° Roll=%.2f° Quality=%d",
                 gpsData.dualHeading, gpsData.dualRoll, gpsData.headingQuality);
    }

    LOG_INFO(EventSource::GNSS, "Status: Valid=%s Fresh=%s Age=%lums",
             gpsData.isValid ? "Yes" : "No",
             isDataFresh() ? "Yes" : "No",
             getDataAge());
}


bool GNSSProcessor::processUBXByte(uint8_t b)
{
    if (!ubxParser) return false;
    
    // Parse the UBX byte - parse() doesn't return a value, check relPosNedReady flag
    ubxParser->parse(b);
    
    // Check if a new RELPOSNED message was received
    if (ubxParser->relPosNedReady)
    {
        // Extract dual antenna heading and roll from RELPOSNED
        gpsData.dualHeading = ubxParser->ubxData.baseRelH;
        gpsData.dualRoll = ubxParser->ubxData.baseRelRoll;
        gpsData.hasDualHeading = true;
        gpsData.headingQuality = (ubxParser->ubxData.carrSoln > 1) ? 4 : 1; // 4=RTK fixed, 1=float
        gpsData.messageTypeMask |= (1 << 3);  // Set RELPOSNED bit
        
        // Clear the ready flag
        ubxParser->relPosNedReady = false;
        
        if (enableDebug)
        {
            LOG_DEBUG(EventSource::GNSS, "RELPOSNED: Heading=%.2f Roll=%.2f Quality=%d",
                     gpsData.dualHeading, gpsData.dualRoll, gpsData.headingQuality);
        }
        
        return true;
    }
    
    return false;
}

bool GNSSProcessor::parseINSPVAA()
{
    // INSPVAA format:
    // #INSPVAA,port,seq,idle%,time_status,week,seconds,reserved,pos_type,reserved;
    // week,seconds,lat,lon,height,north_vel,east_vel,up_vel,roll,pitch,azimuth,status*checksum
    // Fields after semicolon: 10=week, 11=seconds, 12=lat, 13=lon, etc.
    if (fieldCount < 22)  // Need at least up to status field
        return false;
    
    // Debug field output (only occasionally)
    if (enableDebug) {
        static uint32_t fieldDebugCount = 0;
        if (++fieldDebugCount % 100 == 1) {  // Show every 100th message
            LOG_DEBUG(EventSource::GNSS, "INSPVAA Fields (total=%d)", fieldCount);
        }
    }
    
    // Field 12: Latitude (degrees)
    if (fieldRefs[12].length > 0)
    {
        gpsData.latitude = parseDoubleZeroCopy(fieldRefs[12]);
        gpsData.hasPosition = true;
    }
    
    // Field 13: Longitude (degrees)
    if (fieldRefs[13].length > 0)
    {
        gpsData.longitude = parseDoubleZeroCopy(fieldRefs[13]);
    }
    
    // Cache NMEA format coordinates
    if (gpsData.hasPosition) {
        cacheNMEACoordinates(gpsData.latitude, gpsData.longitude);
    }
    
    // Field 14: Height (meters)
    if (fieldRefs[14].length > 0)
    {
        gpsData.altitude = parseFloatZeroCopy(fieldRefs[14]);
    }
    
    // Field 15,16,17: North, East, Up velocities (m/s)
    if (fieldRefs[15].length > 0 && fieldRefs[16].length > 0 && fieldRefs[17].length > 0)
    {
        gpsData.northVelocity = parseFloatZeroCopy(fieldRefs[15]);
        gpsData.eastVelocity = parseFloatZeroCopy(fieldRefs[16]);
        gpsData.upVelocity = parseFloatZeroCopy(fieldRefs[17]);
        
        // Calculate speed in knots from north/east velocities
        float speedMs = sqrt(gpsData.northVelocity * gpsData.northVelocity + 
                            gpsData.eastVelocity * gpsData.eastVelocity);
        gpsData.speedKnots = speedMs * 1.94384f; // m/s to knots
        gpsData.hasVelocity = true;
    }
    
    // Field 18,19,20: Roll, Pitch, Azimuth (degrees)
    if (fieldRefs[18].length > 0 && fieldRefs[19].length > 0 && fieldRefs[20].length > 0)
    {
        gpsData.insRoll = parseFloatZeroCopy(fieldRefs[18]);
        gpsData.insPitch = parseFloatZeroCopy(fieldRefs[19]);
        gpsData.insHeading = parseFloatZeroCopy(fieldRefs[20]);
        
        // For UM981, use INS heading/roll as dual antenna data
        gpsData.dualHeading = gpsData.insHeading;
        gpsData.dualRoll = gpsData.insRoll;
        gpsData.hasDualHeading = true;
    }
    
    // Debug field count
    if (enableDebug) {
        LOG_DEBUG(EventSource::GNSS, "INSPVAA fieldCount=%d", fieldCount);
    }
    
    // Field 21: INS status - map to traditional GPS fix quality
    // GPS Fix Quality mapping for AgOpenGPS:
    // 0 = Invalid/No fix
    // 1 = GPS fix (SPS)
    // 2 = DGPS fix
    // 4 = RTK Fixed
    // 5 = RTK Float
    if (fieldCount > 21 && fieldRefs[21].length > 0)
    {
        // Parse INS status string and map to GPS fix quality
        if (fieldStartsWith(fieldRefs[21], "INS_INACTIVE"))
        {
            gpsData.insAlignmentStatus = 0;  // Binary 0
            gpsData.fixQuality = 0;  // No fix - IMU data invalid
        }
        else if (fieldStartsWith(fieldRefs[21], "INS_ALIGNING"))
        {
            gpsData.insAlignmentStatus = 1;  // Binary 1
            gpsData.fixQuality = 1;  // Basic GPS fix during alignment
        }
        else if (fieldStartsWith(fieldRefs[21], "INS_HIGH_VARIANCE"))
        {
            gpsData.insAlignmentStatus = 2;  // Binary 2
            gpsData.fixQuality = 2;  // DGPS-like quality (degraded)
        }
        else if (fieldStartsWith(fieldRefs[21], "INS_SOLUTION_GOOD"))
        {
            gpsData.insAlignmentStatus = 3;  // Binary 3
            gpsData.fixQuality = 4;  // RTK Fixed quality
        }
        else if (fieldStartsWith(fieldRefs[21], "INS_SOLUTION_FREE"))
        {
            gpsData.insAlignmentStatus = 6;  // Binary 6
            gpsData.fixQuality = 1;  // Basic fix (DR mode, no GNSS)
        }
        else if (fieldStartsWith(fieldRefs[21], "INS_ALIGNMENT_COMPLETE"))
        {
            gpsData.insAlignmentStatus = 7;  // Binary 7
            gpsData.fixQuality = 5;  // RTK Float quality
        }
        else
        {
            // Unknown status - default to basic fix
            gpsData.insAlignmentStatus = 0;
            gpsData.fixQuality = 1;
        }
        
        // Debug output
        if (enableDebug) {
            char statusBuf[32];
            int len = fieldRefs[21].length < 31 ? fieldRefs[21].length : 31;
            memcpy(statusBuf, fieldRefs[21].start, len);
            statusBuf[len] = '\0';
            LOG_DEBUG(EventSource::GNSS, "INS Status: '%s' (alignment=%d, fixQuality=%d)", 
                      statusBuf, gpsData.insAlignmentStatus, gpsData.fixQuality);
        }
    }
    else
    {
        // Fallback if status field is missing
        gpsData.fixQuality = 1;
        gpsData.insAlignmentStatus = 3;
    }
    
    gpsData.posType = 16; // INS position
    gpsData.insStatus = 1; // Mark as having INS
    
    // Set number of satellites (not directly available in INSPVAA, use a reasonable value)
    gpsData.numSatellites = 12; // Typical for INS solution
    gpsData.hdop = 0.9f; // Good HDOP for INS solution
    
    // Set GPS time from fields 5,6 (week, seconds) from header
    if (fieldRefs[5].length > 0 && fieldRefs[6].length > 0)
    {
        // Store GPS week and seconds for UTC conversion
        gpsData.gpsWeek = (uint16_t)parseIntZeroCopy(fieldRefs[5]);
        gpsData.gpsSeconds = parseFloatZeroCopy(fieldRefs[6]);
        
        // Also store as fixTime for compatibility
        float seconds = gpsData.gpsSeconds;
        int hours = (int)(seconds / 3600) % 24;
        int minutes = (int)((seconds - hours * 3600) / 60);
        float secs = seconds - hours * 3600 - minutes * 60;
        int intSecs = (int)secs;
        gpsData.fixTime = hours * 10000 + minutes * 100 + intSecs;
        gpsData.fixTimeFractional = secs - intSecs;
    }
    
    gpsData.hasINS = true;
    gpsData.isValid = true;
    gpsData.messageTypeMask |= (1 << 7);  // Set INSPVA bit
    
    // Update the last update time - this is critical!
    gpsData.lastUpdateTime = millis();
    
    
    return true;
}

bool GNSSProcessor::parseINSPVAXA()
{
    // INSPVAXA format (from UM981):
    // #INSPVAXA,port,seq,idle%,time_status,week,seconds,receiver_status,reserved1,reserved2;
    // INS_status,position_type,lat,lon,height,north_vel,east_vel,up_vel,roll,pitch,azimuth,
    // lat_σ,lon_σ,height_σ,north_vel_σ,east_vel_σ,up_vel_σ,ext_sol_stat,time_since_update,reserved*checksum
    // Header (before semicolon) is field 0, then: 1=INS_status, 2=position_type, 3=lat, 4=lon, etc.
    
    if (fieldCount < 29)  // Need at least up to time_since_update field
    {
        if (enableDebug)
        {
            LOG_WARNING(EventSource::GNSS, "INSPVAXA: Not enough fields! Expected 20+, got %d", fieldCount);
        }
        return false;
    }
    
    // Debug output removed - field mapping confirmed
    
    // Field 10: INS Status (after 10 header fields)
    bool insValid = true;
    if (fieldCount > 10 && fieldRefs[10].length > 0)
    {
        // Parse INS status and map to GPS fix quality
        if (fieldStartsWith(fieldRefs[10], "INS_INACTIVE"))
        {
            gpsData.insAlignmentStatus = 0;  // Binary 0
            gpsData.fixQuality = 0;  // No fix - IMU data invalid
            insValid = false;
        }
        else if (fieldStartsWith(fieldRefs[10], "INS_ALIGNING"))
        {
            gpsData.insAlignmentStatus = 1;  // Binary 1
            gpsData.fixQuality = 1;  // Basic GPS fix during alignment
        }
        else if (fieldStartsWith(fieldRefs[10], "INS_HIGH_VARIANCE"))
        {
            gpsData.insAlignmentStatus = 2;  // Binary 2
            gpsData.fixQuality = 2;  // DGPS-like quality (degraded)
        }
        else if (fieldStartsWith(fieldRefs[10], "INS_SOLUTION_GOOD"))
        {
            gpsData.insAlignmentStatus = 3;  // Binary 3
            gpsData.fixQuality = 4;  // RTK Fixed quality
        }
        else if (fieldStartsWith(fieldRefs[10], "INS_SOLUTION_FREE"))
        {
            gpsData.insAlignmentStatus = 6;  // Binary 6
            gpsData.fixQuality = 1;  // Basic fix (DR mode, no GNSS)
        }
        else if (fieldStartsWith(fieldRefs[10], "INS_ALIGNMENT_COMPLETE"))
        {
            gpsData.insAlignmentStatus = 7;  // Binary 7
            gpsData.fixQuality = 5;  // RTK Float quality
        }
        else
        {
            // Unknown status - default to basic fix
            gpsData.insAlignmentStatus = 0;
            gpsData.fixQuality = 1;
        }
    }
    
    // Field 11: Position type - update fix quality based on position type
    if (fieldCount > 11 && fieldRefs[11].length > 0)
    {
        if (fieldStartsWith(fieldRefs[11], "INS_RTKFIXED"))
        {
            gpsData.posType = 56;  // INS_RTKFIXED from spec
            gpsData.fixQuality = 4;  // RTK Fixed
        }
        else if (fieldStartsWith(fieldRefs[11], "INS_RTKFLOAT"))
        {
            gpsData.posType = 55;  // INS_RTKFLOAT from spec
            gpsData.fixQuality = 5;  // RTK Float
        }
        else if (fieldStartsWith(fieldRefs[11], "INS_PSRDIFF"))
        {
            gpsData.posType = 54;  // INS_PSRDIFF from spec
            gpsData.fixQuality = 2;  // DGPS
        }
        else if (fieldStartsWith(fieldRefs[11], "INS_PSRSP"))
        {
            gpsData.posType = 53;  // INS_PSRSP from spec
            gpsData.fixQuality = 1;  // Basic GPS
        }
        else if (fieldStartsWith(fieldRefs[2], "INS"))
        {
            gpsData.posType = 52;  // INS only from spec
            gpsData.fixQuality = 1;  // Basic fix
        }
        else
        {
            gpsData.posType = 0;  // NONE
            if (insValid) gpsData.fixQuality = 1;  // Keep INS status based quality
        }
    }
    
    // Field 12: Latitude (degrees)
    if (fieldRefs[12].length > 0)
    {
        gpsData.latitude = parseDoubleZeroCopy(fieldRefs[12]);
        gpsData.hasPosition = insValid && 
                             (gpsData.latitude != 0.0 || gpsData.longitude != 0.0);
    }
    
    // Field 13: Longitude (degrees)
    if (fieldRefs[13].length > 0)
    {
        gpsData.longitude = parseDoubleZeroCopy(fieldRefs[13]);
    }
    
    // Cache NMEA format coordinates
    if (gpsData.hasPosition) {
        cacheNMEACoordinates(gpsData.latitude, gpsData.longitude);
    }
    
    // Field 14: Height (meters)
    if (fieldRefs[14].length > 0)
    {
        gpsData.altitude = parseFloatZeroCopy(fieldRefs[14]);
    }
    
    // Field 15,16,17: North, East, Up velocities (m/s)
    if (fieldRefs[15].length > 0 && fieldRefs[16].length > 0 && fieldRefs[17].length > 0)
    {
        gpsData.northVelocity = parseFloatZeroCopy(fieldRefs[15]);
        gpsData.eastVelocity = parseFloatZeroCopy(fieldRefs[16]);
        gpsData.upVelocity = parseFloatZeroCopy(fieldRefs[17]);
        
        // Calculate speed in knots from north/east velocities
        float speedMs = sqrt(gpsData.northVelocity * gpsData.northVelocity + 
                            gpsData.eastVelocity * gpsData.eastVelocity);
        gpsData.speedKnots = speedMs * 1.94384f; // m/s to knots
        gpsData.hasVelocity = true;
    }
    
    // Field 19,20,21: Roll, Pitch, Azimuth (degrees)
    if (fieldRefs[19].length > 0 && fieldRefs[20].length > 0 && fieldRefs[21].length > 0)
    {
        float roll = parseFloatZeroCopy(fieldRefs[19]);
        float pitch = parseFloatZeroCopy(fieldRefs[20]);
        float azimuth = parseFloatZeroCopy(fieldRefs[21]);
        
        // Debug logging removed - axis mapping confirmed working
        
        gpsData.insRoll = roll;
        gpsData.insPitch = pitch;
        gpsData.insHeading = azimuth;
        
        // For UM981, use INS heading/roll as dual antenna data
        gpsData.dualHeading = gpsData.insHeading;
        gpsData.dualRoll = gpsData.insRoll;
        gpsData.hasDualHeading = true;
    }
    
    // Field 22,23,24: Position StdDev (lat, lon, height) in meters  
    if (fieldRefs[22].length > 0 && fieldRefs[23].length > 0 && fieldRefs[24].length > 0)
    {
        gpsData.posStdDevLat = parseFloatZeroCopy(fieldRefs[22]);
        gpsData.posStdDevLon = parseFloatZeroCopy(fieldRefs[23]);
        gpsData.posStdDevAlt = parseFloatZeroCopy(fieldRefs[24]);
    }
    
    // Field 25,26,27: Velocity StdDev (north, east, up) in m/s
    if (fieldRefs[25].length > 0 && fieldRefs[26].length > 0 && fieldRefs[27].length > 0)
    {
        gpsData.velStdDevNorth = parseFloatZeroCopy(fieldRefs[25]);
        gpsData.velStdDevEast = parseFloatZeroCopy(fieldRefs[26]);
        gpsData.velStdDevUp = parseFloatZeroCopy(fieldRefs[27]);
    }
    
    // Field 28: Extended solution status
    if (fieldCount > 28 && fieldRefs[28].length > 0)
    {
        gpsData.extSolStatus = (uint16_t)parseIntZeroCopy(fieldRefs[28]);
    }
    
    // Field 32: Contains time since update followed by *checksum
    // Format: "value*checksum" e.g. "0*cd793b60"
    if (fieldCount > 32 && fieldRefs[32].length > 0)
    {
        // Find the asterisk position
        const char* asteriskPos = nullptr;
        for (int i = 0; i < fieldRefs[32].length; i++) {
            if (fieldRefs[32].start[i] == '*') {
                asteriskPos = &fieldRefs[32].start[i];
                break;
            }
        }
        
        if (asteriskPos) {
            // Extract the number before the asterisk
            int valueLen = asteriskPos - fieldRefs[32].start;
            if (valueLen > 0) {
                FieldRef timeField;
                timeField.start = fieldRefs[32].start;
                timeField.length = valueLen;
                gpsData.timeSinceUpdate = (uint32_t)parseIntZeroCopy(timeField);
                
                if (enableDebug) {
                    LOG_DEBUG(EventSource::GNSS, "INSPVAXA: Time since update = %u seconds", gpsData.timeSinceUpdate);
                }
                
                // Use time since update as age of DGPS for PAOGI message
                gpsData.ageDGPS = (uint16_t)gpsData.timeSinceUpdate;
            }
        }
    }
    
    // Set number of satellites (not directly available in INSPVAXA, use a reasonable value)
    gpsData.numSatellites = 12; // Typical for INS solution
    gpsData.hdop = 0.9f; // Good HDOP for INS solution
    gpsData.insStatus = 1; // Mark as having INS
    gpsData.hasINS = true;
    
    // GPS time from header fields 5 (week) and 6 (seconds)
    if (fieldRefs[5].length > 0 && fieldRefs[6].length > 0)
    {
        // Store GPS week and seconds for UTC conversion
        gpsData.gpsWeek = (uint16_t)parseIntZeroCopy(fieldRefs[5]);
        gpsData.gpsSeconds = parseFloatZeroCopy(fieldRefs[6]);
        
        // Also store as fixTime for compatibility
        float seconds = gpsData.gpsSeconds;
        int hours = (int)(seconds / 3600) % 24;
        int minutes = (int)((seconds - hours * 3600) / 60);
        float secs = seconds - hours * 3600 - minutes * 60;
        int intSecs = (int)secs;
        gpsData.fixTime = hours * 10000 + minutes * 100 + intSecs;
        gpsData.fixTimeFractional = secs - intSecs;
    }
    
    gpsData.hasINS = true;
    gpsData.isValid = true;
    gpsData.messageTypeMask |= (1 << 7);  // Set INSPVA bit
    
    // Update the last update time
    gpsData.lastUpdateTime = millis();
    
    // Debug output
    if (enableDebug)
    {
        LOG_DEBUG(EventSource::GNSS, "INSPVAXA: Lat=%.8f±%.3fm Lon=%.8f±%.3fm Alt=%.1f±%.3fm",
                  gpsData.latitude, gpsData.posStdDevLat, 
                  gpsData.longitude, gpsData.posStdDevLon, 
                  gpsData.altitude, gpsData.posStdDevAlt);
        LOG_DEBUG(EventSource::GNSS, "INSPVAXA: Hdg=%.1f Roll=%.1f Pitch=%.1f VelN=%.2f±%.3f VelE=%.2f±%.3f",
                      gpsData.insHeading, gpsData.insRoll, gpsData.insPitch,
                      gpsData.northVelocity, gpsData.velStdDevNorth,
                      gpsData.eastVelocity, gpsData.velStdDevEast);
    }
    
    return true;
}

// PGN Support Implementation

// External reference to NetworkBase send function
extern void sendUDPbytes(uint8_t *message, int msgLen);

// Get ConfigManager instance
extern ConfigManager configManager;

// Removed registerPGNCallbacks - broadcast PGNs are handled automatically

// Static callback for broadcast PGNs (Hello and Scan Request)
void GNSSProcessor::handleBroadcastPGN(uint8_t pgn, const uint8_t* data, size_t len)
{
    // Check if this is a Hello PGN
    if (pgn == 200)
    {
        // When we receive a Hello from AgIO, we should respond
        
        // GPS Hello reply format from PGN.md: 
        // Source: 0x78 (120), PGN: 0x78 (120), Length: 5
        uint8_t helloFromGPS[] = {0x80, 0x81, GPS_SOURCE_ID, GPS_HELLO_REPLY, 5, 0, 0, 0, 0, 0, 0};
        
        // Calculate and set CRC
        calculateAndSetCRC(helloFromGPS, sizeof(helloFromGPS));
        
        // Send the reply
        sendUDPbytes(helloFromGPS, sizeof(helloFromGPS));
    }
    // Check if this is a Scan Request PGN
    else if (pgn == 202)
    {
        
        // Subnet GPS reply format from PGN.md:
        // Src: 0x78 (120), PGN: 0xCB (203), Len: 7
        // IP_One, IP_Two, IP_Three, IP_Four, Subnet_One, Subnet_Two, Subnet_Three
        uint8_t ip[4];
        configManager.getIPAddress(ip);
        
        uint8_t subnetReply[] = {
            0x80, 0x81,              // PGN header
            GPS_SOURCE_ID,           // Source: 0x78 (120)
            0xCB,                    // PGN: 203
            7,                       // Data length
            ip[0],  // IP_One
            ip[1],  // IP_Two
            ip[2],  // IP_Three
            ip[3],  // IP_Four
            ip[0],  // Subnet_One
            ip[1],  // Subnet_Two
            ip[2],  // Subnet_Three
            0                        // CRC placeholder
        };
        
        // Calculate and set CRC
        calculateAndSetCRC(subnetReply, sizeof(subnetReply));
        
        // Send the reply
        sendUDPbytes(subnetReply, sizeof(subnetReply));
    }
}

void GNSSProcessor::sendGPSData()
{
    // PGN 214 (0xD6) - GPS data format is complex (51 bytes)
    // Not yet implemented - AgIO doesn't currently support GPS data via PGN
    
    if (!gpsData.isValid)
        return;
        
    // Future implementation will send full GPS data packet
    // Format is defined in PGN.md - Main Antenna section
}

void GNSSProcessor::sendCompleteNMEA()
{
    // Send complete NMEA sentence including checksum with CRLF appended
    // This is called when UDP passthrough is enabled
    // The parseBuffer contains the complete sentence including $ or #, data, *, and checksum
    
    if (bufferIndex == 0) {
        return;
    }
    
    // Build complete sentence with CRLF
    uint8_t sentence[310];  // 300 for parseBuffer + 10 extra for CRLF
    
    // Copy the complete sentence (includes $/#, data, *, and checksum)
    memcpy(sentence, parseBuffer, bufferIndex);
    int len = bufferIndex;
    
    // Add CRLF
    sentence[len++] = '\r';
    sentence[len++] = '\n';
    
    // Send via UDP
    sendUDPbytes(sentence, len);
    
    // Debug logging
    static uint32_t lastPassthroughLog = 0;
    static uint32_t passthroughCount = 0;
    passthroughCount++;
    
    if (millis() - lastPassthroughLog > 5000) {
        lastPassthroughLog = millis();
        LOG_DEBUG(EventSource::GNSS, "UDP Passthrough: %lu sentences sent", passthroughCount);
        passthroughCount = 0;
    }
}

GNSSProcessor::MessageType GNSSProcessor::detectMessageType(const char* msgType) {
    // Fast message type detection using character comparison
    // Skip past the talker ID (GP, GN, etc.) if present
    const char* typeStart = msgType;
    
    // Find where the actual message type starts (after talker ID)
    if (strlen(msgType) >= 5) {
        // Standard NMEA has 2-char talker ID
        if (msgType[2] >= 'A' && msgType[2] <= 'Z') {
            typeStart = msgType + 2;
        }
    }
    
    // Now do fast character comparison
    switch(typeStart[0]) {
        case 'G':
            if (typeStart[1] == 'G' && typeStart[2] == 'A') return MSG_GGA;
            if (typeStart[1] == 'N' && typeStart[2] == 'S') return MSG_GNS;
            break;
            
        case 'V':
            if (typeStart[1] == 'T' && typeStart[2] == 'G') return MSG_VTG;
            break;
            
        case 'R':
            if (typeStart[1] == 'M' && typeStart[2] == 'C') return MSG_RMC;
            break;
            
        case 'H':
            if (typeStart[1] == 'P' && typeStart[2] == 'R') return MSG_HPR;
            break;
            
        case 'K':
            if (typeStart[1] == 'S' && typeStart[2] == 'X' && typeStart[3] == 'T') return MSG_KSXT;
            break;
            
        case 'I':
            if (strncmp(typeStart, "INSPVAA", 7) == 0) return MSG_INSPVAA;
            if (strncmp(typeStart, "INSPVAXA", 8) == 0) return MSG_INSPVAXA;
            break;
            
        case 'B':
            if (strncmp(typeStart, "BESTGNSSPOS", 11) == 0) return MSG_BESTGNSSPOS;
            break;
            
        case 'A':
            if (typeStart[1] == 'V' && typeStart[2] == 'R') return MSG_AVR;
            break;
    }
    
    return MSG_UNKNOWN;
}

void GNSSProcessor::cacheNMEACoordinates(double lat, double lon) {
    // Convert decimal degrees to NMEA format for caching
    
    // Latitude
    gpsData.latDir = (lat < 0) ? 'S' : 'N';
    double absLat = fabs(lat);
    int latDegrees = (int)absLat;
    double latMinutes = (absLat - latDegrees) * 60.0;
    gpsData.latitudeNMEA = latDegrees * 100.0 + latMinutes;
    
    // Longitude
    gpsData.lonDir = (lon < 0) ? 'W' : 'E';
    double absLon = fabs(lon);
    int lonDegrees = (int)absLon;
    double lonMinutes = (absLon - lonDegrees) * 60.0;
    gpsData.longitudeNMEA = lonDegrees * 100.0 + lonMinutes;
}

void GNSSProcessor::parseFieldsZeroCopy() {
    fieldCount = 0;
    
    // Skip the '$' or '#' and start parsing
    const char* fieldStart = parseBuffer + 1;
    
    for (int i = 1; i < bufferIndex && fieldCount < 35; i++) {
        char c = parseBuffer[i];
        
        if (c == ',' || c == ';' || c == '\0') {
            // Found end of field
            fieldRefs[fieldCount].start = fieldStart;
            fieldRefs[fieldCount].length = (parseBuffer + i) - fieldStart;
            fieldCount++;
            
            // Next field starts after the delimiter
            fieldStart = parseBuffer + i + 1;
        }
    }
    
    // Handle the last field if buffer doesn't end with delimiter
    if (fieldStart < parseBuffer + bufferIndex && fieldCount < 35) {
        fieldRefs[fieldCount].start = fieldStart;
        fieldRefs[fieldCount].length = (parseBuffer + bufferIndex) - fieldStart;
        fieldCount++;
    }
}

// Zero-copy utility functions
float GNSSProcessor::parseFloatZeroCopy(const FieldRef& field) {
    if (field.length == 0) return 0.0f;
    
    // Temporarily null-terminate the field
    char saved = field.start[field.length];
    const_cast<char*>(field.start)[field.length] = '\0';
    float result = atof(field.start);
    const_cast<char*>(field.start)[field.length] = saved;
    
    return result;
}

double GNSSProcessor::parseDoubleZeroCopy(const FieldRef& field) {
    if (field.length == 0) return 0.0;
    
    // Temporarily null-terminate the field
    char saved = field.start[field.length];
    const_cast<char*>(field.start)[field.length] = '\0';
    double result = atof(field.start);
    const_cast<char*>(field.start)[field.length] = saved;
    
    return result;
}

int GNSSProcessor::parseIntZeroCopy(const FieldRef& field) {
    if (field.length == 0) return 0;
    
    // Temporarily null-terminate the field
    char saved = field.start[field.length];
    const_cast<char*>(field.start)[field.length] = '\0';
    int result = atoi(field.start);
    const_cast<char*>(field.start)[field.length] = saved;
    
    return result;
}

bool GNSSProcessor::fieldEquals(const FieldRef& field, const char* str) {
    size_t len = strlen(str);
    if (field.length != len) return false;
    return strncmp(field.start, str, len) == 0;
}

bool GNSSProcessor::fieldStartsWith(const FieldRef& field, const char* prefix) {
    size_t len = strlen(prefix);
    if (field.length < len) return false;
    return strncmp(field.start, prefix, len) == 0;
}

double GNSSProcessor::parseLatitudeZeroCopy(const FieldRef& lat, const FieldRef& ns) {
    if (lat.length < 4 || ns.length < 1) return 0.0;
    
    // Cache the original NMEA value and direction
    gpsData.latitudeNMEA = parseDoubleZeroCopy(lat);
    gpsData.latDir = ns.start[0];
    
    double degrees = gpsData.latitudeNMEA / 100.0;
    int wholeDegrees = (int)degrees;
    double minutes = (degrees - wholeDegrees) * 100.0;
    
    double result = wholeDegrees + (minutes / 60.0);
    
    if (ns.start[0] == 'S')
        result = -result;
        
    return result;
}

double GNSSProcessor::parseLongitudeZeroCopy(const FieldRef& lon, const FieldRef& ew) {
    if (lon.length < 5 || ew.length < 1) return 0.0;
    
    // Cache the original NMEA value and direction
    gpsData.longitudeNMEA = parseDoubleZeroCopy(lon);
    gpsData.lonDir = ew.start[0];
    
    double degrees = gpsData.longitudeNMEA / 100.0;
    int wholeDegrees = (int)degrees;
    double minutes = (degrees - wholeDegrees) * 100.0;
    
    double result = wholeDegrees + (minutes / 60.0);
    
    if (ew.start[0] == 'W')
        result = -result;
        
    return result;
}

bool GNSSProcessor::parseGGAZeroCopy() {
    if (fieldCount < 9)
        return false;

    // Field 1: Time (HHMMSS.SS format)
    if (fieldRefs[1].length > 0) {
        float time = parseFloatZeroCopy(fieldRefs[1]);
        gpsData.fixTime = (uint32_t)time;
        gpsData.fixTimeFractional = time - (uint32_t)time;
    }

    // Fields 2-3: Latitude
    gpsData.latitude = parseLatitudeZeroCopy(fieldRefs[2], fieldRefs[3]);

    // Fields 4-5: Longitude
    gpsData.longitude = parseLongitudeZeroCopy(fieldRefs[4], fieldRefs[5]);

    // Field 6: Fix quality
    if (fieldRefs[6].length > 0) {
        gpsData.fixQuality = fieldRefs[6].start[0] - '0';  // Direct digit conversion
    }

    // Field 7: Number of satellites
    gpsData.numSatellites = parseIntZeroCopy(fieldRefs[7]);

    // Field 8: HDOP
    gpsData.hdop = parseFloatZeroCopy(fieldRefs[8]);

    // Field 9: Altitude
    if (fieldCount > 9) {
        gpsData.altitude = parseFloatZeroCopy(fieldRefs[9]);
    }

    // Field 13: Age of DGPS
    if (fieldCount > 13) {
        gpsData.ageDGPS = parseIntZeroCopy(fieldRefs[13]);
    }

    // Set status flags
    gpsData.hasPosition = (gpsData.latitude != 0.0 || gpsData.longitude != 0.0) && 
                         gpsData.fixQuality >= 1;
    gpsData.isValid = gpsData.hasPosition;  // GGA messages need valid flag
    gpsData.lastUpdateTime = millis();
    gpsData.messageTypeMask |= (1 << 0);  // Set GGA bit
    
    // Check for duplicate position
    if (gpsData.latitude == lastGGALatitude && 
        gpsData.longitude == lastGGALongitude && 
        lastGGALatitude != 0.0) {  // Don't log on first valid position
        static uint32_t lastDuplicateLog = 0;
        if (millis() - lastDuplicateLog > 5000) {  // Log every 5 seconds max
            lastDuplicateLog = millis();
            LOG_WARNING(EventSource::GNSS, "GGA: Duplicate position detected: %.8f, %.8f", 
                       gpsData.latitude, gpsData.longitude);
        }
    }
    
    // Update last position
    lastGGALatitude = gpsData.latitude;
    lastGGALongitude = gpsData.longitude;

    if (enableDebug) {
        logDebug("GGA processed (zero-copy)");
    }

    return true;
}

bool GNSSProcessor::parseGNSZeroCopy() {
    if (fieldCount < 9)
        return false;

    // Field 1: Time (HHMMSS.SS format)
    if (fieldRefs[1].length > 0) {
        float time = parseFloatZeroCopy(fieldRefs[1]);
        gpsData.fixTime = (uint32_t)time;
        gpsData.fixTimeFractional = time - (uint32_t)time;
    }

    // Fields 2-3: Latitude
    gpsData.latitude = parseLatitudeZeroCopy(fieldRefs[2], fieldRefs[3]);

    // Fields 4-5: Longitude
    gpsData.longitude = parseLongitudeZeroCopy(fieldRefs[4], fieldRefs[5]);

    // Field 6: Mode indicator (convert to fix quality)
    if (fieldRefs[6].length > 0) {
        // GNS mode is a string like "AAN" where each char represents GPS/GLONASS/Galileo
        // Convert first character to fix quality
        char mode = fieldRefs[6].start[0];
        switch(mode) {
            case 'N': gpsData.fixQuality = 0; break;  // No fix
            case 'A': gpsData.fixQuality = 1; break;  // Autonomous
            case 'D': gpsData.fixQuality = 2; break;  // Differential
            case 'P': gpsData.fixQuality = 3; break;  // Precise
            case 'R': gpsData.fixQuality = 4; break;  // RTK Fixed
            case 'F': gpsData.fixQuality = 5; break;  // RTK Float
            default:  gpsData.fixQuality = 0; break;
        }
    }

    // Field 7: Number of satellites
    if (fieldCount > 7 && fieldRefs[7].length > 0) {
        gpsData.numSatellites = parseIntZeroCopy(fieldRefs[7]);
    }
    
    // Field 8: HDOP
    if (fieldCount > 8 && fieldRefs[8].length > 0) {
        gpsData.hdop = parseFloatZeroCopy(fieldRefs[8]);
    }

    // Field 9: Altitude
    if (fieldCount > 9 && fieldRefs[9].length > 0) {
        gpsData.altitude = parseFloatZeroCopy(fieldRefs[9]);
    }

    // Field 12: Age of DGPS (optional)
    if (fieldCount > 12 && fieldRefs[12].length > 0) {
        gpsData.ageDGPS = parseIntZeroCopy(fieldRefs[12]);
    }

    // Set status flags
    gpsData.hasPosition = (gpsData.latitude != 0.0 || gpsData.longitude != 0.0) && 
                         gpsData.fixQuality >= 1;
    gpsData.isValid = gpsData.hasPosition;  // GNS messages need valid flag
    gpsData.lastUpdateTime = millis();
    gpsData.messageTypeMask |= (1 << 1);  // Set GNS bit

    if (enableDebug) {
        logDebug("GNS processed (zero-copy)");
    }

    return true;
}

bool GNSSProcessor::parseVTGZeroCopy() {
    if (fieldCount < 8)
        return false;

    // Field 1: Track made good (true)
    if (fieldRefs[1].length > 0) {
        gpsData.headingTrue = parseFloatZeroCopy(fieldRefs[1]);
    }

    // Field 5: Speed over ground in knots
    if (fieldRefs[5].length > 0) {
        gpsData.speedKnots = parseFloatZeroCopy(fieldRefs[5]);
        gpsData.hasVelocity = true;
    }

    // Field 7: Speed over ground in km/h (optional check)
    // We use knots, but can validate if both are present

    // Set status flags
    // VTG doesn't contain position data, so don't update lastUpdateTime
    gpsData.messageTypeMask |= (1 << 2);  // Set VTG bit

    if (enableDebug) {
        logDebug("VTG processed (zero-copy)");
    }

    return true;
}

bool GNSSProcessor::parseHPRZeroCopy() {
    // HPR format: $GNHPR,time,heading,pitch,roll,quality,satellites,age,reserved*checksum
    // Minimum 8 fields after message type
    if (fieldCount < 8)
        return false;

    // Field 1: Time (HHMMSS.SS format)
    if (fieldRefs[1].length > 0) {
        float time = parseFloatZeroCopy(fieldRefs[1]);
        gpsData.fixTime = (uint32_t)time;
        gpsData.fixTimeFractional = time - (uint32_t)time;
    }

    // Field 2: Heading (degrees)
    if (fieldRefs[2].length > 0) {
        gpsData.dualHeading = parseFloatZeroCopy(fieldRefs[2]);
    }
    
    // Field 3: Pitch (degrees) - used as roll for AgOpenGPS
    if (fieldRefs[3].length > 0) {
        gpsData.dualRoll = parseFloatZeroCopy(fieldRefs[3]);  // Pitch is used as roll in AOG
    }

    // Field 4: Roll (degrees) - not used by AgOpenGPS
    // Skip this field

    // Field 5: Heading quality (0=no fix, 1=single, 2=float RTK, 4=RTK fixed)
    if (fieldRefs[5].length > 0) {
        gpsData.headingQuality = parseIntZeroCopy(fieldRefs[5]);
    }

    // Field 6: Number of satellites
    if (fieldRefs[6].length > 0) {
        gpsData.numSatellites = parseIntZeroCopy(fieldRefs[6]);
    }

    // Field 7: Age (seconds)
    if (fieldRefs[7].length > 0) {
        gpsData.ageDGPS = (uint16_t)(parseFloatZeroCopy(fieldRefs[7]) * 100); // Convert to centiseconds
    }

    // Field 8: Reserved - skip

    // Always set hasDualHeading for HPR messages
    gpsData.hasDualHeading = true;
    
    // HPR doesn't provide position data, only heading/roll
    // Don't set hasPosition based on HPR alone
    
    // Set valid and update time
    gpsData.isValid = true;  // HPR messages are valid even without position
    // HPR doesn't contain position data, so don't update lastUpdateTime
    gpsData.messageTypeMask |= (1 << 4);  // Set HPR bit

    // Debug log
    LOG_DEBUG(EventSource::GNSS, "HPR: Setting hasDualHeading=true, heading=%.1f, roll=%.1f, quality=%d", 
              gpsData.dualHeading, gpsData.dualRoll, gpsData.headingQuality);

    if (enableDebug) {
        LOG_DEBUG(EventSource::GNSS, "HPR processed: sats=%d, age=%.2f", 
                  gpsData.numSatellites, gpsData.ageDGPS / 100.0f);
    }

    return true;
}

