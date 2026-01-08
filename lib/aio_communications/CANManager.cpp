// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

// CANManager.cpp - Simple CAN bus manager
#include "CANManager.h"
#include "EventLogger.h"

bool CANManager::init() {
    LOG_INFO(EventSource::CAN, "CAN Manager Initialization starting");
    
    // Global CAN buses should already be initialized
    LOG_DEBUG(EventSource::CAN, "Using global CAN instances");
    LOG_DEBUG(EventSource::CAN, "CAN1: Ready at 250kbps");
    LOG_DEBUG(EventSource::CAN, "CAN2: Ready at 250kbps");
    LOG_DEBUG(EventSource::CAN, "CAN3: Ready at 250kbps");
    
    // Poll for devices for 1 second
    LOG_DEBUG(EventSource::CAN, "Polling for CAN devices...");
    uint32_t startTime = millis();
    while (millis() - startTime < 1000) {
        pollForDevices();
        delay(10);  // Small delay between polls
    }
    
    // Report detected devices
    if (can1Active) {
        LOG_INFO(EventSource::CAN, "CAN1: Active devices detected");
    }
    if (can2Active) {
        LOG_INFO(EventSource::CAN, "CAN2: Active devices detected");
    }
    if (can3Active) {
        LOG_INFO(EventSource::CAN, "CAN3: Active devices detected");
        if (keyaDetected) {
            LOG_INFO(EventSource::CAN, "Keya motor detected on CAN3");
        }
    }
    
    LOG_INFO(EventSource::CAN, "CAN Manager initialization complete");
    return true;
}

void CANManager::pollForDevices() {
    CAN_message_t msg;
    
    // Only check CAN3 for Keya - process ONE message per loop
    if (can3->read(msg)) {
        if (!can3Active) {
            can3Active = true;
            LOG_DEBUG(EventSource::CAN, "First message on CAN3: ID 0x%08X", msg.id);
        }
        
        // Check for Keya heartbeat (0x07000001)
        if (msg.flags.extended && msg.id == 0x07000001) {
            if (!keyaDetected) {
                keyaDetected = true;
                LOG_INFO(EventSource::CAN, "Keya motor heartbeat detected (0x07000001)");
            }
        }
    }
    
    // Skip CAN1 and CAN2 if not in use
    // CAN polling not currently used - motor drivers handle CAN directly
}