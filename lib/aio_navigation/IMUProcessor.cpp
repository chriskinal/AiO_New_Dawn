#include "IMUProcessor.h"
#include "TM171AiOParser.h"
#include "PGNUtils.h"
#include "EventLogger.h"
#include "QNetworkBase.h"
#include "ConfigManager.h"
#include "SerialManager.h"


// Static instance pointer
IMUProcessor *IMUProcessor::instance = nullptr;

IMUProcessor::IMUProcessor()
    : serialMgr(nullptr), detectedType(IMUType::NONE), isInitialized(false),
      bnoParser(nullptr), imuSerial(&SerialIMU), tm171Parser(nullptr),
      ism330bxProcessor(nullptr), timeSinceLastPacket(0)
{
    instance = this;

    // Initialize current data
    currentData = {0, 0, 0, 0, 0, 0, false};
}

IMUProcessor::~IMUProcessor()
{
    if (bnoParser)
    {
        delete bnoParser;
        bnoParser = nullptr;
    }
    if (tm171Parser)
    {
        delete tm171Parser;
        tm171Parser = nullptr;
    }
    if (ism330bxProcessor)
    {
        delete ism330bxProcessor;
        ism330bxProcessor = nullptr;
    }
    instance = nullptr;
}

IMUProcessor *IMUProcessor::getInstance()
{
    if (instance == nullptr) {
        instance = new IMUProcessor();
    }
    return instance;
}

void IMUProcessor::init()
{
    if (instance == nullptr)
    {
        new IMUProcessor();
    }
}

bool IMUProcessor::initialize()
{
    LOG_INFO(EventSource::IMU, "IMU Processor Initialization starting");

    // Get SerialManager instance
    serialMgr = SerialManager::getInstance();
    if (!serialMgr)
    {
        LOG_ERROR(EventSource::IMU, "SerialManager not available");
        return false;
    }

    // Try to detect IMU type by attempting initialization
    LOG_INFO(EventSource::IMU, "Detecting IMU type...");

    // Try ISM330BX first (I2C-based, fast to detect)
    if (initISM330BX()) {
        detectedType = IMUType::ISM330BX;
        isInitialized = true;
        LOG_INFO(EventSource::IMU, "ISM330BX detected");
        return true;
    }

    // Try BNO085
    if (initBNO085()) {
        detectedType = IMUType::BNO085;
        isInitialized = true;
        LOG_INFO(EventSource::IMU, "BNO085 detected");
        return true;
    }

    // Try TM171
    if (initTM171()) {
        detectedType = IMUType::TM171;
        isInitialized = true;
        LOG_INFO(EventSource::IMU, "TM171 detected");
        return true;
    }

    // No IMU detected
    detectedType = IMUType::NONE;
    isInitialized = false;
    LOG_WARNING(EventSource::IMU, "No IMU detected");
    return false;
}

bool IMUProcessor::initBNO085()
{
    LOG_DEBUG(EventSource::IMU, "Initializing BNO085 RVC mode");

    // Initialize serial port for BNO085 RVC mode
    imuSerial->begin(115200);  // BNO085 RVC uses 115200 baud
    
    // Create parser
    bnoParser = new BNOAiOParser();
    
    // Clear any existing data in serial buffer
    while (imuSerial->available())
    {
        imuSerial->read();
    }
    
    // Wait for valid data to confirm BNO is present
    uint32_t startTime = millis();
    
    while (millis() - startTime < 100)  // Wait up to 100ms
    {
        while (imuSerial->available())
        {
            uint8_t byte = imuSerial->read();
            bnoParser->processByte(byte);
            
            if (bnoParser->isDataValid())
            {
                LOG_INFO(EventSource::IMU, "BNO085 communication established");
                LOG_DEBUG(EventSource::IMU, "Initial data: Yaw=%.1f, Pitch=%.1f, Roll=%.1f",
                          bnoParser->getYaw(), bnoParser->getPitch(), bnoParser->getRoll());
                return true;
            }
        }
        delay(5);
    }

    // No valid data received
    delete bnoParser;
    bnoParser = nullptr;
    return false;
}

