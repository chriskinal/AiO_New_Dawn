// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

// KeyaSerialDriver.cpp - Keya Serial motor driver implementation
#include "KeyaSerialDriver.h"

bool KeyaSerialDriver::init() {
    // SerialRS232 already initialized by SerialManager at 115200
    LOG_INFO(EventSource::AUTOSTEER, "KeyaSerialDriver initialized");
    
    // Clear any pending data
    while (SerialRS232.available()) {
        SerialRS232.read();
    }
    
    return true;
}

void KeyaSerialDriver::enable(bool en) {
    if (enabled != en) {
        LOG_INFO(EventSource::AUTOSTEER, "Keya Serial motor %s", en ? "enabled" : "disabled");
    }
    enabled = en;
    
    // Reset slip detection when motor is disabled
    if (!en) {
        motorSlipDetected = false;
        slipStartTime = 0;
    }
}

void KeyaSerialDriver::setPWM(int16_t pwm) {
    targetPWM = constrain(pwm, -255, 255);
}

void KeyaSerialDriver::stop() {
    targetPWM = 0;
}

void KeyaSerialDriver::process() {
    // Check for response first
    checkResponse();
    
    // Send command every 20ms (50Hz) for smoother control
    if (millis() - lastCommandTime >= 20) {
        sendCommand();
    }
    
    // Check for motor slip if enabled
    if (enabled) {
        motorSlipDetected = checkMotorSlip();
    }
}

void KeyaSerialDriver::sendCommand() {
    // Build command based on state
    buildCommand();
    
    // Clear any pending data
    while (SerialRS232.available()) {
        SerialRS232.read();
    }
    
    // Send command
    SerialRS232.write(commandBuffer, 4);
    
    lastCommandTime = millis();
    responseIndex = 0;
}

void KeyaSerialDriver::checkResponse() {
    // Read available bytes
    while (SerialRS232.available() && responseIndex < sizeof(responseBuffer)) {
        responseBuffer[responseIndex++] = SerialRS232.read();
    }
    
    // Only process when we have complete 15-byte response
    if (responseIndex >= 15) {
        // Validate response
        if (responseBuffer[0] == 0xAC && responseBuffer[5] == 0xAD && responseBuffer[10] == 0xAE) {
            hasValidResponse = true;
            lastResponseTime = millis();
            
            // Parse data from response
            // Frame 1: Position (bytes 1-3)
            motorPosition = ((uint32_t)responseBuffer[1] << 16) | 
                           ((uint32_t)responseBuffer[2] << 8) | 
                           responseBuffer[3];
            
            // Frame 2: Speed, Current, Voltage
            actualRPM = (int8_t)responseBuffer[6];
            motorCurrent = (int8_t)responseBuffer[7];  // in 0.1A units
            motorVoltage = responseBuffer[8];          // in V
            
            // Frame 3: Error, Temperature
            motorErrorCode = ((uint16_t)responseBuffer[11] << 8) | responseBuffer[12];
            motorTemperature = responseBuffer[13];
        }
        
        // Clear buffer for next response
        responseIndex = 0;
    }
}

void KeyaSerialDriver::buildCommand() {
    if (enabled && targetPWM != 0) {
        // Enable with speed command
        commandBuffer[0] = 0xAD;
        
        // Convert PWM to speed value in 0.1 RPM units
        // PWM ±255 -> ±100 RPM -> ±1000 units (0.1 RPM)
        int16_t speedValue = (int16_t)(targetPWM * 1000 / 255);
        commandBuffer[1] = (speedValue >> 8) & 0xFF;
        commandBuffer[2] = speedValue & 0xFF;
    } else {
        // Disable command
        commandBuffer[0] = 0xAC;
        commandBuffer[1] = 0x00;
        commandBuffer[2] = 0x00;
    }
    
    // Calculate checksum
    commandBuffer[3] = calculateChecksum(commandBuffer, 3);
}

uint8_t KeyaSerialDriver::calculateChecksum(uint8_t* data, uint8_t length) {
    uint16_t sum = 0;
    for (uint8_t i = 0; i < length; i++) {
        sum += data[i];
    }
    return sum & 0xFF;
}

MotorStatus KeyaSerialDriver::getStatus() const {
    MotorStatus status;
    status.enabled = enabled;
    status.targetPWM = targetPWM;
    status.actualPWM = hasValidResponse ? (int16_t)(actualRPM * 255 / 100) : targetPWM;
    status.currentDraw = motorCurrent * 0.1f; // Convert to Amps
    status.hasError = !hasValidResponse || (millis() - lastResponseTime > 1000) || 
                      (motorErrorCode != 0 && motorErrorCode != 0x0001) || motorSlipDetected;
    
    return status;
}

void KeyaSerialDriver::handleKickout(KickoutType type, float value) {
    // Keya uses internal slip detection - check in process()
}

bool KeyaSerialDriver::checkMotorSlip() {
    // Only check if motor is enabled and has valid response
    if (!enabled || !hasValidResponse || targetPWM == 0) {
        slipStartTime = 0;  // Reset timer when not checking
        return false;
    }
    
    // Calculate commanded RPM from PWM
    int16_t commandedRPM = (targetPWM * 100) / 255;  // PWM to RPM
    
    // Calculate error between commanded and actual
    int16_t error = abs(commandedRPM - actualRPM);
    
    // Check for significant slip (>30% error or >20 RPM difference)
    if (error > abs(commandedRPM) * 0.3f || error > 20) {
        if (slipStartTime == 0) {
            slipStartTime = millis();
        }
        
        // Require slip for 200ms before triggering
        if (millis() - slipStartTime > 200) {
            LOG_WARNING(EventSource::AUTOSTEER, "Keya Serial slip detected! Cmd=%d Act=%d Error=%d", 
                       commandedRPM, actualRPM, error);
            return true;
        }
    } else {
        // Reset slip timer if within tolerance
        slipStartTime = 0;
    }
    
    return false;
}