// SimpleWebManager.cpp
// Web server implementation using SimpleHTTPServer to replace AsyncWebServer

#include "SimpleWebManager.h"
#include "ConfigManager.h"
#include "EventLogger.h"
#include "Version.h"
#include "HardwareManager.h"
#include "QNetworkBase.h"
#include "ADProcessor.h"
#include "EncoderProcessor.h"
#include "SimpleOTAHandler.h"
#include "TelemetryWebSocket.h"
#include "AutosteerProcessor.h"
#include "NAVProcessor.h"
#include "GNSSProcessor.h"
#include "web_pages/CommonStyles.h"  // Common CSS
#include "web_pages/SimpleDeviceSettingsNoReplace.h"  // Device settings without replacements
#include "web_pages/TouchFriendlyEventLoggerPage.h"  // Touch-friendly event logger page
#include "web_pages/TouchFriendlyLogViewerPage.h"  // Touch-friendly log viewer page
#include "web_pages/TouchFriendlyAnalogWorkSwitchPage.h"  // Touch-friendly analog work switch page
#include "web_pages/TouchFriendlyOTAPage.h"  // Touch-friendly OTA update page
#include "web_pages/TouchFriendlyGPSConfigPage.h"  // Touch-friendly GPS configuration page
#include "web_pages/TouchFriendlyHomePage.h"  // Touch-friendly interface
#include "web_pages/TouchFriendlyStyles.h"  // Touch-friendly CSS
#include "web_pages/TouchFriendlyDeviceSettingsPage.h"  // Touch-friendly device settings
#include "web_pages/TouchFriendlyNetworkPage.h"  // Touch-friendly network settings
#include "web_pages/TouchFriendlyAnalogWorkSwitchPage.h"  // Touch-friendly analog work switch
#include "web_pages/DragDropCANConfigPage.h"  // Drag-and-drop CAN configuration
#include "web_pages/CANInfoJSON.h"  // CAN info JSON data
#include "web_pages/CANConfigUploadPage.h"  // CAN config upload page
#include "CANConfigStorage.h"  // LittleFS storage for custom CAN config
#include <ArduinoJson.h>
#include <QNEthernet.h>
#include "ESP32Interface.h"
#include "UM98xManager.h"
#include "SerialManager.h"

using namespace qindesign::network;

// FPSTR macro for PROGMEM strings
#define FPSTR(pstr_pointer) (reinterpret_cast<const __FlashStringHelper *>(pstr_pointer))

// External references
extern EncoderProcessor* encoderProcessor;
extern GNSSProcessor gnssProcessor;

SimpleWebManager::SimpleWebManager() :
    isRunning(false),
    currentLanguage(WebLanguage::ENGLISH),
    systemReady(false) {
}

SimpleWebManager::~SimpleWebManager() {
    stop();
}

bool SimpleWebManager::begin(uint16_t port) {
    // Initialize LittleFS for custom configuration storage
    if (!CANConfigStorage::init()) {
        LOG_WARNING(EventSource::NETWORK, "LittleFS init failed - custom CAN config not available");
    }

    // Load language preference from EEPROM
    uint8_t savedLang = EEPROM.read(WEB_CONFIG_ADDR);
    if (savedLang <= 1) {  // 0 = English, 1 = German
        currentLanguage = static_cast<WebLanguage>(savedLang);
    }

    // Setup routes first
    setupRoutes();
    
    // Start HTTP server
    if (!httpServer.begin(port)) {
        LOG_ERROR(EventSource::NETWORK, "Failed to start HTTP server");
        return false;
    }
    
    // Start WebSocket telemetry server on port 8082
    if (!telemetryWS.begin(8082)) {
        LOG_WARNING(EventSource::NETWORK, "Failed to start WebSocket telemetry server");
    }

    // Start Log WebSocket server on port 8083
    if (!logWS.begin(8083)) {
        LOG_WARNING(EventSource::NETWORK, "Failed to start Log WebSocket server");
    } else {
        // Connect LogWebSocket to EventLogger
        EventLogger::getInstance()->setLogWebSocket(&logWS);
    }

    isRunning = true;
    
    IPAddress ip = Ethernet.localIP();
    LOG_INFO(EventSource::NETWORK, "Simple web server started on http://%d.%d.%d.%d:%d", 
             ip[0], ip[1], ip[2], ip[3], port);
    
    return true;
}

void SimpleWebManager::stop() {
    if (isRunning) {
        // Disconnect LogWebSocket from EventLogger
        EventLogger::getInstance()->setLogWebSocket(nullptr);

        logWS.stop();
        telemetryWS.stop();
        httpServer.stop();
        isRunning = false;
        LOG_INFO(EventSource::NETWORK, "Simple web server stopped");
    }
}

void SimpleWebManager::handleClient() {
    // Now called by SimpleScheduler at 100Hz
    httpServer.handleClient();
    telemetryWS.handleClients();
    logWS.handleClient();
}