bool IMUProcessor::initTM171()
{
    LOG_DEBUG(EventSource::IMU, "Initializing TM171");

    // Create TM171 parser
    tm171Parser = new TM171AiOParser();
    LOG_DEBUG(EventSource::IMU, "TM171 AiO parser created");

    // Make sure serial port is properly initialized
    if (!imuSerial)
    {
        LOG_ERROR(EventSource::IMU, "IMU serial port is NULL!");
        delete tm171Parser;
        tm171Parser = nullptr;
        return false;
    }

    // Clear serial buffer
    while (imuSerial->available())
    {
        imuSerial->read();
    }

    // Wait for valid TM171 data
    LOG_DEBUG(EventSource::IMU, "Waiting for TM171 data...");
    uint32_t startTime = millis();
    
    while (millis() - startTime < 500) {  // Wait up to 500ms
        while (imuSerial->available()) {
            uint8_t byte = imuSerial->read();
            tm171Parser->processByte(byte);
            
            if (tm171Parser->isDataValid()) {
                LOG_INFO(EventSource::IMU, "TM171 valid data detected!");
                LOG_DEBUG(EventSource::IMU, "Initial data: Yaw=%.1f, Pitch=%.1f, Roll=%.1f",
                          tm171Parser->getYaw(), tm171Parser->getPitch(), tm171Parser->getRoll());
                return true;
            }
        }
        delay(10);
    }
    
    // No valid TM171 data received
    LOG_DEBUG(EventSource::IMU, "No valid TM171 data received");
    delete tm171Parser;
    tm171Parser = nullptr;
    return false;
}

void IMUProcessor::process()
{
    // Legacy method - calls appropriate process method based on IMU type
    // Kept for backward compatibility
    if (detectedType == IMUType::ISM330BX) {
        processI2CIMU();
    } else {
        processSerialIMU();
    }
}

void IMUProcessor::processSerialIMU()
{
    // Process serial-based IMUs (BNO085, TM171)
    // Should be called from EVERY_LOOP to process incoming serial bytes

    // If no IMU detected, still check for serial data to detect invalid data
    if (!isInitialized && serialMgr && imuSerial)
    {
        // Check if any data is coming in on the IMU serial port
        if (imuSerial->available())
        {
            // Read and discard the data, but mark that we received something
            while (imuSerial->available())
            {
                imuSerial->read();
                serialDataReceived = true;
                lastSerialDataTime = millis();
            }
        }
        return;
    }

    if (!isInitialized)
        return;

    switch (detectedType)
    {
    case IMUType::BNO085:
        processBNO085Data();
        break;

    case IMUType::TM171:
        processTM171Data();
        break;

    default:
        // Not a serial IMU - nothing to do
        break;
    }
}

void IMUProcessor::processI2CIMU()
{
    // Process I2C-based IMUs (ISM330BX)
    // Should be called from a timed scheduler group (e.g., 50Hz)
    // No need for rate limiting here - scheduler controls the rate

    if (!isInitialized)
        return;

    if (detectedType == IMUType::ISM330BX) {
        processISM330BXData();
    }
}

void IMUProcessor::processBNO085Data()
{
    if (!bnoParser)
        return;

    // Process all available bytes
    while (imuSerial->available())
    {
        uint8_t byte = imuSerial->read();
        serialDataReceived = true;
        lastSerialDataTime = millis();
        bnoParser->processByte(byte);
    }

    // Update current data if valid
    if (bnoParser->isDataValid())
    {
        extern ConfigManager configManager;

        // Update current data
        currentData.heading = bnoParser->getYaw();

        // Apply scaling (x10) and Y-axis swap if configured
        // Y-axis swap is needed for some mounting orientations
        if (configManager.getIsUseYAxis()) {
            // Swap pitch and roll axes
            currentData.pitch = 10.0f * bnoParser->getRoll();
            currentData.roll = 10.0f * bnoParser->getPitch();
        } else {
            // Normal orientation
            currentData.pitch = 10.0f * bnoParser->getPitch();
            currentData.roll = 10.0f * bnoParser->getRoll();
        }

        currentData.yawRate = bnoParser->getYawRate();
        currentData.quality = bnoParser->isActive() ? 10 : 0;
        currentData.timestamp = millis();
        currentData.isValid = true;

        // Update statistics
        timeSinceLastPacket = 0;

        // Periodic DEBUG level logging (every 500ms for dynamic monitoring)
        static uint32_t lastDebugLog = 0;
        if (millis() - lastDebugLog > 500) {
            lastDebugLog = millis();
            LOG_DEBUG(EventSource::IMU, "BNO085: Heading=%.1f° Roll=%.1f° Pitch=%.1f° YawRate=%.1f°/s Quality=%d/10",
                     currentData.heading, currentData.roll, currentData.pitch, currentData.yawRate, currentData.quality);
        }
    }
    else if (bnoParser->getTimeSinceLastValid() > 100)
    {
        // Mark data as invalid if no recent updates
        currentData.isValid = false;
        currentData.quality = 0;
    }
}

