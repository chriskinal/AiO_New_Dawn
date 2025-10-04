// KeyaCANDriver.h - Minimal Keya CAN motor driver (standalone)
#ifndef KEYA_CAN_DRIVER_H
#define KEYA_CAN_DRIVER_H

#include "MotorDriverInterface.h"
#include "CANGlobals.h"
#include "EventLogger.h"

class KeyaCANDriver : public MotorDriverInterface {
private:
    // Use pointer to global CAN3
    FlexCAN_T4<CAN3, RX_SIZE_256, TX_SIZE_256>* can3;
    
    bool enabled = false;
    int16_t targetPWM = 0;
    // Timing now handled by SimpleScheduler at 50Hz
    
    // Heartbeat-based feedback (from 0x07000001)
    float actualRPM = 0.0f;
    float commandedRPM = 0.0f;
    uint16_t motorPosition = 0;
    float motorCurrentX32 = 0.0f;
    uint16_t motorErrorCode = 0;
    uint32_t lastHeartbeat = 0;
    bool heartbeatValid = false;
    
    // Command alternation state
    enum CommandState {
        SEND_ENABLE,
        SEND_SPEED
    };
    CommandState nextCommand = SEND_ENABLE;
    
    // Slip detection parameters (counter-based like the working example)
    static constexpr uint8_t SLIP_COUNT_THRESHOLD = 8;  // 8 consecutive errors before kickout
    static constexpr float SLIP_RPM_TOLERANCE = 10.0f;  // RPM error tolerance
    
public:
    KeyaCANDriver() : can3(&globalCAN3) {}
    
    bool init() override {
        // CAN3 already initialized by global init
        LOG_INFO(EventSource::AUTOSTEER, "KeyaCANDriver initialized");
        return true;
    }
    
    void enable(bool en) override {
        if (!enabled && en) {
            // Motor is being enabled
            LOG_INFO(EventSource::AUTOSTEER, "Keya motor enabled");
        }
        enabled = en;
    }
    
    void setPWM(int16_t pwm) override {
        pwm = constrain(pwm, -255, 255);
        
        // Log when PWM changes significantly
        if (abs(pwm - targetPWM) > 5) {
            LOG_DEBUG(EventSource::AUTOSTEER, "Keya setPWM: %d -> %d (RPM: %.1f -> %.1f)", 
                      targetPWM, pwm, commandedRPM, (float)pwm * 100.0f / 255.0f);
        }
        
        targetPWM = pwm;
        // Map PWM to RPM: 255 PWM = 100 RPM
        // The CAN command multiplies by 10, so -1000 to +1000 = -100 RPM to +100 RPM
        commandedRPM = (float)pwm * 100.0f / 255.0f;  // 255 PWM = 100 RPM
    }
    
    void stop() override {
        targetPWM = 0;
        commandedRPM = 0.0f;  // Make sure commanded speed is also zeroed
    }
    
