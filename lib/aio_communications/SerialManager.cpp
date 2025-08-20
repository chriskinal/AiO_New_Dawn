#include "SerialManager.h"
#include "EventLogger.h"

// Static instance pointer
SerialManager *SerialManager::instance = nullptr;

SerialManager::SerialManager()
    : isInitialized(false), serialIMU(&Serial4), prevUSB1DTR(false), prevUSB2DTR(false)
{
    instance = this;
}

SerialManager::~SerialManager()
{
    instance = nullptr;
}

SerialManager *SerialManager::getInstance()
{
    return instance;
}

void SerialManager::init()
{
    if (instance == nullptr)
    {
        new SerialManager();
    }
}

bool SerialManager::initializeSerial()
{
    LOG_INFO(EventSource::SYSTEM, "Serial Manager Initialization starting");

    if (!initializeSerialPorts())
    {
        LOG_ERROR(EventSource::SYSTEM, "Serial port initialization FAILED");
        return false;
    }

    // Device detection handled by NAVProcessor

    isInitialized = true;
    LOG_INFO(EventSource::SYSTEM, "Serial initialization SUCCESS");
    return true;
}

bool SerialManager::initializeSerialPorts()
{
    LOG_DEBUG(EventSource::SYSTEM, "Initializing serial ports");

    // GPS1 Serial - use class member buffers
    SerialGPS1.begin(BAUD_GPS);
    SerialGPS1.addMemoryForRead(gps1RxBuffer, sizeof(gps1RxBuffer));
    SerialGPS1.addMemoryForWrite(gps1TxBuffer, sizeof(gps1TxBuffer));

    // GPS2 Serial - use class member buffers
    SerialGPS2.begin(BAUD_GPS);
    SerialGPS2.addMemoryForRead(gps2RxBuffer, sizeof(gps2RxBuffer));
    SerialGPS2.addMemoryForWrite(gps2TxBuffer, sizeof(gps2TxBuffer));

    // Radio Serial (for RTCM data) - use class member buffer
    SerialRadio.begin(BAUD_RADIO);
    SerialRadio.addMemoryForRead(radioRxBuffer, sizeof(radioRxBuffer));

    // RS232 Serial - use class member buffer
    SerialRS232.begin(BAUD_RS232);
    SerialRS232.addMemoryForWrite(rs232TxBuffer, sizeof(rs232TxBuffer));

    // ESP32 Serial - use class member buffers
    SerialESP32.begin(BAUD_ESP32);
    SerialESP32.addMemoryForRead(esp32RxBuffer, sizeof(esp32RxBuffer));
    SerialESP32.addMemoryForWrite(esp32TxBuffer, sizeof(esp32TxBuffer));

    // IMU Serial
    serialIMU->begin(BAUD_IMU);

    LOG_DEBUG(EventSource::SYSTEM, "SerialGPS1/GPS2: %i baud", BAUD_GPS);
    LOG_DEBUG(EventSource::SYSTEM, "SerialRadio: %i baud", BAUD_RADIO);
    LOG_DEBUG(EventSource::SYSTEM, "SerialRS232: %i baud", BAUD_RS232);
    LOG_DEBUG(EventSource::SYSTEM, "SerialESP32: %i baud", BAUD_ESP32);
    LOG_DEBUG(EventSource::SYSTEM, "SerialIMU: %i baud", BAUD_IMU);

    return true;
}

// Device detection handled by NAVProcessor

void SerialManager::processESP32()
{
    // ESP32 PGN processing
    if (SerialESP32.available())
    {
        static uint8_t incomingBytes[50];
        static uint8_t incomingIndex = 0;

        incomingBytes[incomingIndex] = SerialESP32.read();
        incomingIndex++;

        // Check for CRLF termination
        if (incomingIndex >= 2 &&
            incomingBytes[incomingIndex - 2] == 13 &&
            incomingBytes[incomingIndex - 1] == 10)
        {
            if (validatePGNHeader(incomingBytes, incomingIndex))
            {
                processESP32PGN(incomingBytes, incomingIndex - 2);
            }
            incomingIndex = 0;
        }

        // Prevent buffer overflow
        if (incomingIndex >= sizeof(incomingBytes))
        {
            incomingIndex = 0;
        }
    }
}


bool SerialManager::isGPS1Bridged() const
{
#if defined(USB_DUAL_SERIAL) || defined(USB_TRIPLE_SERIAL)
    // Return USB1DTR status when available
    return false;
#else
    return false;
#endif
}

