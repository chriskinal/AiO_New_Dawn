// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

// PWMMotorDriver.cpp - DRV8701 motor driver implementation with complementary PWM
#include "PWMMotorDriver.h"
#include <Arduino.h>
#include "EventLogger.h"
#include "HardwareManager.h"
#include "ConfigManager.h"

// External objects
extern ConfigManager configManager;

PWMMotorDriver::PWMMotorDriver(MotorDriverType type, uint8_t pwm1, uint8_t pwm2, 
                               uint8_t enable, uint8_t current) 
    : driverType(type), pwm1Pin(pwm1), pwm2Pin(pwm2), enablePin(enable), 
      currentPin(current), hasCurrentSense(false), currentScale(0.5f), currentOffset(0.0f) {
    
    // Initialize status
    status = {
        false,    // enabled
        0,        // targetPWM
        0,        // actualPWM  
        0.0f,     // currentDraw
        0,        // errorCount
        0,        // lastUpdateMs
        false,    // hasError
        {0}       // errorMessage (empty string)
    };
    
    // Check if current sensing is available
    if (currentPin != 255) {
        hasCurrentSense = true;
    }
}

bool PWMMotorDriver::init() {
    LOG_INFO(EventSource::AUTOSTEER, "Initializing DRV8701 motor driver...");
    
    // Configure pins
    pinMode(pwm1Pin, OUTPUT);    // PWM1 (pin 5) for LEFT
    pinMode(pwm2Pin, OUTPUT);    // PWM2 (pin 6) for RIGHT
    
    if (enablePin != 255) {
        pinMode(enablePin, OUTPUT);
        // DRV8701 nSLEEP: LOW = sleep, HIGH = awake
        // Start in sleep mode (LOW) - will be enabled when needed
        digitalWrite(enablePin, LOW);
        LOG_DEBUG(EventSource::AUTOSTEER, "DRV8701 nSLEEP pin %d initialized to LOW (sleep mode)", enablePin);
    }
    
    if (hasCurrentSense) {
        pinMode(currentPin, INPUT_DISABLE);  // Analog input - no pull-up
        LOG_DEBUG(EventSource::AUTOSTEER, "Current sensing enabled on pin A%d", currentPin - A0);
    }
    
    // Set initial state - both PWM outputs to LOW
    analogWrite(pwm1Pin, 0);
    analogWrite(pwm2Pin, 0);
    
    // Configure PWM for DRV8701 through HardwareManager
    HardwareManager* hwMgr = HardwareManager::getInstance();
    
    // Request 12-bit resolution to match PWMProcessor
    // We'll scale our 8-bit values to 12-bit
    if (!hwMgr->requestPWMResolution(12, "PWMMotorDriver")) {
        LOG_WARNING(EventSource::AUTOSTEER, "Failed to set PWM resolution to 12-bit");
    }
    
    // Request PWM frequency for motor pins
    if (!hwMgr->requestPWMFrequency(pwm1Pin, PWM_FREQUENCY, "PWMMotorDriver")) {
        LOG_WARNING(EventSource::AUTOSTEER, "Failed to set PWM frequency for pin %d", pwm1Pin);
    }
    if (!hwMgr->requestPWMFrequency(pwm2Pin, PWM_FREQUENCY, "PWMMotorDriver")) {
        LOG_WARNING(EventSource::AUTOSTEER, "Failed to set PWM frequency for pin %d", pwm2Pin);
    }
    
    LOG_INFO(EventSource::AUTOSTEER, "DRV8701 initialized with complementary PWM on pins %d (LEFT) and %d (RIGHT)", pwm1Pin, pwm2Pin);
    
    return true;
}

void PWMMotorDriver::enable(bool en) {
    status.enabled = en;
    
    if (enablePin != 255) {
        // Pin 4 serves dual purpose: motor nSLEEP and LOCK output
        // For DRV8701: HIGH = awake/enabled, LOW = sleep/disabled
        // This also controls the LOCK output through the same MOSFET
        digitalWrite(enablePin, en ? HIGH : LOW);
        
        static bool lastState = false;
        if (en != lastState) {
            LOG_INFO(EventSource::AUTOSTEER, "Motor driver %s (nSLEEP/LOCK pin %d = %s)", 
                     en ? "ENABLED" : "DISABLED", enablePin, en ? "HIGH" : "LOW");
            lastState = en;
        }
    }
    
    if (!en) {
        // Stop motor when disabling - set both outputs to LOW
        analogWrite(pwm1Pin, 0);
        analogWrite(pwm2Pin, 0);
        
        status.targetPWM = 0;
        status.actualPWM = 0;
    }
    
}

