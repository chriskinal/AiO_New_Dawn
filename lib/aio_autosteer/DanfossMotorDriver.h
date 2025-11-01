// DanfossMotorDriver.h - Danfoss valve motor driver
#ifndef DANFOSS_MOTOR_DRIVER_H
#define DANFOSS_MOTOR_DRIVER_H

#include "MotorDriverInterface.h"
#include "EventLogger.h"
#include "HardwareManager.h"
#include "MachineProcessor.h"

class DanfossMotorDriver : public MotorDriverInterface {
private:
    // Status tracking
    MotorStatus status;

    // PCA9685 output channel numbers (NOT Teensy GPIO pins)
    // These are MachineProcessor output numbers that map to PCA9685 channels
    static constexpr uint8_t ENABLE_OUTPUT = 5;   // Machine output 5 (PCA9685 pin 10)
    static constexpr uint8_t CONTROL_OUTPUT = 6;  // Machine output 6 (PCA9685 pin 9)
    
    // PWM mapping constants
    static constexpr uint8_t PWM_LEFT = 64;       // 25% duty cycle
    static constexpr uint8_t PWM_CENTER = 128;    // 50% duty cycle  
    static constexpr uint8_t PWM_RIGHT = 192;     // 75% duty cycle
    static constexpr uint8_t PWM_RANGE = 64;      // ±64 from center
    
    // Hardware manager for output control
    HardwareManager* hwMgr;
    
public:
    DanfossMotorDriver(HardwareManager* hw) : hwMgr(hw) {
        // Initialize status
        status = {
            false,    // enabled
            0,        // targetPWM
            0,        // actualPWM  
            0.0f,     // currentDraw
            0,        // errorCount
            0,        // lastUpdateMs
            false,    // hasError
            {0}       // errorMessage
        };
    }
    
    bool init() override {
        LOG_INFO(EventSource::AUTOSTEER, "Initializing Danfoss valve driver...");
        
        // Note: MachineProcessor must be initialized before we can control outputs
        // We'll set initial states when enable() is first called
        
        LOG_INFO(EventSource::AUTOSTEER, "Danfoss driver initialized - Enable on Output %d, Control on Output %d", 
                 ENABLE_OUTPUT, CONTROL_OUTPUT);
        return true;
    }
    
    void enable(bool en) override {
        // Check if MachineProcessor is initialized first
        if (!MachineProcessor::getInstance()) {
            LOG_ERROR(EventSource::AUTOSTEER, "Cannot enable Danfoss - MachineProcessor not initialized");
            return;
        }
        
        // On first call, ensure we're in a known state
        static bool firstCall = true;
        if (firstCall) {
            firstCall = false;
            // Ensure valve starts centered and disabled
            setOutputPWM(CONTROL_OUTPUT, PWM_CENTER);
            setOutput(ENABLE_OUTPUT, false);
        }
        
        // Only log on state change
        bool wasEnabled = status.enabled;
        status.enabled = en;
        
        // Set enable output
        setOutput(ENABLE_OUTPUT, en);
        
        if (!en) {
            // Return to center when disabled
            setOutputPWM(CONTROL_OUTPUT, PWM_CENTER);
            status.targetPWM = 0;
            status.actualPWM = 0;
        }
        
        // Only log if state actually changed
        if (wasEnabled != en) {
            LOG_INFO(EventSource::AUTOSTEER, "Danfoss valve %s", en ? "ENABLED" : "DISABLED");
        }
    }
    
