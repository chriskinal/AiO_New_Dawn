// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

// PWMMotorDriver.h - DRV8701 motor driver implementation
#ifndef PWM_MOTOR_DRIVER_H
#define PWM_MOTOR_DRIVER_H

#include "MotorDriverInterface.h"

// DRV8701 motor driver with complementary PWM
class PWMMotorDriver : public MotorDriverInterface {
private:
    MotorDriverType driverType;
    MotorStatus status;
    
    // Pin assignments
    uint8_t pwm1Pin;     // PWM1 for LEFT direction
    uint8_t pwm2Pin;     // PWM2 for RIGHT direction
    uint8_t enablePin;   // nSLEEP pin (also LOCK output)
    uint8_t currentPin;  // Optional current sense
    
    // PWM parameters  
    static constexpr uint32_t PWM_FREQUENCY = 75;  // Hz - Matching test code frequency
    static constexpr uint16_t PWM_MAX = 256;  // Note: 256 is special - puts pin in Hi-Z
    
    
    // Current sensing
    bool hasCurrentSense;
    float currentScale;  // ADC to Amps conversion factor
    float currentOffset; // Zero current ADC offset
    
public:
    PWMMotorDriver(MotorDriverType type, uint8_t pwm1, uint8_t pwm2, 
                   uint8_t enable = 255, uint8_t current = 255);
    ~PWMMotorDriver() = default;
    
    // MotorDriverInterface implementation
    bool init() override;
    void enable(bool en) override;
    void setPWM(int16_t pwm) override;
    void stop() override;
    
    MotorStatus getStatus() const override { return status; }
    MotorDriverType getType() const override { return driverType; }
    const char* getTypeName() const override;
    bool hasCurrentSensing() const override { return hasCurrentSense; }
    bool hasPositionFeedback() const override { return false; }
    
    float getCurrent() const override;
    void resetErrors() override;
    
    // PWM-specific configuration
    void setCurrentScaling(float scale, float offset);
    void setPWMFrequency(uint32_t freq);
    
    // New interface methods
    bool isDetected() override { return true; }  // PWM drivers are always "detected"
    void handleKickout(KickoutType type, float value) override;
    float getCurrentDraw() override { return getCurrent(); }
};

#endif // PWM_MOTOR_DRIVER_H