void SimpleWebManager::setupRoutes() {
    // Home page - now using touch-friendly interface
    httpServer.on("/", [this](EthernetClient& client, const String& method, const String& query) {
        sendTouchHomePage(client);
    });
    
    // CSS for touch-friendly interface
    httpServer.on("/touch.css", [this](EthernetClient& client, const String& method, const String& query) {
        extern const char TOUCH_FRIENDLY_CSS[];
        SimpleHTTPServer::send(client, 200, "text/css", FPSTR(TOUCH_FRIENDLY_CSS));
    });
    
    // API status endpoint
    httpServer.on("/api/status", [this](EthernetClient& client, const String& method, const String& query) {
        handleApiStatus(client);
    });
    
    // EventLogger page
    httpServer.on("/eventlogger", [this](EthernetClient& client, const String& method, const String& query) {
        sendEventLoggerPage(client);
    });

    // Log viewer page (tablet-friendly)
    httpServer.on("/logs", [this](EthernetClient& client, const String& method, const String& query) {
        sendLogViewerPage(client);
    });

    // Network settings page
    httpServer.on("/network", [this](EthernetClient& client, const String& method, const String& query) {
        sendNetworkPage(client);
    });
    
    // OTA Update page
    httpServer.on("/ota", [this](EthernetClient& client, const String& method, const String& query) {
        sendOTAPage(client);
    });
    
    // Device Settings page
    httpServer.on("/device", [this](EthernetClient& client, const String& method, const String& query) {
        sendDeviceSettingsPage(client);
    });
    
    // Analog Work Switch page
    httpServer.on("/analogworkswitch", [this](EthernetClient& client, const String& method, const String& query) {
        sendAnalogWorkSwitchPage(client);
    });

    // CAN Configuration page
    httpServer.on("/can", [this](EthernetClient& client, const String& method, const String& query) {
        sendCANConfigPage(client);
    });

    // CAN Config Upload page
    httpServer.on("/can/upload", [this](EthernetClient& client, const String& method, const String& query) {
        sendCANConfigUploadPage(client);
    });

    // WAS Demo page removed - using WebSocket telemetry instead
    
    // Language selection
    httpServer.on("/lang/en", [this](EthernetClient& client, const String& method, const String& query) {
        currentLanguage = WebLanguage::ENGLISH;
        EEPROM.write(WEB_CONFIG_ADDR, static_cast<uint8_t>(currentLanguage));
        SimpleHTTPServer::redirect(client, "/");
    });
    
    httpServer.on("/lang/de", [this](EthernetClient& client, const String& method, const String& query) {
        currentLanguage = WebLanguage::GERMAN;
        EEPROM.write(WEB_CONFIG_ADDR, static_cast<uint8_t>(currentLanguage));
        SimpleHTTPServer::redirect(client, "/");
    });
    
    // API endpoints
    httpServer.on("/api/restart", [](EthernetClient& client, const String& method, const String& query) {
        if (method == "POST") {
            SimpleHTTPServer::sendJSON(client, "{\"status\":\"restarting\"}");
            delay(100);
            SCB_AIRCR = 0x05FA0004;  // System reset
        } else {
            SimpleHTTPServer::send(client, 405, "text/plain", "Method Not Allowed");
        }
    });
    
    // EventLogger API
    httpServer.on("/api/eventlogger/config", [this](EthernetClient& client, const String& method, const String& query) {
        handleEventLoggerConfig(client, method);
    });

    // Log viewer API
    httpServer.on("/api/logs/data", [this](EthernetClient& client, const String& method, const String& query) {
        handleLogViewerData(client);
    });

    // Network API
    httpServer.on("/api/network/config", [this](EthernetClient& client, const String& method, const String& query) {
        handleNetworkConfig(client, method);
    });
    
    // Device settings API
    httpServer.on("/api/device/settings", [this](EthernetClient& client, const String& method, const String& query) {
        handleDeviceSettings(client, method);
    });
    
    // Analog work switch API
    httpServer.on("/api/analogworkswitch/status", [this](EthernetClient& client, const String& method, const String& query) {
        handleAnalogWorkSwitchStatus(client);
    });
    
    httpServer.on("/api/analogworkswitch/config", [this](EthernetClient& client, const String& method, const String& query) {
        if (method == "POST") {
            handleAnalogWorkSwitchConfig(client);
        } else {
            SimpleHTTPServer::send(client, 405, "text/plain", "Method Not Allowed");
        }
    });
    
    httpServer.on("/api/analogworkswitch/setpoint", [this](EthernetClient& client, const String& method, const String& query) {
        if (method == "POST") {
            handleAnalogWorkSwitchSetpoint(client);
        } else {
            SimpleHTTPServer::send(client, 405, "text/plain", "Method Not Allowed");
        }
    });

    // CAN configuration API
    httpServer.on("/api/can/config", [this](EthernetClient& client, const String& method, const String& query) {
        handleCANConfig(client, method);
    });

    // CAN info JSON API (brand capabilities, functions, etc.)
    httpServer.on("/api/can/info", [this](EthernetClient& client, const String& method, const String& query) {
        handleCANInfo(client);
    });

    // CAN config upload endpoint
    httpServer.on("/api/can/config/upload", [this](EthernetClient& client, const String& method, const String& query) {
        if (method == "POST") {
            handleCANConfigUpload(client);
        } else {
            SimpleHTTPServer::send(client, 405, "text/plain", "Method Not Allowed");
        }
    });

    // CAN config restore default endpoint
    httpServer.on("/api/can/config/restore", [this](EthernetClient& client, const String& method, const String& query) {
        if (method == "POST") {
            handleCANConfigRestore(client);
        } else {
            SimpleHTTPServer::send(client, 405, "text/plain", "Method Not Allowed");
        }
    });

    // CAN config status endpoint
    httpServer.on("/api/can/config/status", [this](EthernetClient& client, const String& method, const String& query) {
        handleCANConfigStatus(client);
    });

    // OTA upload endpoint
    httpServer.on("/api/ota/upload", [this](EthernetClient& client, const String& method, const String& query) {
        if (method == "POST") {
            handleOTAUpload(client);
        } else {
            SimpleHTTPServer::send(client, 405, "text/plain", "Method Not Allowed");
        }
    });
    
    // UM98x GPS configuration page
    httpServer.on("/um98x-config", [this](EthernetClient& client, const String& method, const String& query) {
        sendUM98xConfigPage(client);
    });
    
    // GPS config shortcut
    httpServer.on("/gps", [this](EthernetClient& client, const String& method, const String& query) {
        sendUM98xConfigPage(client);
    });
    
    // UM98x API endpoints
    httpServer.on("/api/um98x/read", [this](EthernetClient& client, const String& method, const String& query) {
        if (method == "GET") {
            handleUM98xRead(client);
        } else {
            SimpleHTTPServer::send(client, 405, "text/plain", "Method Not Allowed");
        }
    });
    
    httpServer.on("/api/um98x/write", [this](EthernetClient& client, const String& method, const String& query) {
        if (method == "POST") {
            handleUM98xWrite(client);
        } else {
            SimpleHTTPServer::send(client, 405, "text/plain", "Method Not Allowed");
        }
    });
    
    // Note: Removed polling endpoints like /api/was/angle and /api/encoder/count
    // These are now provided via WebSocket telemetry
    
    LOG_INFO(EventSource::NETWORK, "Simple web routes configured");
}

// Page handlers

void SimpleWebManager::sendHomePage(EthernetClient& client) {
    extern const char SIMPLE_HOME_PAGE[];
    String html = FPSTR(SIMPLE_HOME_PAGE);
    html.replace("%CSS_STYLES%", FPSTR(COMMON_CSS));
    html.replace("%FIRMWARE_VERSION%", FIRMWARE_VERSION);
    
    SimpleHTTPServer::send(client, 200, "text/html", html);
}

void SimpleWebManager::sendTouchHomePage(EthernetClient& client) {
    extern const char TOUCH_FRIENDLY_HOME_PAGE[];
    
    // Send directly from PROGMEM without string manipulation
    SimpleHTTPServer::sendP(client, 200, "text/html", TOUCH_FRIENDLY_HOME_PAGE);
}

void SimpleWebManager::sendEventLoggerPage(EthernetClient& client) {
    extern const char TOUCH_FRIENDLY_EVENT_LOGGER_PAGE[];
    SimpleHTTPServer::sendP(client, 200, "text/html", TOUCH_FRIENDLY_EVENT_LOGGER_PAGE);
}

