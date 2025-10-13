# QNEthernet Code Inventory - Migration Reference

**Date:** 2025-10-11
**Purpose:** Document all QNEthernet usage for migration to NativeEthernet

## Files Using QNEthernet (18 files)

### Core Network Layer
1. **lib/aio_system/QNetworkBase.h/cpp** ⚠️ CRITICAL
   - Ethernet initialization
   - MAC address configuration
   - Static IP and DHCP handling
   - Network status monitoring

2. **lib/aio_system/QNEthernetUDPHandler.h/cpp** ⚠️ CRITICAL
   - UDP socket management (port 8888)
   - Packet receiving and parsing
   - AgOpenGPS communication

### Web Server Components
3. **lib/aio_system/SimpleWebManager.h/cpp** ⚠️ HIGH PRIORITY
   - HTTP server implementation
   - Request parsing and routing
   - API endpoints (/api/*)
   - Static page serving

4. **lib/aio_system/SimpleHTTPServer.h**
   - HTTP protocol handling
   - Request/response management

5. **lib/aio_system/SimpleOTAHandler.h**
   - OTA firmware update handling
   - File upload via HTTP

### WebSocket Components
6. **lib/aio_system/SimpleWebSocket.h**
   - Base WebSocket implementation

7. **lib/aio_system/LogWebSocket.h**
   - Real-time log streaming

8. **lib/aio_system/TelemetryWebSocket.h**
   - Real-time telemetry data

### Application Layer
9. **lib/aio_system/PGNProcessor.h/cpp**
   - Uses network for PGN message handling
   - Likely includes for typing, not direct use

10. **lib/aio_system/RTCMProcessor.h/cpp**
    - RTCM correction data handling
    - May use UDP for NTRIP

11. **lib/aio_system/EventLogger.cpp**
    - Network-based logging (syslog UDP)

12. **lib/aio_system/LEDManagerFSM.cpp**
    - Network status LED indication

### Main Application
13. **src/main.cpp**
    - Network initialization in setup()
    - Main loop network processing

---

## Network Features Inventory

### UDP Services
- **Port 8888** - AgOpenGPS PGN communication (bidirectional)
- **Port 9999** - NTRIP client (RTCM corrections)
- **Syslog** - Event logging to remote server (optional)

### TCP Services
- **Port 80** - HTTP web server
  - Static pages (HTML/CSS)
  - REST API endpoints
  - WebSocket upgrade handling

### Network Configuration
- **MAC Address:** Teensy 4.1 built-in unique MAC
- **Static IP:** 192.168.5.126 (configurable)
- **Subnet:** 255.255.255.0
- **Gateway:** 192.168.5.1
- **DNS:** Configurable
- **DHCP:** Optional fallback

### mDNS
- **Hostname:** aio-newdawn.local (if enabled)
- **Service:** _http._tcp

---

## API Usage Patterns

### Ethernet Initialization
```cpp
// QNEthernet pattern
#include <QNEthernet.h>
using namespace qindesign::network;

Ethernet.begin(mac, ip, subnet, gateway);
Ethernet.waitForLocalIP(timeout);
```

### UDP Usage
```cpp
// QNEthernet pattern
EthernetUDP udp;
udp.begin(8888);
int packetSize = udp.parsePacket();
udp.read(buffer, size);
udp.beginPacket(ip, port);
udp.write(data, len);
udp.endPacket();
```

### TCP Server Usage
```cpp
// QNEthernet pattern
EthernetServer server(80);
server.begin();
EthernetClient client = server.available();
if (client) {
    while (client.connected()) {
        if (client.available()) {
            char c = client.read();
        }
        client.write(data, len);
    }
    client.stop();
}
```

---

## Migration Compatibility Notes

### Expected Drop-in Replacements
- ✅ `#include <QNEthernet.h>` → `#include <NativeEthernet.h>`
- ✅ `EthernetUDP` - Same API (Arduino standard)
- ✅ `EthernetServer` - Same API (Arduino standard)
- ✅ `EthernetClient` - Same API (Arduino standard)

### API Differences to Handle
- ⚠️ `Ethernet.begin()` - Parameter order may differ
- ⚠️ `Ethernet.waitForLocalIP()` - May not exist in NativeEthernet
- ⚠️ Namespace - QNEthernet uses `qindesign::network`, NativeEthernet doesn't
- ⚠️ mDNS - Implementation may differ or be absent

### Features Requiring Investigation
- ❓ WebSocket support in NativeEthernet
- ❓ mDNS/DNS-SD availability
- ❓ UDP multicast support
- ❓ DNS client functionality
- ❓ DHCP robustness
- ❓ Link status detection methods

---

## Testing Requirements

### UDP Communication (CRITICAL)
- [ ] Receive PGN 200, 201, 202, 238 from AgOpenGPS
- [ ] Send PGN 253 @ 100Hz (timing critical!)
- [ ] Send PGN 250 @ 10Hz
- [ ] Zero packet loss over 10-minute test
- [ ] Proper subnet scanning (PGN 202 response)

### Web Server (HIGH PRIORITY)
- [ ] Access all pages: /, /device, /can, /network, /gps, /ota, /logger, /logs
- [ ] POST configuration changes
- [ ] OTA firmware upload
- [ ] WebSocket connections (logs, telemetry)
- [ ] Concurrent client handling (if needed)

### Network Stability
- [ ] Cable unplug/replug recovery
- [ ] DHCP failover (if used)
- [ ] Long-term stability (4+ hours)
- [ ] Memory leak detection

---

## Risk Assessment by Component

| Component | Risk Level | Reason |
|-----------|------------|--------|
| QNetworkBase | Medium | Core network init, well-defined APIs |
| QNEthernetUDPHandler | High | Timing-critical 100Hz autosteer loop |
| SimpleWebManager | High | Complex HTTP state machine |
| SimpleWebSocket | Very High | May not be supported in NativeEthernet |
| EventLogger UDP | Low | Non-critical feature |
| RTCMProcessor | Medium | NTRIP client may need work |

---

## Migration Priority Order

1. **QNetworkBase** - Foundation layer
2. **QNEthernetUDPHandler** - Critical for autosteer
3. **SimpleWebManager** - User interface
4. **SimpleHTTPServer** - HTTP protocol
5. **WebSocket components** - May require workarounds
6. **EventLogger/RTCM** - Lower priority features

---

## Rollback Triggers

Abort migration and rollback if:
- ❌ UDP packet loss > 0.1% during testing
- ❌ 100Hz steer loop timing affected
- ❌ Web server crashes or hangs
- ❌ WebSocket functionality cannot be restored
- ❌ Memory usage doesn't decrease as expected
- ❌ Any safety-critical feature impaired

---

## Notes

- QNEthernet uses lwIP stack (~140 KB RAM)
- NativeEthernet uses FNET stack (~60 KB RAM)
- Expected RAM savings: ~80 KB
- NativeEthernet is Arduino Ethernet.h compatible
- WebSocket support may require custom implementation or third-party library
