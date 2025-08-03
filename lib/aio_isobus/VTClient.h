// VTClient.h
// ISOBUS Virtual Terminal client for New Dawn

#ifndef VT_CLIENT_H
#define VT_CLIENT_H

#include <Arduino.h>
#include <memory>

// Prevent AgIsoStack from using FASTRUN (ITCM)
#include "isobus_no_fastrun.h"

// Use custom CAN plugin instead of FlexCANT4Plugin
#include "can_hardware_interface_single_thread.hpp"
#include "isobus_virtual_terminal_client.hpp"
#include "isobus_diagnostic_protocol.hpp"
#include "can_partnered_control_function.hpp"
#include "can_internal_control_function.hpp"
#include "NewDawnCANPlugin.h"

#include "HelloWorldObjectPool.h"
#include "EventLogger.h"

class VTClient {
public:
    static VTClient* getInstance();
    
    bool init();
    void update();
    void shutdown();
    
    // Get status
    bool isConnected() const { return vtConnected; }
    uint32_t getCounter() const { return counter; }
    
private:
    VTClient();
    ~VTClient() = default;
    
    static VTClient* instance;
    
    // AgIsoStack components
    std::shared_ptr<isobus::NewDawnCANPlugin> canPlugin;
    std::shared_ptr<isobus::InternalControlFunction> internalCF;
    std::shared_ptr<isobus::DiagnosticProtocol> diagnostics;
    std::shared_ptr<isobus::VirtualTerminalClient> vtClient;
    std::shared_ptr<isobus::PartneredControlFunction> partnerVT;
    
    // Event listeners
    std::shared_ptr<void> softKeyListener;
    std::shared_ptr<void> buttonListener;
    
    // State
    bool vtConnected;
    uint32_t counter;
    uint32_t lastUpdateTime;
    
    // Callbacks
    static void handleVTKeyEvents(const isobus::VirtualTerminalClient::VTKeyEvent &event);
    
    // Helper functions
    void setupISOBUSStack();
    void updateCounter();
};

#endif // VT_CLIENT_H