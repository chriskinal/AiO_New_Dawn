// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

#include "HardwareManager.h"
#include "EventLogger.h"
#include "ConfigManager.h"

// Static instance pointer
HardwareManager *HardwareManager::instance = nullptr;

HardwareManager::HardwareManager()
    : isInitialized(false), pwmFrequencyMode(4), globalPWMResolution(8), pwmResolutionOwner("default")
{
    instance = this;
}

HardwareManager::~HardwareManager()
{
    instance = nullptr;
}

HardwareManager *HardwareManager::getInstance()
{
    return instance;
}

void HardwareManager::init()
{
    if (instance == nullptr)
    {
        new HardwareManager();
    }
}

bool HardwareManager::initialize()
{
    return initializeHardware();
}

bool HardwareManager::initializeHardware()
{
    LOG_INFO(EventSource::SYSTEM, "Hardware Manager Initialization starting");

    if (!initializePins())
    {
        LOG_ERROR(EventSource::SYSTEM, "Pin initialization FAILED");
        return false;
    }

    if (!initializePWM())
    {
        LOG_ERROR(EventSource::SYSTEM, "PWM initialization FAILED");
        return false;
    }

    if (!initializeADC())
    {
        LOG_ERROR(EventSource::SYSTEM, "ADC initialization FAILED");
        return false;
    }

    isInitialized = true;
    LOG_INFO(EventSource::SYSTEM, "Hardware initialization SUCCESS");
    return true;
}

bool HardwareManager::initializePins()
{
    LOG_DEBUG(EventSource::SYSTEM, "Pin initialization moved to individual modules");

    // HARDWARE OWNERSHIP - Pins are now initialized by their owner modules:
    // - ADProcessor: STEER_PIN, WORK_PIN, WAS_SENSOR_PIN, CURRENT_PIN, KICKOUT_A_PIN
    // - PWMMotorDriver: PWM1_PIN, PWM2_PIN, SLEEP_PIN
    // - EncoderProcessor: KICKOUT_D_PIN (when encoder enabled)
    // - KickoutMonitor: KICKOUT_D_PIN (when encoder disabled)
    // - PWMProcessor: SPEEDPULSE_PIN

    // HardwareManager only initializes pins it directly controls
    pinMode(getBuzzerPin(), OUTPUT);
    digitalWrite(getBuzzerPin(), LOW);

    LOG_DEBUG(EventSource::SYSTEM, "HardwareManager pin configuration complete");
    return true;
}

bool HardwareManager::initializePWM()
{
    LOG_DEBUG(EventSource::SYSTEM, "Configuring PWM");
    return setPWMFrequency(pwmFrequencyMode);
}

bool HardwareManager::initializeADC()
{
    LOG_DEBUG(EventSource::SYSTEM, "Configuring ADC");

    LOG_DEBUG(EventSource::SYSTEM, "ADC: Using Teensy defaults");
    return true;
}

bool HardwareManager::setPWMFrequency(uint8_t mode)
{
    pwmFrequencyMode = mode;

    uint16_t frequency;
    switch (mode)
    {
    case 0:
        frequency = 490;
        break;
    case 1:
        frequency = 122;
        break;
    case 2:
        frequency = 3921;
        break;
    case 3:
        frequency = 9155;
        break;
    case 4:
        frequency = 18310;
        break;
    default:
        frequency = 18310;
        pwmFrequencyMode = 4;
        break;
    }

    // PWM frequency is now managed by individual motor drivers through SharedResourceManager
    // This method is kept for backward compatibility but logs a warning
    LOG_WARNING(EventSource::SYSTEM, "setPWMFrequency called on HardwareManager - use motor driver instead");

    LOG_DEBUG(EventSource::SYSTEM, "PWM frequency: %i Hz (mode %i)", frequency, pwmFrequencyMode);

    return true;
}

