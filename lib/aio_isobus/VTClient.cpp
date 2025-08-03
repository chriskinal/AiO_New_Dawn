// VTClient.cpp
// ISOBUS Virtual Terminal client implementation

// Prevent this code from being placed in ITCM (fast memory) due to size
#pragma GCC optimize ("O2")
#undef FASTRUN
#define FASTRUN

#include "VTClient.h"
#include "HardwareManager.h"
#include "CANManager.h"
#include "Version.h"
#include "can_stack_logger.hpp"

using namespace isobus;

// Static instance
VTClient* VTClient::instance = nullptr;

// Custom logger for AgIsoStack
class ISOBUSLogger : public CANStackLogger {
public:
    void sink_CAN_stack_log(CANStackLogger::LoggingLevel level, const std::string &text) override {
        EventSource source = EventSource::NETWORK;  // Use NETWORK for CAN-related logs
        
        switch (level) {
            case LoggingLevel::Debug:
                LOG_DEBUG(source, "[ISOBUS] %s", text.c_str());
                break;
            case LoggingLevel::Info:
                LOG_INFO(source, "[ISOBUS] %s", text.c_str());
                break;
            case LoggingLevel::Warning:
                LOG_WARNING(source, "[ISOBUS] %s", text.c_str());
                break;
            case LoggingLevel::Error:
                LOG_ERROR(source, "[ISOBUS] %s", text.c_str());
                break;
            case LoggingLevel::Critical:
                LOG_ERROR(source, "[ISOBUS Critical] %s", text.c_str());
                break;
        }
    }
};

static ISOBUSLogger isobusLogger;

VTClient::VTClient() : 
    vtConnected(false), 
    counter(0), 
    lastUpdateTime(0) {
}

VTClient* VTClient::getInstance() {
    if (!instance) {
        instance = new VTClient();
    }
    return instance;
}

bool VTClient::init() {
    LOG_INFO(EventSource::NETWORK, "=== ISOBUS VT Client Initialization ===");
    
    // Setup ISOBUS stack
    setupISOBUSStack();
    
    LOG_INFO(EventSource::NETWORK, "ISOBUS VT Client initialized");
    return true;
}

void VTClient::setupISOBUSStack() {
    // Configure logger
    CANStackLogger::set_can_stack_logger_sink(&isobusLogger);
    CANStackLogger::set_log_level(CANStackLogger::LoggingLevel::Info);
    
    // Create CAN plugin for CAN2 (channel 1 in New Dawn)
    canPlugin = std::make_shared<NewDawnCANPlugin>(1);  // CAN2 is index 1
    
    // Configure CAN hardware for 250kbps (ISOBUS standard)
    CANHardwareInterface::set_number_of_can_channels(1);
    CANHardwareInterface::assign_can_channel_frame_handler(0, canPlugin);
    
    // Don't start yet - let CANManager handle the FlexCAN initialization
    
    // Create device NAME
    NAME deviceNAME(0);
    deviceNAME.set_arbitrary_address_capable(true);
    deviceNAME.set_industry_group(2);  // Agricultural
    deviceNAME.set_device_class(0);
    deviceNAME.set_function_code(static_cast<uint8_t>(NAME::Function::SteeringControl));
    deviceNAME.set_identity_number(1234);  // Should be unique
    deviceNAME.set_ecu_instance(0);
    deviceNAME.set_function_instance(0);
    deviceNAME.set_device_class_instance(0);
    deviceNAME.set_manufacturer_code(420);  // AgOpenGPS manufacturer code
    
    // Create internal control function (our ECU)
    internalCF = InternalControlFunction::create(deviceNAME, 0x26, 0);  // Address 0x26
    
    // Setup diagnostics
    diagnostics = std::make_shared<DiagnosticProtocol>(internalCF);
    diagnostics->initialize();
    diagnostics->set_product_identification_brand("AgOpenGPS");
    diagnostics->set_product_identification_code("NEWDAWN");
    diagnostics->set_product_identification_model("AiO New Dawn");
    diagnostics->set_software_id_field(0, FIRMWARE_VERSION);
    
    // Setup VT client
    const NAMEFilter filterVT(NAME::NAMEParameters::FunctionCode, 
                             static_cast<uint8_t>(NAME::Function::VirtualTerminal));
    const std::vector<NAMEFilter> vtFilters = { filterVT };
    
    partnerVT = PartneredControlFunction::create(0, vtFilters);
    vtClient = std::make_shared<VirtualTerminalClient>(partnerVT, internalCF);
    
    // Set object pool
    vtClient->set_object_pool(0, VirtualTerminalClient::VTVersion::Version3, 
                             HelloWorldObjectPool, HelloWorldObjectPoolSize, "HW01");
    
    // Register callbacks
    softKeyListener = vtClient->add_vt_soft_key_event_listener(handleVTKeyEvents);
    buttonListener = vtClient->add_vt_button_event_listener(handleVTKeyEvents);
    
    // Initialize VT client (false = don't auto-upload pool on connect)
    vtClient->initialize(true);
    
    // Now start the CAN hardware
    CANHardwareInterface::start();
    
    LOG_INFO(EventSource::NETWORK, "ISOBUS stack configured on CAN2");
}

