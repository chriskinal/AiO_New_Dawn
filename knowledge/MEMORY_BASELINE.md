# Memory Usage Baseline - Before Optimization

**Date:** 2025-10-11
**Branch:** main (commit: 9768c29)
**Firmware Version:** 1.0.76-beta

## Memory Statistics

### RAM1 (FlexRAM - ITCM + DTCM)
```
Total RAM1:       524,288 bytes (512 KB)
Variables:        140,832 bytes (137.5 KB)
Code (ITCM):      337,508 bytes (329.6 KB)
Padding:           22,940 bytes ( 22.4 KB)
--------------------------------
Used:             501,280 bytes (489.5 KB)
Free (stack):      23,008 bytes ( 22.5 KB)
Utilization:       95.5% ⚠️ CRITICAL
```

### Build Output
```
Processing teensy41 (platform: teensy; board: teensy41; framework: arduino)
teensy_size:    RAM1: variables:140832, code:337508, padding:22940   free for local variables:23008
========================= [SUCCESS] Took 3.17 seconds =========================
```

## Key Memory Consumers

### Network (QNEthernet/lwIP)
- PBUF_POOL: ~24 KB
- TCP/UDP PCBs: ~2 KB
- Ethernet DMA buffers: ~15 KB (in RAM2)
- **Total Network: ~41 KB**

### CAN Buffers (FlexCAN_T4)
- CAN1 (V-Bus): RX=256, TX=16 = ~7.6 KB
- CAN2 (K-Bus): RX=256, TX=16 = ~7.6 KB
- CAN3 (ISO-Bus): RX=256, TX=256 = ~14.3 KB
- **Total CAN: ~29.5 KB**

### Other
- System tables: ~10 KB
- Library objects: ~5 KB
- Application data: ~30 KB
- Remaining variables: ~25 KB

## Problem Statement

With only **22.5 KB** free for stack and local variables, the system is at **95.5% RAM utilization**. This leaves minimal headroom for:
- Deep function call stacks
- Large local arrays
- Future features (sensor fusion, VWAS, etc.)
- Safety margin for stack growth

## Optimization Goals

### Target After Optimization
```
Total RAM1:       524,288 bytes (512 KB)
Used:            ~400,000 bytes (390 KB)
Free (stack):    ~124,000 bytes (121 KB)
Utilization:      ~76% ✓ HEALTHY
```

### Expected Savings
- NativeEthernet/FNET migration: **~80 KB**
- CAN buffer optimization: **~21 KB**
- **Total savings: ~101 KB**

## Notes

- Current configuration uses QNEthernet with lwIP stack
- CAN buffers sized for maximum theoretical traffic
- No PSRAM currently utilized
- FlexRAM dynamically partitions between ITCM/DTCM (11 banks ITCM, 5 banks DTCM)
