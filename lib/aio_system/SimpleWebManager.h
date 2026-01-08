// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

// SimpleWebManager.h
// Simple web interface manager for AiO using QNEthernet

#ifndef SIMPLE_WEB_MANAGER_H
#define SIMPLE_WEB_MANAGER_H

#include <Arduino.h>
#include <EEPROM.h>
#include <QNEthernet.h>
#include "SimpleHTTPServer.h"
#include "SimpleWebSocket.h"
#include "LogWebSocket.h"
#include "Version.h"
#include "EEPROMLayout.h"

// Language support
enum class WebLanguage {
    ENGLISH = 0,
    GERMAN = 1
};

class SimpleWebManager {
public:
    SimpleWebManager();
    ~SimpleWebManager();
    
    // Start web server on given port
    bool begin(uint16_t port = 80);
    
    // Stop web server
    void stop();
    
    // Process clients - call this in loop()
    void handleClient();
    
    // Broadcast telemetry via WebSocket
    void broadcastTelemetry();
    
    // Set system ready state (enables telemetry)
    void setSystemReady(bool ready) { systemReady = ready; }
    
private:
    SimpleHTTPServer httpServer;
    SimpleWebSocketServer telemetryWS;
    LogWebSocket logWS;
    bool isRunning;
    WebLanguage currentLanguage;
    bool systemReady;
    // Timing now handled by SimpleScheduler
    
    // Route setup
    void setupRoutes();
    
    // Page handlers
    void sendHomePage(EthernetClient& client);
    void sendTouchHomePage(EthernetClient& client);
    void sendEventLoggerPage(EthernetClient& client);
    void sendLogViewerPage(EthernetClient& client);
    void sendNetworkPage(EthernetClient& client);
    void sendOTAPage(EthernetClient& client);
    void sendDeviceSettingsPage(EthernetClient& client);
    void sendAnalogWorkSwitchPage(EthernetClient& client);
    void sendCANConfigPage(EthernetClient& client);
    void sendCANConfigUploadPage(EthernetClient& client);

    // API handlers
    void handleApiStatus(EthernetClient& client);
    void handleApiRestart(EthernetClient& client);
    void handleEventLoggerConfig(EthernetClient& client, const String& method);
    void handleLogViewerData(EthernetClient& client);
    void handleNetworkConfig(EthernetClient& client, const String& method);
    void handleDeviceSettings(EthernetClient& client, const String& method);
    void handleAnalogWorkSwitchStatus(EthernetClient& client);
    void handleAnalogWorkSwitchConfig(EthernetClient& client);
    void handleAnalogWorkSwitchSetpoint(EthernetClient& client);
    void handleOTAUpload(EthernetClient& client);
    void handleCANConfig(EthernetClient& client, const String& method);
    void handleCANInfo(EthernetClient& client);
    void handleCANConfigUpload(EthernetClient& client);
    void handleCANConfigRestore(EthernetClient& client);
    void handleCANConfigStatus(EthernetClient& client);
    
    // UM98x GPS configuration handlers
    void sendUM98xConfigPage(EthernetClient& client);
    void handleUM98xRead(EthernetClient& client);
    void handleUM98xWrite(EthernetClient& client);
    
    // Helper to parse POST body
    String readPostBody(EthernetClient& client);
    
    // Helper to build select options for log levels
    String buildLevelOptions(uint8_t selectedLevel);
    
    // WebSocket telemetry
    void updateTelemetryClients();
};

// Web config address in EEPROM - use the one from EEPROMLayout.h

#endif // SIMPLE_WEB_MANAGER_H