    void setPWM(int16_t pwm) override {
        if (!status.enabled) {
            return;  // Ignore PWM commands when disabled
        }
        
        // Constrain input
        pwm = constrain(pwm, -255, 255);
        status.targetPWM = pwm;
        
        // Map -255 to +255 PWM to 25%-75% PWM duty cycle
        // -255 = PWM 64 (25%)
        //    0 = PWM 128 (50%)
        // +255 = PWM 192 (75%)
        float scaledPWM = (float)pwm / 255.0f;  // -1.0 to +1.0
        uint8_t pwmValue = PWM_CENTER + (int16_t)(scaledPWM * PWM_RANGE);
        
        // Apply to output
        setOutputPWM(CONTROL_OUTPUT, pwmValue);
        
        // For Danfoss, actual PWM follows target immediately
        status.actualPWM = pwm;
        status.lastUpdateMs = millis();
        
        // Debug logging
        static uint32_t lastDebug = 0;
        if (millis() - lastDebug > 1000) {
            lastDebug = millis();
            LOG_DEBUG(EventSource::AUTOSTEER, "Danfoss PWM: %d -> output %d (%.1f%% duty)", 
                     pwm, pwmValue, (pwmValue / 255.0f) * 100.0f);
        }
    }
    
    void stop() override {
        setOutputPWM(CONTROL_OUTPUT, PWM_CENTER);
        status.targetPWM = 0;
        status.actualPWM = 0;
        status.lastUpdateMs = millis();
    }
    
    MotorStatus getStatus() const override { 
        return status; 
    }
    
    MotorDriverType getType() const override { 
        return MotorDriverType::DANFOSS; 
    }
    
    const char* getTypeName() const override { 
        return "Danfoss Valve"; 
    }
    
    bool hasCurrentSensing() const override { 
        return false;  // Danfoss doesn't provide current feedback
    }
    
    bool hasPositionFeedback() const override { 
        return false;  // No position feedback
    }
    
    void resetErrors() override {
        status.errorCount = 0;
        status.hasError = false;
        status.errorMessage[0] = '\0';
    }
    
    // Interface requirements
    bool isDetected() override { 
        return true;  // Danfoss is configured, not detected
    }
    
    void handleKickout(KickoutType type, float value) override {
        // Handle kickout based on configured type
        switch (type) {
            case KickoutType::WHEEL_ENCODER:
                LOG_WARNING(EventSource::AUTOSTEER, "Danfoss kickout: Wheel encoder count %d", (int)value);
                break;
            case KickoutType::PRESSURE_SENSOR:
                LOG_WARNING(EventSource::AUTOSTEER, "Danfoss kickout: Pressure %.1f", value);
                break;
            default:
                break;
        }
        
        // Disable the valve
        enable(false);
        stop();
    }
    
    float getCurrentDraw() override { 
        return 0.0f;  // No current sensing
    }
    
private:
    // Helper methods to interface with machine outputs
    void setOutput(uint8_t output, bool state) {
        // Machine outputs 5 & 6 map to specific PCA9685 pins
        // Based on SECTION_PINS mapping: outputs 1-6 use pins {0, 1, 4, 5, 10, 9}
        // So Output 5 = PCA9685 pin 10, Output 6 = PCA9685 pin 9
        
        
        if (output == 5) {
            // Output 5 uses PCA9685 pin 10
            if (state) {
                MachineProcessor::getInstance()->setPinHigh(10);
            } else {
                MachineProcessor::getInstance()->setPinLow(10);
            }
            LOG_DEBUG(EventSource::AUTOSTEER, "Set Output %d (PCA pin 10) = %s", output, state ? "HIGH" : "LOW");
        }
    }
    
    void setOutputPWM(uint8_t output, uint8_t pwmValue) {
        // Output 6 uses PCA9685 pin 9 for PWM control
        static uint8_t lastPwmValue = 255;  // Invalid initial value to force first update
        
        if (output == 6) {
            // Only update if value has changed to avoid I2C traffic
            if (pwmValue != lastPwmValue) {
                // Convert 0-255 PWM to PCA9685 12-bit value (0-4095)
                uint16_t pcaValue = (uint16_t)((pwmValue * 4095UL) / 255);
                
                // Use the new setPinPWM method
                MachineProcessor::getInstance()->setPinPWM(9, pcaValue);
                
                LOG_DEBUG(EventSource::AUTOSTEER, "Set Output %d (PCA pin 9) PWM = %d (PCA value %d)", 
                         output, pwmValue, pcaValue);
                
                lastPwmValue = pwmValue;
            }
        }
    }
};

#endif // DANFOSS_MOTOR_DRIVER_H