# Unified CAN Driver Design

## Configuration Structure

```cpp
// Store in EEPROM with other device settings
struct CANSteerConfig {
    // Brand selection
    uint8_t brand = 0;          // 0=Disabled, 1=Keya, 2=Fendt, 3=Valtra, etc.

    // Bus assignments (0=None, 1=CAN1/K_Bus, 2=CAN2/ISO_Bus, 3=CAN3/V_Bus)
    uint8_t steerBus = 0;       // Which bus for steering commands
    uint8_t buttonBus = 0;      // Which bus for work switch/buttons
    uint8_t hitchBus = 0;       // Which bus for hitch control

    // Brand-specific settings
    uint8_t moduleID = 0x1C;    // Some brands need module ID
    uint8_t reserved[3];        // Future expansion
};
```

## TractorCANDriver Class Design

```cpp
class TractorCANDriver : public MotorDriverInterface {
private:
    // Configuration
    CANSteerConfig config;

    // CAN bus pointers (set based on config)
    FlexCAN_T4_Base* steerCAN = nullptr;
    FlexCAN_T4_Base* buttonCAN = nullptr;
    FlexCAN_T4_Base* hitchCAN = nullptr;

    // Common state
    bool steerReady = false;
    uint32_t lastSteerReadyTime = 0;

    // Brand-specific handlers
    void processKeyaMessages(const CAN_message_t& msg, uint8_t bus);
    void processFendtMessages(const CAN_message_t& msg, uint8_t bus);
    // etc...

    void sendKeyaSteerCommand(int16_t pwm);
    void sendFendtSteerCommand(int16_t pwm);
    // etc...
};
```

## Web Configuration Page

```html
<h3>CAN Steering Configuration</h3>

<label>Brand/Model:</label>
<select id="canBrand">
    <option value="0">Disabled</option>
    <option value="1">Keya Motor</option>
    <option value="2">Fendt (SCR/S4/Gen6)</option>
    <option value="3">Valtra/Massey</option>
    <option value="4">Case IH/New Holland</option>
    <option value="5">Fendt One</option>
    <option value="6">Claas</option>
    <option value="7">JCB</option>
</select>

<label>Steering Commands Bus:</label>
<select id="steerBus">
    <option value="0">None (Buttons Only)</option>
    <option value="1">CAN1 (K-Bus)</option>
    <option value="2">CAN2 (ISO-Bus)</option>
    <option value="3">CAN3 (V-Bus)</option>
</select>

<label>Work Switch/Buttons Bus:</label>
<select id="buttonBus">
    <option value="0">None</option>
    <option value="1">CAN1 (K-Bus)</option>
    <option value="2">CAN2 (ISO-Bus)</option>
    <option value="3">CAN3 (V-Bus)</option>
</select>

<label>Hitch Control Bus:</label>
<select id="hitchBus">
    <option value="0">None</option>
    <option value="1">CAN1 (K-Bus)</option>
    <option value="2">CAN2 (ISO-Bus)</option>
    <option value="3">CAN3 (V-Bus)</option>
</select>
```

## Motor Type Detection

In MotorDriverManager::detectMotorType():
```cpp
// Check if CAN steering is configured
CANSteerConfig canConfig = configManager.getCANSteerConfig();
if (canConfig.brand > 0) {  // Not disabled
    return MotorDriverType::TRACTOR_CAN;
}
```

## Key Advantages

1. **Single Motor Type**: All CAN steering appears as TRACTOR_CAN
2. **Flexible Configuration**: User can set up any bus arrangement
3. **Buttons-Only Mode**: steerBus=None allows partial implementations
4. **Future Proof**: Reserved bytes for expansion
5. **Clean Migration**: Existing Keya users just select Keya brand + V_Bus