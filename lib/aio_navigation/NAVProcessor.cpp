#include "NAVProcessor.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include "EventLogger.h"
#include "ConfigManager.h"
#include "MessageBuilder.h"

// External processor instances from main.cpp
extern GNSSProcessor gnssProcessor;
extern IMUProcessor imuProcessor;

// External UDP send function
extern void sendUDPbytes(uint8_t *message, int msgLen);

// Static instance
NAVProcessor* NAVProcessor::instance = nullptr;

NAVProcessor::NAVProcessor() {
    instance = this;
    
    // Initialize timing
    lastGPSMessageTime = 0;
    lastGPSUpdateTime = 0;
    
    // Initialize PAOGI duplicate detection
    lastPAOGILatitude = 0.0;
    lastPAOGILongitude = 0.0;
    
    // Clear message buffer
    memset(messageBuffer, 0, BUFFER_SIZE);
    
    LOG_INFO(EventSource::GNSS, "NAVProcessor initialized");
    
    // Report what we see from GNSSProcessor during startup
    const auto& gnssData = gnssProcessor.getData();
    
    if (gnssData.messageTypeMask == 0) {
        LOG_DEBUG(EventSource::GNSS, "  No NMEA data available yet");
    } else {
        LOG_DEBUG(EventSource::GNSS, "  NMEA messages detected (mask=0x%02X)", gnssData.messageTypeMask);
        
        if (!gnssData.hasPosition) {
            LOG_DEBUG(EventSource::GNSS, "  No position fix (quality=%d, sats=%d)", 
                     gnssData.fixQuality, gnssData.numSatellites);
        } else {
            if (gnssData.hasDualHeading) {
                LOG_DEBUG(EventSource::GNSS, "  Dual antenna mode (heading=%.1f°)", gnssData.dualHeading);
            } else if (gnssData.hasINS) {
                LOG_DEBUG(EventSource::GNSS, "  INS mode (align status=%d)", gnssData.insAlignmentStatus);
            } else {
                LOG_DEBUG(EventSource::GNSS, "  Single antenna mode");
            }
        }
    }
}

NAVProcessor::~NAVProcessor() {
    instance = nullptr;
}

NAVProcessor* NAVProcessor::getInstance() {
    return instance;
}

void NAVProcessor::init() {
    if (instance == nullptr) {
        instance = new NAVProcessor();
        // Note: navPTR is set in main.cpp after calling init()
    }
}

NavMessageType NAVProcessor::selectMessageType() {
    const auto& gnssData = gnssProcessor.getData();
    
    // Debug logging
    static uint32_t lastDebugTime = 0;
    if (millis() - lastDebugTime > 10000) {  // Log every 10 seconds
        lastDebugTime = millis();
        LOG_DEBUG(EventSource::GNSS, "selectMessageType: hasDualHeading=%d, hasINS=%d, hasFix=%d, hasGPS=%d, msgMask=0x%02X", 
                  gnssData.hasDualHeading, gnssData.hasINS, gnssProcessor.hasFix(), 
                  gnssProcessor.hasGPS(), gnssData.messageTypeMask);
    }
    
    // For dual/INS systems, send PAOGI even without fix (for INS_ALIGNING state)
    if (gnssData.hasDualHeading || gnssData.hasINS) {
        return NavMessageType::PAOGI;  // Dual GPS/INS detected
    }
    
    // For single GPS, require fix
    if (!gnssProcessor.hasFix()) {
        return NavMessageType::NONE;
    }
    
    return NavMessageType::PANDA;  // Single GPS with fix
}

void NAVProcessor::convertToNMEACoordinates(double decimalDegrees, bool isLongitude, 
                                           double& nmeaValue, char& direction) {
    // Determine direction
    if (isLongitude) {
        direction = (decimalDegrees < 0) ? 'W' : 'E';
    } else {
        direction = (decimalDegrees < 0) ? 'S' : 'N';
    }
    
    // Work with absolute value
    double absDegrees = fabs(decimalDegrees);
    
    // Extract degrees and minutes
    int degrees = (int)absDegrees;
    double minutes = (absDegrees - degrees) * 60.0;
    
    // Format as DDDMM.MMMMM or DDMM.MMMMM
    nmeaValue = degrees * 100.0 + minutes;
}

uint8_t NAVProcessor::calculateNMEAChecksum(const char* sentence) {
    uint8_t checksum = 0;
    
    // Skip the $ and calculate XOR up to * or end
    const char* p = sentence + 1;
    while (*p && *p != '*') {
        checksum ^= *p++;
    }
    
    return checksum;
}

