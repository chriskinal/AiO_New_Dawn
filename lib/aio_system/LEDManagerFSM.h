// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

// LEDManagerFSM.h - FSM-based LED control for front panel status LEDs
#ifndef LED_MANAGER_FSM_H
#define LED_MANAGER_FSM_H

#include <Arduino.h>
#include <Adafruit_PWMServoDriver.h>
#include "I2CManager.h"  // For PCA9685_ADDRESS

class LEDManagerFSM {
public:
    // LED identifiers
    enum LED_ID { 
        PWR_ETH = 0,  // Power/Ethernet status
        GPS = 1,      // GPS status
        STEER = 2,    // Autosteer status
        INS = 3       // INS/IMU status
    };
    
    // LED colors
    enum LED_COLOR { 
        OFF = 0,
        RED = 1,
        YELLOW = 2,
        GREEN = 3,
        BLUE = 4
    };
    
    // LED modes
    enum LED_MODE { 
        SOLID = 0,
        BLINKING = 1
    };
    
    // Power/Ethernet LED states
    enum PowerState {
        PWR_BOOTING,         // System booting - Red
        PWR_ETHERNET_OK,     // Booted & Ethernet connected - Amber
        PWR_AGIO_CONNECTED   // Data connection to/from AgIO - Green
    };
    
    // GPS LED states
    enum GPSState {
        GPS_NO_DATA,         // No GNSS data received - Red
        GPS_DATA_RECEIVED,   // GNSS data received & parsed - Amber
        GPS_RTK_FIXED        // RTK Fixed solution - Green
    };
    
    // Steer LED states
    enum SteerState {
        STEER_MALFUNCTION,   // WAS or other hardware malfunction - Red
        STEER_READY,         // Steering ready - Amber
        STEER_ENGAGED        // Steering engaged - Green
    };
    
    // IMU/INS LED states
    enum IMUState {
        IMU_OFF,             // No data on serial port - LED OFF
        IMU_INVALID_DATA,    // Data received but not valid IMU format - Red
        IMU_DETECTED,        // IMU detected but not yet providing valid data - Amber
        IMU_VALID            // IMU providing valid data - Green
    };
    
    LEDManagerFSM();
    ~LEDManagerFSM() = default;
    
    // Initialize the LED controller
    bool init();
    
    // Update LEDs (call regularly for blinking)
    void update();
    
    // Brightness control (0-100%)
    void setBrightness(uint8_t percent);
    uint8_t getBrightness() const { return brightness; }
    
    // FSM state transitions
    void transitionPowerState(PowerState newState);
    void transitionGPSState(GPSState newState);
    void transitionSteerState(SteerState newState);
    void transitionIMUState(IMUState newState);
    
    // Get current states
    PowerState getPowerState() const { return powerState; }
    GPSState getGPSState() const { return gpsState; }
    SteerState getSteerState() const { return steerState; }
    IMUState getIMUState() const { return imuState; }
    
    // Test mode - cycle through all colors on all LEDs
    void testLEDs();
    
    // Update all LED states based on system status
    // Call this periodically (e.g., every 100ms) from main loop
    void updateAll();
    
    // Pulse functions for blue overlay
    void pulseRTCM();      // 50ms blue pulse for RTCM packet
    void pulseButton();    // 50ms blue pulse for button press
    
private:
    // PCA9685 controller (address defined in I2CManager.h as PCA9685_ADDRESS)
    Adafruit_PWMServoDriver* pwm;

    // LED channel assignments on PCA9685
    static const uint8_t LED_PINS[4][3];  // [LED_ID][R,G,B]
    
    // Color definitions (PWM values at 100% brightness)
    static const uint16_t COLOR_VALUES[5][3];  // [COLOR][R,G,B] - includes BLUE
    
    // Brightness control
    uint8_t brightness;
    static const uint8_t DEFAULT_BRIGHTNESS = 25;  // 25% default
    
    // Current FSM states
    PowerState powerState;
    GPSState gpsState;
    SteerState steerState;
    IMUState imuState;
    
    // LED physical states
    struct LEDState {
        LED_COLOR color;
        LED_MODE mode;
        bool blinkState;
        uint32_t lastBlinkTime;
        bool pulseActive;        // Blue pulse overlay active
        uint32_t pulseStartTime; // When pulse started
    };
    LEDState leds[4];
    
    // Blink timing
    static const uint32_t BLINK_INTERVAL_MS = 500;
    static const uint32_t PULSE_DURATION_MS = 50;   // Blue pulse duration
    
    // State-to-LED mapping tables
    static const struct PowerStateMap {
        PowerState state;
        LED_COLOR color;
        LED_MODE mode;
    } powerStateMap[];
    
    static const struct GPSStateMap {
        GPSState state;
        LED_COLOR color;
        LED_MODE mode;
    } gpsStateMap[];
    
    static const struct SteerStateMap {
        SteerState state;
        LED_COLOR color;
        LED_MODE mode;
    } steerStateMap[];
    
    static const struct IMUStateMap {
        IMUState state;
        LED_COLOR color;
        LED_MODE mode;
    } imuStateMap[];
    
    // Helper functions
    uint16_t scalePWM(uint16_t value);
    void updateSingleLED(LED_ID id);
    void setLEDPins(LED_ID id, uint16_t r, uint16_t g, uint16_t b);
    void setLED(LED_ID id, LED_COLOR color, LED_MODE mode);
    
    // FSM update functions
    void updatePowerLED();
    void updateGPSLED();
    void updateSteerLED();
    void updateIMULED();
};

// Global instance following established pattern
extern LEDManagerFSM ledManagerFSM;

#endif // LED_MANAGER_FSM_H