void PWMMotorDriver::setPWM(int16_t pwm) {
    if (!status.enabled) {
        return;  // Ignore PWM commands when disabled
    }
    
    // Constrain to valid range
    pwm = constrain(pwm, -255, 255);
    status.targetPWM = pwm;
    
    // DRV8701 complementary PWM mode: PWM1 = LEFT, PWM2 = RIGHT
    // Scale 8-bit input (0-255) to 12-bit output (0-4095)
    // Note: Hi-Z mode in 12-bit is 4096
    
    uint16_t pwmValue = abs(pwm);
    // Scale from 8-bit to 12-bit
    pwmValue = (pwmValue * 4095) / 255;
    // Special case: max value should map to Hi-Z (4096)
    if (abs(pwm) == 255) pwmValue = 4096;
    
    // Check if brake mode is enabled
    bool brakeMode = configManager.getPWMBrakeMode();
    
    if (pwm < 0) {
        // LEFT direction
        if (brakeMode) {
            // Brake mode: PWM2 at (4096-pwmValue), PWM1 at 4096 (Hi-Z)
            analogWrite(pwm2Pin, 4096 - pwmValue);  
            analogWrite(pwm1Pin, 4096);
        } else {
            // Coast mode: PWM1 active, PWM2 low
            analogWrite(pwm1Pin, pwmValue);     
            analogWrite(pwm2Pin, 0);            
        }
    } else if (pwm > 0) {
        // RIGHT direction
        if (brakeMode) {
            // Brake mode: PWM1 at (4096-pwmValue), PWM2 at 4096 (Hi-Z)
            analogWrite(pwm1Pin, 4096 - pwmValue);  
            analogWrite(pwm2Pin, 4096);
        } else {
            // Coast mode: PWM2 active, PWM1 low
            analogWrite(pwm1Pin, 0);            
            analogWrite(pwm2Pin, pwmValue);     
        }
    } else {
        // Stop: both outputs LOW (same for both modes)
        analogWrite(pwm1Pin, 0);
        analogWrite(pwm2Pin, 0);
    }
    
    // Debug output
    static uint32_t lastDebug = 0;
    if (millis() - lastDebug > 1000) {
        lastDebug = millis();
        if (hasCurrentSense) {
            LOG_DEBUG(EventSource::AUTOSTEER, "PWM %s mode: %d -> PWM1=%d, PWM2=%d, Current: %.2fA", 
                     brakeMode ? "BRAKE" : "COAST",
                     pwm, 
                     analogRead(pwm1Pin),
                     analogRead(pwm2Pin),
                     getCurrent());
        } else {
            LOG_DEBUG(EventSource::AUTOSTEER, "PWM: %d -> PWM1=%d, PWM2=%d", 
                     pwm, 
                     pwm < 0 ? pwmValue : 0,
                     pwm > 0 ? pwmValue : 0);
        }
    }
    
    // For PWM motors, actual PWM follows target immediately
    status.actualPWM = pwm;
    status.lastUpdateMs = millis();
}

void PWMMotorDriver::stop() {
    // Set both outputs to LOW
    analogWrite(pwm1Pin, 0);
    analogWrite(pwm2Pin, 0);
    
    status.targetPWM = 0;
    status.actualPWM = 0;
    status.lastUpdateMs = millis();
}

float PWMMotorDriver::getCurrent() const {
    if (!hasCurrentSense) return 0.0f;
    
    // Read ADC value (Teensy 4.1 has 12-bit ADC)
    int adcValue = analogRead(currentPin);
    
    // Convert to voltage (3.3V reference)
    float voltage = (adcValue * 3.3f) / 4095.0f;
    
    // Convert to current using scale and offset
    // For DRV8701: Typical current sense is 0.5V/A
    // Default scale = 0.5V/A, offset = 0V (no current = 0V output)
    float current = (voltage - currentOffset) / currentScale;
    
    // Ensure non-negative (current sensor should only report positive values)
    return max(0.0f, current);
}

void PWMMotorDriver::resetErrors() {
    status.errorCount = 0;
    status.hasError = false;
    status.errorMessage[0] = '\0';  // Clear error message
}

const char* PWMMotorDriver::getTypeName() const {
    switch (driverType) {
        case MotorDriverType::DRV8701:
            return "DRV8701 PWM Driver";
        case MotorDriverType::GENERIC_PWM:
            return "Generic PWM Driver";
        default:
            return "Unknown PWM Driver";
    }
}

void PWMMotorDriver::setCurrentScaling(float scale, float offset) {
    currentScale = scale;
    currentOffset = offset;
    LOG_INFO(EventSource::AUTOSTEER, "Current scaling set: scale=%.3f, offset=%.3f", scale, offset);
}

void PWMMotorDriver::setPWMFrequency(uint32_t freq) {
    HardwareManager* hwMgr = HardwareManager::getInstance();
    
    if (!hwMgr->requestPWMFrequency(pwm1Pin, freq, "PWMMotorDriver")) {
        LOG_WARNING(EventSource::AUTOSTEER, "Failed to change PWM frequency for pin %d", pwm1Pin);
        return;
    }
    if (!hwMgr->requestPWMFrequency(pwm2Pin, freq, "PWMMotorDriver")) {
        LOG_WARNING(EventSource::AUTOSTEER, "Failed to change PWM frequency for pin %d", pwm2Pin);
        return;
    }
    
    LOG_INFO(EventSource::AUTOSTEER, "PWM frequency set to %lu Hz", freq);
}

void PWMMotorDriver::handleKickout(KickoutType type, float value) {
    // Handle kickout based on type
    switch (type) {
        case KickoutType::WHEEL_ENCODER:
            LOG_WARNING(EventSource::AUTOSTEER, "PWM motor kickout: Wheel encoder count %d", (int)value);
            break;
        case KickoutType::PRESSURE_SENSOR:
            LOG_WARNING(EventSource::AUTOSTEER, "PWM motor kickout: Pressure %.1f", value);
            break;
        case KickoutType::CURRENT_SENSOR:
            LOG_WARNING(EventSource::AUTOSTEER, "PWM motor kickout: Current %.2fA", value);
            break;
        default:
            break;
    }
    
    // Disable the motor
    enable(false);
    stop();
}