// SimpleWebSocket.h
// Lightweight WebSocket server implementation using QNEthernet
// Implements minimal WebSocket protocol for telemetry streaming

#ifndef SIMPLE_WEBSOCKET_H
#define SIMPLE_WEBSOCKET_H

#include <Arduino.h>
#include <QNEthernet.h>
#include <vector>

using namespace qindesign::network;

// WebSocket opcodes
enum class WSOpcode : uint8_t {
    CONTINUATION = 0x0,
    TEXT = 0x1,
    BINARY = 0x2,
    CLOSE = 0x8,
    PING = 0x9,
    PONG = 0xA
};

// WebSocket frame header
struct WSFrameHeader {
    bool fin;
    bool rsv1, rsv2, rsv3;
    WSOpcode opcode;
    bool masked;
    uint64_t payloadLength;
    uint8_t maskKey[4];
};

// WebSocket client connection
class WebSocketClient {
public:
    WebSocketClient(EthernetClient client);
    ~WebSocketClient();
    
    bool isConnected() const;
    bool sendBinary(const uint8_t* data, size_t length);
    bool sendText(const String& text);
    bool sendPing();
    void close(uint16_t code = 1000, const String& reason = "");
    
    bool poll();  // Process incoming data
    
    // Callbacks
    void onMessage(std::function<void(const uint8_t*, size_t, bool)> callback) { messageCallback = callback; }
    void onClose(std::function<void()> callback) { closeCallback = callback; }
    
    uint32_t getClientId() const { return clientId; }
    
private:
    EthernetClient tcpClient;
    uint32_t clientId;
    bool handshakeComplete;
    std::vector<uint8_t> receiveBuffer;
    
    std::function<void(const uint8_t*, size_t, bool)> messageCallback;
    std::function<void()> closeCallback;
    
    static uint32_t nextClientId;
    
    bool performHandshake();
    bool readFrame(WSFrameHeader& header, std::vector<uint8_t>& payload);
    bool sendFrame(WSOpcode opcode, const uint8_t* data, size_t length);
    void processFrame(const WSFrameHeader& header, const std::vector<uint8_t>& payload);
    
    String generateAcceptKey(const String& key);
};

// Simple WebSocket server
class SimpleWebSocketServer {
public:
    SimpleWebSocketServer();
    ~SimpleWebSocketServer();
    
    bool begin(uint16_t port = 80);
    void stop();
    
    void handleClients();
    size_t getClientCount() const;
    
    // Broadcast to all connected clients
    void broadcastBinary(const uint8_t* data, size_t length);
    void broadcastText(const String& text);
    void broadcast(const char* text, size_t length);

    // Send to specific client
    void sendToClient(size_t index, const char* text, size_t length);

    // Set max clients (default 4)
    void setMaxClients(size_t max) { maxClients = max; }
    
private:
    EthernetServer server;
    std::vector<std::unique_ptr<WebSocketClient>> clients;
    size_t maxClients;
    bool running;
    
    void acceptNewClients();
    void removeDisconnectedClients();
};

#endif // SIMPLE_WEBSOCKET_H