    void process() override {
        // Now called by SimpleScheduler at 50Hz (20ms)

        // Check for incoming CAN messages
        checkCANMessages();

        // Send commands (scheduler ensures 20ms spacing)
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
                    can3->write(msg);
                    nextCommand = SEND_SPEED;
                    break;
                    
                case SEND_SPEED:
                    {
                        // Send speed command
                        int32_t speedValue = (int32_t)(commandedRPM * 10.0f);  // -1000 to +1000 as 32-bit
                        
                        // Log speed commands periodically
                        static uint32_t lastSpeedLog = 0;
                        if (millis() - lastSpeedLog > 1000 || speedValue == 0 || abs(speedValue) < 50) {
                            LOG_DEBUG(EventSource::AUTOSTEER, "Keya speed cmd: %.1f RPM (raw=%d, 0x%02X %02X %02X %02X) actual=%.1f", 
                                      commandedRPM, speedValue,
                                      (speedValue >> 24) & 0xFF, (speedValue >> 16) & 0xFF,
                                      (speedValue >> 8) & 0xFF, speedValue & 0xFF,
                                      actualRPM);
                            lastSpeedLog = millis();
                        }
                        
                        msg.buf[0] = 0x23;
                        msg.buf[1] = 0x00;  // Speed command
                        msg.buf[2] = 0x20;
                        msg.buf[3] = 0x01;
                        msg.buf[4] = (speedValue >> 8) & 0xFF;   // DATA_L(H) - bits 15-8
                        msg.buf[5] = speedValue & 0xFF;          // DATA_L(L) - bits 7-0
                        msg.buf[6] = (speedValue >> 24) & 0xFF;  // DATA_H(H) - bits 31-24 (sign extension)
                        msg.buf[7] = (speedValue >> 16) & 0xFF;  // DATA_H(L) - bits 23-16 (sign extension)
                        can3->write(msg);
                        nextCommand = SEND_ENABLE;
                    }
                    break;
                }
            } else {
                // Send alternating disable and zero speed commands
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
                    can3->write(msg);
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
                    can3->write(msg);
                }
                
                // Toggle for next time
                sendDisable = !sendDisable;
            }
            
    }
    
    MotorStatus getStatus() const override {
        MotorStatus status;
        status.enabled = enabled;
        status.targetPWM = targetPWM;
        status.actualPWM = heartbeatValid ? (int16_t)(actualRPM * 255.0f / 100.0f) : targetPWM;
        status.currentDraw = motorCurrentX32;
        
        // Simple: no heartbeat = error
        status.hasError = !heartbeatValid;
        
        return status;
    }
    
    // RPM feedback methods
    float getActualRPM() const { return actualRPM; }
    float getCommandedRPM() const { return commandedRPM; }
    bool hasRPMFeedback() const { return heartbeatValid; }
    uint16_t getMotorErrorCode() const { return motorErrorCode; }
    
    // Position feedback methods for sensor fusion
    uint16_t getMotorPosition() const { return motorPosition; }
    int32_t getPositionDelta() {
        // Calculate delta since last call
        static uint16_t lastPosition = 0;
        static bool firstCall = true;
        
        if (!heartbeatValid) {
            return 0;  // No valid data yet
        }
        
        if (firstCall) {
            firstCall = false;
            lastPosition = motorPosition;
            return 0;
        }
        
        // Handle 16-bit rollover
        int32_t delta = (int16_t)(motorPosition - lastPosition);
        lastPosition = motorPosition;
        return delta;
    }
    
    // Check for motor slip (counter-based like working example)
    bool checkMotorSlip() {
        // Static counter persists between calls
        static uint8_t slipCounter = 0;
        static float lastCommandedRPM = 0;
        static uint32_t lastSpeedChangeTime = 0;
        
        // Check if speed command changed significantly
        if (abs(commandedRPM - lastCommandedRPM) > 5.0f) {
            lastSpeedChangeTime = millis();
            lastCommandedRPM = commandedRPM;
            slipCounter = 0;  // Reset counter on speed change
        }
        
        // Give motor 50ms to respond to speed changes
        if (millis() - lastSpeedChangeTime < 50) {
            return false;  // Grace period for motor to catch up
        }
        
        if (!heartbeatValid || !enabled) {
            slipCounter = 0;
            return false;
        }
        
        // Check if motor has error flags (0x0140 = normal/enabled based on example: 40 01)
        // Bit 0 of byte 7 indicates enabled/disabled state
        if (motorErrorCode != 0 && motorErrorCode != 0x4001) {
            // Check specific error bits from the working example
            uint8_t errorLow = motorErrorCode & 0xFF;  // byte 7
            uint8_t errorHigh = (motorErrorCode >> 8) & 0xFF;  // byte 6
            
            // Only trigger kickout on actual errors, not just disabled state
            if (errorLow > 1 || errorHigh > 0) {
                LOG_WARNING(EventSource::AUTOSTEER, "Keya motor error code: 0x%04X", motorErrorCode);
                return true;  // Immediate kickout on error
            }
        }
        
        // Calculate error as absolute difference between actual and commanded
        float error = abs(actualRPM - commandedRPM);
        
        
        // Check if error exceeds commanded speed + tolerance
        // This matches the working example: if (error > abs(keyaCurrentSetSpeed) + 10)
        if (error > abs(commandedRPM) + SLIP_RPM_TOLERANCE) {
            slipCounter++;
            if (slipCounter >= SLIP_COUNT_THRESHOLD) {
                LOG_WARNING(EventSource::AUTOSTEER, 
                           "Keya motor slip detected! Counter=%d Cmd=%.1f Act=%.1f Error=%.1f",
                           slipCounter, commandedRPM, actualRPM, error);
                return true;
            }
        } else {
            // Reset counter if speed is within tolerance
            if (slipCounter > 0) {
                LOG_DEBUG(EventSource::AUTOSTEER, "Keya slip counter reset (was %d)", slipCounter);
            }
            slipCounter = 0;
        }
        return false;
    }
         
    // get Keya current reading
    int getKeyaCurrentX32() {
            return int(motorCurrentX32);
    }

