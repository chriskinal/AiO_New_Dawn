// CANBusWrapper.h
// Wrapper to provide unified access to CAN buses whether using AgIsoStack or native

#ifndef CAN_BUS_WRAPPER_H
#define CAN_BUS_WRAPPER_H

#include <FlexCAN_T4.h>

#ifdef ENABLE_ISOBUS_VT
// Forward declare AgIsoStack's plugin
namespace isobus {
    class FlexCANT4Plugin;
}
#endif

class CANBusWrapper {
public:
    static FlexCAN_T4_Base* getCAN1();
    static FlexCAN_T4_Base* getCAN2();
    static FlexCAN_T4_Base* getCAN3();
    
    static void initializeCANBuses();
    
    // Helper to get specific typed instances when needed
    template<CAN_DEV_TABLE bus, FLEXCAN_RXQUEUE_TABLE rxSize, FLEXCAN_TXQUEUE_TABLE txSize>
    static FlexCAN_T4<bus, rxSize, txSize>* getTypedCAN();
    
private:
#ifndef ENABLE_ISOBUS_VT
    // When ISOBUS is disabled, we own the CAN instances
    static FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> can1Instance;
    static FlexCAN_T4<CAN2, RX_SIZE_256, TX_SIZE_16> can2Instance;
    static FlexCAN_T4<CAN3, RX_SIZE_256, TX_SIZE_256> can3Instance;
#endif
};

// Compatibility defines for existing code
#define globalCAN1 (*static_cast<FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16>*>(CANBusWrapper::getCAN1()))
#define globalCAN2 (*static_cast<FlexCAN_T4<CAN2, RX_SIZE_256, TX_SIZE_16>*>(CANBusWrapper::getCAN2()))
#define globalCAN3 (*static_cast<FlexCAN_T4<CAN3, RX_SIZE_256, TX_SIZE_256>*>(CANBusWrapper::getCAN3()))

// For backward compatibility
inline void initializeGlobalCANBuses() {
    CANBusWrapper::initializeCANBuses();
}

#endif // CAN_BUS_WRAPPER_H