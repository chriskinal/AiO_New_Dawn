// SimpleWebSocket.cpp
// Lightweight WebSocket server implementation using QNEthernet

#include "SimpleWebSocket.h"
#include "EventLogger.h"
#include "base64_simple.h"

// WebSocket magic string for handshake
const char WS_MAGIC_STRING[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

// Static member initialization
uint32_t WebSocketClient::nextClientId = 1;

// SHA1 implementation for WebSocket handshake
#include "sha1_simple.h"

//=============================================================================
// WebSocketClient Implementation
//=============================================================================

WebSocketClient::WebSocketClient(EthernetClient client) 
    : tcpClient(client), clientId(nextClientId++), handshakeComplete(false) {
    receiveBuffer.reserve(1024);
}

WebSocketClient::~WebSocketClient() {
    if (isConnected()) {
        close();
    }
}

bool WebSocketClient::isConnected() const {
    return tcpClient.connected() && handshakeComplete;
}

bool WebSocketClient::poll() {
    if (!tcpClient.connected()) {
        return false;
    }
    
    // Perform handshake if not done
    if (!handshakeComplete) {
        return performHandshake();
    }
    
    // Read available data
    while (tcpClient.available()) {
        WSFrameHeader header;
        std::vector<uint8_t> payload;
        
        if (readFrame(header, payload)) {
            processFrame(header, payload);
        } else {
            // Frame reading error, close connection
            close(1002, "Protocol error");
            return false;
        }
    }
    
    return true;
}

bool WebSocketClient::performHandshake() {
    LOG_DEBUG(EventSource::NETWORK, "WebSocket client %d performing handshake", clientId);
    
    // Read HTTP request
    String line;
    String wsKey;
    bool upgradeFound = false;
    bool connectionFound = false;
    
    // Wait a bit for data to arrive
    uint32_t start = millis();
    while (!tcpClient.available() && millis() - start < 100) {
        delay(1);
    }
    
    if (!tcpClient.available()) {
        LOG_WARNING(EventSource::NETWORK, "WebSocket client %d no data for handshake", clientId);
        return false;
    }
    
    while (tcpClient.available()) {
        char c = tcpClient.read();
        if (c == '\r') continue;
        
        if (c == '\n') {
            if (line.length() == 0) {
                // Empty line marks end of headers
                break;
            }
            
            // Parse header
            if (line.startsWith("Sec-WebSocket-Key: ")) {
                wsKey = line.substring(19);
                wsKey.trim();
            } else if (line.indexOf("Upgrade: websocket") >= 0) {
                upgradeFound = true;
            } else if (line.indexOf("Connection: Upgrade") >= 0) {
                connectionFound = true;
            }
            
            line = "";
        } else {
            line += c;
        }
    }
    
    // Validate handshake
    if (!upgradeFound || !connectionFound || wsKey.length() == 0) {
        tcpClient.print("HTTP/1.1 400 Bad Request\r\n\r\n");
        return false;
    }
    
    // Generate accept key
    String acceptKey = generateAcceptKey(wsKey);
    
    // Send handshake response
    tcpClient.print("HTTP/1.1 101 Switching Protocols\r\n");
    tcpClient.print("Upgrade: websocket\r\n");
    tcpClient.print("Connection: Upgrade\r\n");
    tcpClient.print("Sec-WebSocket-Accept: ");
    tcpClient.print(acceptKey);
    tcpClient.print("\r\n\r\n");
    tcpClient.flush();
    
    handshakeComplete = true;
    LOG_DEBUG(EventSource::NETWORK, "WebSocket client %d handshake complete", clientId);
    
    return true;
}

String WebSocketClient::generateAcceptKey(const String& key) {
    // Concatenate key with magic string
    String concat = key + WS_MAGIC_STRING;
    
    // Calculate SHA1
    uint8_t hash[SHA1_HASH_SIZE];
    sha1((const uint8_t*)concat.c_str(), concat.length(), hash);
    
    // Base64 encode
    String encoded = base64::encode(hash, SHA1_HASH_SIZE);
    return encoded;
}

bool WebSocketClient::readFrame(WSFrameHeader& header, std::vector<uint8_t>& payload) {
    // Read first two bytes
    if (tcpClient.available() < 2) return false;
    
    uint8_t byte1 = tcpClient.read();
    uint8_t byte2 = tcpClient.read();
    
    // Parse header
    header.fin = (byte1 & 0x80) != 0;
    header.rsv1 = (byte1 & 0x40) != 0;
    header.rsv2 = (byte1 & 0x20) != 0;
    header.rsv3 = (byte1 & 0x10) != 0;
    header.opcode = static_cast<WSOpcode>(byte1 & 0x0F);
    header.masked = (byte2 & 0x80) != 0;
    
    // Get payload length
    uint64_t len = byte2 & 0x7F;
    if (len == 126) {
        // Extended payload length (16-bit)
        if (tcpClient.available() < 2) return false;
        len = (tcpClient.read() << 8) | tcpClient.read();
    } else if (len == 127) {
        // Extended payload length (64-bit) - we'll limit to 32-bit
        if (tcpClient.available() < 8) return false;
        tcpClient.read(); tcpClient.read(); tcpClient.read(); tcpClient.read(); // Skip high 32 bits
        len = (tcpClient.read() << 24) | (tcpClient.read() << 16) | 
              (tcpClient.read() << 8) | tcpClient.read();
    }
    header.payloadLength = len;
    
    // Read mask key if present
    if (header.masked) {
        if (tcpClient.available() < 4) return false;
        for (int i = 0; i < 4; i++) {
            header.maskKey[i] = tcpClient.read();
        }
    }
    
    // Read payload
    payload.resize(header.payloadLength);
    size_t bytesRead = 0;
    while (bytesRead < header.payloadLength) {
        if (!tcpClient.available()) {
            delay(1);  // Brief wait for more data
            if (!tcpClient.available()) return false;
        }
        
        size_t toRead = min(tcpClient.available(), (int)(header.payloadLength - bytesRead));
        tcpClient.read(&payload[bytesRead], toRead);
        bytesRead += toRead;
    }
    
    // Unmask payload if needed
    if (header.masked) {
        for (size_t i = 0; i < payload.size(); i++) {
            payload[i] ^= header.maskKey[i % 4];
        }
    }
    
    return true;
}

bool WebSocketClient::sendFrame(WSOpcode opcode, const uint8_t* data, size_t length) {
    if (!isConnected()) return false;
    
    // Build frame header
    uint8_t header[10];
    size_t headerLen = 2;
    
    // FIN = 1, RSV = 0, Opcode
    header[0] = 0x80 | static_cast<uint8_t>(opcode);
    
    // Mask = 0 (server doesn't mask), payload length
    if (length < 126) {
        header[1] = length;
    } else if (length < 65536) {
        header[1] = 126;
        header[2] = (length >> 8) & 0xFF;
        header[3] = length & 0xFF;
        headerLen = 4;
    } else {
        // For larger payloads, we'd need 64-bit length
        // For telemetry, we shouldn't need this
        return false;
    }
    
    // Send header
    tcpClient.write(header, headerLen);
    
    // Send payload
    if (length > 0 && data != nullptr) {
        tcpClient.write(data, length);
    }
    
    return true;
}

bool WebSocketClient::sendBinary(const uint8_t* data, size_t length) {
    return sendFrame(WSOpcode::BINARY, data, length);
}

bool WebSocketClient::sendText(const String& text) {
    return sendFrame(WSOpcode::TEXT, (const uint8_t*)text.c_str(), text.length());
}

bool WebSocketClient::sendPing() {
    return sendFrame(WSOpcode::PING, nullptr, 0);
}

void WebSocketClient::close(uint16_t code, const String& reason) {
    if (tcpClient.connected()) {
        // Send close frame
        uint8_t payload[125];  // Max reason length
        payload[0] = (code >> 8) & 0xFF;
        payload[1] = code & 0xFF;
        
        size_t reasonLen = min(reason.length(), (size_t)123);
        if (reasonLen > 0) {
            memcpy(&payload[2], reason.c_str(), reasonLen);
        }
        
        sendFrame(WSOpcode::CLOSE, payload, 2 + reasonLen);
        
        // Close TCP connection
        tcpClient.stop();
    }
    
    handshakeComplete = false;
    
    if (closeCallback) {
        closeCallback();
    }
}

void WebSocketClient::processFrame(const WSFrameHeader& header, const std::vector<uint8_t>& payload) {
    switch (header.opcode) {
        case WSOpcode::BINARY:
        case WSOpcode::TEXT:
            if (messageCallback) {
                messageCallback(payload.data(), payload.size(), header.opcode == WSOpcode::BINARY);
            }
            break;
            
        case WSOpcode::PING:
            // Respond with pong
            sendFrame(WSOpcode::PONG, payload.data(), payload.size());
            break;
            
        case WSOpcode::CLOSE:
            // Close connection
            close();
            break;
            
        default:
            break;
    }
}

//=============================================================================
// SimpleWebSocketServer Implementation
//=============================================================================

SimpleWebSocketServer::SimpleWebSocketServer() 
    : server(80), maxClients(4), running(false) {
}

SimpleWebSocketServer::~SimpleWebSocketServer() {
    stop();
}

bool SimpleWebSocketServer::begin(uint16_t port) {
    server = EthernetServer(port);
    server.begin();
    running = true;
    
    LOG_INFO(EventSource::NETWORK, "WebSocket server started on port %d", port);
    return true;
}

void SimpleWebSocketServer::stop() {
    if (running) {
        // Close all client connections
        clients.clear();
        
        // Stop server
        server.end();
        running = false;
        
        LOG_INFO(EventSource::NETWORK, "WebSocket server stopped");
    }
}

void SimpleWebSocketServer::handleClients() {
    if (!running) return;
    
    // Accept new clients
    acceptNewClients();
    
    // Poll existing clients
    for (auto& client : clients) {
        if (client && !client->poll()) {
            // Client disconnected
            client.reset();
        }
    }
    
    // Remove null clients
    removeDisconnectedClients();
}

void SimpleWebSocketServer::acceptNewClients() {
    EthernetClient newClient = server.available();
    
    if (newClient) {
        LOG_DEBUG(EventSource::NETWORK, "New TCP connection on WebSocket port");
        
        // Check if we have room
        if (clients.size() >= maxClients) {
            // Reject connection
            newClient.print("HTTP/1.1 503 Service Unavailable\r\n\r\n");
            newClient.stop();
            LOG_WARNING(EventSource::NETWORK, "WebSocket connection rejected - max clients reached");
            return;
        }
        
        // Create WebSocket client
        auto wsClient = std::make_unique<WebSocketClient>(newClient);
        uint32_t clientId = wsClient->getClientId();
        clients.push_back(std::move(wsClient));
        
        LOG_DEBUG(EventSource::NETWORK, "WebSocket client %d created, waiting for handshake", clientId);
    }
}

void SimpleWebSocketServer::removeDisconnectedClients() {
    clients.erase(
        std::remove_if(clients.begin(), clients.end(),
                      [](const std::unique_ptr<WebSocketClient>& client) {
                          return !client || !client->isConnected();
                      }),
        clients.end()
    );
}

size_t SimpleWebSocketServer::getClientCount() const {
    size_t count = 0;
    for (const auto& client : clients) {
        if (client && client->isConnected()) {
            count++;
        }
    }
    return count;
}

void SimpleWebSocketServer::broadcastBinary(const uint8_t* data, size_t length) {
    static uint32_t lastPerfLog = 0;
    static uint32_t sendCount = 0;
    static uint32_t sendTime = 0;
    
    uint32_t start = micros();
    
    for (auto& client : clients) {
        if (client && client->isConnected()) {
            client->sendBinary(data, length);
        }
    }
    
    uint32_t elapsed = micros() - start;
    sendTime += elapsed;
    sendCount++;
    
    // Log performance every 60 seconds
    if (millis() - lastPerfLog >= 60000) {
        if (sendCount > 0) {
            float avgSendTime = sendTime / (float)sendCount;
            LOG_INFO(EventSource::NETWORK, "WebSocket broadcast performance: %.1f us/send, %d clients", 
                     avgSendTime, getClientCount());
        }
        lastPerfLog = millis();
        sendCount = 0;
        sendTime = 0;
    }
}

void SimpleWebSocketServer::broadcastText(const String& text) {
    for (auto& client : clients) {
        if (client && client->isConnected()) {
            client->sendText(text);
        }
    }
}

void SimpleWebSocketServer::broadcast(const char* text, size_t length) {
    String str;
    str.reserve(length);
    for (size_t i = 0; i < length; i++) {
        str += text[i];
    }

    for (auto& client : clients) {
        if (client && client->isConnected()) {
            client->sendText(str);
        }
    }
}

void SimpleWebSocketServer::sendToClient(size_t index, const char* text, size_t length) {
    if (index < clients.size() && clients[index] && clients[index]->isConnected()) {
        String str;
        str.reserve(length);
        for (size_t i = 0; i < length; i++) {
            str += text[i];
        }
        clients[index]->sendText(str);
    }
}