// Pin access method defines
uint8_t HardwareManager::getWASSensorPin() const { return WAS_SENSOR_PIN; }
uint8_t HardwareManager::getSpeedPulsePin() const { return SPEEDPULSE_PIN; }
uint8_t HardwareManager::getSpeedPulse10Pin() const { return SPEEDPULSE10_PIN; }
uint8_t HardwareManager::getBuzzerPin() const { return BUZZER; }
uint8_t HardwareManager::getSleepPin() const { return SLEEP_PIN; }
uint8_t HardwareManager::getPWM1Pin() const { return PWM1_PIN; }
uint8_t HardwareManager::getPWM2Pin() const { return PWM2_PIN; }
uint8_t HardwareManager::getSteerPin() const { return STEER_PIN; }
uint8_t HardwareManager::getWorkPin() const { return WORK_PIN; }
uint8_t HardwareManager::getKickoutDPin() const { return KICKOUT_D_PIN; }
uint8_t HardwareManager::getCurrentPin() const { return CURRENT_PIN; }
uint8_t HardwareManager::getKickoutAPin() const { return KICKOUT_A_PIN; }

void HardwareManager::enableBuzzer()
{
    digitalWrite(getBuzzerPin(), HIGH);
}

void HardwareManager::disableBuzzer()
{
    digitalWrite(getBuzzerPin(), LOW);
}

void HardwareManager::performBuzzerTest()
{
    // Get buzzer volume setting from ConfigManager
    extern ConfigManager configManager;
    bool loudMode = configManager.getBuzzerLoudMode();

    if (loudMode)
    {
        // Loud mode for field use - play multiple tones
        LOG_INFO(EventSource::SYSTEM, "Playing LOUD buzzer test");

        // Play ascending tones
        tone(getBuzzerPin(), 1000, 200); // 1kHz for 200ms
        delay(250);
        tone(getBuzzerPin(), 1500, 200); // 1.5kHz for 200ms
        delay(250);
        tone(getBuzzerPin(), 2000, 300); // 2kHz for 300ms
        delay(350);

        // Play descending tones
        tone(getBuzzerPin(), 1500, 200); // 1.5kHz for 200ms
        delay(250);
        tone(getBuzzerPin(), 1000, 300); // 1kHz for 300ms
        delay(350);
    }
    else
    {
        // Quiet mode for development - cricket-like click
        LOG_INFO(EventSource::SYSTEM, "Playing quiet buzzer test");
        tone(getBuzzerPin(), 4000, 5); // 4kHz for 5ms - very quick click
        delay(10);
    }

    // Make sure buzzer is off
    noTone(getBuzzerPin());
}

void HardwareManager::enableSteerMotor()
{
    digitalWrite(getSleepPin(), HIGH);
}

void HardwareManager::disableSteerMotor()
{
    digitalWrite(getSleepPin(), LOW);
}

void HardwareManager::printHardwareStatus()
{
    LOG_INFO(EventSource::CONFIG, "=== Hardware Manager Status ===");
    LOG_INFO(EventSource::CONFIG, "Initialized: %s", isInitialized ? "YES" : "NO");
    LOG_INFO(EventSource::CONFIG, "CPU Frequency: %i MHz", F_CPU_ACTUAL / 1000000);
    LOG_INFO(EventSource::CONFIG, "PWM Mode: %i (deprecated)", pwmFrequencyMode);

    printPinConfiguration();
    printPinOwnership();
    printResourceStatus();
    LOG_INFO(EventSource::CONFIG, "===============================");
}

void HardwareManager::printPinConfiguration()
{
    LOG_INFO(EventSource::CONFIG, "--- Pin Configuration ---");
    LOG_INFO(EventSource::CONFIG, "WAS Sensor: A%i", getWASSensorPin() - A0);
    LOG_INFO(EventSource::CONFIG, "Speed Pulse: %i", getSpeedPulsePin());
    LOG_INFO(EventSource::CONFIG, "Buzzer: %i", getBuzzerPin());
    LOG_INFO(EventSource::CONFIG, "Motor Sleep: %i", getSleepPin());
    LOG_INFO(EventSource::CONFIG, "PWM1: %i", getPWM1Pin());
    LOG_INFO(EventSource::CONFIG, "PWM2: %i", getPWM2Pin());
    LOG_INFO(EventSource::CONFIG, "Steer Switch: %i", getSteerPin());
    LOG_INFO(EventSource::CONFIG, "Work Input: A%i", getWorkPin() - A0);
    LOG_INFO(EventSource::CONFIG, "Kickout Digital: %i", getKickoutDPin());
    LOG_INFO(EventSource::CONFIG, "Current Sensor: A%i", getCurrentPin() - A0);
    LOG_INFO(EventSource::CONFIG, "Kickout Analog: A%i", getKickoutAPin() - A0);
}

