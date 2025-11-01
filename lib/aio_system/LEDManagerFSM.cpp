// LEDManagerFSM.cpp - FSM-based implementation of front panel LED control
#include "LEDManagerFSM.h"
#include <Wire.h>
#include <QNEthernet.h>
#include "QNetworkBase.h"
#include "EventLogger.h"
#include "GNSSProcessor.h"
#include "IMUProcessor.h"
#include "NAVProcessor.h"
#include "PGNProcessor.h"
#include "AutosteerProcessor.h"

// QNEthernet namespace
using namespace qindesign::network;

// External processor instances needed for status checks
extern GNSSProcessor gnssProcessor;
extern IMUProcessor imuProcessor;

// Global instance
LEDManagerFSM ledManagerFSM;

// LED pin assignments on PCA9685 (from NG-V6)
const uint8_t LEDManagerFSM::LED_PINS[4][3] = {
    {13, 14, 15},  // PWR_ETH: R=13, G=14, B=15
    {5, 7, 12},    // GPS: R=5, G=7, B=12
    {1, 0, 3},     // STEER: R=1, G=0, B=3
    {6, 4, 2}      // INS: R=6, G=4, B=2
};

// Color definitions at 100% brightness (12-bit PWM: 0-4095)
const uint16_t LEDManagerFSM::COLOR_VALUES[5][3] = {
    {0, 0, 0},           // OFF
    {4095, 0, 0},        // RED
    {4095, 2048, 0},     // YELLOW (Red + half Green)
    {0, 4095, 0},        // GREEN
    {0, 0, 4095}         // BLUE
};

// State-to-LED mapping tables
const LEDManagerFSM::PowerStateMap LEDManagerFSM::powerStateMap[] = {
    {PWR_BOOTING,        RED,    SOLID},
    {PWR_ETHERNET_OK,    YELLOW, SOLID},
    {PWR_AGIO_CONNECTED, GREEN,  SOLID}
};

const LEDManagerFSM::GPSStateMap LEDManagerFSM::gpsStateMap[] = {
    {GPS_NO_DATA,         RED,    SOLID},
    {GPS_DATA_RECEIVED,   YELLOW, SOLID},
    {GPS_RTK_FIXED,       GREEN,  SOLID}
};

const LEDManagerFSM::SteerStateMap LEDManagerFSM::steerStateMap[] = {
    {STEER_MALFUNCTION, RED,    SOLID},
    {STEER_READY,       YELLOW, SOLID},
    {STEER_ENGAGED,     GREEN,  SOLID}
};

const LEDManagerFSM::IMUStateMap LEDManagerFSM::imuStateMap[] = {
    {IMU_OFF,           OFF,    SOLID},
    {IMU_INVALID_DATA,  RED,    SOLID},
    {IMU_DETECTED,      YELLOW, SOLID},
    {IMU_VALID,         GREEN,  SOLID}
};

LEDManagerFSM::LEDManagerFSM() : 
    pwm(nullptr), 
    brightness(DEFAULT_BRIGHTNESS),
    powerState(PWR_BOOTING),
    gpsState(GPS_NO_DATA),
    steerState(STEER_READY),
    imuState(IMU_OFF) {
    
    // Initialize LED states
    for (int i = 0; i < 4; i++) {
        leds[i].color = OFF;
        leds[i].mode = SOLID;
        leds[i].blinkState = false;
        leds[i].lastBlinkTime = 0;
        leds[i].pulseActive = false;
        leds[i].pulseStartTime = 0;
    }
}