void SimpleWebManager::sendLogViewerPage(EthernetClient& client) {
    extern const char TOUCH_FRIENDLY_LOG_VIEWER_PAGE[];
    SimpleHTTPServer::sendP(client, 200, "text/html", TOUCH_FRIENDLY_LOG_VIEWER_PAGE);
}

void SimpleWebManager::sendNetworkPage(EthernetClient& client) {
    extern const char TOUCH_FRIENDLY_NETWORK_PAGE[];
    
    // Send directly from PROGMEM without string manipulation
    SimpleHTTPServer::sendP(client, 200, "text/html", TOUCH_FRIENDLY_NETWORK_PAGE);
}

void SimpleWebManager::sendOTAPage(EthernetClient& client) {
    extern const char TOUCH_FRIENDLY_OTA_PAGE[];
    SimpleHTTPServer::sendP(client, 200, "text/html", TOUCH_FRIENDLY_OTA_PAGE);
}

void SimpleWebManager::sendDeviceSettingsPage(EthernetClient& client) {
    extern const char TOUCH_FRIENDLY_DEVICE_SETTINGS_PAGE[];
    
    // Send directly from PROGMEM without any string manipulation
    SimpleHTTPServer::sendP(client, 200, "text/html", TOUCH_FRIENDLY_DEVICE_SETTINGS_PAGE);
}

void SimpleWebManager::sendAnalogWorkSwitchPage(EthernetClient& client) {
    extern const char TOUCH_FRIENDLY_ANALOG_WORK_SWITCH_PAGE[];
    SimpleHTTPServer::sendP(client, 200, "text/html", TOUCH_FRIENDLY_ANALOG_WORK_SWITCH_PAGE);
}

void SimpleWebManager::sendCANConfigPage(EthernetClient& client) {
    extern const char DRAG_DROP_CAN_CONFIG_PAGE[];
    SimpleHTTPServer::sendP(client, 200, "text/html", DRAG_DROP_CAN_CONFIG_PAGE);
}

void SimpleWebManager::sendCANConfigUploadPage(EthernetClient& client) {
    extern const char CAN_CONFIG_UPLOAD_PAGE[];
    SimpleHTTPServer::sendP(client, 200, "text/html", CAN_CONFIG_UPLOAD_PAGE);
}

// WAS Demo page removed - using WebSocket telemetry instead

// API handlers

void SimpleWebManager::handleApiStatus(EthernetClient& client) {
    StaticJsonDocument<512> doc;
    
    // Basic system info
    doc["version"] = FIRMWARE_VERSION;
    doc["uptime"] = millis();
    // Free memory calculation for Teensy 4.1
    extern unsigned long _heap_start;
    extern unsigned long _heap_end;
    extern char *__brkval;
    int freeMemory = (char *)&_heap_end - __brkval;
    doc["freeMemory"] = freeMemory;
    
    // Network info
    JsonObject network = doc.createNestedObject("network");
    IPAddress localIP = Ethernet.localIP();
    char ipStr[16];
    snprintf(ipStr, sizeof(ipStr), "%d.%d.%d.%d", localIP[0], localIP[1], localIP[2], localIP[3]);
    network["ip"] = ipStr;
    network["connected"] = Ethernet.linkState();
    network["linkSpeed"] = Ethernet.linkSpeed();
    
    // Module info
    doc["deviceType"] = "Steer";  // Fixed for steer module
    doc["moduleId"] = 126;  // Fixed steer module ID
    
    // ESP32 status
    doc["esp32Detected"] = esp32Interface.isDetected();
    doc["esp32Active"] = esp32Interface.isDetected();  // Active if detected
    
    // System status
    doc["systemHealthy"] = true;
    
    String json;
    serializeJson(doc, json);
    SimpleHTTPServer::sendJSON(client, json);
}

void SimpleWebManager::handleEventLoggerConfig(EthernetClient& client, const String& method) {
    EventLogger* logger = EventLogger::getInstance();
    
    if (method == "GET") {
        // Return current configuration
        EventConfig& config = logger->getConfig();
        StaticJsonDocument<256> doc;
        doc["serialEnabled"] = config.enableSerial;
        doc["serialLevel"] = config.serialLevel;
        doc["udpEnabled"] = config.enableUDP;
        doc["udpLevel"] = config.udpLevel;
        doc["rateLimitDisabled"] = config.disableRateLimit;
        
        String json;
        serializeJson(doc, json);
        SimpleHTTPServer::sendJSON(client, json);
        
    } else if (method == "POST") {
        // Read POST body
        String body = readPostBody(client);
        
        LOG_INFO(EventSource::NETWORK, "EventLogger POST body: %s", body.c_str());
        
        // Parse JSON
        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, body);
        
        if (error) {
            LOG_ERROR(EventSource::NETWORK, "EventLogger JSON parse error: %s", error.c_str());
            SimpleHTTPServer::sendJSON(client, "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
            return;
        }
        
        // Apply EventLogger settings
        if (!doc["serialEnabled"].isNull()) {
            bool enabled = doc["serialEnabled"];
            logger->enableSerial(enabled);
            LOG_INFO(EventSource::NETWORK, "Set serial enabled: %d", enabled);
        }
        if (!doc["udpEnabled"].isNull()) {
            bool enabled = doc["udpEnabled"];
            logger->enableUDP(enabled);
            LOG_INFO(EventSource::NETWORK, "Set UDP enabled: %d", enabled);
        }
        if (!doc["serialLevel"].isNull()) {
            int level = doc["serialLevel"];
            logger->setSerialLevel(static_cast<EventSeverity>(level));
            LOG_INFO(EventSource::NETWORK, "Set serial level: %d", level);
        }
        if (!doc["udpLevel"].isNull()) {
            int level = doc["udpLevel"];
            logger->setUDPLevel(static_cast<EventSeverity>(level));
            LOG_INFO(EventSource::NETWORK, "Set UDP level: %d", level);
        }
        if (!doc["rateLimitDisabled"].isNull()) {
            bool disabled = doc["rateLimitDisabled"];
            logger->setRateLimitEnabled(!disabled);
            LOG_INFO(EventSource::NETWORK, "Set rate limit disabled: %d", disabled);
        }
        
        // Force save after all changes
        logger->saveConfig();
        
        EventConfig& config = logger->getConfig();
        LOG_INFO(EventSource::NETWORK, "EventLogger config after update: Serial=%d/%d, UDP=%d/%d, RateLimit=%d", 
                 config.enableSerial, config.serialLevel, 
                 config.enableUDP, config.udpLevel, 
                 config.disableRateLimit);
        
        SimpleHTTPServer::sendJSON(client, "{\"status\":\"saved\"}");
        
    } else {
        SimpleHTTPServer::send(client, 405, "text/plain", "Method Not Allowed");
    }
}

