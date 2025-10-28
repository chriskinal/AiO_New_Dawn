#ifndef RTCMProcessor_H_
#define RTCMProcessor_H_

#include "Arduino.h"
#include "QNetworkBase.h"
#include <QNEthernet.h>
#include <QNEthernetUDP.h>

// QNEthernet namespace
using namespace qindesign::network;

// RTCM data sources
enum class RTCMSource {
    NETWORK,    // From UDP port 9999
    RADIO       // From SerialRadio (Xbee)
};

class RTCMProcessor
{
public:
    static RTCMProcessor *instance;

private:
    //RTCMProcessor();
    RTCMProcessor(HardwareSerialIMXRT& _SerialGPS1, HardwareSerialIMXRT& _SerialRadio);
    ~RTCMProcessor();

    HardwareSerialIMXRT& gpsSerial;
    HardwareSerialIMXRT& radioSerial;
    //Stream& gpsSerial;
    //Stream& radioSerial;

public:
    // Get singleton instance
    /*static RTCMProcessor* getInstance(HardwareSerialIMXRT& _SerialGPS1, HardwareSerialIMXRT& _SerialRadio) {
        if (instance == nullptr) {
            instance = new RTCMProcessor(_SerialGPS1, _SerialRadio);
        }
        return instance;
    }*/
    static RTCMProcessor* getInstance() {
        return instance;
    }
    
    // Process incoming RTCM data from network
    void processRTCM(const uint8_t* data, size_t len, const IPAddress& remoteIP, uint16_t remotePort);
    
    // Process incoming RTCM data from radio
    void processRadioRTCM();
    
    // Process all RTCM sources (called from main loop)
    void process();

    // Initialize the handler
    static void init(HardwareSerialIMXRT& _SerialGPS1, HardwareSerialIMXRT& _SerialRadio);
};

#endif // RTCMProcessor_H_