#include "EventLogger.h"
#include "LogWebSocket.h"
#include "EEPROMLayout.h"
#include "EEPROM.h"
#include "QNetworkBase.h"
#include <QNEthernet.h>
#include <QNEthernetUDP.h>
#include <cstdio>
#include <cstring>

// External function for sending UDP
extern void sendUDPbytes(uint8_t* data, int length);

// Static UDP instance for syslog
static EthernetUDP udpSyslog;

// QNEthernet namespace
using namespace qindesign::network;

// Static instance pointer
EventLogger* EventLogger::instance = nullptr;

// Use EEPROM layout from header

EventLogger::EventLogger() {
    loadConfig();

    // Initialize WebSocket pointer
    logWebSocket = nullptr;

    // Initialize token buckets
    uint32_t now = millis();
    for (int i = 0; i < 8; i++) {
        buckets[i].tokens = maxMessagesPerSecond[i];
        buckets[i].lastRefillTime = now;
    }

    // Initialize UDP socket for syslog
    // QNEthernet UDP sockets don't need explicit begin() call
    // They are initialized on first use
}

EventLogger::~EventLogger() {
    instance = nullptr;
}

EventLogger* EventLogger::getInstance() {
    if (instance == nullptr) {
        instance = new EventLogger();
    }
    return instance;
}

void EventLogger::init() {
    getInstance();  // Create instance if needed
}

void EventLogger::log(EventSeverity severity, EventSource source, const char* format, ...) {
    // Check rate limiting first (unless disabled)
    if (!config.disableRateLimit && !checkRateLimit(severity)) {
        return;
    }

    // Format the message
    va_list args;
    va_start(args, format);
    vsnprintf(messageBuffer, sizeof(messageBuffer), format, args);
    va_end(args);

    eventCounter++;

    // Add to circular buffer for web viewer
    addToBuffer(severity, source, messageBuffer);

    // Output to enabled channels
    if (config.enableSerial && shouldLog(severity, false)) {
        outputSerial(severity, source, messageBuffer);
    }

    if (config.enableUDP && shouldLog(severity, true)) {
        outputUDP(severity, source, messageBuffer);
    }
}

void EventLogger::outputSerial(EventSeverity severity, EventSource source, const char* message) {
    // Format: [HH:MM:SS.mmm] SEVERITY/SOURCE: message
    uint32_t now = millis();
    uint32_t hours = (now / 3600000) % 24;
    uint32_t minutes = (now / 60000) % 60;
    uint32_t seconds = (now / 1000) % 60;
    uint32_t milliseconds = now % 1000;
    
    Serial.printf("[%02lu:%02lu:%02lu.%03lu] %s/%s: %s\r\n",
                  hours, minutes, seconds, milliseconds,
                  severityNames[static_cast<uint8_t>(severity)],
                  sourceNames[static_cast<uint8_t>(source)],
                  message);
}

void EventLogger::outputUDP(EventSeverity severity, EventSource source, const char* message) {
    // RFC3164 syslog format: <priority>timestamp hostname tag: message
    // Priority = facility * 8 + severity
    // We'll use facility 16 (local0) as base + our source as offset
    uint8_t facility = 16 + static_cast<uint8_t>(source);
    uint8_t priority = facility * 8 + static_cast<uint8_t>(severity);
    
    // Get timestamp in syslog format (simplified - no RTC)
    uint32_t now = millis();
    uint32_t days = now / 86400000;
    uint32_t hours = (now / 3600000) % 24;
    uint32_t minutes = (now / 60000) % 60;
    uint32_t seconds = (now / 1000) % 60;
    
    // Month approximation (30 days per month)
    const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", 
                           "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    uint8_t month = (days / 30) % 12;
    uint8_t day = (days % 30) + 1;
    
    // Format syslog message
    char syslogMsg[512];
    snprintf(syslogMsg, sizeof(syslogMsg), 
             "<%d>%s %2d %02lu:%02lu:%02lu AiO-%s[%lu]: %s",
             priority,
             months[month],
             day,
             hours, minutes, seconds,
             sourceNames[static_cast<uint8_t>(source)],
             eventCounter,
             message);
    
    // Send via UDP broadcast to syslog port
    if (QNetworkBase::isConnected()) {
        // Get current IP for broadcast address
        IPAddress currentIP = QNetworkBase::getIP();
        
        // Create broadcast address (xxx.xxx.xxx.255)
        IPAddress broadcastIP(currentIP[0], currentIP[1], currentIP[2], 255);
        
        // Get syslog port from config
        uint16_t port = (config.syslogPort[0] << 8) | config.syslogPort[1];
        
        // Send using QNEthernet UDP
        udpSyslog.beginPacket(broadcastIP, port);
        udpSyslog.write((const uint8_t*)syslogMsg, strlen(syslogMsg));
        udpSyslog.endPacket();
    }
}

