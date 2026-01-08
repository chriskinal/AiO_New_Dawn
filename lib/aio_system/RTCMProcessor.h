// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

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
    RTCMProcessor();
    ~RTCMProcessor();

public:
    // Get singleton instance
    static RTCMProcessor* getInstance() {
        if (instance == nullptr) {
            instance = new RTCMProcessor();
        }
        return instance;
    }
    
    // Process incoming RTCM data from network
    void processRTCM(const uint8_t* data, size_t len, const IPAddress& remoteIP, uint16_t remotePort);
    
    // Process incoming RTCM data from radio
    void processRadioRTCM();
    
    // Process all RTCM sources (called from main loop)
    void process();

    // Initialize the handler
    static void init();
};

#endif // RTCMProcessor_H_