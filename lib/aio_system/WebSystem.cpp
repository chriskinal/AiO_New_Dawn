// WebSystem.cpp
// Combined implementation file for all AsyncWebServer-using code
// This unusual structure is required because AsyncWebServer_Teensy41 has
// inline implementations in headers that cause multiple definition errors
// when included in multiple compilation units. The library is archived
// on GitHub so we can't fix it upstream.

#include "WebManager.h"
#include "OTAHandler.h"
#include "QNetworkBase.h"
#include <QNEthernet.h>
#include <AsyncWebServer_Teensy41.h>
#include <ArduinoJson.h>
#include "EventLogger.h"
#include "Version.h"
#include "FXUtil.h"

// For network config
extern NetworkConfig netConfig;
extern void save_current_net();

using namespace qindesign::network;

// Macro to read strings from PROGMEM
#define FPSTR(pstr_pointer) (reinterpret_cast<const __FlashStringHelper *>(pstr_pointer))

//=============================================================================
// WebManager Implementation
//=============================================================================

WebManager::WebManager() : server(nullptr), isRunning(false), currentLanguage(WebLanguage::ENGLISH) {
    // TODO: Load language preference from config
}

WebManager::~WebManager() {
    stop();
}

bool WebManager::begin(uint16_t port) {
    if (isRunning) {
        return true;  // Already running
    }
    
    // Create server instance
    server = new AsyncWebServer(port);
    if (!server) {
        LOG_ERROR(EventSource::NETWORK, "Failed to create AsyncWebServer");
        return false;
    }
    
    // Setup all routes
    setupRoutes();
    
    // Start the server
    server->begin();
    isRunning = true;
    
    IPAddress ip = Ethernet.localIP();
    LOG_INFO(EventSource::NETWORK, "Web server started on http://%d.%d.%d.%d:%d", 
             ip[0], ip[1], ip[2], ip[3], port);
    
    return true;
}

void WebManager::stop() {
    if (server && isRunning) {
        server->end();
        delete server;
        server = nullptr;
        isRunning = false;
        LOG_INFO(EventSource::NETWORK, "Web server stopped");
    }
}

void WebManager::setupRoutes() {
    if (!server) return;
    
    // Root/home page
    server->on("/", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleRoot(request);
    });
    
    // API status endpoint
    server->on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleApiStatus(request);
    });
    
    // EventLogger configuration page
    server->on("/eventlogger", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleEventLoggerPage(request);
    });
    
    // Network settings page
    server->on("/network", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleNetworkPage(request);
    });
    
    // OTA Update page
    server->on("/ota", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleOTAPage(request);
    });
    
    // Language selection
    server->on("/lang/en", HTTP_GET, [this](AsyncWebServerRequest* request) {
        currentLanguage = WebLanguage::ENGLISH;
        // TODO: Save language preference to EEPROM
        request->redirect("/");
    });
    
    server->on("/lang/de", HTTP_GET, [this](AsyncWebServerRequest* request) {
        currentLanguage = WebLanguage::GERMAN;
        // TODO: Save language preference to EEPROM
        request->redirect("/");
    });
    
    // Setup EventLogger API routes
    setupEventLoggerAPI();
    
    // Setup Network API routes
    setupNetworkAPI();
    
    // Setup OTA routes
    setupOTARoutes();
    
    // Restart API endpoint
    server->on("/api/restart", HTTP_POST, [](AsyncWebServerRequest* request) {
        request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"System restarting...\"}");
        
        // Schedule restart after response is sent
        // Using Teensy's restart mechanism
        delay(100);  // Give time for response to be sent
        SCB_AIRCR = 0x05FA0004;  // System reset request for Teensy
    });
    
    // 404 handler
    server->onNotFound([this](AsyncWebServerRequest* request) {
        handleNotFound(request);
    });
}

void WebManager::handleRoot(AsyncWebServerRequest* request) {
    // Get IP address and link speed
    IPAddress ip = Ethernet.localIP();
    String ipStr = String(ip[0]) + "." + String(ip[1]) + "." + String(ip[2]) + "." + String(ip[3]);
    String linkSpeed = String(Ethernet.linkSpeed());
    
    // Load template from PROGMEM and process replacements
    String html = FPSTR(WebPageSelector::getHomePage(currentLanguage));
    html.replace("%CSS_STYLES%", FPSTR(COMMON_CSS));
    html.replace("%IP_ADDRESS%", ipStr);
    html.replace("%LINK_SPEED%", linkSpeed);
    html.replace("%FIRMWARE_VERSION%", FIRMWARE_VERSION);
    
    request->send(200, "text/html", html);
}