bool LEDManagerFSM::init() {
    LOG_INFO(EventSource::SYSTEM, "Initializing LED Manager (FSM)");
    
    // Create PCA9685 driver - explicitly specify Wire like NG-V6
    pwm = new Adafruit_PWMServoDriver(PCA9685_ADDRESS, Wire);

    // Check if PCA9685 is present
    Wire.beginTransmission(PCA9685_ADDRESS);
    uint8_t error = Wire.endTransmission();
    if (error != 0) {
        LOG_ERROR(EventSource::SYSTEM, "PCA9685 not found at 0x70 (error=%d)", error);
        delete pwm;
        pwm = nullptr;
        return false;
    }
    LOG_DEBUG(EventSource::SYSTEM, "PCA9685 detected at 0x70");
    
    // Initialize PCA9685
    pwm->begin();
    
    // Set I2C speed to 1MHz like NG-V6
    Wire.setClock(1000000);
    
    pwm->setPWMFreq(120);  // 120Hz like NG-V6 to avoid flicker
    pwm->setOutputMode(false);  // false: open drain mode for common anode LEDs
    
    // Turn off all channels using setPin like NG-V6
    for (int i = 0; i < 16; i++) {
        pwm->setPin(i, 0, true);  // 0 with invert=true means fully off
    }
    
    LOG_INFO(EventSource::SYSTEM, "LED Manager (FSM) initialized (brightness=%d%%)", brightness);
    
    // Quick test flash - all LEDs green for 100ms
    for (int i = 0; i < 4; i++) {
        setLED((LED_ID)i, GREEN, SOLID);
    }
    delay(100);
    
    // Initialize LEDs to their starting states
    updatePowerLED();
    updateGPSLED();
    updateSteerLED();
    updateIMULED();
    
    return true;
}

void LEDManagerFSM::update() {
    if (!pwm) return;
    
    uint32_t now = millis();
    
    // Use a global blink state for synchronized blinking
    static bool globalBlinkState = false;
    static uint32_t lastGlobalBlinkTime = 0;
    
    // Update global blink state
    if (now - lastGlobalBlinkTime >= BLINK_INTERVAL_MS) {
        globalBlinkState = !globalBlinkState;
        lastGlobalBlinkTime = now;
        
        // Update all blinking LEDs at once
        for (int i = 0; i < 4; i++) {
            if (leds[i].mode == BLINKING) {
                leds[i].blinkState = globalBlinkState;
                updateSingleLED((LED_ID)i);
            }
        }
    }
    
    // Check for pulse timeout
    for (int i = 0; i < 4; i++) {
        if (leds[i].pulseActive && (now - leds[i].pulseStartTime >= PULSE_DURATION_MS)) {
            leds[i].pulseActive = false;
            updateSingleLED((LED_ID)i);
        }
    }
}

void LEDManagerFSM::setBrightness(uint8_t percent) {
    brightness = constrain(percent, 5, 100);  // Minimum 5% to ensure visibility
    
    // Update all LEDs with new brightness
    for (int i = 0; i < 4; i++) {
        updateSingleLED((LED_ID)i);
    }
}

uint16_t LEDManagerFSM::scalePWM(uint16_t value) {
    return (value * brightness) / 100;
}

// FSM state transition functions
void LEDManagerFSM::transitionPowerState(PowerState newState) {
    if (powerState != newState) {
        LOG_DEBUG(EventSource::SYSTEM, "Power LED state transition: %d -> %d", powerState, newState);
        powerState = newState;
        updatePowerLED();
    }
}

void LEDManagerFSM::transitionGPSState(GPSState newState) {
    if (gpsState != newState) {
        LOG_DEBUG(EventSource::SYSTEM, "GPS LED state transition: %d -> %d", gpsState, newState);
        gpsState = newState;
        updateGPSLED();
    }
}

void LEDManagerFSM::transitionSteerState(SteerState newState) {
    if (steerState != newState) {
        const char* stateNames[] = {"MALFUNCTION", "READY", "ENGAGED"};
        LOG_DEBUG(EventSource::SYSTEM, "Steer LED state transition: %s -> %s", 
                 stateNames[steerState], stateNames[newState]);
        steerState = newState;
        updateSteerLED();
    }
}

void LEDManagerFSM::transitionIMUState(IMUState newState) {
    if (imuState != newState) {
        LOG_DEBUG(EventSource::SYSTEM, "IMU LED state transition: %d -> %d", imuState, newState);
        imuState = newState;
        updateIMULED();
    }
}

// FSM LED update functions
void LEDManagerFSM::updatePowerLED() {
    for (uint8_t i = 0; i < sizeof(powerStateMap)/sizeof(powerStateMap[0]); i++) {
        if (powerStateMap[i].state == powerState) {
            setLED(PWR_ETH, powerStateMap[i].color, powerStateMap[i].mode);
            break;
        }
    }
}

void LEDManagerFSM::updateGPSLED() {
    for (uint8_t i = 0; i < sizeof(gpsStateMap)/sizeof(gpsStateMap[0]); i++) {
        if (gpsStateMap[i].state == gpsState) {
            setLED(GPS, gpsStateMap[i].color, gpsStateMap[i].mode);
            break;
        }
    }
}