bool SerialManager::isGPS2Bridged() const
{
#if defined(USB_TRIPLE_SERIAL)
    // Return USB2DTR status when available
    return false;
#else
    return false;
#endif
}

void SerialManager::handleGPS1BridgeMode()
{
#if defined(USB_DUAL_SERIAL) || defined(USB_TRIPLE_SERIAL)
    // Bridge GPS1 to USB1 for u-center access
    if (SerialGPS1.available())
    {
        while (SerialGPS1.available())
        {
            SerialUSB1.write(SerialGPS1.read());
        }
    }

    if (SerialUSB1.available())
    {
        while (SerialUSB1.available())
        {
            SerialGPS1.write(SerialUSB1.read());
        }
    }
#endif
}

void SerialManager::handleGPS2BridgeMode()
{
#if defined(USB_TRIPLE_SERIAL)
    // Bridge GPS2 to USB2 for u-center access
    if (SerialGPS2.available())
    {
        while (SerialGPS2.available())
        {
            SerialUSB2.write(SerialGPS2.read());
        }
    }

    if (SerialUSB2.available())
    {
        while (SerialUSB2.available())
        {
            SerialGPS2.write(SerialUSB2.read());
        }
    }
#endif
}

void SerialManager::clearSerialBuffers()
{
    SerialGPS1.clear();
    SerialGPS2.clear();
    SerialESP32.clear();
}

void SerialManager::sendToRS232(uint8_t *data, uint16_t length)
{
    SerialRS232.write(data, length);
}

void SerialManager::sendToESP32(uint8_t *data, uint16_t length)
{
    SerialESP32.write(data, length);
}

void SerialManager::processESP32PGN(uint8_t *data, uint8_t length)
{
    // ESP32 PGN processing - placeholder for network forwarding
    // This will send PGN data to AgIO via UDP when network is available
    LOG_DEBUG(EventSource::NETWORK, "ESP32 PGN received, length: %d", length);
}

bool SerialManager::validatePGNHeader(uint8_t *data, uint8_t length)
{
    if (length < 2)
        return false;

    return (data[0] == 128 && data[1] == 129);
}

int32_t SerialManager::getGPSBaudRate() const
{
    return BAUD_GPS;
}

int32_t SerialManager::getRadioBaudRate() const
{
    return BAUD_RADIO;
}

int32_t SerialManager::getESP32BaudRate() const
{
    return BAUD_ESP32;
}

int32_t SerialManager::getRS232BaudRate() const
{
    return BAUD_RS232;
}

int32_t SerialManager::getIMUBaudRate() const
{
    return BAUD_IMU;
}

void SerialManager::printSerialStatus()
{
    LOG_INFO(EventSource::SYSTEM, "=== Serial Manager Status ===");
    LOG_INFO(EventSource::SYSTEM, "Initialized: %s", isInitialized ? "YES" : "NO");
    LOG_INFO(EventSource::SYSTEM, "GPS1 Bridged: %s", isGPS1Bridged() ? "YES" : "NO");
    LOG_INFO(EventSource::SYSTEM, "GPS2 Bridged: %s", isGPS2Bridged() ? "YES" : "NO");

    // Device detection handled by NAVProcessor

    printSerialConfiguration();
    LOG_INFO(EventSource::SYSTEM, "=============================");
}

void SerialManager::printSerialConfiguration()
{
    LOG_INFO(EventSource::SYSTEM, "--- Serial Configuration ---");
    LOG_INFO(EventSource::SYSTEM, "SerialGPS1 (Serial5): %i baud", BAUD_GPS);
    LOG_INFO(EventSource::SYSTEM, "SerialGPS2 (Serial8): %i baud", BAUD_GPS);
    LOG_INFO(EventSource::SYSTEM, "SerialRadio (Serial3): %i baud", BAUD_RADIO);
    LOG_INFO(EventSource::SYSTEM, "SerialRS232 (Serial7): %i baud", BAUD_RS232);
    LOG_INFO(EventSource::SYSTEM, "SerialESP32 (Serial2): %i baud", BAUD_ESP32);
    LOG_INFO(EventSource::SYSTEM, "SerialIMU (Serial4): %i baud", BAUD_IMU);
}

bool SerialManager::getInitializationStatus() const
{
    return isInitialized;
}

bool SerialManager::isSerialInitialized() const
{
    return isInitialized;
}