void SimpleWebManager::handleLogViewerData(EthernetClient& client) {
    EventLogger* logger = EventLogger::getInstance();

    // Get log buffer info - capture snapshot to avoid race conditions
    __disable_irq();  // Disable interrupts briefly
    size_t count = logger->getLogBufferCount();
    size_t head = logger->getLogBufferHead();
    size_t bufferSize = logger->getLogBufferSize();

    // Copy buffer entries to avoid corruption during output
    LogEntry snapshot[100];
    const LogEntry* buffer = logger->getLogBuffer();
    memcpy(snapshot, buffer, sizeof(snapshot));
    __enable_irq();  // Re-enable interrupts

    // Send response headers manually for chunked streaming
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: application/json");
    client.println("Connection: close");
    client.println();

    // Stream JSON directly to client
    client.print("{\"logs\":[");

    // Read from oldest to newest (circular buffer)
    size_t start = (count < bufferSize) ? 0 : head;

    for (size_t i = 0; i < count; i++) {
        size_t index = (start + i) % bufferSize;
        const LogEntry& entry = snapshot[index];

        if (i > 0) client.print(",");

        client.print("{\"timestamp\":");
        client.print(entry.timestamp);
        client.print(",\"severity\":");
        client.print(static_cast<uint8_t>(entry.severity));
        client.print(",\"source\":");
        client.print(static_cast<uint8_t>(entry.source));
        client.print(",\"message\":\"");

        // Properly escape JSON special characters
        const char* msg = entry.message;
        for (size_t j = 0; msg[j] != '\0'; j++) {
            char c = msg[j];
            switch (c) {
                case '"':  client.print("\\\""); break;
                case '\\': client.print("\\\\"); break;
                case '\n': client.print("\\n"); break;
                case '\r': client.print("\\r"); break;
                case '\t': client.print("\\t"); break;
                case '\b': client.print("\\b"); break;
                case '\f': client.print("\\f"); break;
                default:
                    if (c >= 32 && c < 127) {
                        client.print(c);
                    }
                    // Skip control chars and non-ASCII
                    break;
            }
        }

        client.print("\",\"severityName\":\"");
        client.print(logger->severityToString(entry.severity));
        client.print("\",\"sourceName\":\"");
        client.print(logger->sourceToString(entry.source));
        client.print("\"}");
    }

    client.print("]}");
}

