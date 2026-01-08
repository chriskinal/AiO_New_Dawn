// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

#ifndef IMUPROCESSOR_H_
#define IMUPROCESSOR_H_

#include "Arduino.h"
#include "SerialManager.h"
#include "BNOAiOParser.h"
#include "TM171AiOParser.h"
#include "elapsedMillis.h"
#include "PGNProcessor.h"
#include "NavigationTypes.h"

// PGN Constants for IMU module
constexpr uint8_t IMU_SOURCE_ID = 0x79;     // 121 decimal - IMU source address
constexpr uint8_t IMU_PGN_DATA = 0xD3;      // 211 decimal - IMU data PGN
constexpr uint8_t IMU_HELLO_REPLY = 0x79;   // 121 decimal - IMU hello reply

// IMU data structure
struct IMUData
{
    float heading;      // degrees (0-360)
    float roll;         // degrees
    float pitch;        // degrees
    float yawRate;      // degrees/second
    uint8_t quality;    // 0-10 quality indicator
    uint32_t timestamp; // millis() when data was received
    bool isValid;       // data validity flag
};

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
    void processBNO085Data();
    void processTM171Data();

public:
    IMUProcessor();
    ~IMUProcessor();

    // Singleton access
    static IMUProcessor *getInstance();
    static void init();

    // Main interface
    bool initialize();
    void process();
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