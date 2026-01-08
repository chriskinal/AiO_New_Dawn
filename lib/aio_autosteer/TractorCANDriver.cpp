// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

// TractorCANDriver.cpp - Unified CAN driver implementation
#include "TractorCANDriver.h"

bool TractorCANDriver::init() {
    // Load configuration from EEPROM
    config = configManager.getCANSteerConfig();

    // Assign CAN bus pointers based on configuration
    assignCANBuses();

    LOG_INFO(EventSource::AUTOSTEER, "TractorCANDriver initialized - Brand: %d", config.brand);

    return true;
}

void TractorCANDriver::assignCANBuses() {
    // Reset all buses first
    steerBusNum = 0;
    steerCAN = nullptr;
    buttonBusNum = 0;
    buttonCAN = nullptr;
    hitchBusNum = 0;
    hitchCAN = nullptr;

    // For Keya function, find which bus has it
    if (hasKeyaFunction()) {
        if (config.can1Function & static_cast<uint8_t>(CANFunction::KEYA)) {
            steerBusNum = 1;
            steerCAN = getBusPointer(1);
        } else if (config.can2Function & static_cast<uint8_t>(CANFunction::KEYA)) {
            steerBusNum = 2;
            steerCAN = getBusPointer(2);
        } else if (config.can3Function & static_cast<uint8_t>(CANFunction::KEYA)) {
            steerBusNum = 3;
            steerCAN = getBusPointer(3);
        }
    }
    // For other brands, find bus with steering function
    else if (config.brand != static_cast<uint8_t>(TractorBrand::DISABLED)) {
        // Check which bus has steering function
        if (config.can1Function & static_cast<uint8_t>(CANFunction::STEERING)) {
            steerBusNum = 1;
            steerCAN = getBusPointer(1);
        } else if (config.can2Function & static_cast<uint8_t>(CANFunction::STEERING)) {
            steerBusNum = 2;
            steerCAN = getBusPointer(2);
        } else if (config.can3Function & static_cast<uint8_t>(CANFunction::STEERING)) {
            steerBusNum = 3;
            steerCAN = getBusPointer(3);
        }

        // Check for buttons/hitch functions
        if (config.can1Function & (static_cast<uint8_t>(CANFunction::BUTTONS) |
                                   static_cast<uint8_t>(CANFunction::HITCH))) {
            buttonBusNum = 1;
            buttonCAN = getBusPointer(1);
        } else if (config.can2Function & (static_cast<uint8_t>(CANFunction::BUTTONS) |
                                          static_cast<uint8_t>(CANFunction::HITCH))) {
            buttonBusNum = 2;
            buttonCAN = getBusPointer(2);
        } else if (config.can3Function & (static_cast<uint8_t>(CANFunction::BUTTONS) |
                                          static_cast<uint8_t>(CANFunction::HITCH))) {
            buttonBusNum = 3;
            buttonCAN = getBusPointer(3);
        }
    }
}

void* TractorCANDriver::getBusPointer(uint8_t busNum) {
    switch (busNum) {
        case 1: return (void*)&globalCAN1;  // K_Bus
        case 2: return (void*)&globalCAN2;  // ISO_Bus
        case 3: return (void*)&globalCAN3;  // V_Bus
        default: return nullptr;         // None
    }
}

bool TractorCANDriver::readCANMessage(uint8_t busNum, CAN_message_t& msg) {
    switch (busNum) {
        case 1: return globalCAN1.read(msg);
        case 2: return globalCAN2.read(msg);
        case 3: return globalCAN3.read(msg);
        default: return false;
    }
}

void TractorCANDriver::writeCANMessage(uint8_t busNum, const CAN_message_t& msg) {
    switch (busNum) {
        case 1: globalCAN1.write(msg); break;
        case 2: globalCAN2.write(msg); break;
        case 3: globalCAN3.write(msg); break;
    }
}

void TractorCANDriver::enable(bool en) {
    if (!enabled && en) {
        LOG_INFO(EventSource::AUTOSTEER, "TractorCAN enabled - %s", getTypeName());
    } else if (enabled && !en) {
        LOG_INFO(EventSource::AUTOSTEER, "TractorCAN disabled");
        targetPWM = 0;
        commandedRPM = 0.0f;
    }
    enabled = en;
}

void TractorCANDriver::setPWM(int16_t pwm) {
    pwm = constrain(pwm, -255, 255);
    targetPWM = pwm;

    // For Keya, convert PWM to RPM
    if (hasKeyaFunction()) {
        commandedRPM = (float)pwm * 100.0f / 255.0f;  // 255 PWM = 100 RPM
    }
}

void TractorCANDriver::stop() {
    targetPWM = 0;
    commandedRPM = 0.0f;
}