private:
    void checkCANMessages() {
        CAN_message_t rxMsg;
        
        // Process only ONE message per call to prevent blocking
        if (can3->read(rxMsg)) {
            // Check for heartbeat message from Keya (ID: 0x07000001)
            if (rxMsg.id == 0x07000001 && rxMsg.flags.extended) {
                // Heartbeat format (from manual - big-endian/MSB first):
                // Bytes 0-1: Position/Angle (uint16) - high byte first
                // Bytes 2-3: Speed/RPM (int16) - high byte first, with sign
                // Bytes 4-5: Current (int16) - high byte first, with sign
                // Bytes 6-7: Error code (uint16) - high byte first
                
                // Extract position (high byte first)
                motorPosition = (uint16_t)((rxMsg.buf[0] << 8) | rxMsg.buf[1]);
                
                // Extract speed (high byte first, signed)
                int16_t speedRaw = (int16_t)((rxMsg.buf[2] << 8) | rxMsg.buf[3]);
                actualRPM = (float)speedRaw;
                
                // Extract current (high byte first, signed)
                int16_t currentRaw = abs((int16_t)((rxMsg.buf[4] << 8) | rxMsg.buf[5]));
                float newValue = float(abs(currentRaw)<<5); // multiply by 32 to get x32 value
                // Simple moving average filter for current
                motorCurrentX32 = motorCurrentX32 * 0.9 + newValue * 0.1;
                
                // Extract error code (high byte first)
                motorErrorCode = (uint16_t)((rxMsg.buf[6] << 8) | rxMsg.buf[7]);
                
                // Log when connection is restored
                if (!heartbeatValid) {
                    LOG_INFO(EventSource::AUTOSTEER, "Keya CAN connection restored");
                }
                
                heartbeatValid = true;
                lastHeartbeat = millis();
                
            }
        }
        
        // Invalidate heartbeat if not received for 500ms
        if (heartbeatValid && millis() - lastHeartbeat > 500) {
            heartbeatValid = false;
            LOG_ERROR(EventSource::AUTOSTEER, "Keya CAN connection lost - no heartbeat for 500ms");
        }
    }
    
    MotorDriverType getType() const override { return MotorDriverType::KEYA_CAN; }
    const char* getTypeName() const override { return "Keya CAN"; }
    bool hasCurrentSensing() const override { return false; }
    bool hasPositionFeedback() const override { return false; }
    
    // New interface methods
    bool isDetected() override { return heartbeatValid; }
    void handleKickout(KickoutType type, float value) override {
        // Keya uses motor slip detection, not external kickout
        // This is handled internally by checkMotorSlip()
    }
    float getCurrentDraw() override { return motorCurrentX32; }
};

#endif // KEYA_CAN_DRIVER_H