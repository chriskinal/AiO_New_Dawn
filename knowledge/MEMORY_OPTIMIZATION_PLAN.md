# Memory Optimization Plan: NativeEthernet Migration + CAN Buffer Reduction

## Executive Summary

**Objective:** Reduce RAM usage from 95.5% to ~76% by migrating from QNEthernet/lwIP to NativeEthernet/FNET and optimizing CAN buffer sizes.

**Expected Savings:**
- NativeEthernet migration: ~80 KB
- CAN buffer optimization: ~21 KB
- **Total: ~101 KB RAM freed**

**Risk Level:** Medium
- NativeEthernet is less maintained than QNEthernet
- API changes require code modifications
- Thorough testing required

---

## Phase 1: Preparation and Analysis

### 1.1 Backup and Branch Creation
**Tasks:**
- [ ] Ensure all current work is committed
- [ ] Create backup branch: `backup-before-memory-optimization`
- [ ] Create feature branch: `feature/memory-optimization`
- [ ] Document current memory usage baseline
- [ ] Run full build and capture current metrics

**Deliverable:** Clean working branch with documented baseline

**Risk:** Low
**Estimated Time:** 15 minutes

---

### 1.2 Code Inventory
**Tasks:**
- [ ] Identify all files using QNEthernet APIs
- [ ] Document current network features:
  - UDP communication (port 8888)
  - Web server endpoints
  - mDNS configuration
  - DHCP/Static IP handling
- [ ] List all AsyncUDP handlers
- [ ] Document Ethernet initialization sequence

**Files to Review:**
- `lib/aio_communications/AsyncUDPHandler.h/cpp`
- `lib/aio_system/SimpleWebManager.h/cpp`
- `lib/aio_system/QNetworkBase.h/cpp`
- Any files including `<QNEthernet.h>`

**Deliverable:** Complete inventory of QNEthernet usage

**Risk:** Low
**Estimated Time:** 30 minutes

---

## Phase 2: NativeEthernet Migration

### 2.1 Library Installation (Local Copies)
**Tasks:**
- [ ] Clone NativeEthernet locally:
  ```bash
  cd lib/
  git clone https://github.com/vjmuzik/NativeEthernet.git
  cd NativeEthernet
  git log -1  # Document commit hash for reference
  ```
- [ ] Clone FNET locally:
  ```bash
  cd lib/
  git clone https://github.com/vjmuzik/FNET.git
  cd FNET
  git log -1  # Document commit hash for reference
  ```
- [ ] Document library versions in `knowledge/LIBRARY_VERSIONS.md`:
  ```markdown
  ## NativeEthernet
  - Repository: https://github.com/vjmuzik/NativeEthernet
  - Commit: [hash from git log]
  - Date: [date]

  ## FNET
  - Repository: https://github.com/vjmuzik/FNET
  - Commit: [hash from git log]
  - Date: [date]
  ```