void TractorCANDriver::process() {
    // Process incoming CAN messages
    processIncomingMessages();

    // Send commands if we have a steering bus configured
    // For Keya, we need to send commands even when disabled to keep CAN alive
    if (steerCAN && steerBusNum > 0) {
        sendSteerCommands();
    }

    // Check for timeouts
    static bool timeoutLogged = false;
    if (config.brand != static_cast<uint8_t>(TractorBrand::DISABLED)) {
        if (steerReady && (millis() - lastSteerReadyTime > 250)) {
            steerReady = false;
            if (!timeoutLogged) {
                if (hasKeyaFunction()) {
                    heartbeatValid = false;
                    LOG_ERROR(EventSource::AUTOSTEER, "TractorCAN connection lost - no heartbeat");
                } else {
                    LOG_WARNING(EventSource::AUTOSTEER, "%s connection timeout - no valve ready for >250ms", getTypeName());
                }
                timeoutLogged = true;
            }
        } else if (steerReady) {
            // Reset the timeout logged flag when connection is good
            timeoutLogged = false;
        }
    }
}

void TractorCANDriver::processIncomingMessages() {
    CAN_message_t msg;

    // Process messages from each configured bus
    if (steerCAN && steerBusNum > 0) {
        while (readCANMessage(steerBusNum, msg)) {
            // If this bus has Keya function, process Keya messages
            if (hasKeyaFunction()) {
                processKeyaMessage(msg);
            } else {
                // Otherwise process based on brand
                switch (static_cast<TractorBrand>(config.brand)) {
                    case TractorBrand::CASEIH_NH:
                        processCaseIHMessage(msg);
                        break;
                    case TractorBrand::FENDT:
                    case TractorBrand::FENDT_ONE:
                        processFendtMessage(msg);
                        break;
                    case TractorBrand::VALTRA_MASSEY:
                        processValtraMessage(msg);
                        break;
                    case TractorBrand::CAT_MT:
                        processCATMessage(msg);
                        break;
                    case TractorBrand::CLAAS:
                        processClaasMessage(msg);
                        break;
                    case TractorBrand::JCB:
                        processJcbMessage(msg);
                        break;
                    case TractorBrand::LINDNER:
                        processLindnerMessage(msg);
                        break;
                    // Add other brands as needed
                }
            }
        }
    }

    // Process button bus messages if configured
    if (buttonCAN && buttonBusNum > 0 && buttonBusNum != steerBusNum) {
        while (readCANMessage(buttonBusNum, msg)) {
            switch (static_cast<TractorBrand>(config.brand)) {
                case TractorBrand::CASEIH_NH:
                    processCaseIHKBusMessage(msg);
                    break;
                case TractorBrand::VALTRA_MASSEY:
                    processMasseyKBusMessage(msg);
                    break;
                case TractorBrand::FENDT:
                case TractorBrand::FENDT_ONE:
                    processFendtKBusMessage(msg);
                    break;
                case TractorBrand::CAT_MT:
                    processCATKBusMessage(msg);
                    break;
                case TractorBrand::CLAAS:
                    processClaasKBusMessage(msg);
                    break;
                case TractorBrand::JCB:
                    processJcbKBusMessage(msg);
                    break;
                case TractorBrand::LINDNER:
                    processLindnerKBusMessage(msg);
                    break;
                // TODO: Process work switch messages for other brands
            }
        }
    }

    // Process hitch bus messages if configured
    if (hitchCAN && hitchBusNum > 0 && hitchBusNum != steerBusNum && hitchBusNum != buttonBusNum) {
        while (readCANMessage(hitchBusNum, msg)) {
            // TODO: Process hitch control messages
        }
    }
}

void TractorCANDriver::sendSteerCommands() {
    // If any bus has Keya function, send Keya commands
    if (hasKeyaFunction()) {
        sendKeyaCommands();
    } else {
        // Otherwise send based on brand
        switch (static_cast<TractorBrand>(config.brand)) {
            case TractorBrand::CASEIH_NH:
                sendCaseIHCommands();
                break;
            case TractorBrand::FENDT:
            case TractorBrand::FENDT_ONE:
                sendFendtCommands();
                break;
            case TractorBrand::VALTRA_MASSEY:
                sendValtraCommands();
                break;
            case TractorBrand::CAT_MT:
                sendCATCommands();
                break;
            case TractorBrand::CLAAS:
                sendClaasCommands();
                break;
            case TractorBrand::JCB:
                sendJcbCommands();
                break;
            case TractorBrand::LINDNER:
                sendLindnerCommands();
                break;
            // Add other brands as needed
        }
    }
}