void VTClient::update() {
    // Update ISOBUS stack
    CANHardwareInterface::update();
    
    if (diagnostics) {
        diagnostics->update();
    }
    
    if (vtClient) {
        vtClient->update();
        
        // Check connection status
        bool isConnected = vtClient->get_is_connected();
        if (isConnected != vtConnected) {
            vtConnected = isConnected;
            if (vtConnected) {
                LOG_INFO(EventSource::NETWORK, "VT connected successfully!");
                counter = 0;
                // Initialize display values
                vtClient->send_change_numeric_value(HelloWorld_NumberVariable, counter);
            } else {
                LOG_WARNING(EventSource::NETWORK, "VT disconnected");
            }
        }
        
        // Update counter every second if connected
        if (vtConnected && (millis() - lastUpdateTime >= 1000)) {
            updateCounter();
            lastUpdateTime = millis();
        }
    }
}

void VTClient::updateCounter() {
    counter++;
    
    // Update the counter display on VT
    if (vtClient && vtConnected) {
        vtClient->send_change_numeric_value(HelloWorld_NumberVariable, counter);
        LOG_DEBUG(EventSource::NETWORK, "VT counter updated to %d", counter);
    }
}

void VTClient::shutdown() {
    LOG_INFO(EventSource::NETWORK, "Shutting down ISOBUS VT client");
    
    if (vtClient) {
        vtClient->terminate();
    }
    
    CANHardwareInterface::stop();
}

// Static callback handlers
void VTClient::handleVTKeyEvents(const VirtualTerminalClient::VTKeyEvent &event) {
    VTClient* self = getInstance();
    
    switch (event.keyEvent) {
        case VirtualTerminalClient::KeyActivationCode::ButtonUnlatchedOrReleased:
            LOG_INFO(EventSource::NETWORK, "VT button released: Object ID %d", event.objectID);
            
            switch (event.objectID) {
                case HelloWorld_SoftKey_Exit:
                    LOG_INFO(EventSource::NETWORK, "Exit soft key pressed");
                    // Could change to a different mask or perform some action
                    break;
                    
                case HelloWorld_SoftKey_Increment:
                    LOG_INFO(EventSource::NETWORK, "Increment soft key pressed");
                    self->counter++;
                    self->vtClient->send_change_numeric_value(HelloWorld_NumberVariable, self->counter);
                    break;
            }
            break;
            
        case VirtualTerminalClient::KeyActivationCode::ButtonStillHeld:
            LOG_DEBUG(EventSource::NETWORK, "VT button held: Object ID %d", event.objectID);
            break;
            
        case VirtualTerminalClient::KeyActivationCode::ButtonPressedOrLatched:
            LOG_DEBUG(EventSource::NETWORK, "VT button pressed: Object ID %d", event.objectID);
            break;
    }
}