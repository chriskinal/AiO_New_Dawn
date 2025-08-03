// CANGlobals.cpp - Global CAN bus instances
#include "CANGlobals.h"
#include <Arduino.h>

// Instantiate the global CAN objects
FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> globalCAN1;
FlexCAN_T4<CAN2, RX_SIZE_256, TX_SIZE_16> globalCAN2;
FlexCAN_T4<CAN3, RX_SIZE_256, TX_SIZE_256> globalCAN3;

void initializeGlobalCANBuses() {
    Serial.print("\r\n=== Initializing Global CAN Buses ===");
    
    // Initialize CAN1
    Serial.print("\r\n- CAN1: ");
    globalCAN1.begin();
    globalCAN1.setBaudRate(250000);
    Serial.print("250kbps");
    
    // Initialize CAN2 (unless using ISOBUS VT)
    #ifndef ENABLE_ISOBUS_VT
    Serial.print("\r\n- CAN2: ");
    globalCAN2.begin();
    globalCAN2.setBaudRate(250000);
    Serial.print("250kbps");
    #else
    Serial.print("\r\n- CAN2: Reserved for ISOBUS VT");
    #endif
    
    // Initialize CAN3
    Serial.print("\r\n- CAN3: ");
    globalCAN3.begin();
    globalCAN3.setBaudRate(250000);
    Serial.print("250kbps");
    
    Serial.print("\r\n=== Global CAN Buses Ready ===\r\n");
}