// ===== Keya Implementation =====
void TractorCANDriver::processKeyaMessage(const CAN_message_t& msg) {
    // Check for heartbeat message (ID: 0x07000001)
    if (msg.id == 0x07000001 && msg.flags.extended) {
        // Heartbeat format (big-endian/MSB first):
        // Bytes 0-1: Position/Angle (uint16)
        // Bytes 2-3: Speed/RPM (int16)
        // Bytes 4-5: Current (int16)
        // Bytes 6-7: Error code (uint16)

        motorPosition = (uint16_t)((msg.buf[0] << 8) | msg.buf[1]);

        int16_t speedRaw = (int16_t)((msg.buf[2] << 8) | msg.buf[3]);
        actualRPM = (float)speedRaw;

        int16_t currentRaw = (int16_t)((msg.buf[4] << 8) | msg.buf[5]);
        motorCurrent = (uint16_t)abs(currentRaw);

        motorErrorCode = (uint16_t)((msg.buf[6] << 8) | msg.buf[7]);

        // Update status
        if (!steerReady) {
            LOG_INFO(EventSource::AUTOSTEER, "Keya motor detected and ready");
        }
        steerReady = true;
        heartbeatValid = true;
        lastSteerReadyTime = millis();
        lastHeartbeat = millis();
    }
}

void TractorCANDriver::sendKeyaCommands() {
    CAN_message_t msg;
    msg.id = 0x06000001;
    msg.flags.extended = 1;
    msg.len = 8;

    if (enabled) {
        switch (nextCommand) {
            case SEND_ENABLE:
                // Send enable command
                msg.buf[0] = 0x23;
                msg.buf[1] = 0x0D;  // Enable
                msg.buf[2] = 0x20;
                msg.buf[3] = 0x01;
                msg.buf[4] = 0x00;
                msg.buf[5] = 0x00;
                msg.buf[6] = 0x00;
                msg.buf[7] = 0x00;
                writeCANMessage(steerBusNum, msg);
                nextCommand = SEND_SPEED;
                break;

            case SEND_SPEED:
                {
                    // Send speed command
                    int32_t speedValue = (int32_t)(commandedRPM * 10.0f);  // -1000 to +1000

                    msg.buf[0] = 0x23;
                    msg.buf[1] = 0x00;  // Speed command
                    msg.buf[2] = 0x20;
                    msg.buf[3] = 0x01;
                    msg.buf[4] = (speedValue >> 8) & 0xFF;   // DATA_L(H)
                    msg.buf[5] = speedValue & 0xFF;          // DATA_L(L)
                    msg.buf[6] = (speedValue >> 24) & 0xFF;  // DATA_H(H)
                    msg.buf[7] = (speedValue >> 16) & 0xFF;  // DATA_H(L)
                    writeCANMessage(steerBusNum, msg);
                    nextCommand = SEND_ENABLE;
                }
                break;
        }
    } else {
        // Send alternating disable and zero speed commands to keep CAN alive
        static bool sendDisable = true;

        if (sendDisable) {
            // Send disable command
            msg.buf[0] = 0x23;
            msg.buf[1] = 0x0C;  // Disable
            msg.buf[2] = 0x20;
            msg.buf[3] = 0x01;
            msg.buf[4] = 0x00;
            msg.buf[5] = 0x00;
            msg.buf[6] = 0x00;
            msg.buf[7] = 0x00;
            writeCANMessage(steerBusNum, msg);
        } else {
            // Send zero speed command
            msg.buf[0] = 0x23;
            msg.buf[1] = 0x00;  // Speed command
            msg.buf[2] = 0x20;
            msg.buf[3] = 0x01;
            msg.buf[4] = 0x00;  // Speed 0
            msg.buf[5] = 0x00;
            msg.buf[6] = 0x00;
            msg.buf[7] = 0x00;
            writeCANMessage(steerBusNum, msg);
        }

        // Toggle for next time
        sendDisable = !sendDisable;
    }
}

// ===== Fendt Implementation (placeholder) =====
void TractorCANDriver::processFendtMessage(const CAN_message_t& msg) {
    // Check for Fendt valve status message (0x0CEF2CF0)
    if (msg.id == 0x0CEF2CF0 && msg.flags.extended) {
        // Special valve ready detection for Fendt
        // If message length is 3 and byte 2 is 0, valve is NOT ready
        if (msg.len == 3 && msg.buf[2] == 0) {
            if (steerReady) {
                LOG_WARNING(EventSource::AUTOSTEER, "Fendt steering valve not ready");
            }
            steerReady = false;
        } else {
            if (!steerReady) {
                LOG_INFO(EventSource::AUTOSTEER, "Fendt steering valve ready");
            }
            steerReady = true;
            lastSteerReadyTime = millis();

            // Extract curve value if available (for feedback)
            if (msg.len >= 2) {
                // Fendt uses big-endian format
                int16_t estCurve = (msg.buf[0] << 8) | msg.buf[1];
                actualRPM = (float)estCurve / 100.0f;  // Store as scaled value
            }
        }
    }
}

