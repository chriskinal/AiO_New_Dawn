// CANGlobalsWithISOBUS.h
// Alternative CAN globals that use AgIsoStack's FlexCAN instances when ISOBUS is enabled
// This avoids duplicating FlexCAN_T4 instances and saves memory

#ifndef CAN_GLOBALS_WITH_ISOBUS_H
#define CAN_GLOBALS_WITH_ISOBUS_H

#include <FlexCAN_T4.h>

#ifdef ENABLE_ISOBUS_VT
    // When ISOBUS is enabled, use AgIsoStack's FlexCAN instances
    #include "flex_can_t4_plugin.hpp"
    
    // Create references to AgIsoStack's CAN instances
    #define globalCAN1 isobus::FlexCANT4Plugin::can0
    #define globalCAN2 isobus::FlexCANT4Plugin::can1
    #define globalCAN3 isobus::FlexCANT4Plugin::can2
    
    // AgIsoStack will initialize these, so we just need a stub
    inline void initializeGlobalCANBuses() {
        // AgIsoStack handles initialization
        // We just need to ensure our code doesn't re-initialize
    }
#else
    // When ISOBUS is disabled, use New Dawn's original CAN instances
    extern FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> globalCAN1;
    extern FlexCAN_T4<CAN2, RX_SIZE_256, TX_SIZE_16> globalCAN2;
    extern FlexCAN_T4<CAN3, RX_SIZE_256, TX_SIZE_256> globalCAN3;
    
    // Initialize all CAN buses
    void initializeGlobalCANBuses();
#endif

#endif // CAN_GLOBALS_WITH_ISOBUS_H