float NAVProcessor::convertGPStoUTC(uint16_t gpsWeek, float gpsSeconds) {
    // GPS epoch (January 6, 1980) to Unix epoch (January 1, 1970) offset
    const uint32_t GPS_EPOCH_OFFSET = 315964800UL;
    
    // Current leap seconds as of 2024
    const uint8_t LEAP_SECONDS = 18;
    
    // Calculate total seconds since GPS epoch
    uint32_t totalGPSSeconds = (uint32_t)gpsWeek * 7 * 24 * 60 * 60 + (uint32_t)gpsSeconds;
    
    // Convert to Unix time and adjust for leap seconds
    uint32_t unixTime = GPS_EPOCH_OFFSET + totalGPSSeconds - LEAP_SECONDS;
    
    // Extract UTC time components
    uint32_t secondsToday = unixTime % (24 * 60 * 60);
    uint8_t hours = secondsToday / 3600;
    uint8_t minutes = (secondsToday % 3600) / 60;
    uint8_t seconds = secondsToday % 60;
    
    // Get milliseconds from the fractional part of gpsSeconds
    float fractionalSeconds = gpsSeconds - (uint32_t)gpsSeconds;
    
    // Return as HHMMSS.S format
    return hours * 10000.0f + minutes * 100.0f + seconds + fractionalSeconds;
}

bool NAVProcessor::formatPANDAMessage() {
    if (!gnssProcessor.hasGPS()) {
        LOG_DEBUG(EventSource::GNSS, "PANDA format failed - No GPS data");
        return false;
    }
    
    // Check if GPS data is fresh (max 150ms old for 10Hz GPS)
    if (!gnssProcessor.isDataFresh(150)) {
        LOG_ERROR(EventSource::GNSS, "Skipping PANDA - GPS data too old: %lums", gnssProcessor.getDataAge());
        return false;
    }
    
    const auto& gnssData = gnssProcessor.getData();
    
    // Use cached NMEA coordinates - no conversion needed!
    double latNMEA = gnssData.latitudeNMEA;
    double lonNMEA = gnssData.longitudeNMEA;
    char latDir = gnssData.latDir;
    char lonDir = gnssData.lonDir;
    
    // Get IMU data if available - using strings like old code
    char imuHeading[10] = "65535";  // Default "no IMU" value
    char imuRoll[10] = "0";         // Default "no IMU" value
    char imuPitch[10] = "0";        // Default "no IMU" value
    char imuYawRate[10] = "0";      // Default "no IMU" value
    
    if (imuProcessor.hasValidData()) {
        const auto& imuData = imuProcessor.getCurrentData();
        snprintf(imuHeading, sizeof(imuHeading), "%d", (int)(imuData.heading * 10.0));
        snprintf(imuRoll, sizeof(imuRoll), "%d", (int)round(imuData.roll));
        snprintf(imuPitch, sizeof(imuPitch), "%d", (int)round(imuData.pitch));
        snprintf(imuYawRate, sizeof(imuYawRate), "%.2f", imuData.yawRate);
    }
    
    // Format time (HHMMSS.S)
    // Use the fractional seconds from the GPS data if available
    float timeFloat = gnssData.fixTime + gnssData.fixTimeFractional;
    
    // Build PANDA message using MessageBuilder
    NMEAMessageBuilder builder(messageBuffer);
    
    builder.addString("$PANDA");
    builder.addComma();
    
    // Add time
    builder.addFloat(timeFloat, 1);
    builder.addComma();
    
    // Add latitude
    builder.addLatitude(latNMEA);
    builder.addComma();
    builder.addChar(latDir);
    builder.addComma();
    
    // Add longitude
    builder.addLongitude(lonNMEA);
    builder.addComma();
    builder.addChar(lonDir);
    builder.addComma();
    
    // Add navigation fields
    builder.addInt(gnssData.fixQuality);
    builder.addComma();
    builder.addInt(gnssData.numSatellites);
    builder.addComma();
    builder.addFloat(gnssData.hdop, 1);
    builder.addComma();
    builder.addFloat(gnssData.altitude, 3);
    builder.addComma();
    builder.addFloat((float)gnssData.ageDGPS, 1);
    builder.addComma();
    builder.addFloat(gnssData.speedKnots, 3);
    builder.addComma();
    
    // Add IMU fields
    builder.addString(imuHeading);
    builder.addComma();
    builder.addString(imuRoll);
    builder.addComma();
    builder.addString(imuPitch);
    builder.addComma();
    builder.addString(imuYawRate);
    
    // Add checksum and terminate
    builder.addChecksum();
    builder.terminate();
    
    return true;
}