void TractorCANDriver::sendFendtCommands() {
    if (!steerCAN || steerBusNum == 0) return;

    CAN_message_t msg;
    msg.id = 0x0CEFF02C;  // Fendt steering command ID
    msg.flags.extended = 1;
    msg.len = 6;  // Fendt uses 6-byte messages

    // Fixed bytes
    msg.buf[0] = 0x05;
    msg.buf[1] = 0x09;
    msg.buf[3] = 0x0A;

    if (enabled && steerReady) {
        // Active steering
        msg.buf[2] = 0x03;  // Steer active

        // Calculate Fendt curve value with offset
        int16_t fendtCurve = targetPWM - 32128;

        // Big-endian format (MSB first)
        msg.buf[4] = (fendtCurve >> 8) & 0xFF;
        msg.buf[5] = fendtCurve & 0xFF;
    } else {
        // Inactive steering
        msg.buf[2] = 0x02;  // Steer inactive
        msg.buf[4] = 0x00;
        msg.buf[5] = 0x00;
    }

    writeCANMessage(steerBusNum, msg);
}

void TractorCANDriver::processFendtKBusMessage(const CAN_message_t& msg) {
    // Check for Fendt armrest buttons (0x613 - Standard ID, not extended!)
    if (msg.id == 0x613 && !msg.flags.extended) {
        // Check for button state in byte 1
        bool buttonState = (msg.buf[1] & 0x80) != 0;

        // Check for auto steer active state (disables valve)
        if (msg.buf[1] == 0x8A && msg.buf[4] == 0x80) {
            // Auto steer is active on the tractor - set valve not ready
            if (steerReady) {
                LOG_INFO(EventSource::AUTOSTEER, "Fendt auto steer active - disabling valve");
            }
            steerReady = false;
        }

        // Track button state changes for autosteer control
        if (buttonState != fendtButtonPressed) {
            fendtButtonPressed = buttonState;
            LOG_INFO(EventSource::AUTOSTEER, "Fendt armrest button %s",
                     buttonState ? "pressed" : "released");
        }
    }
}

// ===== Case IH/New Holland Implementation =====
void TractorCANDriver::processCaseIHMessage(const CAN_message_t& msg) {
    // Check for valve status message (0x0CACAA08)
    if (msg.id == 0x0CACAA08 && msg.flags.extended) {
        // Extract steering curve (little-endian)
        int16_t estCurve = msg.buf[0] | (msg.buf[1] << 8);
        actualRPM = (float)estCurve / 100.0f;  // Store as scaled value

        // Check valve ready (byte 2)
        if (msg.buf[2] != 0) {
            if (!steerReady) {
                LOG_INFO(EventSource::AUTOSTEER, "Case IH steering valve ready");
            }
            steerReady = true;
            lastSteerReadyTime = millis();
        } else {
            if (steerReady) {
                LOG_WARNING(EventSource::AUTOSTEER, "Case IH steering valve not ready");
            }
            steerReady = false;
        }
    }
}

void TractorCANDriver::sendCaseIHCommands() {
    // Only send if we have a valid steering bus
    if (!steerCAN || steerBusNum == 0) return;

    CAN_message_t msg;
    msg.id = 0x0CAD08AA;  // Case IH steering command ID
    msg.flags.extended = 1;  // Extended ID (29-bit)
    msg.len = 8;

    // Convert PWM to Case IH curve value (similar to Valtra)
    int16_t setCurve = 0;
    if (enabled && steerReady) {
        // Scale PWM to curve value
        setCurve = (int16_t)(targetPWM * 128);  // Scale factor TBD
    }

    // Build message
    msg.buf[0] = setCurve & 0xFF;        // Curve low byte
    msg.buf[1] = (setCurve >> 8) & 0xFF; // Curve high byte
    msg.buf[2] = enabled ? 253 : 252;    // 253 = steer intent, 252 = no intent
    msg.buf[3] = 0xFF;  // Case IH uses 0xFF for bytes 3-7
    msg.buf[4] = 0xFF;
    msg.buf[5] = 0xFF;
    msg.buf[6] = 0xFF;
    msg.buf[7] = 0xFF;

    writeCANMessage(steerBusNum, msg);
}

void TractorCANDriver::processCaseIHKBusMessage(const CAN_message_t& msg) {
    // Check for engage message (0x14FF7706)
    if (msg.id == 0x14FF7706 && msg.flags.extended) {
        // Two possible engage conditions:
        // 1) Buf[0] == 130 && Buf[1] == 1
        // 2) Buf[0] == 178 && Buf[4] == 1
        bool newEngageState = ((msg.buf[0] == 130 && msg.buf[1] == 1) ||
                               (msg.buf[0] == 178 && msg.buf[4] == 1));

        if (newEngageState != caseIHEngaged) {
            caseIHEngaged = newEngageState;
            LOG_INFO(EventSource::AUTOSTEER, "Case IH engage %s",
                     caseIHEngaged ? "ON" : "OFF");
        }
    }

    // Check for rear hitch information (0x18FE4523)
    if (msg.id == 0x18FE4523 && msg.flags.extended) {
        // Byte 0 contains rear hitch pressure status
        // Log it for future use
        static uint8_t lastHitchStatus = 0xFF;
        if (msg.buf[0] != lastHitchStatus) {
            lastHitchStatus = msg.buf[0];
            LOG_DEBUG(EventSource::AUTOSTEER, "Case IH rear hitch status: 0x%02X", msg.buf[0]);
        }
    }
}

