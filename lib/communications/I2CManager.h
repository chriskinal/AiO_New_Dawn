// I2CManager.h - Manages I2C bus initialization and device detection
#ifndef I2C_MANAGER_H
#define I2C_MANAGER_H

#include <Arduino.h>
#include <Wire.h>

// Common I2C device addresses
#define BNO08X_DEFAULT_ADDRESS  0x4A  // BNO08x IMU default address
#define BNO08X_ALT_ADDRESS      0x4B  // BNO08x IMU alternate address
#define CMPS14_ADDRESS          0x60  // CMPS14 compass
#define ADS1115_ADDRESS_GND     0x48  // ADS1115 ADC with ADDR->GND
#define ADS1115_ADDRESS_VDD     0x49  // ADS1115 ADC with ADDR->VDD
#define ADS1115_ADDRESS_SDA     0x4A  // ADS1115 ADC with ADDR->SDA
#define ADS1115_ADDRESS_SCL     0x4B  // ADS1115 ADC with ADDR->SCL
#define MCP23017_ADDRESS        0x20  // MCP23017 I/O expander base address

// I2C speeds
#define I2C_SPEED_STANDARD      100000  // 100 kHz
#define I2C_SPEED_FAST          400000  // 400 kHz
#define I2C_SPEED_FAST_PLUS     1000000 // 1 MHz

// Device types that can be detected
enum class I2CDeviceType {
    UNKNOWN,
    BNO08X,
    CMPS14,
    ADS1115,
    MCP23017,
    GENERIC
};

class I2CManager {
private:
    // Detected devices on each bus
    struct I2CBusInfo {
        bool initialized;
        uint32_t speed;
        uint8_t deviceCount;
        uint8_t deviceAddresses[128];  // Track which addresses have devices
    };
    
    I2CBusInfo wire0Info;
    I2CBusInfo wire1Info;
    I2CBusInfo wire2Info;
    
    // Device detection
    bool scanBus(TwoWire& wire, I2CBusInfo& busInfo);
    I2CDeviceType identifyDevice(TwoWire& wire, uint8_t address);
    const char* getDeviceTypeName(I2CDeviceType type);
    
public:
    I2CManager();
    ~I2CManager() = default;
    
    // Initialize I2C buses
    bool initializeI2C();
    bool initializeBus(TwoWire& wire, uint32_t speed = I2C_SPEED_FAST);
    
    // Device detection
    bool detectDevices();
    bool isDevicePresent(TwoWire& wire, uint8_t address);
    I2CDeviceType getDeviceType(TwoWire& wire, uint8_t address);
    
    // Bus management
    bool setBusSpeed(TwoWire& wire, uint32_t speed);
    bool resetBus(TwoWire& wire);
    
    // Status and debugging
    void printI2CStatus();
    void printBusStatus(TwoWire& wire, const char* busName);
    uint8_t getDeviceCount(TwoWire& wire);
    
    // Getters for bus info
    bool isWire0Initialized() const { return wire0Info.initialized; }
    bool isWire1Initialized() const { return wire1Info.initialized; }
    bool isWire2Initialized() const { return wire2Info.initialized; }
};

// Global pointer following established pattern
extern I2CManager* i2cPTR;

#endif // I2C_MANAGER_H