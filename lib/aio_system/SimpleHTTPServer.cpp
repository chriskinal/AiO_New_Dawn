// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

// SimpleHTTPServer.cpp
// Lightweight HTTP server implementation using QNEthernet

#include "SimpleHTTPServer.h"
#include "EventLogger.h"

SimpleHTTPServer::SimpleHTTPServer() : 
    server(80),
    serverPort(80),
    running(false) {
}

SimpleHTTPServer::~SimpleHTTPServer() {
    stop();
}

bool SimpleHTTPServer::begin(uint16_t port) {
    serverPort = port;
    server = EthernetServer(port);
    server.begin();
    running = true;
    
    LOG_INFO(EventSource::NETWORK, "HTTP server started on port %d", port);
    return true;
}

void SimpleHTTPServer::stop() {
    if (running) {
        server.end();
        running = false;
        LOG_INFO(EventSource::NETWORK, "HTTP server stopped");
    }
}

void SimpleHTTPServer::handleClient() {
    if (!running) return;
    
    EthernetClient client = server.available();
    if (client) {
        String method, path, query;
        
        if (parseRequest(client, method, path, query)) {
            // Debug logging - comment out for production
            // Serial.printf("HTTP: %s %s\n", method.c_str(), path.c_str());
            
            // Find matching route
            Route* route = findRoute(path);
            
            if (route) {
                // Call handler with client, method, and query
                route->handler(client, method, query);
            } else {
                Serial.printf("404: %s\n", path.c_str());
                handleNotFound(client);
            }
        } else {
            Serial.println("HTTP: Parse failed");
        }
        
        // Close connection
        client.stop();
    }
}

bool SimpleHTTPServer::parseRequest(EthernetClient& client, String& method, String& path, String& query) {
    char line[256];
    
    // Read request line
    int len = client.readBytesUntil('\n', line, sizeof(line) - 1);
    if (len <= 0) return false;
    
    line[len] = '\0';
    
    // Parse method and path
    char methodBuf[16] = {0};
    char pathBuf[128] = {0};
    
    if (sscanf(line, "%15s %127s", methodBuf, pathBuf) != 2) {
        return false;
    }
    
    method = String(methodBuf);
    String fullPath = String(pathBuf);
    
    // Split path and query
    int queryIndex = fullPath.indexOf('?');
    if (queryIndex >= 0) {
        path = fullPath.substring(0, queryIndex);
        query = fullPath.substring(queryIndex + 1);
    } else {
        path = fullPath;
        query = "";
    }
    
    // Skip remaining headers
    while (client.available()) {
        len = client.readBytesUntil('\n', line, sizeof(line) - 1);
        if (len <= 1) break;  // Empty line marks end of headers
    }
    
    return true;
}

void SimpleHTTPServer::on(const String& path, HTTPHandler handler) {
    Route route;
    route.path = path;
    route.handler = handler;
    routes.push_back(route);
}

SimpleHTTPServer::Route* SimpleHTTPServer::findRoute(const String& path) {
    for (auto& route : routes) {
        if (route.path == path) {
            return &route;
        }
    }
    return nullptr;
}

void SimpleHTTPServer::handleNotFound(EthernetClient& client) {
    send(client, 404, "text/plain", "Not Found");
}

// Static helper methods

void SimpleHTTPServer::send(EthernetClient& client, int code, const String& contentType, const String& content) {
    String status;
    switch (code) {
        case 200: status = "OK"; break;
        case 301: status = "Moved Permanently"; break;
        case 302: status = "Found"; break;
        case 400: status = "Bad Request"; break;
        case 404: status = "Not Found"; break;
        case 500: status = "Internal Server Error"; break;
        case 503: status = "Service Unavailable"; break;
        default: status = "Unknown"; break;
    }
    
    // Debug - comment out for production
    // Serial.printf("HTTP Send: %d %s, estimated len=%d\n", code, status.c_str(), content.length());
    
    // Send response without Content-Length to avoid mismatch
    client.printf("HTTP/1.1 %d %s\r\n", code, status.c_str());
    client.printf("Content-Type: %s\r\n", contentType.c_str());
    client.print("Connection: close\r\n");
    client.print("\r\n");
    
    // Send content
    client.print(content);
    client.flush();
}

void SimpleHTTPServer::sendP(EthernetClient& client, int code, const String& contentType, const char* content) {
    String status;
    switch (code) {
        case 200: status = "OK"; break;
        case 301: status = "Moved Permanently"; break;
        case 302: status = "Found"; break;
        case 400: status = "Bad Request"; break;
        case 404: status = "Not Found"; break;
        case 500: status = "Internal Server Error"; break;
        case 503: status = "Service Unavailable"; break;
        default: status = "Unknown"; break;
    }
    
    // Debug - comment out for production
    // Serial.printf("HTTP SendP: %d %s\n", code, status.c_str());
    
    // Send without Content-Length
    client.printf("HTTP/1.1 %d %s\r\n", code, status.c_str());
    client.printf("Content-Type: %s\r\n", contentType.c_str());
    client.print("Connection: close\r\n");
    client.print("\r\n");
    
    // Send PROGMEM content in chunks
    const size_t chunkSize = 256;  // Smaller chunks
    char buffer[chunkSize + 1];  // +1 for null terminator
    const char* ptr = content;
    size_t totalSent = 0;
    
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        size_t copied = 0;
        
        // Copy up to chunkSize bytes from PROGMEM
        for (size_t i = 0; i < chunkSize; i++) {
            char c = pgm_read_byte(ptr++);
            if (c == 0) {
                // End of string
                if (copied > 0) {
                    size_t sent = client.write(buffer, copied);
                    totalSent += sent;
                }
                // Serial.printf("HTTP SendP: Complete, sent %d bytes total\n", totalSent);
                client.flush();
                return;
            }
            buffer[i] = c;
            copied++;
        }
        
        // Send this chunk
        if (copied > 0) {
            size_t sent = 0;
            size_t toSend = copied;
            size_t offset = 0;
            
            // Send in smaller pieces if needed, waiting for client to be ready
            while (toSend > 0) {
                // Wait for client to be ready (up to 100ms)
                uint32_t waitStart = millis();
                while (!client.availableForWrite() && (millis() - waitStart < 100)) {
                    delay(1);
                }
                
                // Try to send what we can
                size_t canSend = client.availableForWrite();
                if (canSend > toSend) canSend = toSend;
                if (canSend > 64) canSend = 64;  // Limit chunk size to avoid buffer issues
                
                if (canSend > 0) {
                    size_t written = client.write(buffer + offset, canSend);
                    if (written > 0) {
                        sent += written;
                        offset += written;
                        toSend -= written;
                        totalSent += written;
                    } else {
                        // Write failed completely
                        Serial.printf("HTTP SendP: Write failed at %d bytes\n", totalSent);
                        return;
                    }
                } else {
                    // Client not ready after timeout
                    Serial.printf("HTTP SendP: Client not ready at %d bytes\n", totalSent);
                    return;
                }
                
                // Small delay to let network catch up
                if (totalSent % 512 == 0) {
                    delay(1);
                }
            }
        }
    }
}

void SimpleHTTPServer::sendJSON(EthernetClient& client, const String& json) {
    send(client, 200, "application/json", json);
}

void SimpleHTTPServer::redirect(EthernetClient& client, const String& location) {
    client.print("HTTP/1.1 302 Found\r\n");
    client.printf("Location: %s\r\n", location.c_str());
    client.print("Content-Length: 0\r\n");
    client.print("Connection: close\r\n");
    client.print("\r\n");
    client.flush();
}