void IMUProcessor::processTM171Data()
{
    if (!tm171Parser)
        return;

    while (imuSerial->available())
    {
        uint8_t byte = imuSerial->read();
        serialDataReceived = true;
        lastSerialDataTime = millis();
        tm171Parser->processByte(byte);

        // Check if we have new valid data
        if (tm171Parser->isDataValid())
        {
            // Update current data structure
            currentData.heading = tm171Parser->getYaw();
            currentData.pitch = tm171Parser->getPitch();
            currentData.roll = tm171Parser->getRoll();
            currentData.yawRate = 0;  // TM171 doesn't provide yaw rate
            currentData.quality = 10; // Assume good quality if data is valid
            currentData.timestamp = millis();
            currentData.isValid = true;

            // Update statistics from parser
            // TM171 packet received
            timeSinceLastPacket = 0;

            // Periodic DEBUG level logging (every 500ms for dynamic monitoring)
            static uint32_t lastDebugLog = 0;
            if (millis() - lastDebugLog > 500) {
                lastDebugLog = millis();
                LOG_DEBUG(EventSource::IMU, "TM171: Heading=%.1f° Roll=%.1f° Pitch=%.1f° YawRate=%.1f°/s Quality=%d/10",
                         currentData.heading, currentData.roll, currentData.pitch, currentData.yawRate, currentData.quality);
            }
        }
    }

    // Update time since last packet
    if (tm171Parser->getTimeSinceLastValid() > 100)
    {
        currentData.isValid = false;
        currentData.quality = 0;
    }
}

const char *IMUProcessor::getIMUTypeName() const
{
    switch (detectedType) {
        case IMUType::ISM330BX:
            return "ISM330BX";
        case IMUType::BNO085:
            return "BNO085";
        case IMUType::TM171:
            return "TM171";
        case IMUType::UM981_INTEGRATED:
            return "UM981 Integrated";
        case IMUType::CMPS14:
            return "CMPS14";
        case IMUType::GENERIC:
            return "Generic";
        default:
            return "None";
    }
}

void IMUProcessor::printStatus()
{
    LOG_INFO(EventSource::IMU, "=== IMU Processor Status ===");
    LOG_INFO(EventSource::IMU, "IMU Type: %s", getIMUTypeName());
    LOG_INFO(EventSource::IMU, "Initialized: %s", isInitialized ? "YES" : "NO");
    LOG_INFO(EventSource::IMU, "Active: %s", isActive() ? "YES" : "NO");
    LOG_INFO(EventSource::IMU, "Time since last packet: %lu ms", (uint32_t)timeSinceLastPacket);
    LOG_INFO(EventSource::IMU, "Time since last packet: %lu ms", (uint32_t)timeSinceLastPacket);

    if (currentData.isValid)
    {
        LOG_INFO(EventSource::IMU, "Current Data:");
        LOG_INFO(EventSource::IMU, "  Heading: %.1f°", currentData.heading);
        LOG_INFO(EventSource::IMU, "  Roll: %.1f°", currentData.roll);
        LOG_INFO(EventSource::IMU, "  Pitch: %.1f°", currentData.pitch);
        LOG_INFO(EventSource::IMU, "  Yaw Rate: %.1f°/s", currentData.yawRate);
        LOG_INFO(EventSource::IMU, "  Quality: %u", currentData.quality);
    }
    else
    {
        LOG_INFO(EventSource::IMU, "No valid data");
    }

    // If TM171, print parser debug info
    if (detectedType == IMUType::TM171 && tm171Parser)
    {
        tm171Parser->printStats();
    }
}

void IMUProcessor::printCurrentData()
{
    if (currentData.isValid)
    {
        LOG_INFO(EventSource::IMU, "%lu IMU: H=%.1f° R=%.1f° P=%.1f° YR=%.1f°/s Q=%u",
                 currentData.timestamp, currentData.heading, currentData.roll,
                 currentData.pitch, currentData.yawRate, currentData.quality);
    }
}

