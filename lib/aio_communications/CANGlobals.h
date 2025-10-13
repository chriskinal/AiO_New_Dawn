// CANGlobals.h - Global CAN bus instances to avoid ownership conflicts
#ifndef CAN_GLOBALS_H
#define CAN_GLOBALS_H

#include <FlexCAN_T4.h>

// Global CAN instances - defined here, instantiated in CANGlobals.cpp
// Reduced buffer sizes with hardware mailbox filtering protection
extern FlexCAN_T4<CAN1, RX_SIZE_16, TX_SIZE_16> globalCAN1;  // 256->16 saves ~12KB
extern FlexCAN_T4<CAN2, RX_SIZE_16, TX_SIZE_16> globalCAN2;  // 256->16 saves ~12KB
extern FlexCAN_T4<CAN3, RX_SIZE_32, TX_SIZE_64> globalCAN3;  // 256->32 saves ~11KB

// Initialize all CAN buses
void initializeGlobalCANBuses();

// Set CAN bus speed (must be called before any CAN usage)
void setCAN1Speed(uint32_t speed);
void setCAN2Speed(uint32_t speed);
void setCAN3Speed(uint32_t speed);

#endif // CAN_GLOBALS_H