// ===== CAT MT Series Implementation =====
void TractorCANDriver::processCATMessage(const CAN_message_t& msg) {
    // Check for curve data message (0x0FFF9880)
    if (msg.id == 0x0FFF9880 && msg.flags.extended) {
        // Extract steering curve from bytes 4-5 (big-endian)
        int16_t estCurve = (msg.buf[4] << 8) | msg.buf[5];
        actualRPM = (float)estCurve / 100.0f;  // Store as scaled value

        // Check valve ready - curve value between 15000 and 17000
        if (estCurve >= 15000 && estCurve <= 17000) {
            if (!steerReady) {
                LOG_INFO(EventSource::AUTOSTEER, "CAT MT steering valve ready (curve=%d)", estCurve);
            }
            steerReady = true;
            lastSteerReadyTime = millis();
        } else {
            if (steerReady) {
                LOG_WARNING(EventSource::AUTOSTEER, "CAT MT steering valve not ready (curve=%d)", estCurve);
            }
            steerReady = false;
        }
    }
}

void TractorCANDriver::sendCATCommands() {
    // Only send if we have a valid steering bus
    if (!steerCAN || steerBusNum == 0) return;

    CAN_message_t msg;
    msg.id = 0x0EF87F80;  // CAT MT steering command ID
    msg.flags.extended = 1;  // Extended ID (29-bit)
    msg.len = 8;

    // Convert PWM to CAT curve value with special calculation
    int16_t setCurve = 0;
    if (enabled && steerReady) {
        // Scale PWM to curve value
        int16_t scaledPWM = (int16_t)(targetPWM * 128);  // Scale factor TBD

        // CAT MT special curve calculation: curve = setCurve - 2048
        // So to send the desired curve, we need: setCurve = curve + 2048
        setCurve = scaledPWM + 2048;

        // Handle negative values specially as per documentation
        if (scaledPWM < 0) {
            // For negative values, the calculation might be different
            // Based on the guide's note about "special handling for negatives"
            setCurve = scaledPWM + 2048;
        }
    }

    // Build message
    msg.buf[0] = 0x40;  // Fixed values for bytes 0-1
    msg.buf[1] = 0x01;
    msg.buf[2] = (setCurve >> 8) & 0xFF;  // Curve high byte (big-endian)
    msg.buf[3] = setCurve & 0xFF;         // Curve low byte
    msg.buf[4] = 0xFF;  // Fixed 0xFF for bytes 4-7
    msg.buf[5] = 0xFF;
    msg.buf[6] = 0xFF;
    msg.buf[7] = 0xFF;

    writeCANMessage(steerBusNum, msg);
}

void TractorCANDriver::processCATKBusMessage(const CAN_message_t& msg) {
    // Check for engage message (0x18F00400)
    if (msg.id == 0x18F00400 && msg.flags.extended) {
        // Engage if (Buf[0] & 0x0F) == 4
        bool newEngageState = ((msg.buf[0] & 0x0F) == 4);

        if (newEngageState != catMTEngaged) {
            catMTEngaged = newEngageState;
            LOG_INFO(EventSource::AUTOSTEER, "CAT MT engage %s",
                     catMTEngaged ? "ON" : "OFF");
        }
    }
}

// ===== CLAAS Implementation =====
void TractorCANDriver::processClaasMessage(const CAN_message_t& msg) {
    // Check for valve status message (0x0CAC1E13)
    if (msg.id == 0x0CAC1E13 && msg.flags.extended) {
        // Extract steering curve (little-endian)
        int16_t estCurve = msg.buf[0] | (msg.buf[1] << 8);
        actualRPM = (float)estCurve / 100.0f;  // Store as scaled value

        // Check valve ready (byte 2)
        if (msg.buf[2] != 0) {
            if (!steerReady) {
                LOG_INFO(EventSource::AUTOSTEER, "Claas steering valve ready");
            }
            steerReady = true;
            lastSteerReadyTime = millis();
        } else {
            if (steerReady) {
                LOG_WARNING(EventSource::AUTOSTEER, "Claas steering valve not ready");
            }
            steerReady = false;
        }
    }
}

