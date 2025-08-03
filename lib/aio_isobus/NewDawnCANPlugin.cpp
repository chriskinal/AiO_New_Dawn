// NewDawnCANPlugin.cpp
// Implementation of custom CAN plugin for AgIsoStack

#include "NewDawnCANPlugin.h"
#include "can_stack_logger.hpp"
#include <Arduino.h>

namespace isobus {

NewDawnCANPlugin::NewDawnCANPlugin(std::uint8_t channel) : 
    selectedChannel(channel),
    isOpen(false) {
}

bool NewDawnCANPlugin::get_is_valid() const {
    return (selectedChannel < 3);  // We have CAN1, CAN2, CAN3
}

void NewDawnCANPlugin::close() {
    if (isOpen) {
        // CANManager handles the actual hardware, we just track state
        isOpen = false;
        CANStackLogger::CAN_stack_log(CANStackLogger::LoggingLevel::Info, 
            "[NewDawnCANPlugin] Closed CAN channel " + std::to_string(selectedChannel));
    }
}

void NewDawnCANPlugin::open() {
    if (!isOpen && get_is_valid()) {
        // The CAN hardware is already initialized by CANManager
        // We just need to mark ourselves as open
        isOpen = true;
        
        // Clear any pending messages
        while (!rxQueue.empty()) {
            rxQueue.pop();
        }
        
        CANStackLogger::CAN_stack_log(CANStackLogger::LoggingLevel::Info, 
            "[NewDawnCANPlugin] Opened CAN channel " + std::to_string(selectedChannel));
    }
}

bool NewDawnCANPlugin::read_frame(isobus::CANMessageFrame &canFrame) {
    if (!isOpen) {
        return false;
    }
    
    CAN_message_t msg;
    bool messageRead = false;
    
    // Try to read a message based on channel
    switch (selectedChannel) {
        case 0:
            messageRead = globalCAN1.read(msg);
            break;
        case 1:
            messageRead = globalCAN2.read(msg);
            break;
        case 2:
            messageRead = globalCAN3.read(msg);
            break;
        default:
            return false;
    }
    
    if (messageRead) {
        // Convert FlexCAN message to ISOBUS format
        canFrame.identifier = msg.id;
        canFrame.isExtendedFrame = (msg.flags.extended != 0);
        canFrame.dataLength = msg.len;
        
        // Copy data
        for (std::uint8_t i = 0; i < msg.len && i < 8; i++) {
            canFrame.data[i] = msg.buf[i];
        }
        
        // Set timestamp (convert from microseconds to milliseconds)
        canFrame.timestamp_us = millis() * 1000UL;
        
        return true;
    }
    
    return false;
}

bool NewDawnCANPlugin::write_frame(const isobus::CANMessageFrame &canFrame) {
    if (!isOpen || !get_is_valid()) {
        return false;
    }
    
    // Convert ISOBUS frame to FlexCAN format
    CAN_message_t msg;
    msg.id = canFrame.identifier;
    msg.flags.extended = canFrame.isExtendedFrame ? 1 : 0;
    msg.flags.remote = 0;  // ISOBUS doesn't use remote frames
    msg.len = canFrame.dataLength;
    
    // Copy data
    for (std::uint8_t i = 0; i < canFrame.dataLength && i < 8; i++) {
        msg.buf[i] = canFrame.data[i];
    }
    
    // Send the message based on channel
    int result = -1;
    switch (selectedChannel) {
        case 0:
            result = globalCAN1.write(msg);
            break;
        case 1:
            result = globalCAN2.write(msg);
            break;
        case 2:
            result = globalCAN3.write(msg);
            break;
    }
    
    if (result < 0) {
        CANStackLogger::CAN_stack_log(CANStackLogger::LoggingLevel::Warning, 
            "[NewDawnCANPlugin] Failed to write CAN frame");
        return false;
    }
    
    return true;
}

FlexCAN_T4_Base* NewDawnCANPlugin::getCANInstance() const {
    switch (selectedChannel) {
        case 0:
            return &globalCAN1;
        case 1:
            return &globalCAN2;
        case 2:
            return &globalCAN3;
        default:
            return nullptr;
    }
}

} // namespace isobus