void WebManager::handleApiStatus(AsyncWebServerRequest* request) {
    // Simple JSON response for now
    JsonDocument doc;
    doc["status"] = "ok";
    doc["uptime"] = millis();
    doc["message"] = "Hello from AsyncWebServer on Teensy 4.1!";
    
    String response;
    serializeJsonPretty(doc, response);
    request->send(200, "application/json", response);
}

void WebManager::setupEventLoggerAPI() {
    // GET current EventLogger configuration
    server->on("/api/eventlogger/config", HTTP_GET, [](AsyncWebServerRequest* request) {
        EventLogger* logger = EventLogger::getInstance();
        EventConfig& config = logger->getConfig();
        
        JsonDocument doc;
        doc["serialEnabled"] = config.enableSerial;
        doc["serialLevel"] = config.serialLevel;
        doc["udpEnabled"] = config.enableUDP;
        doc["udpLevel"] = config.udpLevel;
        doc["syslogPort"] = (config.syslogPort[0] << 8) | config.syslogPort[1];
        
        // Add level names for display
        doc["serialLevelName"] = logger->getLevelName(static_cast<EventSeverity>(config.serialLevel));
        doc["udpLevelName"] = logger->getLevelName(static_cast<EventSeverity>(config.udpLevel));
        
        String response;
        serializeJsonPretty(doc, response);
        request->send(200, "application/json", response);
    });
    
    // POST to update EventLogger configuration
    server->on("/api/eventlogger/config", HTTP_POST, 
        [](AsyncWebServerRequest* request) {
            request->send(200, "application/json", "{\"status\":\"ok\"}");
        },
        nullptr,  // No upload handler
        [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            // Body handler
            if (index == 0) {
                // First chunk, parse the JSON
                JsonDocument doc;
                DeserializationError error = deserializeJson(doc, data, len);
                if (error) {
                    request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
                    return;
                }
                
                EventLogger* logger = EventLogger::getInstance();
                
                // Update settings if provided
                if (doc["serialEnabled"].is<bool>()) {
                    logger->enableSerial(doc["serialEnabled"]);
                }
                if (doc["serialLevel"].is<int>()) {
                    logger->setSerialLevel(static_cast<EventSeverity>(doc["serialLevel"].as<int>()));
                }
                if (doc["udpEnabled"].is<bool>()) {
                    logger->enableUDP(doc["udpEnabled"]);
                }
                if (doc["udpLevel"].is<int>()) {
                    logger->setUDPLevel(static_cast<EventSeverity>(doc["udpLevel"].as<int>()));
                }
                
                // Log the configuration change
                LOG_INFO(EventSource::CONFIG, "EventLogger configuration updated via web interface");
            }
        }
    );
    
    // GET recent log entries - placeholder for now
    server->on("/api/eventlogger/logs", HTTP_GET, [](AsyncWebServerRequest* request) {
        JsonDocument doc;
        JsonArray logs = doc["logs"].to<JsonArray>();
        
        // TODO: Implement log retrieval when EventLogger supports it
        // For now, return empty array
        
        String response;
        serializeJsonPretty(doc, response);
        request->send(200, "application/json", response);
    });
}

void WebManager::handleEventLoggerPage(AsyncWebServerRequest* request) {
    // Get current configuration
    EventLogger* logger = EventLogger::getInstance();
    EventConfig& config = logger->getConfig();
    
    // Load template from PROGMEM
    String html = FPSTR(WebPageSelector::getEventLoggerPage(currentLanguage));
    
    // Replace placeholders
    html.replace("%CSS_STYLES%", FPSTR(COMMON_CSS));
    
    // Set checked states for checkboxes
    html.replace("%SERIAL_CHECKED%", config.enableSerial ? "checked" : "");
    html.replace("%UDP_CHECKED%", config.enableUDP ? "checked" : "");
    
    // Build level select options
    html.replace("%SERIAL_LEVEL_OPTIONS%", buildLevelOptions(config.serialLevel));
    html.replace("%UDP_LEVEL_OPTIONS%", buildLevelOptions(config.udpLevel));
    
    // Set UDP port
    uint16_t udpPort = (config.syslogPort[0] << 8) | config.syslogPort[1];
    html.replace("%UDP_PORT%", String(udpPort));
    
    request->send(200, "text/html", html);
}