- [ ] Update `.gitignore` to NOT ignore these local libraries (ensure they're tracked)
- [ ] Update `platformio.ini` lib_deps (if needed - local libs auto-detected)
- [ ] Comment out QNEthernet dependency (keep for potential rollback)
- [ ] Test clean build (will fail, expected)

**Rationale:**
- Local copies ensure code stability (NativeEthernet/FNET are not actively maintained)
- Prevents breaking changes from upstream updates
- Allows local modifications if needed
- Maintains full control over dependencies
- Libraries become part of version control

**Deliverable:** NativeEthernet and FNET available locally in `lib/` directory

**Risk:** Low
**Estimated Time:** 15 minutes

---

### 2.2 Core Network Layer Migration

#### 2.2.1 Replace QNetworkBase with NativeEthernet
**File:** `lib/aio_system/QNetworkBase.h/cpp`

**Changes:**
```cpp
// OLD:
#include <QNEthernet.h>
using namespace qindesign::network;

// NEW:
#include <NativeEthernet.h>
// No namespace needed
```

**Tasks:**
- [ ] Replace `Ethernet.begin()` call - API should be compatible
- [ ] Update MAC address handling
- [ ] Update static IP configuration
- [ ] Update DHCP handling
- [ ] Test: Verify Ethernet initialization

**API Differences to Handle:**
| QNEthernet | NativeEthernet | Notes |
|------------|----------------|-------|
| `Ethernet.begin()` | `Ethernet.begin(mac, ip)` | Similar API |
| `Ethernet.setMACAddress()` | Pass to `begin()` | Different approach |
| `Ethernet.waitForLocalIP()` | `Ethernet.begin()` blocks | Behavioral difference |
| `Ethernet.setDNSServerIP()` | `Ethernet.setDnsServerIP()` | Case difference |

**Deliverable:** Ethernet initialization working with NativeEthernet

**Risk:** Medium - Core network functionality
**Estimated Time:** 1-2 hours

---

#### 2.2.2 Migrate UDP Handler
**File:** `lib/aio_communications/AsyncUDPHandler.h/cpp`

**Changes:**
```cpp
// OLD:
#include <QNEthernet.h>
EthernetUDP udp;  // QNEthernet UDP

// NEW:
#include <NativeEthernet.h>
EthernetUDP udp;  // NativeEthernet UDP - API compatible!
```

**Tasks:**
- [ ] Replace QNEthernet UDP includes
- [ ] Verify `udp.begin(port)` compatibility
- [ ] Verify `udp.parsePacket()` behavior
- [ ] Verify `udp.read()` / `udp.write()` APIs
- [ ] Verify `udp.beginPacket()` / `udp.endPacket()` sequence
- [ ] Test: Send/receive UDP packets from AgOpenGPS

**Known Compatibility:**
NativeEthernet implements Arduino Ethernet.h API, so most UDP methods should be identical.

**Deliverable:** UDP communication working on port 8888

**Risk:** Medium - Critical for AgOpenGPS communication
**Estimated Time:** 1-2 hours

---

#### 2.2.3 Migrate Web Server
**File:** `lib/aio_system/SimpleWebManager.h/cpp`

**Current Implementation:** Custom HTTP parser over raw TCP (doesn't use QNEthernet-specific APIs heavily)

**Changes:**
```cpp
// OLD:
#include <QNEthernet.h>
EthernetServer server(80);  // QNEthernet

// NEW:
#include <NativeEthernet.h>
EthernetServer server(80);  // NativeEthernet - API compatible!
```

**Tasks:**
- [ ] Replace QNEthernet includes
- [ ] Verify `EthernetServer` initialization
- [ ] Verify `server.available()` behavior
- [ ] Verify `EthernetClient` read/write operations
- [ ] Verify `client.connected()` status checking
- [ ] Test: Access web interface at http://192.168.5.126
- [ ] Test: All web pages load correctly
- [ ] Test: POST requests save configuration
- [ ] Test: Concurrent client handling (if applicable)

**Deliverable:** Web server fully functional

**Risk:** Medium-High - Complex state machine
**Estimated Time:** 2-3 hours

---

### 2.3 Network Configuration Updates

#### 2.3.1 FNET Buffer Configuration
**File:** Create `lib/aio_system/NetworkConfig.h` (new file)

**Content:**
```cpp
#ifndef NETWORK_CONFIG_H
#define NETWORK_CONFIG_H

// FNET Stack Configuration for Single-Client Usage
// Optimized for AgOpenGPS communication only

// Socket configuration
#define FNET_CFG_SOCKET_MAX         4    // Max 4 sockets (UDP + TCP server + 2 clients)
#define FNET_CFG_SOCKET_TCP_TX_BUF_SIZE  2048   // TCP send buffer per socket
#define FNET_CFG_SOCKET_TCP_RX_BUF_SIZE  2048   // TCP receive buffer per socket

// Memory pools
#define FNET_CFG_HEAP_SIZE          (20 * 1024)  // 20 KB heap (vs lwIP's 24KB)

// UDP configuration
#define FNET_CFG_UDP_MAX            3    // UDP sockets (AgOpen data, broadcast, spare)

// TCP configuration
#define FNET_CFG_TCP_MAX            2    // TCP connections (web server)

#endif // NETWORK_CONFIG_H
```

**Tasks:**
- [ ] Research FNET configuration options
- [ ] Set conservative buffer sizes
- [ ] Configure for single-client scenario
- [ ] Verify FNET accepts configuration

**Deliverable:** FNET configured for minimal memory usage

**Risk:** Low - Can adjust if needed
**Estimated Time:** 1 hour

---

#### 2.3.2 Remove lwIP Dependencies
**Tasks:**
- [ ] Remove `lib/QNEthernet` directory (after verification)
- [ ] Remove lwIP includes from all source files
- [ ] Remove `lwipopts.h` references
- [ ] Clean up any lwIP-specific code
- [ ] Update `platformio.ini` to remove QNEthernet dependency

**Deliverable:** lwIP completely removed

**Risk:** Low
**Estimated Time:** 30 minutes

---

## Phase 3: CAN Buffer Optimization

### 3.1 Update CAN Buffer Sizes
**File:** `lib/aio_communications/CANGlobals.h`

**Current:**
```cpp
extern FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> globalCAN1;
extern FlexCAN_T4<CAN2, RX_SIZE_256, TX_SIZE_16> globalCAN2;
extern FlexCAN_T4<CAN3, RX_SIZE_256, TX_SIZE_256> globalCAN3;
```

**New (Conservative):**
```cpp
// CAN1 (V-Bus): Valve/Hitch/Buttons - low traffic
extern FlexCAN_T4<CAN1, RX_SIZE_64, TX_SIZE_16> globalCAN1;

// CAN2 (K-Bus): Keya Motor - 100Hz periodic, predictable
extern FlexCAN_T4<CAN2, RX_SIZE_32, TX_SIZE_8> globalCAN2;

// CAN3 (ISO-Bus): ISOBUS - can be high traffic with multiple implements
extern FlexCAN_T4<CAN3, RX_SIZE_128, TX_SIZE_64> globalCAN3;
```

**Tasks:**
- [ ] Update buffer sizes in `CANGlobals.h`
- [ ] Update buffer sizes in `CANGlobals.cpp`
- [ ] Document buffer sizing rationale in comments
- [ ] Add buffer monitoring functions (optional)

**Deliverable:** CAN buffers optimized

**Risk:** Low-Medium - Monitor for overruns
**Estimated Time:** 15 minutes

---

### 3.2 CAN Buffer Monitoring (Optional but Recommended)
**File:** Add to `lib/aio_communications/CANManager.cpp`

**New Functions:**
```cpp
void CANManager::logBufferStatus() {
    LOG_INFO(EventSource::CAN,
             "CAN1 RX: %d/%d, CAN2 RX: %d/%d, CAN3 RX: %d/%d",
             globalCAN1.getRXQueueCount(), 64,
             globalCAN2.getRXQueueCount(), 32,
             globalCAN3.getRXQueueCount(), 128);
}
```

**Tasks:**
- [ ] Add buffer monitoring methods
- [ ] Log buffer status periodically during testing
- [ ] Add overflow detection
- [ ] Document maximum observed buffer usage

**Deliverable:** CAN buffer health monitoring

**Risk:** Low
**Estimated Time:** 30 minutes

---

## Phase 4: Testing and Validation

### 4.1 Memory Verification
**Tasks:**
- [ ] Clean build and capture memory statistics
- [ ] Verify RAM1 usage reduction:
  - Expected: ~400 KB used (vs 501 KB current)
  - Expected: ~121 KB free (vs 23 KB current)
- [ ] Verify Flash usage (should be similar)
- [ ] Document before/after comparison

**Success Criteria:**
- RAM1 usage: ≤ 80%
- Free stack space: ≥ 100 KB
- No compilation warnings

**Deliverable:** Confirmed memory savings

**Risk:** Low
**Estimated Time:** 15 minutes

---

### 4.2 Network Functionality Testing

#### 4.2.1 Basic Network Tests
**Tasks:**
- [ ] Test: Ethernet link up/down detection
- [ ] Test: Static IP configuration (192.168.5.126)
- [ ] Test: DHCP IP acquisition (if supported)
- [ ] Test: Ping from PC to Teensy
- [ ] Test: ARP resolution
- [ ] Test: Network reconnection after cable unplug

**Deliverable:** Ethernet layer functional

---

#### 4.2.2 UDP Communication Tests
**Tasks:**
- [ ] Test: Receive PGN messages from AgOpenGPS
  - PGN 200 (steer config)
  - PGN 201 (steer data)
  - PGN 202 (machine config)
  - PGN 238 (steer settings)
- [ ] Test: Send PGN messages to AgOpenGPS
  - PGN 253 (machine status) @ 100Hz
  - PGN 250 (sensor data) @ 10Hz
- [ ] Test: UDP packet loss rate (should be 0%)
- [ ] Test: High-frequency UDP traffic (100Hz steer loop)
- [ ] Test: Subnet scanning (PGN 202 response)

**Success Criteria:**
- All PGN messages received correctly
- No packet loss during 10-minute test
- 100Hz autosteer loop maintains timing

**Deliverable:** UDP communication verified

---

#### 4.2.3 Web Server Tests
**Tasks:**
- [ ] Test: Access home page (/)
- [ ] Test: Access all configuration pages
  - Device Settings
  - CAN Configuration
  - Network Settings
  - GPS Configuration
  - OTA Update
  - Event Logger
  - Log Viewer
- [ ] Test: Save configuration (POST requests)
- [ ] Test: Configuration persistence (reboot test)
- [ ] Test: Multiple page loads in sequence
- [ ] Test: CSS/static content loading
- [ ] Test: Long-running web session stability

**Success Criteria:**
- All pages load without errors
- Configuration saves and persists
- No crashes during web usage

**Deliverable:** Web interface fully functional

---

### 4.3 CAN Bus Testing

#### 4.3.1 CAN Buffer Stress Tests
**Tasks:**
- [ ] Test: CAN1 (V-Bus) under normal load
  - Monitor buffer usage: should stay < 50%
  - Test valve commands
  - Test hitch commands
  - Test button inputs
- [ ] Test: CAN2 (K-Bus) Keya motor @ 100Hz
  - Monitor buffer usage: should stay < 60%
  - Test continuous autosteer operation
  - Test rapid command changes
- [ ] Test: CAN3 (ISO-Bus) with implements
  - Monitor buffer usage: should stay < 70%
  - Test implement control
  - Test multi-implement scenarios (if applicable)
- [ ] Check for buffer overruns in logs
- [ ] Monitor for dropped CAN messages

**Success Criteria:**
- No buffer overruns during 30-minute test
- All CAN messages processed correctly
- Buffer usage remains below 75% peak

**Deliverable:** CAN buffers adequately sized

---

#### 4.3.2 CAN Functional Tests
**Tasks:**
- [ ] Test: Keya motor control (CAN2)
  - Autosteer engagement
  - Motor direction changes
  - Motor speed control
  - Motor encoder feedback
- [ ] Test: Valve control (CAN1)
  - All valve functions
  - Danfoss valve control
- [ ] Test: Hitch control (CAN1)
  - Hitch position commands
- [ ] Test: Button inputs (CAN1)
  - Remote autosteer toggle
  - Other button functions
- [ ] Test: ISOBUS communication (CAN3)
  - Implement detection
  - Section control

**Success Criteria:**
- All CAN functions work identically to before
- No loss of functionality

**Deliverable:** CAN functionality verified

---

### 4.4 Integration Testing

#### 4.4.1 Full System Test
**Tasks:**
- [ ] Test: Complete autosteer session
  - Connect to AgOpenGPS
  - Engage autosteer
  - Run for 30+ minutes
  - Monitor for crashes/freezes
- [ ] Test: Web interface during autosteer
  - Access web pages while steering active
  - Verify no performance impact
- [ ] Test: Configuration changes during operation
  - Change settings via web
  - Verify settings apply correctly
- [ ] Test: Network reconnection scenarios
  - Unplug/replug network cable
  - Restart AgOpenGPS
  - Change network configuration

**Success Criteria:**
- System runs continuously for 1+ hour
- No crashes or hangs
- All features work as expected

**Deliverable:** System integration verified

---

#### 4.4.2 Memory Stability Test
**Tasks:**
- [ ] Run system for extended period (4+ hours)
- [ ] Monitor for memory leaks
- [ ] Check stack high-water mark
- [ ] Verify no stack overflows
- [ ] Monitor heap fragmentation (if applicable)

**Success Criteria:**
- No memory leaks detected
- Stack usage remains stable
- System stable after extended runtime

**Deliverable:** Memory stability confirmed

---

## Phase 5: Documentation and Cleanup

### 5.1 Update Documentation
**Tasks:**
- [ ] Update `CLAUDE.md` with NativeEthernet usage
- [ ] Update `README.md` library dependencies
- [ ] Document FNET configuration rationale
- [ ] Document CAN buffer sizing decisions
- [ ] Update memory usage in documentation
- [ ] Add migration notes for future reference

**Deliverable:** Complete documentation

**Risk:** Low
**Estimated Time:** 1 hour

---

### 5.2 Code Cleanup
**Tasks:**
- [ ] Remove commented-out QNEthernet code
- [ ] Remove unused includes
- [ ] Remove debugging/monitoring code (if temporary)
- [ ] Format code consistently
- [ ] Run linter/formatter if available

**Deliverable:** Clean, production-ready code

**Risk:** Low
**Estimated Time:** 30 minutes

---

### 5.3 Version Update
**Tasks:**
- [ ] Update `Version.h` to 1.0.78-beta
- [ ] Update version comment: "Memory optimization: Migrated to NativeEthernet/FNET, reduced CAN buffers"
- [ ] Create git commit with detailed message
- [ ] Tag release if appropriate

**Deliverable:** Version bumped and committed

**Risk:** Low
**Estimated Time:** 15 minutes

---

## Phase 6: Rollback Plan (If Needed)

### 6.1 Rollback Procedure
**If migration fails or shows critical issues:**

1. [ ] Switch back to `backup-before-memory-optimization` branch
2. [ ] Restore QNEthernet in `platformio.ini`
3. [ ] Clean build
4. [ ] Deploy known-good firmware

**Success Criteria:**
- System restored to pre-migration state within 15 minutes

---

### 6.2 Partial Rollback Options

**Option A: Keep CAN changes, rollback Ethernet**
- Keep optimized CAN buffers (21 KB savings)
- Revert to QNEthernet
- Net savings: 21 KB

**Option B: Keep Ethernet, rollback CAN**
- Keep NativeEthernet (80 KB savings)
- Revert CAN buffer sizes
- Net savings: 80 KB

**Decision Criteria:**
- If network issues: Rollback Ethernet, keep CAN
- If CAN overruns: Rollback CAN, keep Ethernet
- If both problematic: Full rollback

---

## Risk Assessment

### High Risk Items
1. **Web server stability** - Complex state machine may have lwIP dependencies
2. **UDP timing** - 100Hz autosteer loop is timing-critical
3. **NativeEthernet maintenance** - Library less actively developed

### Mitigation Strategies
1. **Extensive testing** - Follow all test procedures
2. **Incremental approach** - Test each component separately
3. **Quick rollback** - Keep backup branch ready
4. **Buffer monitoring** - Add instrumentation to catch issues early

---

## Success Criteria Summary

### Memory Goals
- ✅ RAM1 usage: ≤ 80% (target: 76%)
- ✅ Free stack: ≥ 100 KB (target: 121 KB)
- ✅ No memory leaks after 4-hour test

### Functionality Goals
- ✅ All UDP communication working
- ✅ Web interface fully functional
- ✅ All CAN buses operational
- ✅ No packet/message loss
- ✅ Autosteer performs identically

### Performance Goals
- ✅ 100Hz steer loop maintains timing
- ✅ Web response time similar or better
- ✅ No crashes during 4+ hour test

---

## Timeline Estimate

| Phase | Estimated Time | Risk Level |
|-------|----------------|------------|
| Phase 1: Preparation | 1 hour | Low |
| Phase 2: NativeEthernet Migration | 4-8 hours | Medium-High |
| Phase 3: CAN Optimization | 1 hour | Low-Medium |
| Phase 4: Testing | 6-8 hours | Medium |
| Phase 5: Documentation | 2 hours | Low |
| **Total** | **14-20 hours** | **Medium** |

**Recommended Approach:** Allocate 2-3 development sessions, with testing spread across multiple days to catch stability issues.

---

## Approval Checklist

Before proceeding, confirm:
- [ ] Backup of current firmware exists
- [ ] Test environment available (AgOpenGPS PC + test setup)
- [ ] Time allocated for thorough testing
- [ ] Understanding of rollback procedure
- [ ] Acceptance of Medium risk level
- [ ] Commitment to complete all testing phases

---

## Post-Migration Monitoring

### Week 1 After Deployment
- [ ] Monitor for crashes/freezes
- [ ] Monitor CAN buffer usage
- [ ] Monitor network stability
- [ ] Collect user feedback (if applicable)

### Week 2-4 After Deployment
- [ ] Verify long-term stability
- [ ] Check for edge cases
- [ ] Document any issues found
- [ ] Consider buffer adjustments if needed

---

## Notes

**Important Considerations:**
1. NativeEthernet uses FNET, not lwIP - completely different stack
2. API is Arduino Ethernet.h compatible, but behavioral differences may exist
3. FNET configuration is different from lwIP configuration
4. This is a significant architectural change - thorough testing is essential

**Alternative Approaches:**
- If NativeEthernet proves problematic, consider optimizing QNEthernet buffers instead (17 KB savings)
- CAN buffer optimization can be done independently
- Can implement in two separate PRs if preferred

**Future Optimization Opportunities:**
- PSRAM for sensor fusion data (if/when implemented)
- FNET buffer fine-tuning based on real-world usage
- Additional CAN buffer reduction after monitoring
