// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

// CANGlobals.cpp - Global CAN bus instances
#include "CANGlobals.h"
#include <Arduino.h>
#include "EventLogger.h"

// Instantiate the global CAN objects with reduced buffers (hardware filtering protects against overflow)
FlexCAN_T4<CAN1, RX_SIZE_16, TX_SIZE_16> globalCAN1;
FlexCAN_T4<CAN2, RX_SIZE_16, TX_SIZE_16> globalCAN2;
FlexCAN_T4<CAN3, RX_SIZE_32, TX_SIZE_64> globalCAN3;

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