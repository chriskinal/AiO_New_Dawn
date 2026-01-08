// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

#include "PWMProcessor.h"
#include "EventLogger.h"
#include "HardwareManager.h"
#include "GNSSProcessor.h"
#include "AutosteerProcessor.h"

// Static instance
PWMProcessor* PWMProcessor::instance = nullptr;

PWMProcessor::PWMProcessor() : 
    pulseFrequency(0.0f),
    pulseDuty(0.5f),        // 50% duty cycle default
    pulseEnabled(false),
    currentSpeedKmh(0.0f),
    pulsesPerMeter(1.0f),   // Default 1 pulse per meter
    lastSpeedUpdate(0)
{
    instance = this;
}

PWMProcessor* PWMProcessor::getInstance()
{
    if (instance == nullptr) {
        instance = new PWMProcessor();
    }
    return instance;
}

bool PWMProcessor::init()
{
    LOG_INFO(EventSource::AUTOSTEER, "=== PWM Processor Initialization ===");

    // Get HardwareManager instance and read pin assignments
    hwManager = HardwareManager::getInstance();
    if (!hwManager) {
        LOG_ERROR(EventSource::AUTOSTEER, "HardwareManager not available!");
        return false;
    }

    // Cache pin assignments from HardwareManager
    speedPulsePin = hwManager->getSpeedPulsePin();
    speedPulseLEDPin = hwManager->getSpeedPulse10Pin();

    // Configure speed pulse pin as output
    pinMode(speedPulsePin, OUTPUT);
    pinMode(speedPulseLEDPin, OUTPUT);

    // Start with output LOW (transistor OFF = output HIGH due to pull-up)
    // Remember: output is inverted through transistor
    digitalWrite(speedPulsePin, LOW);
    digitalWrite(speedPulseLEDPin, LOW);

    // Use cached HardwareManager reference
    HardwareManager* hwMgr = hwManager;
    
    // Request 12-bit resolution for speed pulse
    if (!hwMgr->requestPWMResolution(12, "PWMProcessor")) {
        LOG_WARNING(EventSource::AUTOSTEER, "Failed to set PWM resolution to 12-bit");
        // Fall back to whatever resolution is set
    }
    
    // Request initial frequency
    if (!hwMgr->requestPWMFrequency(speedPulsePin, 0, "PWMProcessor")) {
        LOG_WARNING(EventSource::AUTOSTEER, "Failed to set initial Speed Pulse PWM frequency");
    }
    if (!hwMgr->requestPWMFrequency(speedPulseLEDPin, 0, "PWMProcessor")) {
        LOG_WARNING(EventSource::AUTOSTEER, "Failed to set initial Speed Pulse LED PWM frequency");
    }
   
    LOG_DEBUG(EventSource::AUTOSTEER, "Speed pulse pin (D%d) configured", speedPulsePin);
    LOG_DEBUG(EventSource::AUTOSTEER, "Speed pulse LED pin (D%d) configured", speedPulseLEDPin);
    LOG_DEBUG(EventSource::AUTOSTEER, "PWM resolution: 12-bit");
    LOG_DEBUG(EventSource::AUTOSTEER, "Output type: Open collector (inverted)");
    LOG_INFO(EventSource::AUTOSTEER, "PWM Processor initialization SUCCESS");
    
    return true;
}

void PWMProcessor::process()
{
    // Update PWM speed pulse from GPS every 200ms
    if (millis() - lastSpeedUpdate > 200)  // Update every 200ms like V6-NG
    {
        lastSpeedUpdate = millis();
        
        if (pulseEnabled)
        {
            float speedKmh = 0.0f;
            
            // Use actual GPS speed
            extern GNSSProcessor gnssProcessor;
            const auto &gpsData = gnssProcessor.getData();
            if (gpsData.hasVelocity)
            {
                // Convert knots to km/h
                speedKmh = gpsData.speedKnots * 1.852f;
            }
            else
            {
                // Fallback to PGN 254 speed if GPS velocity is not available
                // This is useful for systems that are running in SIM mode
                // - could also always use PGN254 speed as it returns GPS speed if available
                speedKmh = AutosteerProcessor::getInstance()->getVehicleSpeed();
            }
            
            // Set the speed
            setSpeedKmh(speedKmh);
        }
    }
}