String WebManager::buildLevelOptions(uint8_t selectedLevel) {
    String options;
    EventLogger* logger = EventLogger::getInstance();
    
    for (int i = 0; i <= 4; i++) {
        options += "<option value=\"" + String(i) + "\"";
        if (i == selectedLevel) {
            options += " selected";
        }
        options += ">" + String(logger->getLevelName(static_cast<EventSeverity>(i))) + "</option>";
    }
    
    return options;
}

void WebManager::handleNetworkPage(AsyncWebServerRequest* request) {
    // Get current IP configuration
    IPAddress ip = Ethernet.localIP();
    
    String html = FPSTR(WebPageSelector::getNetworkPage(currentLanguage));
    html.replace("%CSS_STYLES%", FPSTR(COMMON_CSS));
    html.replace("%IP1%", String(ip[0]));
    html.replace("%IP2%", String(ip[1]));
    html.replace("%IP3%", String(ip[2]));
    
    request->send(200, "text/html", html);
}

void WebManager::setupNetworkAPI() {
    // GET current network configuration
    server->on("/api/network/config", HTTP_GET, [](AsyncWebServerRequest* request) {
        IPAddress ip = Ethernet.localIP();
        
        JsonDocument doc;
        JsonArray ipArray = doc["ip"].to<JsonArray>();
        ipArray.add(ip[0]);
        ipArray.add(ip[1]);
        ipArray.add(ip[2]);
        ipArray.add(126);  // Fixed last octet
        
        String response;
        serializeJsonPretty(doc, response);
        request->send(200, "application/json", response);
    });
    
    // POST to update network configuration
    server->on("/api/network/config", HTTP_POST,
        [](AsyncWebServerRequest* request) {
            request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Network configuration saved. Please reboot.\"}");
        },
        nullptr,
        [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            if (index == 0) {
                JsonDocument doc;
                DeserializationError error = deserializeJson(doc, data, len);
                if (error) {
                    request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
                    return;
                }
                
                // Extract IP settings
                JsonArray ipArray = doc["ip"];
                if (ipArray.size() >= 4) {
                    uint8_t ip1 = ipArray[0];
                    uint8_t ip2 = ipArray[1];
                    uint8_t ip3 = ipArray[2];
                    // ipArray[3] should be 126 but we ignore it as it's fixed
                    
                    // Update network configuration
                    netConfig.ipAddress[0] = ip1;
                    netConfig.ipAddress[1] = ip2;
                    netConfig.ipAddress[2] = ip3;
                    netConfig.ipAddress[3] = 126;  // Fixed last octet
                    
                    // Also update the other IP arrays
                    netConfig.currentIP[0] = ip1;
                    netConfig.currentIP[1] = ip2;
                    netConfig.currentIP[2] = ip3;
                    netConfig.currentIP[3] = 126;
                    netConfig.currentIP[4] = 0;
                    
                    // Update broadcast IP
                    netConfig.broadcastIP[0] = ip1;
                    netConfig.broadcastIP[1] = ip2;
                    netConfig.broadcastIP[2] = ip3;
                    netConfig.broadcastIP[3] = 255;
                    netConfig.broadcastIP[4] = 0;
                    
                    netConfig.destIP[0] = ip1;
                    netConfig.destIP[1] = ip2;
                    netConfig.destIP[2] = ip3;
                    netConfig.destIP[3] = 255;
                    
                    // Update gateway (assume .1)
                    netConfig.gateway[0] = ip1;
                    netConfig.gateway[1] = ip2;
                    netConfig.gateway[2] = ip3;
                    netConfig.gateway[3] = 1;
                    
                    // Save to EEPROM
                    save_current_net();
                    
                    LOG_INFO(EventSource::CONFIG, "Network settings updated - New IP: %d.%d.%d.126", 
                             ip1, ip2, ip3);
                }
            }
        }
    );
}

void WebManager::handleOTAPage(AsyncWebServerRequest* request) {
    String html = FPSTR(WebPageSelector::getOTAPage(currentLanguage));
    html.replace("%CSS_STYLES%", FPSTR(COMMON_CSS));
    html.replace("%FIRMWARE_VERSION%", FIRMWARE_VERSION);
    html.replace("%BOARD_TYPE%", TEENSY_BOARD_TYPE);
    
    request->send(200, "text/html", html);
}