bool HardwareManager::getInitializationStatus() const
{
    return isInitialized;
}

// Pin ownership management
bool HardwareManager::requestPinOwnership(uint8_t pin, PinOwner owner, const char *ownerName)
{
    auto it = pinOwnership.find(pin);

    if (it != pinOwnership.end() && it->second.isOwned)
    {
        if (it->second.owner != owner)
        {
            LOG_ERROR(EventSource::SYSTEM, "Pin %d already owned by %s, %s cannot claim it",
                      pin, it->second.ownerName, ownerName);
            return false;
        }
        // Same owner reclaiming is OK
        return true;
    }

    // Claim ownership
    pinOwnership[pin] = {owner, ownerName, 0, true};
    LOG_DEBUG(EventSource::SYSTEM, "Pin %d claimed by %s", pin, ownerName);
    return true;
}

bool HardwareManager::releasePinOwnership(uint8_t pin, PinOwner owner)
{
    auto it = pinOwnership.find(pin);

    if (it == pinOwnership.end() || !it->second.isOwned)
    {
        LOG_WARNING(EventSource::SYSTEM, "Attempted to release unowned pin %d", pin);
        return false;
    }

    if (it->second.owner != owner)
    {
        LOG_ERROR(EventSource::SYSTEM, "Pin %d owned by %s, cannot be released by different owner",
                  pin, it->second.ownerName);
        return false;
    }

    LOG_DEBUG(EventSource::SYSTEM, "Pin %d released by %s", pin, it->second.ownerName);
    it->second.isOwned = false;
    it->second.owner = OWNER_NONE;
    it->second.ownerName = "none";
    return true;
}

bool HardwareManager::transferPinOwnership(uint8_t pin, PinOwner fromOwner, PinOwner toOwner,
                                           const char *toOwnerName, void (*cleanupCallback)(uint8_t))
{
    auto it = pinOwnership.find(pin);

    // Verify current ownership
    if (it == pinOwnership.end() || !it->second.isOwned || it->second.owner != fromOwner)
    {
        LOG_ERROR(EventSource::SYSTEM, "Pin %d not owned by expected owner, transfer failed", pin);
        return false;
    }

    LOG_INFO(EventSource::SYSTEM, "Transferring pin %d from %s to %s",
             pin, it->second.ownerName, toOwnerName);

    // Run cleanup callback if provided
    if (cleanupCallback)
    {
        cleanupCallback(pin);
    }

    // Transfer ownership
    it->second.owner = toOwner;
    it->second.ownerName = toOwnerName;

    return true;
}

HardwareManager::PinOwner HardwareManager::getPinOwner(uint8_t pin) const
{
    auto it = pinOwnership.find(pin);
    return (it != pinOwnership.end() && it->second.isOwned) ? it->second.owner : OWNER_NONE;
}

const char *HardwareManager::getPinOwnerName(uint8_t pin) const
{
    auto it = pinOwnership.find(pin);
    return (it != pinOwnership.end() && it->second.isOwned) ? it->second.ownerName : "none";
}

bool HardwareManager::isPinOwned(uint8_t pin) const
{
    auto it = pinOwnership.find(pin);
    return (it != pinOwnership.end() && it->second.isOwned);
}

void HardwareManager::updatePinMode(uint8_t pin, uint8_t mode)
{
    auto it = pinOwnership.find(pin);
    if (it != pinOwnership.end())
    {
        it->second.pinMode = mode;
    }
}

