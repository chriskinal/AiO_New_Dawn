// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

#ifndef EVENTLOGGER_H_
#define EVENTLOGGER_H_

#include "Arduino.h"
#include <vector>
#include <cstdarg>

// Forward declaration to avoid circular dependency
class LogWebSocket;

// Event severity levels (following syslog standards)
enum class EventSeverity : uint8_t {
    EMERGENCY = 0,  // System is unusable
    ALERT = 1,      // Action must be taken immediately
    CRITICAL = 2,   // Critical conditions
    ERROR = 3,      // Error conditions
    WARNING = 4,    // Warning conditions
    NOTICE = 5,     // Normal but significant condition
    INFO = 6,       // Informational messages
    DEBUG = 7       // Debug-level messages
};

// Event sources (facilities in syslog terms)
enum class EventSource : uint8_t {
    SYSTEM = 0,
    NETWORK = 1,
    GNSS = 2,
    IMU = 3,
    AUTOSTEER = 4,
    MACHINE = 5,
    CAN = 6,
    CONFIG = 7,
    USER = 8
};

// Event configuration stored in EEPROM
struct EventConfig {
    uint8_t serialLevel = static_cast<uint8_t>(EventSeverity::INFO);     // Default to INFO and above
    uint8_t udpLevel = static_cast<uint8_t>(EventSeverity::WARNING);     // Default to WARNING and above
    bool enableSerial = true;
    bool enableUDP = false;
    uint8_t syslogPort[2] = {2, 2};  // Port 514 (0x0202)
    bool disableRateLimit = false;  // Flag to disable rate limiting
    uint8_t reserved[9];  // Future expansion
};

// Log entry for circular buffer (web viewer)
struct LogEntry {
    uint32_t timestamp;       // millis() when logged
    EventSeverity severity;
    EventSource source;
    char message[128];        // Truncated message for buffer
};

class EventLogger {
private:
    static EventLogger* instance;
    EventConfig config;
    char messageBuffer[256];
    uint32_t eventCounter = 0;
    
    // For rate limiting - token bucket algorithm
    static constexpr uint32_t RATE_WINDOW_MS = 1000;  // 1 second window
    
    // Maximum messages per second for each severity level
    uint8_t maxMessagesPerSecond[8] = {
        100,  // EMERG - no limit (100/sec is effectively unlimited)
        100,  // ALERT - no limit
        50,   // CRIT - 50/sec
        50,   // ERROR - 50/sec  
        10,   // WARN - 10/sec (was 100ms = 10/sec)
        10,   // NOTICE - 10/sec
        10,   // INFO - 10/sec (was 100ms = 10/sec)
        5     // DEBUG - 5/sec (was 200ms = 5/sec)
    };
    
    // Token bucket state for each severity level
    struct TokenBucket {
        float tokens = 0;
        uint32_t lastRefillTime = 0;
    };
    TokenBucket buckets[8];
    
    // Severity names for display
    const char* severityNames[8] = {
        "EMERG", "ALERT", "CRIT", "ERROR", "WARN", "NOTICE", "INFO", "DEBUG"
    };
    
    // Source names for display
    const char* sourceNames[9] = {
        "SYS", "NET", "GNSS", "IMU", "STEER", "MACH", "CAN", "CFG", "USER"
    };
    
    EventLogger();
    
    // Format message according to RFC3164 syslog format
    void formatSyslog(EventSeverity severity, EventSource source, const char* message);
    
    // Send to outputs
    void outputSerial(EventSeverity severity, EventSource source, const char* message);
    void outputUDP(EventSeverity severity, EventSource source, const char* message);
    
    // Check if we should log this severity level
    bool shouldLog(EventSeverity severity, bool forUDP = false);
    
    // Rate limiting check
    bool checkRateLimit(EventSeverity severity);
    
    // QNEthernet doesn't need explicit log level management
    
    // Startup mode tracking
    bool startupMode = true;  // Don't enforce levels during startup
    
    // Network stability tracking for system ready message
    bool systemReadyShown = false;
    bool networkWasReady = false;
    uint32_t networkReadyTime = 0;
    uint32_t lastNetworkDownTime = 0;