void SimpleWebManager::handleNetworkConfig(EthernetClient& client, const String& method) {
    if (method == "GET") {
        // Return current IP as array
        IPAddress ip = Ethernet.localIP();
        
        StaticJsonDocument<128> doc;
        JsonArray ipArray = doc.createNestedArray("ip");
        ipArray.add(ip[0]);
        ipArray.add(ip[1]);
        ipArray.add(ip[2]);
        ipArray.add(ip[3]);
        
        String json;
        serializeJson(doc, json);
        SimpleHTTPServer::sendJSON(client, json);
        
    } else if (method == "POST") {
        // Read POST body
        String body = readPostBody(client);
        
        // Parse JSON
        StaticJsonDocument<128> doc;
        DeserializationError error = deserializeJson(doc, body);
        
        if (error) {
            SimpleHTTPServer::sendJSON(client, "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
            return;
        }
        
        // Get IP array
        JsonArray ipArray = doc["ip"];
        if (ipArray.size() >= 3) {
            uint8_t octet1 = ipArray[0];
            uint8_t octet2 = ipArray[1];
            uint8_t octet3 = ipArray[2];
            
            // Update network configuration using ConfigManager
            ConfigManager* config = ConfigManager::getInstance();
            
            // Update IP addresses (keeping 4th octet as 126)
            uint8_t newIP[4] = {octet1, octet2, octet3, 126};
            config->setIPAddress(newIP);
            
            // Update destination IP to broadcast on new subnet
            uint8_t newDest[4] = {octet1, octet2, octet3, 255};
            config->setDestIP(newDest);
            
            // Update gateway to .1 on new subnet
            uint8_t newGateway[4] = {octet1, octet2, octet3, 1};
            config->setGateway(newGateway);
            
            // Save to EEPROM
            config->saveNetworkConfig();
            
            LOG_INFO(EventSource::NETWORK, "Network IP saved: %d.%d.%d.126 (reboot required)", 
                     octet1, octet2, octet3);
            
            SimpleHTTPServer::sendJSON(client, "{\"status\":\"ok\"}");
        } else {
            SimpleHTTPServer::sendJSON(client, "{\"status\":\"error\",\"error\":\"Invalid IP format\"}");
        }
        
    } else {
        SimpleHTTPServer::send(client, 405, "text/plain", "Method Not Allowed");
    }
}

void SimpleWebManager::handleDeviceSettings(EthernetClient& client, const String& method) {
    if (method == "GET") {
        // Return current settings from ConfigManager
        ConfigManager* config = ConfigManager::getInstance();
        
        StaticJsonDocument<256> doc;
        doc["deviceType"] = "Steer";  // Fixed for steer module
        doc["moduleId"] = 126;  // Steer module ID
        doc["udpPassthrough"] = config->getGPSPassThrough();
        doc["sensorFusion"] = false;  // Sensor fusion not implemented yet
        doc["pwmBrakeMode"] = config->getPWMBrakeMode();
        doc["encoderType"] = config->getEncoderType();
        doc["serialRadioBaud"] = config->getSerialRadioBaudRate();
        doc["jdPWMEnabled"] = config->getJDPWMEnabled();
        doc["jdPWMSensitivity"] = config->getJDPWMSensitivity();
        
        String json;
        serializeJson(doc, json);
        SimpleHTTPServer::sendJSON(client, json);
        
    } else if (method == "POST") {
        // Read POST body
        String body = readPostBody(client);
        
        // Parse JSON
        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, body);
        
        if (error) {
            SimpleHTTPServer::sendJSON(client, "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
            return;
        }
        
        // Extract settings from JSON
        bool udpPassthrough = doc["udpPassthrough"] | false;
        bool sensorFusion = doc["sensorFusion"] | false;
        bool pwmBrakeMode = doc["pwmBrakeMode"] | false;
        int encoderType = doc["encoderType"] | 1;
        uint32_t serialRadioBaud = doc["serialRadioBaud"] | 115200;
        bool jdPWMEnabled = doc["jdPWMEnabled"] | false;
        int jdPWMSensitivity = doc["jdPWMSensitivity"] | 5;

        // Save to ConfigManager
        ConfigManager* config = ConfigManager::getInstance();
        config->setGPSPassThrough(udpPassthrough);
        config->setPWMBrakeMode(pwmBrakeMode);
        config->setEncoderType(encoderType);
        config->setSerialRadioBaudRate(serialRadioBaud);
        config->setJDPWMEnabled(jdPWMEnabled);
        config->setJDPWMSensitivity(jdPWMSensitivity);
        // Sensor fusion configuration not implemented yet
        
        // Save to EEPROM
        config->saveTurnSensorConfig();  // This saves encoder type and JD PWM settings
        config->saveSteerConfig();       // This saves PWM brake mode
        config->saveGPSConfig();         // This saves GPS passthrough
        
        // Apply JD PWM mode change to ADProcessor
        extern ADProcessor adProcessor;
        adProcessor.setJDPWMMode(jdPWMEnabled);
        
        // Update GNSSProcessor with new passthrough setting
        gnssProcessor.setUDPPassthrough(udpPassthrough);

        // Apply new radio baud rate immediately
        extern SerialManager serialManager;
        serialManager.updateRadioBaudRate(serialRadioBaud);

        LOG_DEBUG(EventSource::NETWORK, "Device settings saved: UDP=%d, Brake=%d, Encoder=%d, RadioBaud=%lu",
                  udpPassthrough, pwmBrakeMode, encoderType, serialRadioBaud);
        
        SimpleHTTPServer::sendJSON(client, "{\"status\":\"saved\"}");
        
    } else {
        SimpleHTTPServer::send(client, 405, "text/plain", "Method Not Allowed");
    }
}

void SimpleWebManager::handleAnalogWorkSwitchStatus(EthernetClient& client) {
    LOG_DEBUG(EventSource::NETWORK, "Analog work switch status requested");
    ADProcessor* adProc = ADProcessor::getInstance();
    if (!adProc) {
        LOG_ERROR(EventSource::NETWORK, "ADProcessor not available");
        SimpleHTTPServer::send(client, 503, "application/json", "{\"error\":\"ADProcessor not available\"}");
        return;
    }
    
    StaticJsonDocument<256> doc;
    doc["enabled"] = adProc->isAnalogWorkSwitchEnabled();
    doc["setpoint"] = (int)round(adProc->getWorkSwitchSetpoint());
    doc["hysteresis"] = (int)round(adProc->getWorkSwitchHysteresis());
    doc["invert"] = adProc->getInvertWorkSwitch();
    doc["percent"] = adProc->getWorkSwitchAnalogPercent();
    doc["state"] = adProc->isWorkSwitchOn();
    doc["raw"] = adProc->getWorkSwitchAnalogRaw();
    
    String json;
    serializeJson(doc, json);
    SimpleHTTPServer::sendJSON(client, json);
}

void SimpleWebManager::handleAnalogWorkSwitchConfig(EthernetClient& client) {
    // Read POST body
    String body = readPostBody(client);
    
    // Parse JSON
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, body);
    
    if (error) {
        SimpleHTTPServer::sendJSON(client, "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
        return;
    }
    
    ADProcessor* adProc = ADProcessor::getInstance();
    if (!adProc) {
        SimpleHTTPServer::sendJSON(client, "{\"status\":\"error\",\"message\":\"ADProcessor not available\"}");
        return;
    }
    
    // Update settings if provided
    if (!doc["enabled"].isNull()) {
        adProc->setAnalogWorkSwitchEnabled(doc["enabled"]);
    }
    if (!doc["hysteresis"].isNull()) {
        adProc->setWorkSwitchHysteresis(doc["hysteresis"]);
    }
    if (!doc["invert"].isNull()) {
        adProc->setInvertWorkSwitch(doc["invert"]);
    }
    
    LOG_INFO(EventSource::NETWORK, "Analog work switch config updated");
    
    SimpleHTTPServer::sendJSON(client, "{\"status\":\"saved\"}");
}

void SimpleWebManager::handleAnalogWorkSwitchSetpoint(EthernetClient& client) {
    ADProcessor* adProc = ADProcessor::getInstance();
    if (!adProc) {
        SimpleHTTPServer::sendJSON(client, "{\"status\":\"error\",\"message\":\"ADProcessor not available\"}");
        return;
    }
    
    // Set current reading as new setpoint
    float currentPercent = adProc->getWorkSwitchAnalogPercent();
    adProc->setWorkSwitchSetpoint(currentPercent);
    
    StaticJsonDocument<128> doc;
    doc["status"] = "saved";
    doc["newSetpoint"] = (int)round(currentPercent);
    
    String json;
    serializeJson(doc, json);
    SimpleHTTPServer::sendJSON(client, json);
    
    LOG_INFO(EventSource::NETWORK, "Analog work switch setpoint set to %.1f%%", currentPercent);
}

void SimpleWebManager::handleOTAUpload(EthernetClient& client) {
    // OTA upload request received
    
    // Initialize OTA handler if needed
    static bool otaInitialized = false;
    if (!otaInitialized) {
        // Initialize OTA handler
        if (!SimpleOTAHandler::init()) {
            LOG_ERROR(EventSource::NETWORK, "OTA init failed");
            SimpleHTTPServer::send(client, 500, "text/plain", "OTA init failed");
            return;
        }
        otaInitialized = true;
    }
    
    // Reset OTA handler for new upload
    SimpleOTAHandler::reset();
    
    // Read hex data from client
    
    // Read data in chunks directly from client
    uint8_t buffer[1024];
    size_t totalBytes = 0;
    bool foundStart = false;
    
    // Set a longer timeout for large files
    unsigned long timeout = 30000; // 30 seconds
    unsigned long start = millis();
    unsigned long lastDataTime = millis();
    
    while (client.connected() && (millis() - start < timeout) && !SimpleOTAHandler::isComplete()) {
        if (client.available()) {
            size_t bytesRead = client.readBytes(buffer, sizeof(buffer));
            
            if (bytesRead > 0) {
                lastDataTime = millis(); // Track when we last received data
                
                // Check for hex file start on first chunk
                if (!foundStart) {
                    // Look for ':' which starts a hex file
                    for (size_t i = 0; i < bytesRead; i++) {
                        if (buffer[i] == ':') {
                            foundStart = true;
                            // Process from this point
                            if (!SimpleOTAHandler::processChunk(&buffer[i], bytesRead - i)) {
                                const char* error = SimpleOTAHandler::getError();
                                LOG_ERROR(EventSource::NETWORK, "OTA processing failed: %s", error ? error : "Unknown error");
                                SimpleHTTPServer::send(client, 400, "text/plain", error ? error : "Processing failed");
                                return;
                            }
                            totalBytes += (bytesRead - i);
                            break;
                        }
                    }
                    if (!foundStart) {
                        LOG_ERROR(EventSource::NETWORK, "No hex data found in first chunk");
                        SimpleHTTPServer::send(client, 400, "text/plain", "Invalid hex file format");
                        return;
                    }
                } else {
                    // Process subsequent chunks
                    if (!SimpleOTAHandler::processChunk(buffer, bytesRead)) {
                        const char* error = SimpleOTAHandler::getError();
                        LOG_ERROR(EventSource::NETWORK, "OTA processing failed: %s", error ? error : "Unknown error");
                        SimpleHTTPServer::send(client, 400, "text/plain", error ? error : "Processing failed");
                        return;
                    }
                    totalBytes += bytesRead;
                }
                
                // Reset timeout on data received
                start = millis();
                
                // Log progress every 10KB
                if (totalBytes % 10240 < 1024) {
                    // Progress: totalBytes
                }
                
                // Check if OTA is complete
                if (SimpleOTAHandler::isComplete()) {
                    // Upload complete
                    break;
                }
            }
        } else {
            // No data available
            if (foundStart && millis() - lastDataTime > 1000) {
                // If we've started receiving data but haven't gotten any new data for 1 second,
                // assume the upload is complete
                // No data for 1 second, assuming complete
                break;
            }
            
            delay(1);
            
            // Also check for completion during idle time
            if (SimpleOTAHandler::isComplete()) {
                // Upload complete during idle
                break;
            }
        }
    }
    
    LOG_INFO(EventSource::NETWORK, "Received %lu total bytes", totalBytes);
    
    if (totalBytes == 0) {
        LOG_ERROR(EventSource::NETWORK, "No data received");
        SimpleHTTPServer::send(client, 400, "text/plain", "No data received");
        return;
    }
    
    // Check if we need to finalize even without explicit EOF
    if (!SimpleOTAHandler::isComplete() && foundStart) {
        // No EOF record found, check if valid
        // Process any remaining data in buffer
        if (SimpleOTAHandler::processChunk((const uint8_t*)"\n", 1)) {
            // Force a newline to process any pending hex line
        }
    }
    
    // Finalize OTA upload
    
    // Finalize the upload
    if (SimpleOTAHandler::finalize()) {
        LOG_INFO(EventSource::NETWORK, "OTA upload successful, sending response");
        
        // Send a quick success response
        SimpleHTTPServer::send(client, 200, "text/plain", "OK");
        
        // Make sure the response is fully sent
        client.flush();
        client.stop();  // Close the connection cleanly
        
        // Small delay to ensure TCP FIN is sent
        delay(100);
        
        // Apply the update
        LOG_INFO(EventSource::NETWORK, "Applying firmware update now");
        SimpleOTAHandler::applyUpdate();
    } else {
        const char* error = SimpleOTAHandler::getError();
        LOG_ERROR(EventSource::NETWORK, "OTA finalization failed: %s", error ? error : "Unknown error");
        SimpleHTTPServer::send(client, 400, "text/plain", error ? error : "Finalization failed");
    }
}

// Helper methods

String SimpleWebManager::readPostBody(EthernetClient& client) {
    String body;
    body.reserve(20480); // Pre-allocate for 20KB (handles JSON config files)

    // Note: SimpleHTTPServer should have already consumed headers
    // Just read any remaining data
    int timeout = 2000; // 2 second timeout for large files
    unsigned long start = millis();
    unsigned long lastDataTime = millis();

    while (millis() - start < timeout) {
        while (client.available()) {
            // Read in chunks for better performance
            char buffer[512];
            size_t bytesRead = client.readBytes(buffer, sizeof(buffer));
            if (bytesRead > 0) {
                // Temporarily null-terminate and append
                char tempChar = buffer[bytesRead];
                buffer[bytesRead] = '\0';
                body += buffer;
                buffer[bytesRead] = tempChar;
                lastDataTime = millis(); // Reset timeout on data
            }
        }

        // If we have data and haven't received anything for 50ms, we're done
        if (body.length() > 0 && (millis() - lastDataTime > 50)) {
            break;
        }

        delay(1); // Small delay to yield
    }

    return body;
}

String SimpleWebManager::buildLevelOptions(uint8_t selectedLevel) {
    String options;
    // Use proper syslog severity levels
    const char* levels[] = {"EMERGENCY", "ALERT", "CRITICAL", "ERROR", "WARNING", "NOTICE", "INFO", "DEBUG"};
    const uint8_t values[] = {0, 1, 2, 3, 4, 5, 6, 7};
    
    for (uint8_t i = 0; i < 8; i++) {
        options += "<option value='" + String(values[i]) + "'";
        if (values[i] == selectedLevel) {
            options += " selected";
        }
        options += ">" + String(levels[i]) + "</option>";
    }
    
    return options;
}

// Telemetry broadcast

void SimpleWebManager::broadcastTelemetry() {
    // Track connection state
    static uint32_t connectionStart = 0;
    static bool connectionPrimed = false;
    static size_t lastClientCount = 0;

    size_t currentClientCount = telemetryWS.getClientCount();

    // Early exit if no clients connected
    if (currentClientCount == 0) {
        connectionStart = 0;
        connectionPrimed = false;
        lastClientCount = 0;
        return;
    }

    // Now called by SimpleScheduler at 100Hz
    uint32_t now = millis();

    // Check for new connection
    if (currentClientCount > 0 && lastClientCount == 0) {
        connectionStart = now;
        connectionPrimed = false;
        LOG_DEBUG(EventSource::NETWORK, "WebSocket client connected, priming connection");
    }
    lastClientCount = currentClientCount;

    // During connection priming (first 5 seconds), send extra updates
    // SimpleScheduler calls us at 100Hz, but we can send more frequently during priming
    if (!connectionPrimed && now - connectionStart < 5000) {
        // Send immediately during priming period (up to 100Hz from scheduler)
        connectionPrimed = (now - connectionStart >= 5000);
    } else if (!connectionPrimed) {
        connectionPrimed = true;
    }
    
    // Build telemetry packet
    TelemetryPacket packet;
    packet.timestamp = now;
    
    // Get WAS data
    ADProcessor* adProc = ADProcessor::getInstance();
    if (adProc) {
        packet.was_angle = adProc->getWASAngle();
        packet.was_angle_target = AutosteerProcessor::getInstance()->getTargetAngle();
        packet.current_draw = adProc->getMotorCurrent() / 1000.0f;  // Convert mA to A
    } else {
        packet.was_angle = 0;
        packet.was_angle_target = 0;
        packet.current_draw = 0;
    }
    
    // Get encoder data
    if (encoderProcessor) {
        packet.encoder_count = encoderProcessor->getPulseCount();
    } else {
        packet.encoder_count = 0;
    }
    
    // Get switch states from ADProcessor
    if (adProc) {
        packet.steer_switch = adProc->isSteerSwitchOn() ? 1 : 0;
        packet.work_switch = adProc->isWorkSwitchOn() ? 1 : 0;
        packet.work_analog_percent = (uint8_t)round(adProc->getWorkSwitchAnalogPercent());
    } else {
        packet.steer_switch = 0;
        packet.work_switch = 0;
        packet.work_analog_percent = 0;
    }
    
    // Get speed and heading from GNSSProcessor
    const auto& gpsData = gnssProcessor.getData();
    if (gpsData.isValid) {
        packet.speed_kph = gpsData.speedKnots * 1.852f;  // Convert knots to km/h
        packet.heading = gpsData.headingTrue;
    } else {
        packet.speed_kph = 0;
        packet.heading = 0;
    }
    
    // Status flags
    packet.status_flags = 0;
    if (AutosteerProcessor::getInstance()->isEnabled()) {
        packet.status_flags |= 0x01;  // Bit 0: Autosteer enabled
    }
    
    packet.reserved[0] = 0;
    
    // Broadcast to all connected clients
    telemetryWS.broadcastBinary((const uint8_t*)&packet, sizeof(packet));
}

// UM98x GPS Configuration handlers

void SimpleWebManager::sendUM98xConfigPage(EthernetClient& client) {
    extern const char TOUCH_FRIENDLY_GPS_CONFIG_PAGE[];
    SimpleHTTPServer::sendP(client, 200, "text/html", TOUCH_FRIENDLY_GPS_CONFIG_PAGE);
}

void SimpleWebManager::handleUM98xRead(EthernetClient& client) {
    LOG_INFO(EventSource::NETWORK, "handleUM98xRead() called");
    
    // Create UM98xManager instance
    static UM98xManager um98xManager;
    static bool managerInitialized = false;
    
    if (!managerInitialized) {
        // Initialize with GPS1 serial port (SerialGPS1)
        if (!um98xManager.init(&SerialGPS1)) {
            StaticJsonDocument<128> doc;
            doc["success"] = false;
            doc["error"] = "Failed to initialize UM98x manager";
            
            String response;
            serializeJson(doc, response);
            SimpleHTTPServer::send(client, 500, "application/json", response);
            return;
        }
        managerInitialized = true;
    }
    
    // Read configuration
    UM98xManager::UM98xConfig config;
    bool success = um98xManager.readConfiguration(config);
    
    // Build JSON response
    StaticJsonDocument<2048> doc;
    doc["success"] = success;
    
    if (success) {
        doc["config"] = config.configCommands;
        doc["mode"] = config.modeSettings;
        doc["messages"] = config.messageSettings;
    } else {
        doc["error"] = "Failed to read GPS configuration";
    }
    
    String response;
    serializeJson(doc, response);
    SimpleHTTPServer::send(client, success ? 200 : 500, "application/json", response);
}

void SimpleWebManager::handleUM98xWrite(EthernetClient& client) {
    LOG_INFO(EventSource::NETWORK, "handleUM98xWrite() called");
    
    // Parse POST body
    String body = readPostBody(client);
    
    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, body);
    
    if (error) {
        StaticJsonDocument<128> responseDoc;
        responseDoc["success"] = false;
        responseDoc["error"] = "Invalid JSON";
        
        String response;
        serializeJson(responseDoc, response);
        SimpleHTTPServer::send(client, 400, "application/json", response);
        return;
    }
    
    // Create UM98xManager instance
    static UM98xManager um98xManager;
    static bool managerInitialized = false;
    
    if (!managerInitialized) {
        // Initialize with GPS1 serial port (SerialGPS1)
        if (!um98xManager.init(&SerialGPS1)) {
            StaticJsonDocument<128> responseDoc;
            responseDoc["success"] = false;
            responseDoc["error"] = "Failed to initialize UM98x manager";
            
            String response;
            serializeJson(responseDoc, response);
            SimpleHTTPServer::send(client, 500, "application/json", response);
            return;
        }
        managerInitialized = true;
    }
    
    // Extract configuration from JSON
    UM98xManager::UM98xConfig config;
    config.configCommands = doc["config"] | "";
    config.modeSettings = doc["mode"] | "";
    config.messageSettings = doc["messages"] | "";
    
    // Write configuration
    bool success = um98xManager.writeConfiguration(config);
    
    // Build response
    StaticJsonDocument<128> responseDoc;
    responseDoc["success"] = success;
    if (!success) {
        responseDoc["error"] = "Failed to write GPS configuration";
    }
    
    String response;
    serializeJson(responseDoc, response);
    SimpleHTTPServer::send(client, success ? 200 : 500, "application/json", response);
}