bool EventLogger::shouldLog(EventSeverity severity, bool forUDP) {
    // During startup, log everything to serial (UDP still respects configured level)
    if (startupMode && !forUDP) {
        return true;  // Log all messages during startup
    }
    
    uint8_t level = forUDP ? config.udpLevel : config.serialLevel;
    return static_cast<uint8_t>(severity) <= level;
}

bool EventLogger::checkRateLimit(EventSeverity severity) {
    // No rate limiting during startup mode
    if (startupMode) {
        return true;
    }
    
    uint8_t sevIndex = static_cast<uint8_t>(severity);
    uint32_t now = millis();
    
    // Get the bucket for this severity level
    TokenBucket& bucket = buckets[sevIndex];
    uint8_t maxPerSecond = maxMessagesPerSecond[sevIndex];
    
    // Calculate time elapsed since last refill
    uint32_t elapsed = now - bucket.lastRefillTime;
    if (elapsed > RATE_WINDOW_MS) {
        // More than 1 second passed, reset bucket
        bucket.tokens = maxPerSecond;
        bucket.lastRefillTime = now;
    } else if (elapsed > 0) {
        // Refill tokens based on time elapsed
        float tokensToAdd = (float)elapsed * maxPerSecond / RATE_WINDOW_MS;
        bucket.tokens = min(bucket.tokens + tokensToAdd, (float)maxPerSecond);
        bucket.lastRefillTime = now;
    }
    
    // Check if we have a token available
    if (bucket.tokens >= 1.0f) {
        bucket.tokens -= 1.0f;
        return true;
    }
    
    return false;
}

void EventLogger::loadConfig() {
    // Check if we have valid config in EEPROM
    uint8_t marker;
    EEPROM.get(EVENT_CONFIG_ADDR - 1, marker);
    
    if (marker == 0xEE) {  // Valid config marker
        EEPROM.get(EVENT_CONFIG_ADDR, config);
    } else {
        // Use defaults and save
        saveConfig();
    }
}

void EventLogger::saveConfig() {
    uint8_t marker = 0xEE;
    EEPROM.put(EVENT_CONFIG_ADDR - 1, marker);
    EEPROM.put(EVENT_CONFIG_ADDR, config);
}

void EventLogger::setSerialLevel(EventSeverity level) {
    config.serialLevel = static_cast<uint8_t>(level);
    saveConfig();
}

void EventLogger::setUDPLevel(EventSeverity level) {
    config.udpLevel = static_cast<uint8_t>(level);
    saveConfig();
}

void EventLogger::enableSerial(bool enable) {
    config.enableSerial = enable;
    saveConfig();
}

void EventLogger::enableUDP(bool enable) {
    config.enableUDP = enable;
    saveConfig();
}

EventSeverity EventLogger::stringToSeverity(const char* str) {
    for (int i = 0; i < 8; i++) {
        if (strcasecmp(str, severityNames[i]) == 0) {
            return static_cast<EventSeverity>(i);
        }
    }
    return EventSeverity::INFO;  // Default
}

const char* EventLogger::severityToString(EventSeverity severity) {
    uint8_t index = static_cast<uint8_t>(severity);
    if (index < 8) {
        return severityNames[index];
    }
    return "UNKNOWN";
}

const char* EventLogger::sourceToString(EventSource source) {
    uint8_t index = static_cast<uint8_t>(source);
    if (index < 9) {
        return sourceNames[index];
    }
    return "UNKNOWN";
}

