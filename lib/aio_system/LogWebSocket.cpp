// LogWebSocket.cpp
// WebSocket server for real-time log streaming

#include "LogWebSocket.h"
#include "EventLogger.h"
#include <cstring>

LogWebSocket::LogWebSocket()
    : serverPort(0), running(false) {
}

LogWebSocket::~LogWebSocket() {
    stop();
}

bool LogWebSocket::begin(uint16_t port) {
    serverPort = port;

    if (!wsServer.begin(port)) {
        LOG_ERROR(EventSource::NETWORK, "Failed to start Log WebSocket server on port %d", port);
        return false;
    }

    running = true;
    LOG_INFO(EventSource::NETWORK, "Log WebSocket server started on port %d", port);
    return true;
}

void LogWebSocket::stop() {
    if (running) {
        wsServer.stop();
        running = false;
        LOG_INFO(EventSource::NETWORK, "Log WebSocket server stopped");
    }
}

void LogWebSocket::handleClient() {
    if (!running) return;
    wsServer.handleClients();
}

size_t LogWebSocket::getClientCount() const {
    if (!running) return 0;
    return wsServer.getClientCount();
}

void LogWebSocket::broadcastLog(uint32_t timestamp, EventSeverity severity, EventSource source, const char* message) {
    if (!running || wsServer.getClientCount() == 0) {
        return;
    }

    // Format log entry as JSON
    String json = formatLogEntry(timestamp, severity, source, message);

    // Broadcast to all connected clients
    wsServer.broadcast(json.c_str(), json.length());
}

void LogWebSocket::sendLogHistory(size_t clientIndex) {
    EventLogger* logger = EventLogger::getInstance();

    // Get log buffer info
    size_t count = logger->getLogBufferCount();
    size_t head = logger->getLogBufferHead();
    size_t bufferSize = logger->getLogBufferSize();
    const LogEntry* buffer = logger->getLogBuffer();

    // Send history message header
    wsServer.sendToClient(clientIndex, "{\"type\":\"history\",\"logs\":[", 27);

    // Read from oldest to newest
    size_t start = (count < bufferSize) ? 0 : head;
    for (size_t i = 0; i < count; i++) {
        size_t index = (start + i) % bufferSize;
        const LogEntry& entry = buffer[index];

        if (i > 0) {
            wsServer.sendToClient(clientIndex, ",", 1);
        }

        String json = formatLogEntry(entry.timestamp, entry.severity, entry.source, entry.message);
        wsServer.sendToClient(clientIndex, json.c_str(), json.length());
    }

    // Close history message
    wsServer.sendToClient(clientIndex, "]}", 2);
}

String LogWebSocket::formatLogEntry(uint32_t timestamp, EventSeverity severity, EventSource source, const char* message) {
    EventLogger* logger = EventLogger::getInstance();

    String json = "{\"timestamp\":";
    json += timestamp;
    json += ",\"severity\":";
    json += static_cast<uint8_t>(severity);
    json += ",\"source\":";
    json += static_cast<uint8_t>(source);
    json += ",\"severityName\":\"";
    json += logger->severityToString(severity);
    json += "\",\"sourceName\":\"";
    json += logger->sourceToString(source);
    json += "\",\"message\":\"";
    appendEscapedString(json, message);
    json += "\"}";

    return json;
}

void LogWebSocket::appendEscapedString(String& json, const char* str) {
    for (size_t i = 0; str[i] != '\0'; i++) {
        char c = str[i];
        switch (c) {
            case '"':  json += "\\\""; break;
            case '\\': json += "\\\\"; break;
            case '\n': json += "\\n"; break;
            case '\r': json += "\\r"; break;
            case '\t': json += "\\t"; break;
            case '\b': json += "\\b"; break;
            case '\f': json += "\\f"; break;
            default:
                if (c >= 32 && c < 127) {
                    json += c;
                }
                // Skip control chars and non-ASCII
                break;
        }
    }
}