void LEDManagerFSM::updateSteerLED() {
    for (uint8_t i = 0; i < sizeof(steerStateMap)/sizeof(steerStateMap[0]); i++) {
        if (steerStateMap[i].state == steerState) {
            setLED(STEER, steerStateMap[i].color, steerStateMap[i].mode);
            break;
        }
    }
}

void LEDManagerFSM::updateIMULED() {
    for (uint8_t i = 0; i < sizeof(imuStateMap)/sizeof(imuStateMap[0]); i++) {
        if (imuStateMap[i].state == imuState) {
            setLED(INS, imuStateMap[i].color, imuStateMap[i].mode);
            break;
        }
    }
}

void LEDManagerFSM::setLED(LED_ID id, LED_COLOR color, LED_MODE mode) {
    if (!pwm || id > INS) return;
    
    leds[id].color = color;
    leds[id].mode = mode;
    
    // Reset blink state when changing modes
    if (mode == SOLID) {
        leds[id].blinkState = false;
    } else if (mode == BLINKING) {
        // Start blinking LEDs in sync with current global state
        // This is set properly in the update() function
        leds[id].blinkState = false; // Will sync on next update
    }
    
    // Don't update if pulse is active - let the pulse complete
    if (!leds[id].pulseActive) {
        updateSingleLED(id);
    }
}

void LEDManagerFSM::updateSingleLED(LED_ID id) {
    if (!pwm || id > INS) return;
    
    // Check if pulse is active - blue pulse overrides normal color
    if (leds[id].pulseActive) {
        uint16_t b = scalePWM(COLOR_VALUES[BLUE][2]);
        setLEDPins(id, 0, 0, b);
        return;
    }
    
    // Determine if LED should be on
    bool ledOn = (leds[id].mode == SOLID) || 
                 (leds[id].mode == BLINKING && leds[id].blinkState);
    
    if (!ledOn || leds[id].color == OFF) {
        // Turn off LED
        setLEDPins(id, 0, 0, 0);
    } else {
        // Get color values and apply brightness
        uint16_t r = scalePWM(COLOR_VALUES[leds[id].color][0]);
        uint16_t g = scalePWM(COLOR_VALUES[leds[id].color][1]);
        uint16_t b = scalePWM(COLOR_VALUES[leds[id].color][2]);
        
        // Additional scaling for colors that tend to be too bright
        if (leds[id].color == RED) {
            r = (r * 80) / 100;  // Red at 80% of scaled value
        } else if (leds[id].color == YELLOW) {
            r = (r * 60) / 100;  // Yellow red at 60%
            g = (g * 60) / 100;  // Yellow green at 60%
        }
        
        setLEDPins(id, r, g, b);
    }
}

void LEDManagerFSM::setLEDPins(LED_ID id, uint16_t r, uint16_t g, uint16_t b) {
    if (!pwm || id > INS) return;
    
    // Use setPin with invert flag for common anode LEDs
    // The library will handle the inversion when invert=true
    pwm->setPin(LED_PINS[id][0], r, true);
    delayMicroseconds(50);  // Small delay to avoid crosstalk
    pwm->setPin(LED_PINS[id][1], g, true);
    delayMicroseconds(50);
    pwm->setPin(LED_PINS[id][2], b, true);
}

void LEDManagerFSM::testLEDs() {
    if (!pwm) return;
    
    LOG_INFO(EventSource::SYSTEM, "Running LED test sequence (FSM)");
    
    // Test each LED with each color
    for (int led = 0; led < 4; led++) {
        const char* ledNames[] = {"PWR_ETH", "GPS", "STEER", "INS"};
        LOG_DEBUG(EventSource::SYSTEM, "Testing %s LED:", ledNames[led]);
        
        for (int color = 1; color <= 3; color++) {  // Skip OFF
            const char* colorNames[] = {"", "RED", "YELLOW", "GREEN"};
            LOG_DEBUG(EventSource::SYSTEM, "  %s", colorNames[color]);
            
            setLED((LED_ID)led, (LED_COLOR)color, SOLID);
            delay(500);
            setLED((LED_ID)led, OFF, SOLID);
            delay(100);
        }
    }
    
    // Test blinking
    LOG_DEBUG(EventSource::SYSTEM, "Testing all LEDs blinking green");
    for (int led = 0; led < 4; led++) {
        setLED((LED_ID)led, GREEN, BLINKING);
    }
    
    // Let them blink for 3 seconds
    for (int i = 0; i < 30; i++) {
        update();
        delay(100);
    }
    
    // Turn off all LEDs
    for (int led = 0; led < 4; led++) {
        setLED((LED_ID)led, OFF, SOLID);
    }
    
    LOG_INFO(EventSource::SYSTEM, "LED test sequence (FSM) complete");
}