bool NAVProcessor::formatPAOGIMessage() {
    if (!gnssProcessor.getData().hasDualHeading) {
        return false;
    }
    // Allow PAOGI even without valid position for INS_ALIGNING state
    
    // Check if GPS data is fresh (max 150ms old for 10Hz GPS)
    if (!gnssProcessor.isDataFresh(150)) {
        LOG_ERROR(EventSource::GNSS, "Skipping PAOGI - GPS data too old: %lums", gnssProcessor.getDataAge());
        return false;
    }
    
    const auto& gnssData = gnssProcessor.getData();
    
    // Check for duplicate position
    if (gnssData.latitude == lastPAOGILatitude && 
        gnssData.longitude == lastPAOGILongitude && 
        lastPAOGILatitude != 0.0) {  // Don't log on first valid position
        static uint32_t lastDuplicateLog = 0;
        if (millis() - lastDuplicateLog > 5000) {  // Log every 5 seconds max
            lastDuplicateLog = millis();
            LOG_WARNING(EventSource::GNSS, "PAOGI: Duplicate position detected: %.8f, %.8f", 
                       gnssData.latitude, gnssData.longitude);
        }
    }
    
    // Update last position
    lastPAOGILatitude = gnssData.latitude;
    lastPAOGILongitude = gnssData.longitude;
    
    // Use cached NMEA coordinates - no conversion needed!
    double latNMEA = gnssData.latitudeNMEA;
    double lonNMEA = gnssData.longitudeNMEA;
    char latDir = gnssData.latDir;
    char lonDir = gnssData.lonDir;
    
    // Get IMU data if available (for pitch and yaw rate)
    int16_t pitch = 0;
    float yawRate = 0.0;
    
    // Prefer INS pitch if available (from UM981)
    if (gnssData.hasINS) {
        pitch = (int16_t)round(gnssData.insPitch);
    }
    else if (imuProcessor.hasValidData()) {
        const auto& imuData = imuProcessor.getCurrentData();
        pitch = (int16_t)round(imuData.pitch);
        yawRate = imuData.yawRate;
    }
    
    // Use dual GPS roll (from KSXT pitch field)
    float roll = gnssData.dualRoll;
    
    // Format time - use UTC from GPS week/seconds if available
    float timeFloat;
    if (gnssData.gpsWeek > 0 && gnssData.gpsSeconds > 0) {
        // Convert GPS time to UTC
        timeFloat = convertGPStoUTC(gnssData.gpsWeek, gnssData.gpsSeconds);
    } else {
        // Fallback to fixTime with fractional seconds from GPS
        timeFloat = gnssData.fixTime + gnssData.fixTimeFractional;
    }
    
    // Build PAOGI message using MessageBuilder
    NMEAMessageBuilder builder(messageBuffer);
    
    builder.addString("$PAOGI");
    builder.addComma();
    
    // Add time
    builder.addFloat(timeFloat, 1);
    builder.addComma();
    
    // Add latitude
    builder.addLatitude(latNMEA);
    builder.addComma();
    builder.addChar(latDir);
    builder.addComma();
    
    // Add longitude
    builder.addLongitude(lonNMEA);
    builder.addComma();
    builder.addChar(lonDir);
    builder.addComma();
    
    // Add navigation fields
    builder.addInt(gnssData.fixQuality);
    builder.addComma();
    builder.addInt(gnssData.numSatellites);
    builder.addComma();
    builder.addFloat(gnssData.hdop, 1);
    builder.addComma();
    builder.addFloat(gnssData.altitude, 3);
    builder.addComma();
    builder.addFloat((float)gnssData.ageDGPS, 1);
    builder.addComma();
    builder.addFloat(gnssData.speedKnots, 3);
    builder.addComma();
    
    // Add IMU/dual antenna fields
    builder.addFloat(gnssData.dualHeading, 1);
    builder.addComma();
    builder.addFloat(roll, 2);
    builder.addComma();
    builder.addInt(pitch);
    builder.addComma();
    builder.addFloat(yawRate, 2);
    
    // Add checksum and terminate
    builder.addChecksum();
    builder.terminate();
    
    return true;
}

void NAVProcessor::sendMessage(const char* message) {
    // NMEA messages must end with CR+LF
    // Buffer size increased by 3 to accommodate "\r\n" and null terminator
    char buffer[BUFFER_SIZE + 3];
    snprintf(buffer, sizeof(buffer), "%s\r\n", message);
    
    // Send via UDP to AgIO
    sendUDPbytes((uint8_t*)buffer, strlen(buffer));
    
}

