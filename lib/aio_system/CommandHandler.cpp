#include "CommandHandler.h"
#include "ConfigManager.h"
#include "HardwareManager.h"

// Static instance pointer
CommandHandler* CommandHandler::instance = nullptr;

CommandHandler::CommandHandler() {
    loggerPtr = EventLogger::getInstance();
}

CommandHandler::~CommandHandler() {
    instance = nullptr;
}

CommandHandler* CommandHandler::getInstance() {
    if (instance == nullptr) {
        instance = new CommandHandler();
    }
    return instance;
}

void CommandHandler::init() {
    getInstance();  // Create instance if needed
}

void CommandHandler::process() {
    if (!Serial.available()) {
        return;
    }
    
    char cmd = Serial.read();
    
    // Ignore line ending characters (CR and LF)
    if (cmd == '\r' || cmd == '\n') {
        return;
    }
    
    // Handle command
    handleCommand(cmd);
}

void CommandHandler::handleCommand(char cmd) {
    EventConfig& config = loggerPtr->getConfig();
    
    switch (cmd) {
        case '1':  // Toggle serial output
            loggerPtr->enableSerial(!config.enableSerial);
            Serial.printf("\r\nSerial logging %s\r\n", config.enableSerial ? "ENABLED" : "DISABLED");
            break;
            
        case '2':  // Toggle UDP syslog
            loggerPtr->enableUDP(!config.enableUDP);
            Serial.printf("\r\nUDP syslog %s\r\n", config.enableUDP ? "ENABLED" : "DISABLED");
            break;
            
        case '3':  // Increase serial level
            if (config.serialLevel > 0) {
                config.serialLevel--;
                loggerPtr->setSerialLevel(static_cast<EventSeverity>(config.serialLevel));
                Serial.printf("\r\nSerial level: %s\r\n", loggerPtr->severityToString(static_cast<EventSeverity>(config.serialLevel)));
            }
            break;
            
        case '4':  // Decrease serial level
            if (config.serialLevel < 7) {
                config.serialLevel++;
                loggerPtr->setSerialLevel(static_cast<EventSeverity>(config.serialLevel));
                Serial.printf("\r\nSerial level: %s\r\n", loggerPtr->severityToString(static_cast<EventSeverity>(config.serialLevel)));
            }
            break;
            
        case '5':  // Increase UDP level
            if (config.udpLevel > 0) {
                config.udpLevel--;
                loggerPtr->setUDPLevel(static_cast<EventSeverity>(config.udpLevel));
                Serial.printf("\r\nUDP level: %s\r\n", loggerPtr->severityToString(static_cast<EventSeverity>(config.udpLevel)));
            }
            break;
            
        case '6':  // Decrease UDP level
            if (config.udpLevel < 7) {
                config.udpLevel++;
                loggerPtr->setUDPLevel(static_cast<EventSeverity>(config.udpLevel));
                Serial.printf("\r\nUDP level: %s\r\n", loggerPtr->severityToString(static_cast<EventSeverity>(config.udpLevel)));
            }
            break;
            
        case '7':  // Toggle rate limiting
            loggerPtr->setRateLimitEnabled(!loggerPtr->isRateLimitEnabled());
            break;
            
        case 't':  // Test log messages
        case 'T':
            LOG_INFO(EventSource::USER, "Generating test log messages...");
            LOG_DEBUG(EventSource::USER, "Test DEBUG message");
            LOG_INFO(EventSource::USER, "Test INFO message");
            LOG_NOTICE(EventSource::USER, "Test NOTICE message");
            LOG_WARNING(EventSource::USER, "Test WARNING message");
            LOG_ERROR(EventSource::USER, "Test ERROR message");
            LOG_CRITICAL(EventSource::USER, "Test CRITICAL message");
            LOG_ALERT(EventSource::USER, "Test ALERT message");
            LOG_EMERGENCY(EventSource::USER, "Test EMERGENCY message");
            break;
            
        case 's':  // Show statistics
        case 'S':
            Serial.printf("\r\n\nEvent Statistics:");
            Serial.printf("\r\nTotal events logged: %lu\r\n", loggerPtr->getEventCount());
            break;
            
        case 'r':  // Reset counter
        case 'R':
            loggerPtr->resetEventCount();
            Serial.print("Event counter reset\r\n");
            break;
            
        // QNEthernet doesn't need explicit log level management
        // Removed Mongoose log level option
            
        case 'l':  // Loop timing diagnostics (moved from T to avoid conflict)
        case 'L':
            toggleLoopTiming();
            break;
            
        case 'b':  // Buzzer test
        case 'B':
            {
                extern HardwareManager hardwareManager;
                Serial.print("\r\nTesting buzzer...\r\n");
                hardwareManager.performBuzzerTest();
            }
            break;
            
        case 'v':  // Toggle buzzer volume
        case 'V':
            {
                extern ConfigManager configManager;
                bool currentMode = configManager.getBuzzerLoudMode();
                configManager.setBuzzerLoudMode(!currentMode);
                configManager.saveMiscConfig();  // Save to EEPROM
                Serial.printf("\r\nBuzzer volume set to: %s\r\n", 
                             (!currentMode) ? "LOUD (field use)" : "QUIET (development)");
            }
            break;
            
        case '?':
        case 'h':
        case 'H':
            showMenu();
            break;
            
        default:
            Serial.printf("\r\nUnknown command: '%c'\r\n", cmd);
            break;
    }
}


void CommandHandler::showMenu() {
    loggerPtr->printConfig();
    Serial.print("\r\n=== Firmware Controls ===");
    Serial.print("\r\n1 - Toggle serial output");
    Serial.print("\r\n2 - Toggle UDP syslog");
    Serial.print("\r\n3/4 - Decrease/Increase serial level");
    Serial.print("\r\n5/6 - Decrease/Increase UDP level");
    Serial.print("\r\n7 - Toggle rate limiting");
    Serial.print("\r\nT - Generate test messages");
    Serial.print("\r\nS - Show statistics");
    Serial.print("\r\nR - Reset event counter");
    Serial.print("\r\nL - Toggle loop timing diagnostics");
    Serial.print("\r\nB - Test buzzer");
    Serial.print("\r\nV - Toggle buzzer volume (loud/quiet)");
    Serial.print("\r\n? - Show this menu");
    Serial.print("\r\n=========================\r\n");
}