void TractorCANDriver::sendClaasCommands() {
    // Only send if we have a valid steering bus
    if (!steerCAN || steerBusNum == 0) return;

    CAN_message_t msg;
    msg.id = 0x0CAD131E;  // Claas steering command ID
    msg.flags.extended = 1;  // Extended ID (29-bit)
    msg.len = 8;

    // Convert PWM to Claas curve value (similar to Valtra/Case IH)
    int16_t setCurve = 0;
    if (enabled && steerReady) {
        // Scale PWM to curve value
        setCurve = (int16_t)(targetPWM * 128);  // Scale factor TBD
    }

    // Build message
    msg.buf[0] = setCurve & 0xFF;        // Curve low byte
    msg.buf[1] = (setCurve >> 8) & 0xFF; // Curve high byte
    msg.buf[2] = enabled ? 253 : 252;    // 253 = steer intent, 252 = no intent
    msg.buf[3] = 0xFF;  // Claas uses 0xFF for bytes 3-7
    msg.buf[4] = 0xFF;
    msg.buf[5] = 0xFF;
    msg.buf[6] = 0xFF;
    msg.buf[7] = 0xFF;

    writeCANMessage(steerBusNum, msg);
}

void TractorCANDriver::processClaasKBusMessage(const CAN_message_t& msg) {
    // Check for engage message (0x18EF1CD2)
    if (msg.id == 0x18EF1CD2 && msg.flags.extended) {
        // Engage conditions: Buf[1] == 0x81 OR Buf[1] == 0xF1
        bool newEngageState = (msg.buf[1] == 0x81 || msg.buf[1] == 0xF1);

        if (newEngageState != claasEngaged) {
            claasEngaged = newEngageState;
            LOG_INFO(EventSource::AUTOSTEER, "Claas engage %s",
                     claasEngaged ? "ON" : "OFF");
        }
    }
}

// ===== JCB Implementation =====
void TractorCANDriver::processJcbMessage(const CAN_message_t& msg) {
    // Check for valve status message (0x0CACAB13)
    // Module ID: 0xAB
    if (msg.id == 0x0CACAB13 && msg.flags.extended) {
        // Extract steering curve (little-endian)
        int16_t estCurve = msg.buf[0] | (msg.buf[1] << 8);
        actualRPM = (float)estCurve / 100.0f;  // Store as scaled value

        // Check valve ready (byte 2)
        if (msg.buf[2] != 0) {
            if (!steerReady) {
                LOG_INFO(EventSource::AUTOSTEER, "JCB steering valve ready");
            }
            steerReady = true;
            lastSteerReadyTime = millis();
        } else {
            if (steerReady) {
                LOG_WARNING(EventSource::AUTOSTEER, "JCB steering valve not ready");
            }
            steerReady = false;
        }
    }
}

void TractorCANDriver::sendJcbCommands() {
    // Only send if we have a valid steering bus
    if (!steerCAN || steerBusNum == 0) return;

    CAN_message_t msg;
    msg.id = 0x0CAD13AB;  // JCB steering command ID (module 0xAB)
    msg.flags.extended = 1;  // Extended ID (29-bit)
    msg.len = 8;

    // Convert PWM to JCB curve value (similar to Case IH/CLAAS)
    int16_t setCurve = 0;
    if (enabled && steerReady) {
        // Scale PWM to curve value
        setCurve = (int16_t)(targetPWM * 128);  // Scale factor TBD
    }

    // Build message (little-endian, same as Case IH/CLAAS)
    msg.buf[0] = setCurve & 0xFF;        // Curve low byte
    msg.buf[1] = (setCurve >> 8) & 0xFF; // Curve high byte
    msg.buf[2] = enabled ? 253 : 252;    // 253 = steer intent, 252 = no intent
    msg.buf[3] = 0xFF;  // JCB uses 0xFF for bytes 3-7
    msg.buf[4] = 0xFF;
    msg.buf[5] = 0xFF;
    msg.buf[6] = 0xFF;
    msg.buf[7] = 0xFF;

    writeCANMessage(steerBusNum, msg);
}

void TractorCANDriver::processJcbKBusMessage(const CAN_message_t& msg) {
    // Check for engage message (0x18EFAB27 or 0x0CEFAB27)
    if ((msg.id == 0x18EFAB27 || msg.id == 0x0CEFAB27) && msg.flags.extended) {
        // Message received = engaged
        bool newEngageState = true;

        if (newEngageState != jcbEngaged) {
            jcbEngaged = newEngageState;
            LOG_INFO(EventSource::AUTOSTEER, "JCB engage %s",
                     jcbEngaged ? "ON" : "OFF");
        }
    }
}