void NAVProcessor::process() {
    // Check if UDP passthrough is enabled - if so, don't send PANDA/PAOGI
    extern ConfigManager configManager;
    bool passthroughEnabled = configManager.getGPSPassThrough();
    
    // Debug log periodically
    static uint32_t lastDebugLog = 0;
    if (millis() - lastDebugLog > 5000) {
        lastDebugLog = millis();
        LOG_DEBUG(EventSource::GNSS, "NAVProcessor: UDP passthrough is %s", 
                  passthroughEnabled ? "ENABLED" : "DISABLED");
    }
    
    if (passthroughEnabled) {
        return;  // UDP passthrough is enabled, skip PANDA/PAOGI messages
    }
    
    // For single antenna systems, we need at least position data
    // For dual/INS systems, we can send messages even without full fix (for alignment)
    const auto& gnssData = gnssProcessor.getData();
    bool isDualSystem = gnssData.hasDualHeading || gnssData.hasINS;
    
    if (!isDualSystem && !gnssData.hasPosition) {
        return;  // Don't send messages yet
    }
    
    // Check if we have new GPS data since last send
    if (!hasNewGPSData()) {
        // No new GPS data, skip this cycle
        return;
    }
    
    // Select and format appropriate message type
    NavMessageType msgType = selectMessageType();
    bool success = false;
    
    // Track message type changes
    static NavMessageType lastMsgType = NavMessageType::NONE;
    
    if (msgType != lastMsgType) {
        if (msgType != NavMessageType::NONE) {
            LOG_DEBUG(EventSource::GNSS, "Switching to %s messages", 
                      msgType == NavMessageType::PANDA ? "PANDA" : "PAOGI");
        }
        lastMsgType = msgType;
    }
    
    switch (msgType) {
        case NavMessageType::PANDA:
            success = formatPANDAMessage();
            if (success) {
                sendMessage(messageBuffer);
                // Message sent successfully
                lastGPSUpdateTime = gnssProcessor.getData().lastUpdateTime;
            }
            break;
            
        case NavMessageType::PAOGI:
            success = formatPAOGIMessage();
            if (success) {
                sendMessage(messageBuffer);
                // Message sent successfully
                lastGPSUpdateTime = gnssProcessor.getData().lastUpdateTime;
            }
            break;
            
        case NavMessageType::NONE:
        default:
            // No valid data to send
            break;
    }
    
    if (!success && msgType != NavMessageType::NONE) {
        // Message format error occurred
        LOG_ERROR(EventSource::GNSS, "Failed to format %s message", 
                  msgType == NavMessageType::PANDA ? "PANDA" : "PAOGI");
    }
    
    lastGPSMessageTime = millis();
}

void NAVProcessor::setMessageRate(uint32_t intervalMs) {
    // Clamp to reasonable values (1Hz to 100Hz)
    if (intervalMs >= 10 && intervalMs <= 1000) {
        const_cast<uint32_t&>(MESSAGE_INTERVAL_MS) = intervalMs;
    }
}

NavMessageType NAVProcessor::getCurrentMessageType() {
    return selectMessageType();
}

// Removed complex detection methods - simplified checking in process()

void NAVProcessor::printStatus() {
    LOG_INFO(EventSource::GNSS, "=== NAVProcessor Status ===");
    
    NavMessageType currentType = getCurrentMessageType();
    LOG_INFO(EventSource::GNSS, "Current mode: %s", 
        currentType == NavMessageType::PANDA ? "PANDA (Single GPS)" :
        currentType == NavMessageType::PAOGI ? "PAOGI (Dual GPS)" : "NONE");
    
    LOG_INFO(EventSource::GNSS, "Message rate: %d Hz", 1000 / MESSAGE_INTERVAL_MS);
    
    if (lastGPSMessageTime > 0) {
        LOG_INFO(EventSource::GNSS, "Time since last GPS message: %lu ms", 
            millis() - lastGPSMessageTime);
    }
    
    // Show data sources
    LOG_INFO(EventSource::GNSS, "Data sources:");
    if (gnssProcessor.isValid()) {
        const auto& gnssData = gnssProcessor.getData();
        LOG_INFO(EventSource::GNSS, "  GPS: Valid (Fix=%d, Sats=%d)", 
            gnssData.fixQuality, gnssData.numSatellites);
        if (gnssData.hasDualHeading) {
            LOG_INFO(EventSource::GNSS, "  Dual GPS: Active (Quality=%d)", 
                gnssData.headingQuality);
        }
    } else {
        LOG_INFO(EventSource::GNSS, "  GPS: No valid fix");
    }
    
    if (imuProcessor.hasValidData()) {
        LOG_INFO(EventSource::GNSS, "  IMU: %s connected", imuProcessor.getIMUTypeName());
    } else {
        LOG_INFO(EventSource::GNSS, "  IMU: Not detected");
    }
}

bool NAVProcessor::hasNewGPSData() const {
    const auto& gnssData = gnssProcessor.getData();
    return gnssData.lastUpdateTime > lastGPSUpdateTime;
}