// PGN Support Implementation

// External reference to NetworkBase send function
extern void sendUDPbytes(uint8_t *message, int msgLen);

// Get ConfigManager instance
extern ConfigManager configManager;

void IMUProcessor::registerPGNCallbacks()
{
    // Only register PGN callbacks if we actually have an IMU detected
    if (detectedType == IMUType::NONE) {
        LOG_DEBUG(EventSource::IMU, "No IMU detected - skipping PGN registration");
        return;
    }
    
    LOG_DEBUG(EventSource::IMU, "Attempting to register PGN callbacks...");
    
    // Get PGNProcessor instance and register for IMU messages
    PGNProcessor* pgnProcessor = PGNProcessor::instance;
    if (pgnProcessor)
    {
        // Register for broadcast PGNs (200 and 202)
        bool success = pgnProcessor->registerBroadcastCallback(handleBroadcastPGN, "IMU Handler");
        LOG_DEBUG(EventSource::IMU, "Broadcast registration %s", success ? "SUCCESS" : "FAILED");
    }
    else
    {
        LOG_ERROR(EventSource::IMU, "PGNProcessor instance is NULL!");
    }
}

// Static callback for broadcast PGNs (Hello and Scan Request)
void IMUProcessor::handleBroadcastPGN(uint8_t pgn, const uint8_t* data, size_t len)
{
    // Check if this is a Hello PGN
    if (pgn == 200)
    {
        // When we receive a Hello from AgIO, we should respond
        // Using the same format as V6-NG
        // IMU Hello reply from V6-NG: {128, 129, 121, 121, 5, 0, 0, 0, 0, 0, 71}
        uint8_t helloFromIMU[] = {128, 129, 121, 121, 5, 0, 0, 0, 0, 0, 71};
        
        // Send the reply
        sendUDPbytes(helloFromIMU, sizeof(helloFromIMU));
    }
    // Check if this is a Scan Request PGN
    else if (pgn == 202)
    {
        
        // Subnet IMU reply format from PGN.md:
        // Src: 0x79 (121), PGN: 0xCB (203), Len: 7
        // IP_One, IP_Two, IP_Three, IP_Four, Subnet_One, Subnet_Two, Subnet_Three
        uint8_t ip[4];
        configManager.getIPAddress(ip);
        
        uint8_t subnetReply[] = {
            0x80, 0x81,              // PGN header
            IMU_SOURCE_ID,           // Source: 0x79 (121)
            0xCB,                    // PGN: 203
            7,                       // Data length
            ip[0],  // IP_One
            ip[1],  // IP_Two
            ip[2],  // IP_Three
            ip[3],  // IP_Four
            ip[0],  // Subnet_One
            ip[1],  // Subnet_Two  
            ip[2],  // Subnet_Three
            0                        // CRC placeholder
        };
        
        // Calculate and set CRC
        calculateAndSetCRC(subnetReply, sizeof(subnetReply));
        
        // Send the reply
        sendUDPbytes(subnetReply, sizeof(subnetReply));
        LOG_DEBUG(EventSource::IMU, "Scan reply sent: %d.%d.%d.%d / Subnet: %d.%d.%d", 
                  ip[0], ip[1], ip[2], ip[3],
                  ip[0], ip[1], ip[2]);
    }
}

void IMUProcessor::sendIMUData()
{
    if (!currentData.isValid)
        return;
        
    // PGN 211 (0xD3) format:
    // 0x80, 0x81, 0x79, 0xD3, 8, Heading_Lo, Heading_Hi, Roll_Lo, Roll_Hi, Gyro_Lo, Gyro_Hi, 0, 0, CRC
    
    // Convert float values to int16 (*10 for one decimal place precision)
    int16_t headingX10 = (int16_t)(currentData.heading * 10);
    int16_t rollX10 = (int16_t)(currentData.roll * 10);
    int16_t gyroX10 = (int16_t)(currentData.yawRate * 10);  // yaw rate as gyro
    
    uint8_t imuData[] = {
        0x80, 0x81,                     // PGN header
        IMU_SOURCE_ID,                  // Source: 0x79 (121)
        IMU_PGN_DATA,                   // PGN: 0xD3 (211)
        8,                              // Data length
        (uint8_t)(headingX10 & 0xFF),   // Heading low byte
        (uint8_t)(headingX10 >> 8),     // Heading high byte
        (uint8_t)(rollX10 & 0xFF),      // Roll low byte
        (uint8_t)(rollX10 >> 8),        // Roll high byte
        (uint8_t)(gyroX10 & 0xFF),      // Gyro low byte
        (uint8_t)(gyroX10 >> 8),        // Gyro high byte
        0,                              // Reserved
        0,                              // Reserved
        0                               // CRC placeholder
    };
    
    // Calculate and set CRC
    calculateAndSetCRC(imuData, sizeof(imuData));

    // Send the data
    sendUDPbytes(imuData, sizeof(imuData));
}