// ===== Lindner Implementation =====
void TractorCANDriver::processLindnerMessage(const CAN_message_t& msg) {
    // Check for valve status message (0x0CACF013)
    // Module ID: 0xF0
    if (msg.id == 0x0CACF013 && msg.flags.extended) {
        // Extract steering curve (little-endian)
        int16_t estCurve = msg.buf[0] | (msg.buf[1] << 8);
        actualRPM = (float)estCurve / 100.0f;  // Store as scaled value

        // Check valve ready (byte 2)
        if (msg.buf[2] != 0) {
            if (!steerReady) {
                LOG_INFO(EventSource::AUTOSTEER, "Lindner steering valve ready");
            }
            steerReady = true;
            lastSteerReadyTime = millis();
        } else {
            if (steerReady) {
                LOG_WARNING(EventSource::AUTOSTEER, "Lindner steering valve not ready");
            }
            steerReady = false;
        }
    }
}

void TractorCANDriver::sendLindnerCommands() {
    // Only send if we have a valid steering bus
    if (!steerCAN || steerBusNum == 0) return;

    CAN_message_t msg;
    msg.id = 0x0CADF013;  // Lindner steering command ID (module 0xF0)
    msg.flags.extended = 1;  // Extended ID (29-bit)
    msg.len = 8;

    // Convert PWM to Lindner curve value (similar to Case IH/CLAAS/JCB)
    int16_t setCurve = 0;
    if (enabled && steerReady) {
        // Scale PWM to curve value
        setCurve = (int16_t)(targetPWM * 128);  // Scale factor TBD
    }

    // Build message (little-endian, same as Case IH/CLAAS/JCB)
    msg.buf[0] = setCurve & 0xFF;        // Curve low byte
    msg.buf[1] = (setCurve >> 8) & 0xFF; // Curve high byte
    msg.buf[2] = enabled ? 253 : 252;    // 253 = steer intent, 252 = no intent
    msg.buf[3] = 0xFF;  // Lindner uses 0xFF for bytes 3-7
    msg.buf[4] = 0xFF;
    msg.buf[5] = 0xFF;
    msg.buf[6] = 0xFF;
    msg.buf[7] = 0xFF;

    writeCANMessage(steerBusNum, msg);
}

void TractorCANDriver::processLindnerKBusMessage(const CAN_message_t& msg) {
    // Check for engage message (0x0CEFF021)
    if (msg.id == 0x0CEFF021 && msg.flags.extended) {
        // Message received = engaged
        bool newEngageState = true;

        if (newEngageState != lindnerEngaged) {
            lindnerEngaged = newEngageState;
            LOG_INFO(EventSource::AUTOSTEER, "Lindner engage %s",
                     lindnerEngaged ? "ON" : "OFF");
        }
    }
}

// ===== Valtra Implementation =====
void TractorCANDriver::processValtraMessage(const CAN_message_t& msg) {
    // Check for curve data and valve state message
    if (msg.id == 0x0CAC1C13 && msg.flags.extended) {
        // Extract steering curve (little-endian)
        int16_t estCurve = (msg.buf[1] << 8) | msg.buf[0];

        // Extract valve ready state from byte 2
        bool valveReady = (msg.buf[2] != 0);

        if (valveReady) {
            if (!steerReady) {
                LOG_INFO(EventSource::AUTOSTEER, "Valtra steering valve ready");
            }
            steerReady = true;
            lastSteerReadyTime = millis();
        }

        // Store actual position for feedback (convert to our scale)
        // Valtra curve range appears to be different from our PWM range
        actualRPM = (float)estCurve / 100.0f;  // Store as scaled value
    }

    // Check for engage messages
    if (msg.flags.extended && (msg.id == 0x18EF1C32 || msg.id == 0x18EF1CFC || msg.id == 0x18EF1C00)) {
        // These are engage/disengage messages from different Valtra/MF variants
        // Could be used to auto-enable/disable steering if needed
    }
}

void TractorCANDriver::sendValtraCommands() {
    // Only send if we have a valid steering bus
    if (!steerCAN || steerBusNum == 0) return;

    CAN_message_t msg;
    msg.id = 0x0CAD131C;  // Valtra steering command ID
    msg.flags.extended = 1;  // Extended ID (29-bit)
    msg.len = 8;

    // Convert PWM to Valtra curve value
    // PWM range: -255 to +255
    // Valtra curve: needs to be determined, using similar scale for now
    int16_t setCurve = 0;

    if (enabled && steerReady) {
        // Scale PWM to curve value (assuming similar range)
        setCurve = (int16_t)(targetPWM * 128);  // Scale factor TBD
    }

    // Build message
    msg.buf[0] = setCurve & 0xFF;        // Curve low byte
    msg.buf[1] = (setCurve >> 8) & 0xFF; // Curve high byte
    msg.buf[2] = enabled ? 253 : 252;    // 253 = steer intent, 252 = no intent
    msg.buf[3] = 0;
    msg.buf[4] = 0;
    msg.buf[5] = 0;
    msg.buf[6] = 0;
    msg.buf[7] = 0;

    writeCANMessage(steerBusNum, msg);
}