void EventLogger::printConfig() {
    Serial.println("\r\n===== Event Logger Configuration =====");
    Serial.printf("Serial Output: %s (Level: %s%s)\r\n", 
                  config.enableSerial ? "ENABLED" : "DISABLED",
                  severityNames[config.serialLevel],
                  startupMode ? " - STARTUP MODE" : "");
    Serial.printf("UDP Syslog: %s (Level: %s, Port: %d)\r\n",
                  config.enableUDP ? "ENABLED" : "DISABLED",
                  severityNames[config.udpLevel],
                  (config.syslogPort[0] << 8) | config.syslogPort[1]);
    Serial.printf("Rate Limiting: %s\r\n", 
                  config.disableRateLimit ? "DISABLED" : "ENABLED");
    // QNEthernet handles its own logging internally
    Serial.printf("Total Events Logged: %lu\r\n", eventCounter);
    Serial.println("=====================================");
}

// QNEthernet doesn't need explicit log level management - removed setMongooseLogLevel

void EventLogger::checkNetworkReady() {
    // Check current network state using QNEthernet
    bool networkReady = QNetworkBase::isConnected() && Ethernet.linkStatus();
    
    // Track network state changes for stability
    if (!networkReady && networkWasReady) {
        // Network went down - reset our tracking
        networkWasReady = false;
        lastNetworkDownTime = millis();
    } else if (networkReady && !networkWasReady) {
        // Network came up - but wait to ensure it's stable
        if (millis() - lastNetworkDownTime > 1000) {  // Only if network was down for > 1 second
            networkWasReady = true;
            networkReadyTime = millis();
            
            // Log the assigned IP address
            IPAddress ip = QNetworkBase::getIP();
            LOG_INFO(EventSource::NETWORK, "Network ready - IP Address: %d.%d.%d.%d", 
                     ip[0], ip[1], ip[2], ip[3]);
        }
    }
    
    // Show system ready message 3 seconds after network is stable
    if (!systemReadyShown && networkWasReady && networkReady && (millis() - networkReadyTime > 3000)) {
        systemReadyShown = true;
        
        // Display the complete boxed message as separate lines to avoid rate limiting
        // Use Serial.print directly for the visual box to ensure it displays properly
        Serial.println("\r\n**************************************************");
        if (config.enableUDP) {
            Serial.printf("*** System ready - UDP syslog active at %s level ***\r\n", 
                          getLevelName(getEffectiveLogLevel()));
        } else {
            Serial.println("*** System ready - UDP syslog disabled ***");
        }
        Serial.println("*** Press '?' for menu ***");
        Serial.println("**************************************************\r\n");
        
        // Send a syslog-friendly message with menu instructions
        LOG_WARNING(EventSource::SYSTEM, "* System ready - Press '?' for menu *");
    }
}

void EventLogger::setStartupMode(bool startup) {
    if (!startup && startupMode) {
        // Exiting startup mode - now enforce configured levels
        startupMode = false;
        LOG_INFO(EventSource::SYSTEM, "System initialization complete - enforcing log level: %s", 
                 severityNames[config.serialLevel]);
    }
    // Note: We don't support re-entering startup mode after exiting
}

void EventLogger::setRateLimitEnabled(bool enabled) {
    config.disableRateLimit = !enabled;
    saveConfig();

    if (enabled) {
        LOG_INFO(EventSource::SYSTEM, "Rate limiting ENABLED");
    } else {
        LOG_WARNING(EventSource::SYSTEM, "Rate limiting DISABLED - all messages will be logged!");
    }
}

void EventLogger::addToBuffer(EventSeverity severity, EventSource source, const char* message) {
    // Add entry to circular buffer
    LogEntry& entry = logBuffer[logBufferHead];
    entry.timestamp = millis();
    entry.severity = severity;
    entry.source = source;

    // Truncate message to fit buffer
    strncpy(entry.message, message, sizeof(entry.message) - 1);
    entry.message[sizeof(entry.message) - 1] = '\0';

    // Broadcast to WebSocket clients if available
    if (logWebSocket) {
        logWebSocket->broadcastLog(entry.timestamp, severity, source, entry.message);
    }

    // Advance head pointer
    logBufferHead = (logBufferHead + 1) % LOG_BUFFER_SIZE;

    // Track count (up to max)
    if (logBufferCount < LOG_BUFFER_SIZE) {
        logBufferCount++;
    }
}