bool IMUProcessor::initISM330BX()
{
    LOG_DEBUG(EventSource::IMU, "Initializing ISM330BX");

    // Create ISM330BX processor
    ism330bxProcessor = new ISM330BXProcessor();

    // Try to initialize on Wire (I2C0) first, then Wire1, then Wire2
    if (ism330bxProcessor->begin(&Wire, ISM330BX_ADDRESS_PRIMARY)) {
        LOG_INFO(EventSource::IMU, "ISM330BX initialized on Wire at 0x6A");
        return true;
    }

    if (ism330bxProcessor->begin(&Wire, ISM330BX_ADDRESS_SECONDARY)) {
        LOG_INFO(EventSource::IMU, "ISM330BX initialized on Wire at 0x6B");
        return true;
    }

    if (ism330bxProcessor->begin(&Wire1, ISM330BX_ADDRESS_PRIMARY)) {
        LOG_INFO(EventSource::IMU, "ISM330BX initialized on Wire1 at 0x6A");
        return true;
    }

    if (ism330bxProcessor->begin(&Wire1, ISM330BX_ADDRESS_SECONDARY)) {
        LOG_INFO(EventSource::IMU, "ISM330BX initialized on Wire1 at 0x6B");
        return true;
    }

    if (ism330bxProcessor->begin(&Wire2, ISM330BX_ADDRESS_PRIMARY)) {
        LOG_INFO(EventSource::IMU, "ISM330BX initialized on Wire2 at 0x6A");
        return true;
    }

    if (ism330bxProcessor->begin(&Wire2, ISM330BX_ADDRESS_SECONDARY)) {
        LOG_INFO(EventSource::IMU, "ISM330BX initialized on Wire2 at 0x6B");
        return true;
    }

    // No ISM330BX found
    delete ism330bxProcessor;
    ism330bxProcessor = nullptr;
    LOG_DEBUG(EventSource::IMU, "ISM330BX not found on any I2C bus");
    return false;
}

void IMUProcessor::processISM330BXData()
{
    if (!ism330bxProcessor)
        return;

    // Update sensor data
    ism330bxProcessor->update();

    // Get IMU data structure
    IMUData data = ism330bxProcessor->getIMUData();

    // Update current data
    currentData.heading = data.heading;
    currentData.roll = data.roll;
    currentData.pitch = data.pitch;
    currentData.yawRate = data.yawRate;
    currentData.quality = data.quality;
    currentData.timestamp = data.timestamp;
    currentData.isValid = data.isValid;

    // Update statistics
    if (data.isValid) {
        timeSinceLastPacket = 0;
    }

    // Periodic DEBUG level logging for IMU data (every 500ms for dynamic monitoring)
    static uint32_t lastDebugLog = 0;
    if (millis() - lastDebugLog > 500) {
        lastDebugLog = millis();
        if (data.isValid) {
            LOG_DEBUG(EventSource::IMU, "ISM330BX: Heading=%.1f° Roll=%.1f° Pitch=%.1f° YawRate=%.1f°/s Quality=%d/10",
                     data.heading, data.roll, data.pitch, data.yawRate, data.quality);
        }
    }

    // INFO level logging for MLC state changes
    static MLCState lastLoggedState = MLCState::UNKNOWN;
    MLCState currentState = ism330bxProcessor->getMLCState();
    if (currentState != lastLoggedState) {
        LOG_INFO(EventSource::IMU, "MLC State changed: %s (Quality: %d/10)",
                  ism330bxProcessor->getMLCStateString(), currentData.quality);
        lastLoggedState = currentState;
    }
}