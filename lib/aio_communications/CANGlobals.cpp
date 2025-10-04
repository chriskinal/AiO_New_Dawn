// CANGlobals.cpp - Global CAN bus instances
#include "CANGlobals.h"
#include <Arduino.h>
#include "EventLogger.h"

// Instantiate the global CAN objects
FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> globalCAN1;
FlexCAN_T4<CAN2, RX_SIZE_256, TX_SIZE_16> globalCAN2;
FlexCAN_T4<CAN3, RX_SIZE_256, TX_SIZE_256> globalCAN3;

// Default speeds
static uint32_t can1Speed = 250000;
static uint32_t can2Speed = 250000;
static uint32_t can3Speed = 250000;

void setCAN1Speed(uint32_t speed) {
    can1Speed = speed;
}

void setCAN2Speed(uint32_t speed) {
    can2Speed = speed;
}

void setCAN3Speed(uint32_t speed) {
    can3Speed = speed;
}

void initializeGlobalCANBuses() {
    LOG_INFO(EventSource::CAN, "Initializing Global CAN Buses");

    // Initialize CAN1
    globalCAN1.begin();
    globalCAN1.setBaudRate(can1Speed);
    LOG_INFO(EventSource::CAN, "CAN1: %d bps", can1Speed);

    // Initialize CAN2
    globalCAN2.begin();
    globalCAN2.setBaudRate(can2Speed);
    LOG_INFO(EventSource::CAN, "CAN2: %d bps", can2Speed);

    // Initialize CAN3
    globalCAN3.begin();
    globalCAN3.setBaudRate(can3Speed);
    LOG_INFO(EventSource::CAN, "CAN3: %d bps", can3Speed);

    LOG_INFO(EventSource::CAN, "Global CAN Buses Ready");
}