void WebManager::setupOTARoutes() {
    // Initialize OTA handler
    OTAHandler::init();
    
    // OTA upload endpoint
    server->on("/api/ota/upload", HTTP_POST,
        // Request handler - called when upload is complete
        [](AsyncWebServerRequest* request) {
            OTAHandler::handleOTAComplete(request);
        },
        // Upload handler - called for each chunk of data
        [](AsyncWebServerRequest* request, String filename, size_t index, uint8_t* data, size_t len, bool final) {
            OTAHandler::handleOTAUpload(request, filename, index, data, len, final);
        }
    );
}

void WebManager::handleNotFound(AsyncWebServerRequest* request) {
    String message = "404 Not Found\n\n";
    message += "URI: ";
    message += request->url();
    message += "\nMethod: ";
    message += (request->method() == HTTP_GET) ? "GET" : "POST";
    
    request->send(404, "text/plain", message);
}

//=============================================================================
// OTAHandler Implementation
//=============================================================================

// Static member definitions
bool OTAHandler::otaInProgress = false;
bool OTAHandler::otaComplete = false;
bool OTAHandler::otaApply = false;
uint32_t OTAHandler::bufferAddr = 0;
uint32_t OTAHandler::bufferSize = 0;
char OTAHandler::line[96] = {0};
int OTAHandler::lineIndex = 0;
char OTAHandler::data[32] __attribute__ ((aligned (8))) = {0};
OTAHandler::HexInfo OTAHandler::hexInfo = {
    OTAHandler::data, 0, 0, 0,        // data, addr, num, code
    0, 0xFFFFFFFF, 0,                 // base, min, max
    0, 0                              // eof, lines
};

// External flash ID from main.cpp
extern const char* flash_id;

bool OTAHandler::init() {
    // Nothing to initialize - all state is reset per upload
    LOG_INFO(EventSource::SYSTEM, "OTA handler initialized");
    return true;
}

void OTAHandler::handleOTAUpload(AsyncWebServerRequest *request, String filename, 
                                 size_t index, uint8_t *data, size_t len, bool final) {
    
    // Start OTA process on first chunk
    if (!otaInProgress) {
        LOG_INFO(EventSource::NETWORK, "Starting OTA firmware upload: %s", filename.c_str());
        
        // Initialize firmware buffer
        if (firmware_buffer_init(&bufferAddr, &bufferSize) == 0) {
            LOG_ERROR(EventSource::NETWORK, "Failed to create firmware buffer");
            request->send(500, "text/plain", "Failed to create firmware buffer");
            return;
        }
        
        LOG_INFO(EventSource::NETWORK, "Created firmware buffer: %luK %s (0x%08lX - 0x%08lX)",
                 bufferSize/1024, IN_FLASH(bufferAddr) ? "FLASH" : "RAM",
                 bufferAddr, bufferAddr + bufferSize);
        
        // Reset hex parsing state
        lineIndex = 0;
        hexInfo.lines = 0;
        hexInfo.eof = 0;
        hexInfo.base = 0;
        hexInfo.min = 0xFFFFFFFF;
        hexInfo.max = 0;
        
        otaInProgress = true;
        otaComplete = false;
    }
    
    // Process data chunk
    if (otaInProgress && len > 0) {
        size_t i = 0;
        while (i < len) {
            // Process line by line
            if (data[i] == '\n' || lineIndex == sizeof(line) - 1) {
                line[lineIndex] = 0;  // null-terminate
                
                // Parse hex line
                if (parse_hex_line(line, hexInfo.data, &hexInfo.addr, 
                                  &hexInfo.num, &hexInfo.code) == 0) {
                    LOG_ERROR(EventSource::NETWORK, "Invalid hex line: %s", line);
                    request->send(400, "text/plain", "Invalid hex line");
                    return;
                }
                
                // Process hex record
                if (process_hex_record(&hexInfo) != 0) {
                    LOG_ERROR(EventSource::NETWORK, "Invalid hex code: %d", hexInfo.code);
                    request->send(400, "text/plain", "Invalid hex code");
                    return;
                }
                
                // Handle data records
                if (hexInfo.code == 0) {
                    uint32_t addr = bufferAddr + hexInfo.base + hexInfo.addr - FLASH_BASE_ADDR;
                    
                    // Check address bounds
                    if (hexInfo.max > (FLASH_BASE_ADDR + bufferSize)) {
                        LOG_ERROR(EventSource::NETWORK, "Address 0x%08lX exceeds buffer (base=0x%08lX, bufSize=%lu)", 
                                  hexInfo.max, FLASH_BASE_ADDR, bufferSize);
                        LOG_ERROR(EventSource::NETWORK, "hexInfo: base=0x%08lX, addr=0x%04X, num=%u", 
                                  hexInfo.base, hexInfo.addr, hexInfo.num);
                        request->send(400, "text/plain", "Address exceeds buffer");
                        return;
                    }
                    
                    // Write to buffer
                    if (!IN_FLASH(bufferAddr)) {
                        // RAM buffer - direct copy
                        memcpy((void*)addr, (void*)hexInfo.data, hexInfo.num);
                    } else {
                        // Flash buffer - use flash write
                        int error = flash_write_block(addr, hexInfo.data, hexInfo.num);
                        if (error) {
                            LOG_ERROR(EventSource::NETWORK, "Flash write error: 0x%02X", error);
                            request->send(400, "text/plain", "Flash write error");
                            return;
                        }
                    }
                }
                
                hexInfo.lines++;
                lineIndex = 0;
            } else if (data[i] != '\r') {
                // Add character to line (skip CR)
                line[lineIndex++] = data[i];
            }
            i++;
        }
    }
    
    // Handle final chunk
    if (final) {
        LOG_INFO(EventSource::NETWORK, "OTA upload complete: %d lines, %lu bytes (0x%08lX - 0x%08lX)",
                 hexInfo.lines, hexInfo.max - hexInfo.min, hexInfo.min, hexInfo.max);
        otaComplete = true;
    }
}