void HardwareManager::printPinOwnership()
{
    LOG_INFO(EventSource::SYSTEM, "=== Pin Ownership Status ===");

    for (const auto &entry : pinOwnership)
    {
        if (entry.second.isOwned)
        {
            const char *modeStr = "UNKNOWN";
            switch (entry.second.pinMode)
            {
            case INPUT:
                modeStr = "INPUT";
                break;
            case OUTPUT:
                modeStr = "OUTPUT";
                break;
            case INPUT_PULLUP:
                modeStr = "INPUT_PULLUP";
                break;
            case INPUT_PULLDOWN:
                modeStr = "INPUT_PULLDOWN";
                break;
            case OUTPUT_OPENDRAIN:
                modeStr = "OUTPUT_OPENDRAIN";
                break;
            case INPUT_DISABLE:
                modeStr = "INPUT_DISABLE";
                break;
            }
            LOG_INFO(EventSource::SYSTEM, "Pin %d: %s (mode: %s)",
                     entry.first, entry.second.ownerName, modeStr);
        }
    }

    LOG_INFO(EventSource::SYSTEM, "=============================");
}

// PWM resource management
bool HardwareManager::requestPWMFrequency(uint8_t pin, uint32_t frequency, const char *owner)
{
    PWMTimerGroup group = getPWMTimerGroup(pin);
    if (group == TIMER_GROUP_UNKNOWN)
    {
        LOG_ERROR(EventSource::SYSTEM, "Unknown PWM timer group for pin %d", pin);
        return false;
    }

    // Check if timer group already configured
    auto it = pwmConfigs.find(group);
    if (it != pwmConfigs.end())
    {
        if (it->second.frequency != frequency)
        {
            // Check if it's the same owner trying to change frequency
            if (strcmp(it->second.owner, owner) == 0)
            {
                // Same owner can change their own frequency
                analogWriteFrequency(pin, frequency);
                LOG_DEBUG(EventSource::SYSTEM, "PWM timer group %d frequency changed to %luHz by %s",
                          group, frequency, owner);
                it->second.frequency = frequency;
                return true;
            }
            else
            {
                // Different owner - conflict
                LOG_WARNING(EventSource::SYSTEM,
                            "PWM frequency conflict on timer group %d: %s wants %luHz, %s has %luHz",
                            group, owner, frequency, it->second.owner, it->second.frequency);
                return false;
            }
        }
        // Same frequency is OK
        return true;
    }

    // Configure the frequency
    analogWriteFrequency(pin, frequency);
    pwmConfigs[group] = {frequency, globalPWMResolution, owner};
    LOG_INFO(EventSource::SYSTEM, "PWM timer group %d set to %luHz by %s", group, frequency, owner);
    return true;
}

bool HardwareManager::requestPWMResolution(uint8_t resolution, const char *owner)
{
    if (globalPWMResolution != resolution &&
        strcmp(pwmResolutionOwner, "default") != 0)
    {
        LOG_WARNING(EventSource::SYSTEM,
                    "PWM resolution conflict: %s wants %d-bit, %s has %d-bit",
                    owner, resolution, pwmResolutionOwner, globalPWMResolution);
        return false;
    }

    if (globalPWMResolution != resolution)
    {
        analogWriteResolution(resolution);
        globalPWMResolution = resolution;
        pwmResolutionOwner = owner;
        LOG_INFO(EventSource::SYSTEM, "PWM resolution set to %d-bit by %s", resolution, owner);
    }
    return true;
}

uint32_t HardwareManager::getPWMFrequency(PWMTimerGroup group)
{
    auto it = pwmConfigs.find(group);
    return (it != pwmConfigs.end()) ? it->second.frequency : 0;
}

uint8_t HardwareManager::getPWMResolution() const
{
    return globalPWMResolution;
}

