// CANManager.h - Simple CAN bus manager (like SerialManager)
#ifndef CAN_MANAGER_H
#define CAN_MANAGER_H

#include <Arduino.h>
#include "CANGlobals.h"

class CANManager {
public:
    // Use pointers to global CAN instances (reduced buffer sizes with hardware filtering)
    FlexCAN_T4<CAN1, RX_SIZE_16, TX_SIZE_16>* can1;
    FlexCAN_T4<CAN2, RX_SIZE_16, TX_SIZE_16>* can2;
    FlexCAN_T4<CAN3, RX_SIZE_32, TX_SIZE_64>* can3;
    
    CANManager() : can1(&globalCAN1), can2(&globalCAN2), can3(&globalCAN3) {}
    ~CANManager() = default;
    
    // Initialize all CAN buses
    bool init();
    
    // Poll for device detection (sets flags, doesn't process messages)
    void pollForDevices();
    
    // Poll for devices for a specific duration (milliseconds)
    void pollForDevicesWithTimeout(uint32_t timeoutMs);
    
    // Simple detection flags
    bool isKeyaDetected() const { return keyaDetected; }
    bool isCAN1Active() const { return can1Active; }
    bool isCAN2Active() const { return can2Active; }
    bool isCAN3Active() const { return can3Active; }
    
private:
    // Detection flags
    bool keyaDetected = false;
    bool can1Active = false;
    bool can2Active = false;
    bool can3Active = false;
};

#endif // CAN_MANAGER_H