// ===== Massey K_Bus Implementation =====
void TractorCANDriver::processMasseyKBusMessage(const CAN_message_t& msg) {
    // Check for K_Bus button status message (0xCFF2621)
    if (msg.id == 0x0CFF2621 && msg.flags.extended) {
        // Store the entire message for rolling counter
        memcpy(mfRollingCounter, msg.buf, 8);

        // Check bit 2 of byte 3 for engage button state
        bool newEngageState = (msg.buf[3] & 0x04) != 0;

        if (newEngageState != engageButtonPressed) {
            engageButtonPressed = newEngageState;
            LOG_INFO(EventSource::AUTOSTEER, "Massey K_Bus engage button %s",
                     engageButtonPressed ? "pressed" : "released");
        }
    }
}

void TractorCANDriver::sendMasseyF1() {
    if (!buttonCAN || buttonBusNum == 0) return;

    CAN_message_t msg;
    msg.id = 0x0CFF2621;  // K_Bus button command
    msg.flags.extended = 1;
    msg.len = 8;

    // Increment rolling counter in byte 6
    mfRollingCounter[6] = (mfRollingCounter[6] + 1) & 0xFF;

    // Copy the last received message as base
    memcpy(msg.buf, mfRollingCounter, 8);

    // Set F1 bit (bit 4 of byte 3)
    msg.buf[3] |= 0x10;

    writeCANMessage(buttonBusNum, msg);

    LOG_INFO(EventSource::AUTOSTEER, "Massey F1 button pressed");
}

void TractorCANDriver::sendMasseyF2() {
    if (!buttonCAN || buttonBusNum == 0) return;

    CAN_message_t msg;
    msg.id = 0x0CFF2621;  // K_Bus button command
    msg.flags.extended = 1;
    msg.len = 8;

    // Increment rolling counter in byte 6
    mfRollingCounter[6] = (mfRollingCounter[6] + 1) & 0xFF;

    // Copy the last received message as base
    memcpy(msg.buf, mfRollingCounter, 8);

    // Set F2 bit (bit 5 of byte 3)
    msg.buf[3] |= 0x20;

    writeCANMessage(buttonBusNum, msg);

    LOG_INFO(EventSource::AUTOSTEER, "Massey F2 button pressed");
}

// ===== Common Methods =====
MotorStatus TractorCANDriver::getStatus() const {
    MotorStatus status;
    status.enabled = enabled;
    status.targetPWM = targetPWM;

    // For Keya, we have actual feedback
    if (hasKeyaFunction() && heartbeatValid) {
        status.actualPWM = (int16_t)(actualRPM * 255.0f / 100.0f);
    } else {
        status.actualPWM = targetPWM;
    }

    status.currentDraw = 0.0f;  // No current sensing via CAN

    // Only report error if we're enabled and trying to steer but no connection
    status.hasError = enabled && !steerReady;

    if (status.hasError) {
        snprintf(status.errorMessage, sizeof(status.errorMessage),
                 "No CAN connection");
    } else {
        status.errorMessage[0] = '\0';
    }

    return status;
}

const char* TractorCANDriver::getTypeName() const {
    // Check if Keya function is active
    if (hasKeyaFunction()) {
        return "Keya CAN";
    }

    switch (static_cast<TractorBrand>(config.brand)) {
        case TractorBrand::FENDT:         return "Fendt SCR/S4/Gen6";
        case TractorBrand::FENDT_ONE:     return "Fendt One";
        case TractorBrand::VALTRA_MASSEY: return "Valtra/Massey";
        case TractorBrand::CASEIH_NH:     return "Case IH/NH";
        case TractorBrand::CLAAS:         return "Claas";
        case TractorBrand::JCB:           return "JCB";
        case TractorBrand::LINDNER:       return "Lindner";
        case TractorBrand::CAT_MT:        return "CAT MT";
        case TractorBrand::GENERIC:       return "Generic CAN";
        default:                          return "Tractor CAN";
    }
}

void TractorCANDriver::handleKickout(KickoutType type, float value) {
    // For CAN-based systems, kickout is usually handled by the tractor
    // This is here for compatibility with the interface
}

void TractorCANDriver::setConfig(const CANSteerConfig& newConfig) {
    config = newConfig;
    // Reassign buses when config changes
    assignCANBuses();

    // Reset state
    steerReady = false;
    heartbeatValid = false;

    LOG_INFO(EventSource::AUTOSTEER, "TractorCAN config updated - Brand: %d, SteerBus: %d",
             config.brand, steerBusNum);
}

bool TractorCANDriver::hasKeyaFunction() const {
    return ((config.can1Function & static_cast<uint8_t>(CANFunction::KEYA)) ||
            (config.can2Function & static_cast<uint8_t>(CANFunction::KEYA)) ||
            (config.can3Function & static_cast<uint8_t>(CANFunction::KEYA)));
}