    // Circular buffer for web viewer
    static constexpr size_t LOG_BUFFER_SIZE = 100;
    LogEntry logBuffer[LOG_BUFFER_SIZE];
    size_t logBufferHead = 0;  // Next position to write
    size_t logBufferCount = 0; // Number of entries in buffer

    // Add entry to circular buffer
    void addToBuffer(EventSeverity severity, EventSource source, const char* message);

    // WebSocket for real-time log streaming
    LogWebSocket* logWebSocket;

public:
    ~EventLogger();
    
    // Singleton access
    static EventLogger* getInstance();
    static void init();
    
    // Main logging function
    void log(EventSeverity severity, EventSource source, const char* format, ...);
    
    // Configuration
    void loadConfig();
    void saveConfig();
    void setSerialLevel(EventSeverity level);
    void setUDPLevel(EventSeverity level);
    void enableSerial(bool enable);
    void enableUDP(bool enable);
    
    // Get current config
    EventConfig& getConfig() { return config; }
    
    // Utility to convert string to severity
    EventSeverity stringToSeverity(const char* str);
    const char* severityToString(EventSeverity severity);
    const char* sourceToString(EventSource source);
    
    // Statistics
    uint32_t getEventCount() { return eventCounter; }
    void resetEventCount() { eventCounter = 0; }
    
    // Display current configuration
    void printConfig();
    
    // Network status check
    void checkNetworkReady();  // Check when network is ready
    
    // System startup logging control
    void setStartupMode(bool startup);
    bool isStartupMode() { return startupMode; }
    
    // Rate limiting control
    void setRateLimitEnabled(bool enabled);
    bool isRateLimitEnabled() const { return !config.disableRateLimit; }

    // Get the effective log level for UDP syslog
    EventSeverity getEffectiveLogLevel() {
        return static_cast<EventSeverity>(config.udpLevel);
    }

    // Get human-readable name for a log level
    const char* getLevelName(EventSeverity level) {
        switch(level) {
            case EventSeverity::EMERGENCY: return "EMERGENCY";
            case EventSeverity::ALERT: return "ALERT";
            case EventSeverity::CRITICAL: return "CRITICAL";
            case EventSeverity::ERROR: return "ERROR";
            case EventSeverity::WARNING: return "WARNING";
            case EventSeverity::NOTICE: return "NOTICE";
            case EventSeverity::INFO: return "INFO";
            case EventSeverity::DEBUG: return "DEBUG";
            default: return "UNKNOWN";
        }
    }

    // Web viewer access to log buffer
    size_t getLogBufferCount() const { return logBufferCount; }
    const LogEntry* getLogBuffer() const { return logBuffer; }
    size_t getLogBufferHead() const { return logBufferHead; }
    size_t getLogBufferSize() const { return LOG_BUFFER_SIZE; }

    // WebSocket management
    void setLogWebSocket(LogWebSocket* ws) { logWebSocket = ws; }
    LogWebSocket* getLogWebSocket() const { return logWebSocket; }
};

// Convenience macros for common logging patterns
#define LOG_EMERGENCY(source, ...) EventLogger::getInstance()->log(EventSeverity::EMERGENCY, source, __VA_ARGS__)
#define LOG_ALERT(source, ...) EventLogger::getInstance()->log(EventSeverity::ALERT, source, __VA_ARGS__)
#define LOG_CRITICAL(source, ...) EventLogger::getInstance()->log(EventSeverity::CRITICAL, source, __VA_ARGS__)
#define LOG_ERROR(source, ...) EventLogger::getInstance()->log(EventSeverity::ERROR, source, __VA_ARGS__)
#define LOG_WARNING(source, ...) EventLogger::getInstance()->log(EventSeverity::WARNING, source, __VA_ARGS__)
#define LOG_NOTICE(source, ...) EventLogger::getInstance()->log(EventSeverity::NOTICE, source, __VA_ARGS__)
#define LOG_INFO(source, ...) EventLogger::getInstance()->log(EventSeverity::INFO, source, __VA_ARGS__)
#define LOG_DEBUG(source, ...) EventLogger::getInstance()->log(EventSeverity::DEBUG, source, __VA_ARGS__)

#endif // EVENTLOGGER_H_