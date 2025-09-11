#ifndef SERIALMANAGER_H_
#define SERIALMANAGER_H_

#include "Arduino.h"

// Serial port definitions (self-contained, no pcb.h dependency)
#define SerialRadio Serial3
#define SerialGPS1 Serial5
#define SerialGPS2 Serial8
#define SerialRS232 Serial7
#define SerialESP32 Serial2

// GPS and IMU type enumerations removed - all detection moved to NAVProcessor

class SerialManager
{
private:
    static SerialManager *instance;
    bool isInitialized;

    // Private serial buffers (encapsulated, not global)
    uint8_t gps1RxBuffer[128];
    uint8_t gps1TxBuffer[256];
    uint8_t gps2RxBuffer[128];
    uint8_t gps2TxBuffer[256];
    uint8_t radioRxBuffer[64];
    uint8_t rs232TxBuffer[256];
    uint8_t esp32RxBuffer[256];
    uint8_t esp32TxBuffer[256];

    // SerialIMU - owned by SerialManager
    HardwareSerial *serialIMU;

    // Bridge mode tracking
    bool prevUSB1DTR;
    bool prevUSB2DTR;

public:
    // Buffer sizes (matching pcb.h values - using existing global buffers)
    static const uint16_t GPS_BUFFER_SIZE = 128;    // GPS1rxbuffer size from pcb.h
    static const uint16_t GPS_TX_BUFFER_SIZE = 256; // GPS1txbuffer size from pcb.h
    static const uint16_t RADIO_BUFFER_SIZE = 64;
    static const uint16_t RS232_BUFFER_SIZE = 256;
    static const uint16_t ESP32_BUFFER_SIZE = 256;

    // Baud rates (matching pcb.h values)
    static const int32_t BAUD_GPS = 460800;
    static const int32_t BAUD_RADIO = 115200;
    static const int32_t BAUD_RS232 = 115200;
    static const int32_t BAUD_ESP32 = 460800;
    static const int32_t BAUD_IMU = 115200;

    SerialManager();
    ~SerialManager();

    static SerialManager *getInstance();
    static void init();

    // Initialization
    bool initializeSerial();
    bool initializeSerialPorts();

    // Device detection handled by NAVProcessor

    // Serial processing methods - Most processing moved to specialized processors
    // Only ESP32 PGN processing remains here
    void processESP32();

    // Bridge mode management
    bool isGPS1Bridged() const;
    bool isGPS2Bridged() const;
    void handleGPS1BridgeMode();
    void handleGPS2BridgeMode();

    // Utility methods
    void clearSerialBuffers();
    void sendToRS232(uint8_t *data, uint16_t length);
    void sendToESP32(uint8_t *data, uint16_t length);

    // Baud rate getters
    int32_t getGPSBaudRate() const;
    int32_t getRadioBaudRate() const;
    int32_t getESP32BaudRate() const;
    int32_t getRS232BaudRate() const;
    int32_t getIMUBaudRate() const;

    // ESP32 PGN handling
    void processESP32PGN(uint8_t *data, uint8_t length);
    bool validatePGNHeader(uint8_t *data, uint8_t length);

    // Status and debug
    void printSerialStatus();
    void printSerialConfiguration();
    bool getInitializationStatus() const;
    bool isSerialInitialized() const;
};

// Global instance (following the same pattern as configManager)
extern SerialManager serialManager;

#endif // SERIALMANAGER_H_