void SimpleWebManager::handleCANInfo(EthernetClient& client) {
    // Check if custom configuration exists in LittleFS
    if (CANConfigStorage::hasCustomConfig()) {
        // Stream custom configuration directly from flash to save RAM
        LittleFS_Program& fs = CANConfigStorage::getFS();
        File file = fs.open("/can_config.json", FILE_READ);

        if (file && file.size() > 0) {
            // Send HTTP headers
            client.print("HTTP/1.1 200 OK\r\n");
            client.print("Content-Type: application/json\r\n");
            client.print("Connection: close\r\n");
            client.print("\r\n");

            // Stream file in chunks to avoid loading entire file into RAM
            uint8_t buffer[512];
            while (file.available()) {
                size_t bytesRead = file.read(buffer, sizeof(buffer));
                if (bytesRead > 0) {
                    size_t written = client.write(buffer, bytesRead);

                    // If write returned 0, buffer is full - wait for it to drain
                    if (written == 0) {
                        client.flush();
                        delay(10);
                        written = client.write(buffer, bytesRead);
                    }

                    // Pace writes to prevent buffer overflow
                    if (file.position() % 2048 == 0) {
                        client.flush();
                        delay(1);
                    }
                }
            }
            file.close();
            client.flush();
            return;
        }

        if (file) file.close();
    }

    // Fallback to default configuration from PROGMEM
    SimpleHTTPServer::sendP(client, 200, "application/json", CAN_INFO_JSON);
}