HardwareManager::PWMTimerGroup HardwareManager::getPWMTimerGroup(uint8_t pin)
{
    switch (pin)
    {
    case 0:
    case 1:
    case 24:
    case 25:
    case 28:
    case 29:
        return TIMER_GROUP_1;
    case 2:
    case 3:
        return TIMER_GROUP_2;
    case 4:
    case 33:
        return TIMER_GROUP_3;
    case 5:
        return TIMER_GROUP_4;
    case 6:
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
    case 32:
        return TIMER_GROUP_5;
    case 7:
    case 8:
    case 36:
    case 37:
        return TIMER_GROUP_6;
    case 14:
    case 15:
    case 18:
    case 19:
        return TIMER_GROUP_7;
    case 22:
    case 23:
        return TIMER_GROUP_8;
    default:
        return TIMER_GROUP_UNKNOWN;
    }
}

// ADC resource management
bool HardwareManager::requestADCConfig(ADCModule module, uint8_t resolution, uint8_t averaging, const char *owner)
{
    auto it = adcConfigs.find(module);
    if (it != adcConfigs.end())
    {
        // Check for conflicts
        if (it->second.resolution != resolution || it->second.averaging != averaging)
        {
            LOG_WARNING(EventSource::SYSTEM,
                        "ADC%d config conflict: %s wants %d-bit/%d avg, %s has %d-bit/%d avg",
                        module, owner, resolution, averaging,
                        it->second.owner, it->second.resolution, it->second.averaging);
            return false;
        }
        return true;
    }

    // Store config (actual ADC configuration done by owner)
    adcConfigs[module] = {resolution, averaging, owner};
    LOG_INFO(EventSource::SYSTEM, "ADC%d config: %d-bit, %d averaging by %s",
             module, resolution, averaging, owner);
    return true;
}

// I2C resource management
bool HardwareManager::requestI2CSpeed(I2CBus bus, uint32_t speed, const char *owner)
{
    auto it = i2cConfigs.find(bus);
    if (it != i2cConfigs.end())
    {
        if (it->second.clockSpeed != speed)
        {
            // Allow speed increase but warn
            if (speed > it->second.clockSpeed)
            {
                LOG_WARNING(EventSource::SYSTEM,
                            "I2C bus %d speed increased from %luHz to %luHz by %s (was set by %s)",
                            bus, it->second.clockSpeed, speed, owner, it->second.owner);
                it->second.clockSpeed = speed;
                it->second.owner = owner;
                return true;
            }
            else
            {
                LOG_WARNING(EventSource::SYSTEM,
                            "I2C bus %d speed conflict: %s wants %luHz, %s has %luHz",
                            bus, owner, speed, it->second.owner, it->second.clockSpeed);
                return false;
            }
        }
        return true;
    }

    i2cConfigs[bus] = {speed, owner};
    LOG_INFO(EventSource::SYSTEM, "I2C bus %d set to %luHz by %s", bus, speed, owner);
    return true;
}

void HardwareManager::printResourceStatus()
{
    LOG_INFO(EventSource::SYSTEM, "=== Hardware Resource Status ===");

    // PWM Status
    LOG_INFO(EventSource::SYSTEM, "PWM Resolution: %d-bit (owner: %s)",
             globalPWMResolution, pwmResolutionOwner);

    for (const auto &pwm : pwmConfigs)
    {
        LOG_INFO(EventSource::SYSTEM, "PWM Timer Group %d: %luHz (owner: %s)",
                 pwm.first, pwm.second.frequency, pwm.second.owner);
    }

    // ADC Status
    for (const auto &adc : adcConfigs)
    {
        LOG_INFO(EventSource::SYSTEM, "ADC%d: %d-bit, %d avg (owner: %s)",
                 adc.first, adc.second.resolution, adc.second.averaging, adc.second.owner);
    }

    // I2C Status
    for (const auto &i2c : i2cConfigs)
    {
        LOG_INFO(EventSource::SYSTEM, "I2C Bus %d: %luHz (owner: %s)",
                 i2c.first, i2c.second.clockSpeed, i2c.second.owner);
    }

    LOG_INFO(EventSource::SYSTEM, "=================================");
}