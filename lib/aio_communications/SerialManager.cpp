#include "SerialManager.h"
#include "EventLogger.h"
#include "ConfigManager.h"

// Static instance pointer
SerialManager *SerialManager::instance = nullptr;

SerialManager::SerialManager()
    : isInitialized(false), serialIMU(&SerialIMU), prevUSB1DTR(false), prevUSB2DTR(false)
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
    // Get baud rate from configuration
    ConfigManager* config = ConfigManager::getInstance();
    uint32_t radioBaud = config->getSerialRadioBaudRate();
    if (radioBaud == 0 || radioBaud == 0xFFFFFFFF) {
        radioBaud = BAUD_RADIO; // Use default if not configured
    }
    SerialRadio.begin(radioBaud);
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
    LOG_DEBUG(EventSource::SYSTEM, "SerialRadio: %lu baud", radioBaud);
    LOG_DEBUG(EventSource::SYSTEM, "SerialRS232: %i baud", BAUD_RS232);
    LOG_DEBUG(EventSource::SYSTEM, "SerialESP32: %i baud", BAUD_ESP32);
    LOG_DEBUG(EventSource::SYSTEM, "SerialIMU: %i baud", BAUD_IMU);

    return true;
}

// Device detection handled by NAVProcessor

void SerialManager::processESP32()
{
    // DEPRECATED - ESP32 processing now handled by ESP32Interface
    // This method kept for compatibility but does nothing
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

void SerialManager::updateRadioBaudRate(uint32_t newBaudRate)
{
    // Validate baud rate
    if (newBaudRate < 4800 || newBaudRate > 921600) {
        LOG_ERROR(EventSource::SYSTEM, "Invalid radio baud rate: %lu", newBaudRate);
        return;
    }

    LOG_INFO(EventSource::SYSTEM, "Updating radio baud rate to %lu", newBaudRate);

    // Close and reopen the serial port with new baud rate
    SerialRadio.end();
    delay(10); // Brief delay to ensure port closes properly
    SerialRadio.begin(newBaudRate);
    SerialRadio.addMemoryForRead(radioRxBuffer, sizeof(radioRxBuffer));

    LOG_INFO(EventSource::SYSTEM, "Radio baud rate updated successfully");
}

void SerialManager::startBufferMonitoring()
{
    LOG_INFO(EventSource::SYSTEM, "Starting serial buffer monitoring");
    monitoringEnabled = true;

    // Reset all stats
    gps1RxStats = {0, 0, millis()};
    gps2RxStats = {0, 0, millis()};
    radioRxStats = {0, 0, millis()};
    esp32RxStats = {0, 0, millis()};
    imuRxStats = {0, 0, millis()};
}

void SerialManager::updateBufferStats()
{
    if (!monitoringEnabled) return;

    // Check GPS1 RX buffer
    size_t gps1Avail = SerialGPS1.available();
    if (gps1Avail > gps1RxStats.peakUsage) {
        gps1RxStats.peakUsage = gps1Avail;
    }
    // Check for near-overflow (>75% of 128 byte custom buffer)
    if (gps1Avail > 96) {
        gps1RxStats.overflowCount++;
    }

    // Check GPS2 RX buffer
    size_t gps2Avail = SerialGPS2.available();
    if (gps2Avail > gps2RxStats.peakUsage) {
        gps2RxStats.peakUsage = gps2Avail;
    }
    if (gps2Avail > 96) {
        gps2RxStats.overflowCount++;
    }

    // Check Radio RX buffer
    size_t radioAvail = SerialRadio.available();
    if (radioAvail > radioRxStats.peakUsage) {
        radioRxStats.peakUsage = radioAvail;
    }
    // Radio buffer is only 64 bytes
    if (radioAvail > 48) {
        radioRxStats.overflowCount++;
    }

    // Check ESP32 RX buffer
    size_t esp32Avail = SerialESP32.available();
    if (esp32Avail > esp32RxStats.peakUsage) {
        esp32RxStats.peakUsage = esp32Avail;
    }
    if (esp32Avail > 192) {
        esp32RxStats.overflowCount++;
    }

    // Check IMU RX buffer
    size_t imuAvail = serialIMU->available();
    if (imuAvail > imuRxStats.peakUsage) {
        imuRxStats.peakUsage = imuAvail;
    }
}

void SerialManager::printBufferUsage()
{
    if (!monitoringEnabled) {
        LOG_INFO(EventSource::SYSTEM, "Buffer monitoring not started. Call startBufferMonitoring() first.");
        return;
    }

    uint32_t uptime = (millis() - gps1RxStats.lastCheckTime) / 1000;

    LOG_INFO(EventSource::SYSTEM, "=== Serial Buffer Usage (uptime: %lu sec) ===", uptime);

    // GPS1 - custom 128 byte RX buffer + core default
    LOG_INFO(EventSource::SYSTEM, "GPS1 RX: Peak=%u bytes (custom buf=128), overflows=%u",
             gps1RxStats.peakUsage, gps1RxStats.overflowCount);

    // GPS2 - custom 128 byte RX buffer + core default
    LOG_INFO(EventSource::SYSTEM, "GPS2 RX: Peak=%u bytes (custom buf=128), overflows=%u",
             gps2RxStats.peakUsage, gps2RxStats.overflowCount);

    // Radio - custom 64 byte RX buffer + core default
    LOG_INFO(EventSource::SYSTEM, "Radio RX: Peak=%u bytes (custom buf=64), overflows=%u",
             radioRxStats.peakUsage, radioRxStats.overflowCount);

    // ESP32 - custom 256 byte RX buffer + core default
    LOG_INFO(EventSource::SYSTEM, "ESP32 RX: Peak=%u bytes (custom buf=256), overflows=%u",
             esp32RxStats.peakUsage, esp32RxStats.overflowCount);

    // IMU - core default buffer only
    LOG_INFO(EventSource::SYSTEM, "IMU RX: Peak=%u bytes (core buf only), overflows=%u",
             imuRxStats.peakUsage, imuRxStats.overflowCount);

    LOG_INFO(EventSource::SYSTEM, "============================================");

    // Recommendations
    if (gps1RxStats.peakUsage > 128 || gps2RxStats.peakUsage > 128) {
        LOG_WARNING(EventSource::SYSTEM, "GPS buffer peak exceeds custom 128 bytes - using core buffers!");
    }
    if (radioRxStats.peakUsage > 64) {
        LOG_WARNING(EventSource::SYSTEM, "Radio buffer peak exceeds custom 64 bytes - using core buffers!");
    }
    if (esp32RxStats.peakUsage > 256) {
        LOG_WARNING(EventSource::SYSTEM, "ESP32 buffer peak exceeds custom 256 bytes - using core buffers!");
    }

    if (gps1RxStats.peakUsage <= 128 && gps2RxStats.peakUsage <= 128 &&
        radioRxStats.peakUsage <= 64 && esp32RxStats.peakUsage <= 256) {
        LOG_INFO(EventSource::SYSTEM, "All buffers within custom sizes - core buffers may be reducible!");
    }
}