void OTAHandler::handleOTAComplete(AsyncWebServerRequest *request) {
    if (!otaComplete) {
        request->send(400, "text/plain", "Upload incomplete");
        return;
    }
    
    bool valid = true;
    
    // Verify FSEC value for Kinetis (not needed for Teensy 4.x)
    #if defined(KINETISK) || defined(KINETISL)
    uint32_t fsec = *(uint32_t *)(0x40C + bufferAddr);
    if (fsec != 0xfffff9de) {
        LOG_ERROR(EventSource::NETWORK, "Invalid FSEC value: 0x%08lX (expected 0xFFFFF9DE)", fsec);
        valid = false;
    }
    #endif
    
    // Verify flash ID
    if (valid && !check_flash_id(bufferAddr, hexInfo.max - hexInfo.min)) {
        LOG_ERROR(EventSource::NETWORK, "Firmware missing target ID: %s", flash_id);
        valid = false;
    } else if (valid) {
        LOG_INFO(EventSource::NETWORK, "Firmware contains correct target ID: %s", flash_id);
    }
    
    // Send response
    if (valid) {
        request->send(200, "text/plain", "OTA Success! System will reboot in 2 seconds...");
        otaApply = true;
    } else {
        request->send(500, "text/plain", "OTA validation failed");
        // Clean up buffer
        firmware_buffer_free(bufferAddr, bufferSize);
        otaInProgress = false;
        otaComplete = false;
    }
}

void OTAHandler::applyUpdate() {
    if (!otaApply || !otaComplete) {
        return;
    }
    
    LOG_INFO(EventSource::NETWORK, "Applying firmware update...");
    delay(100);  // Let log message send
    
    // Move firmware from buffer to flash base
    flash_move(FLASH_BASE_ADDR, bufferAddr, hexInfo.max - hexInfo.min);
    
    // Reboot
    SCB_AIRCR = 0x05FA0004;  // System reset
    while(1) {}  // Wait for reset
}

// Intel hex parsing helper
int OTAHandler::process_hex_record(HexInfo *hex) {
    if (hex->code == 0) {  // data record
        uint32_t addr = hex->base + hex->addr;
        if (addr < hex->min) hex->min = addr;
        addr += hex->num;
        if (addr > hex->max) hex->max = addr;
    } else if (hex->code == 1) {  // EOF record
        hex->eof = 1;
    } else if (hex->code == 2) {  // extended segment address
        hex->base = ((hex->data[0] << 8) | hex->data[1]) << 4;
    } else if (hex->code == 4) {  // extended linear address
        hex->base = ((hex->data[0] << 8) | hex->data[1]) << 16;
    } else if (hex->code == 5) {  // start address
        // Ignore - we don't use the start address
    } else {
        return -1;  // unknown hex code
    }
    return 0;
}