void SimpleWebManager::handleCANConfig(EthernetClient& client, const String& method) {
    extern ConfigManager configManager;

    if (method == "GET") {
        // Return current CAN configuration
        CANSteerConfig config = configManager.getCANSteerConfig();

        StaticJsonDocument<512> doc;
        doc["brand"] = config.brand;
        doc["can1Speed"] = config.can1Speed;
        doc["can1Function"] = config.can1Function;
        doc["can1Name"] = config.can1Name;
        doc["can2Speed"] = config.can2Speed;
        doc["can2Function"] = config.can2Function;
        doc["can2Name"] = config.can2Name;
        doc["can3Speed"] = config.can3Speed;
        doc["can3Function"] = config.can3Function;
        doc["can3Name"] = config.can3Name;
        doc["moduleID"] = config.moduleID;

        String json;
        serializeJson(doc, json);
        SimpleHTTPServer::sendJSON(client, json);

    } else if (method == "POST") {
        // Read POST body
        String body = readPostBody(client);

        LOG_INFO(EventSource::NETWORK, "CAN config POST body: %s", body.c_str());

        // Parse JSON
        StaticJsonDocument<512> doc;
        DeserializationError error = deserializeJson(doc, body);

        if (error) {
            LOG_ERROR(EventSource::NETWORK, "CAN config JSON parse error: %s", error.c_str());
            SimpleHTTPServer::sendJSON(client, "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
            return;
        }

        // Get current config
        CANSteerConfig config = configManager.getCANSteerConfig();

        // Update config from JSON
        if (!doc["brand"].isNull()) {
            config.brand = doc["brand"];
        }
        if (!doc["can1Speed"].isNull()) {
            config.can1Speed = doc["can1Speed"];
        }
        if (!doc["can1Function"].isNull()) {
            config.can1Function = doc["can1Function"];
        }
        if (!doc["can1Name"].isNull()) {
            config.can1Name = doc["can1Name"];
        }
        if (!doc["can2Speed"].isNull()) {
            config.can2Speed = doc["can2Speed"];
        }
        if (!doc["can2Function"].isNull()) {
            config.can2Function = doc["can2Function"];
        }
        if (!doc["can2Name"].isNull()) {
            config.can2Name = doc["can2Name"];
        }
        if (!doc["can3Speed"].isNull()) {
            config.can3Speed = doc["can3Speed"];
        }
        if (!doc["can3Function"].isNull()) {
            config.can3Function = doc["can3Function"];
        }
        if (!doc["can3Name"].isNull()) {
            config.can3Name = doc["can3Name"];
        }
        if (!doc["moduleID"].isNull()) {
            config.moduleID = doc["moduleID"];
        }

        // Validate: ensure no duplicate bus names (except None/0)
        uint8_t busNames[3] = {config.can1Name, config.can2Name, config.can3Name};
        for (int i = 0; i < 3; i++) {
            if (busNames[i] != 0) {  // Skip "None"
                for (int j = i + 1; j < 3; j++) {
                    if (busNames[i] == busNames[j]) {
                        LOG_ERROR(EventSource::NETWORK, "Duplicate bus name detected: CAN%d and CAN%d both set to %d",
                                  i+1, j+1, busNames[i]);
                        SimpleHTTPServer::sendJSON(client, "{\"status\":\"error\",\"message\":\"Each bus name can only be used once\"}");
                        return;
                    }
                }
            }
        }

        // Save to EEPROM
        configManager.setCANSteerConfig(config);
        configManager.saveCANSteerConfig();

        LOG_INFO(EventSource::NETWORK, "CAN config saved - Brand: %d, CAN1: %d/%d, CAN2: %d/%d, CAN3: %d/%d",
                 config.brand, config.can1Speed, config.can1Function,
                 config.can2Speed, config.can2Function,
                 config.can3Speed, config.can3Function);

        SimpleHTTPServer::sendJSON(client, "{\"status\":\"ok\",\"message\":\"Configuration saved. Restart required.\"}");
    } else {
        SimpleHTTPServer::send(client, 405, "text/plain", "Method Not Allowed");
    }
}

void SimpleWebManager::handleCANConfigUpload(EthernetClient& client) {
    // Read JSON content from POST body
    String jsonContent = readPostBody(client);

    LOG_INFO(EventSource::NETWORK, "CAN config upload - received %d bytes", jsonContent.length());

    if (jsonContent.length() == 0) {
        SimpleHTTPServer::send(client, 400, "text/plain", "No content received");
        return;
    }

    // Validate JSON by parsing it - use DynamicJsonDocument for large files
    DynamicJsonDocument testDoc(24576);  // 24KB buffer for validation
    DeserializationError error = deserializeJson(testDoc, jsonContent);

    if (error) {
        String errorMsg = "Invalid JSON: ";
        errorMsg += error.c_str();
        SimpleHTTPServer::send(client, 400, "text/plain", errorMsg);
        LOG_ERROR(EventSource::NETWORK, "CAN config upload - invalid JSON: %s", error.c_str());
        return;
    }

    // Write to LittleFS
    if (CANConfigStorage::writeCustomConfig(jsonContent)) {
        LOG_INFO(EventSource::NETWORK, "Custom CAN config uploaded (%d bytes)", jsonContent.length());
        SimpleHTTPServer::send(client, 200, "text/plain", "Upload successful");
    } else {
        LOG_ERROR(EventSource::NETWORK, "Failed to write custom CAN config to flash");
        SimpleHTTPServer::send(client, 500, "text/plain", "Failed to write to flash");
    }
}

void SimpleWebManager::handleCANConfigRestore(EthernetClient& client) {
    // Delete custom configuration to restore default
    if (CANConfigStorage::deleteCustomConfig()) {
        LOG_INFO(EventSource::NETWORK, "Custom CAN config deleted, restored to default");
        SimpleHTTPServer::send(client, 200, "text/plain", "Default configuration restored");
    } else {
        LOG_ERROR(EventSource::NETWORK, "Failed to delete custom CAN config");
        SimpleHTTPServer::send(client, 500, "text/plain", "Failed to restore default");
    }
}

void SimpleWebManager::handleCANConfigStatus(EthernetClient& client) {
    StaticJsonDocument<256> doc;

    if (CANConfigStorage::hasCustomConfig()) {
        doc["custom"] = true;

        // Try to get version from custom config
        String customConfig = CANConfigStorage::readCustomConfig();
        if (customConfig.length() > 0) {
            StaticJsonDocument<512> configDoc;
            DeserializationError error = deserializeJson(configDoc, customConfig);
            if (!error && !configDoc["version"].isNull()) {
                doc["version"] = configDoc["version"].as<String>();
            } else {
                doc["version"] = "Unknown";
            }
            doc["size"] = CANConfigStorage::getCustomConfigSize();
        } else {
            doc["version"] = "Error reading";
            doc["size"] = 0;
        }
    } else {
        doc["custom"] = false;
        doc["version"] = "2.0";  // Default version from PROGMEM
    }

    String json;
    serializeJson(doc, json);
    SimpleHTTPServer::sendJSON(client, json);
}