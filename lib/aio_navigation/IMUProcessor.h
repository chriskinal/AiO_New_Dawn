#ifndef IMUPROCESSOR_H_
#define IMUPROCESSOR_H_

#include "Arduino.h"
#include "SerialManager.h"
#include "BNOAiOParser.h"
#include "TM171AiOParser.h"
#include "ISM330BXProcessor.h"
#include "elapsedMillis.h"
#include "PGNProcessor.h"
#include "NavigationTypes.h"

// PGN Constants for IMU module
constexpr uint8_t IMU_SOURCE_ID = 0x79;     // 121 decimal - IMU source address
constexpr uint8_t IMU_PGN_DATA = 0xD3;      // 211 decimal - IMU data PGN
constexpr uint8_t IMU_HELLO_REPLY = 0x79;   // 121 decimal - IMU hello reply

// IMU Processor class
class IMUProcessor
{
private:
    static IMUProcessor *instance;
    SerialManager *serialMgr;
    IMUType detectedType;
    bool isInitialized;

    // BNO085 RVC support
    BNOAiOParser *bnoParser;
    HardwareSerial *imuSerial;

    // TM171 support
    TM171AiOParser *tm171Parser;

    // ISM330BX support
    ISM330BXProcessor *ism330bxProcessor;

    // Latest IMU data
    IMUData currentData;

    // Timing
    elapsedMillis timeSinceLastPacket;

    // Serial data tracking
    uint32_t lastSerialDataTime = 0;
    bool serialDataReceived = false;

    // Private methods
    bool initBNO085();
    bool initTM171();
    bool initISM330BX();
    void processBNO085Data();
    void processTM171Data();
    void processISM330BXData();

public:
    IMUProcessor();
    ~IMUProcessor();

    // Singleton access
    static IMUProcessor *getInstance();
    static void init();

    // Main interface
    bool initialize();
    void process();  // Legacy method - calls appropriate process method based on IMU type
    void processSerialIMU();  // Process serial-based IMUs (BNO085, TM171) - call from EVERY_LOOP
    void processI2CIMU();     // Process I2C-based IMUs (ISM330BX) - call from timed scheduler
    bool isActive() const { return isInitialized && timeSinceLastPacket < 100; }
    bool isIMUInitialized() const { return isInitialized; }

    // Data access
    IMUData getCurrentData() const { return currentData; }
    bool hasValidData() const { return currentData.isValid; }

    // Info and stats
    IMUType getIMUType() const { return detectedType; }
    const char *getIMUTypeName() const;
    uint32_t getTimeSinceLastPacket() const { return timeSinceLastPacket; }

    // Serial data status
    bool hasSerialData() const { return serialDataReceived && (millis() - lastSerialDataTime < 1000); }
    
    // Debug
    void printStatus();
    void printCurrentData();
    
    // PGN support
    void registerPGNCallbacks();
    void sendIMUData();  // Send PGN 211 (0xD3)
    
    // Static callback for PGN Hello (200)
    static void handleBroadcastPGN(uint8_t pgn, const uint8_t* data, size_t len);
};

// Global instance
extern IMUProcessor imuProcessor;

#endif // IMUPROCESSOR_H_