void PWMProcessor::setSpeedPulseHz(float hz)
{
    if (hz < 0.0f) hz = 0.0f;
    if (hz > 10000.0f) hz = 10000.0f;  // Limit to 10kHz
    
    pulseFrequency = hz;
    
    if (hz > 0.0f) {
        HardwareManager* hwMgr = HardwareManager::getInstance();
        if (!hwMgr->requestPWMFrequency(speedPulsePin, (int)hz, "PWMProcessor")) {
            LOG_WARNING(EventSource::AUTOSTEER, "Failed to change Speed Pulse PWM frequency to %dHz", (int)hz);
        }
    }
    
    updatePWM();
}

void PWMProcessor::setSpeedPulseDuty(float duty)
{
    if (duty < 0.0f) duty = 0.0f;
    if (duty > 1.0f) duty = 1.0f;
    
    pulseDuty = duty;
    updatePWM();
}

void PWMProcessor::enableSpeedPulse(bool enable)
{
    pulseEnabled = enable;
    updatePWM();
}

void PWMProcessor::setSpeedKmh(float speedKmh)
{
    if (speedKmh < 0.0f) speedKmh = 0.0f;
    
    currentSpeedKmh = speedKmh;
    
    // Convert speed to pulse frequency
    float hz = speedToFrequency(speedKmh);
    setSpeedPulseHz(hz);
}

void PWMProcessor::setPulsesPerMeter(float ppm)
{
    if (ppm <= 0.0f) ppm = 1.0f;  // Minimum 1 pulse per meter
    
    pulsesPerMeter = ppm;
    
    // Update frequency if we have a speed set
    if (currentSpeedKmh > 0.0f) {
        float hz = speedToFrequency(currentSpeedKmh);
        setSpeedPulseHz(hz);
    }
}

void PWMProcessor::updatePWM()
{
    if (pulseEnabled && pulseFrequency > 0.0f) {
        // Calculate PWM value (12-bit: 0-4095)
        // Note: Output is inverted through transistor
        // HIGH PWM = transistor ON = output LOW
        // LOW PWM = transistor OFF = output HIGH (pull-up)
        // So we invert the duty cycle
        int pwmValue = (int)((1.0f - pulseDuty) * 4095.0f);
        analogWrite(speedPulsePin, pwmValue);

        float ledFreq = pulseFrequency / 10.0f;
        if (ledFreq > 2.000f)                        // Minimum frequency for tone() to work
            tone(speedPulseLEDPin, (int)ledFreq); // LED blinks at 1/10 speed pulse frequency
        else
            noTone(speedPulseLEDPin); // Disable LED if frequency is too low

    } else {
        // Disable PWM - set output LOW (transistor OFF = output HIGH)
        //digitalWrite(speedPulsePin, LOW);   // does not work for disabling pulse
        analogWrite(speedPulsePin, 0);
        noTone(speedPulseLEDPin); // Disable LED
    }
}

float PWMProcessor::speedToFrequency(float speedKmh) const
{
    // Convert km/h to m/s
    float speedMs = speedKmh / 3.6f;
    
    // Calculate pulse frequency
    // frequency (Hz) = speed (m/s) * pulses per meter
    float hz = speedMs * pulsesPerMeter;
    
    return hz;
}

void PWMProcessor::printStatus() const
{
    LOG_INFO(EventSource::AUTOSTEER, "=== PWM Processor Status ===");

    LOG_INFO(EventSource::AUTOSTEER, "Speed Pulse Output:");
    LOG_INFO(EventSource::AUTOSTEER, "  Enabled: %s", pulseEnabled ? "YES" : "NO");
    LOG_INFO(EventSource::AUTOSTEER, "  Frequency: %.1f Hz", pulseFrequency);
    LOG_INFO(EventSource::AUTOSTEER, "  Duty Cycle: %.1f%%", pulseDuty * 100.0f);
    LOG_INFO(EventSource::AUTOSTEER, "  Pin: D%d (open collector)", speedPulsePin);

    LOG_INFO(EventSource::AUTOSTEER, "Speed Settings:");
    LOG_INFO(EventSource::AUTOSTEER, "  Current Speed: %.1f km/h", currentSpeedKmh);
    LOG_INFO(EventSource::AUTOSTEER, "  Pulses/Meter: %.2f", pulsesPerMeter);
    LOG_INFO(EventSource::AUTOSTEER, "  Calculated Hz: %.1f", speedToFrequency(currentSpeedKmh));

    LOG_INFO(EventSource::AUTOSTEER, "=============================");
}