void LEDManagerFSM::updateAll() {
    // Power/Ethernet LED state determination
    static bool bootComplete = false;
    static uint32_t bootTime = millis();
    
    // Consider boot complete after 5 seconds
    if (!bootComplete && (millis() - bootTime > 5000)) {
        bootComplete = true;
    }
    
    bool ethernetUp = QNetworkBase::isConnected();
    bool hasAgIO = PGNProcessor::instance && PGNProcessor::instance->isReceivingFromAgIO();
    
    PowerState newPowerState;
    if (!bootComplete) {
        newPowerState = PWR_BOOTING;
    } else if (ethernetUp && hasAgIO) {
        newPowerState = PWR_AGIO_CONNECTED;
    } else if (ethernetUp) {
        newPowerState = PWR_ETHERNET_OK;
    } else {
        newPowerState = PWR_BOOTING;  // Red for no ethernet
    }
    transitionPowerState(newPowerState);
    
    // GPS LED state determination
    GPSState newGPSState;
    if (!gnssProcessor.hasGPS()) {
        newGPSState = GPS_NO_DATA;
    } else {
        uint8_t fixQuality = gnssProcessor.getData().fixQuality;
        if (fixQuality == 4) {  // RTK Fixed
            newGPSState = GPS_RTK_FIXED;
        } else {
            newGPSState = GPS_DATA_RECEIVED;  // All other states show amber
        }
    }
    transitionGPSState(newGPSState);
    
    // Steer LED state is handled by AutosteerProcessor
    // Don't change it here - just update the blinking
    
    // IMU/INS LED state determination
    IMUState newIMUState;
    if (imuProcessor.getIMUType() != IMUType::NONE) {
        // Separate IMU detected (BNO08x or TM171)
        if (!imuProcessor.isIMUInitialized() || !imuProcessor.hasValidData()) {
            newIMUState = IMU_DETECTED;  // Amber for detected but not ready
        } else {
            newIMUState = IMU_VALID;      // Green for valid data
        }
    } else if (gnssProcessor.getData().hasINS) {
        // UM981 INS system
        const auto& gpsData = gnssProcessor.getData();
        if (gpsData.insAlignmentStatus == 3) { // Solution good
            newIMUState = IMU_VALID;      // Green for aligned
        } else {
            newIMUState = IMU_DETECTED;   // Amber for aligning
        }
    } else if (imuProcessor.hasSerialData()) {
        // Data coming in but not recognized as valid IMU format
        newIMUState = IMU_INVALID_DATA;  // Red - invalid data
    } else {
        // No data on serial port - LED OFF
        newIMUState = IMU_OFF;
    }
    transitionIMUState(newIMUState);
    
    // Update LED hardware (handles blinking)
    update();
}

void LEDManagerFSM::pulseRTCM() {
    // Pulse GPS LED blue for 50ms when RTCM packet received
    if (!pwm) return;
    
    // Don't start a new pulse if one is already active
    if (leds[GPS].pulseActive) {
        return;
    }
    
    // Minimum 200ms between pulses for visual clarity
    static uint32_t lastPulseTime = 0;
    uint32_t now = millis();
    if (now - lastPulseTime < 200) {
        return;
    }
    
    lastPulseTime = now;
    leds[GPS].pulseActive = true;
    leds[GPS].pulseStartTime = now;
    updateSingleLED(GPS);
}

void LEDManagerFSM::pulseButton() {
    // Pulse STEER LED blue for 50ms when button pressed
    if (!pwm) return;
    
    leds[STEER].pulseActive = true;
    leds[STEER].pulseStartTime = millis();
    updateSingleLED(STEER);
}