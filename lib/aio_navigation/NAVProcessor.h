#ifndef NAV_PROCESSOR_H
#define NAV_PROCESSOR_H

#include "Arduino.h"
#include "GNSSProcessor.h"
#include "IMUProcessor.h"
#include "elapsedMillis.h"
#include "QNetworkBase.h"

enum class NavMessageType {
    NONE,
    PANDA,  // Single GPS with/without IMU
    PAOGI   // Dual GPS with/without IMU
};

class NAVProcessor {
private:
    static NAVProcessor* instance;
    
    
    // Message buffer
    static constexpr size_t BUFFER_SIZE = 256;
    char messageBuffer[BUFFER_SIZE];
    
    // Timing control
    elapsedMillis timeSinceLastMessage;
    static constexpr uint32_t MESSAGE_INTERVAL_MS = 100; // 10Hz
    
    // Track when we last sent GPS data to AgIO
    uint32_t lastGPSMessageTime;
    
    // Track last GPS update time to detect duplicates
    uint32_t lastGPSUpdateTime;
    
    // Track last sent PAOGI position for duplicate detection
    double lastPAOGILatitude;
    double lastPAOGILongitude;
    
    // Private constructor for singleton
    NAVProcessor();
    
    // Message formatting methods
    NavMessageType selectMessageType();
    bool formatPANDAMessage();
    bool formatPAOGIMessage();
    
    // Utility methods
    void convertToNMEACoordinates(double decimalDegrees, bool isLongitude, 
                                  double& nmeaValue, char& direction);
    uint8_t calculateNMEAChecksum(const char* sentence);
    float convertGPStoUTC(uint16_t gpsWeek, float gpsSeconds);
    void sendMessage(const char* message);
    
public:
    ~NAVProcessor();
    
    // Singleton access
    static NAVProcessor* getInstance();
    static void init();
    
    // Main processing method
    void process();
    
    // Check if we have new GPS data since last send
    bool hasNewGPSData() const;
    
    // Configuration
    void setMessageRate(uint32_t intervalMs);
    
    // Status and debugging
    void printStatus();
    uint32_t getLastGPSMessageTime() const { return lastGPSMessageTime; }
    NavMessageType getCurrentMessageType();
    
    // GPS data flow status - are we sending GPS data to AgIO?
    bool hasGPSDataFlow() const {
        return (millis() - lastGPSMessageTime) < 5000;
    }
};

// Global instance declaration
extern NAVProcessor navProcessor;

#endif // NAV_PROCESSOR_H