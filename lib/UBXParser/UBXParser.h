// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

#include "elapsedMillis.h"
//#include <stdint.h>
class UBX_Parser {
private:

  typedef enum {
    GOT_NONE,
    GOT_SYNC1,
    GOT_SYNC2,
    GOT_CLASS,
    GOT_ID,
    GOT_LENGTH1,
    GOT_LENGTH2,
    GOT_PAYLOAD,
    GOT_CHKA
  } state_t;

  state_t state;
  int msgclass;
  int msgid;
  int msglen;
  char chka;
  char chkb;
  int count;
  char payload[1000];
  uint32_t prevRelposnedMsgTime, prevPvtMsgTime, startMsgTime;

  void addchk(int b) {
    this->chka = (this->chka + b) & 0xFF;
    this->chkb = (this->chkb + this->chka) & 0xFF;
  }


  void dispatchMessage() {

    //Serial.print("\r\nTTRU: "); Serial.println(micros() - startMsgTime);
    msgReadTime = micros() - startMsgTime;

    // UBX frame structure detailed here
    // https://content.u-blox.com/sites/default/files/documents/u-blox-F9-HPG-1.32_InterfaceDescription_UBX-22008968.pdf

    switch (this->msgid) {
      //RELPOSNED
      case 0x3C:
        {
          /*uint32_t iT = (int32_t)this->payload[4] + ((int32_t)this->payload[5] << 8)
              + ((int32_t)this->payload[6] << 16) + ((int32_t)this->payload[7] << 24);
          Serial.print("                    "); Serial.print((double)iT / 1000.0, 2);*/

          unsigned long iTOW   = (unsigned long)this->unpack_int32(4);
          long relPosN         = this->unpack_int32(8);
          long relPosE         = this->unpack_int32(12);
          long relPosD         = this->unpack_int32(16);
          long relPosL         = this->unpack_int32(20);
          long relPosH         = this->unpack_int32(24);
          int  relPosHPD       = this->payload[34];
          int  relPosHPL       = this->payload[35];
          uint32_t relPosFlags = this->unpack_int32(60);
          this->handle_NAV_RELPOSNED(iTOW, relPosN, relPosE, relPosD, relPosL, relPosH, relPosHPD, relPosHPL, relPosFlags);
        }
        break;

      //PVT
      case 0x07:
        {
          unsigned long iTOW = (unsigned long)this->unpack_int32(0);
          unsigned short year = (unsigned short)this->unpack_int16(4);
          unsigned char month = (unsigned char)this->payload[6];
          unsigned char day = (unsigned char)this->payload[7];
          unsigned char hour = (unsigned char)this->payload[8];
          unsigned char min = (unsigned char)this->payload[9];
          unsigned char sec = (unsigned char)this->payload[10];
          unsigned char vaild = (unsigned char)this->payload[11];
          unsigned char fixType = (unsigned char)this->payload[20];
          unsigned char flags = (unsigned char)this->payload[21];
          unsigned char flags2 = (unsigned char)this->payload[22];
          unsigned char numSV = (unsigned char)this->payload[23];
          signed long lon = (signed long)this->unpack_int32(24);
          signed long lat = (signed long)this->unpack_int32(28);
          signed long height = (signed long)this->unpack_int32(32);
          signed long hMSL = (signed long)this->unpack_int32(36);
          signed long velN = (signed long)this->unpack_int32(48);
          signed long velE = (signed long)this->unpack_int32(52);
          signed long velD = (signed long)this->unpack_int32(56);
          signed long gSpeed = (signed long)this->unpack_int32(60);
          signed long headMot = (signed long)this->unpack_int32(64);
          signed long headVeh = (signed long)this->unpack_int32(84);

          this->handle_NAV_PVT(iTOW, year, month, day, hour, min, sec, vaild, fixType, flags, flags2, numSV,
                               lon, lat, height, hMSL, velN, velE, velD, gSpeed, headMot, headVeh);
        }
        break;

      default:
        this->reportUnhandled(this->msgid);
        break;
    }
  }

  long unpack_int32(int offset) {
    return this->unpack32(offset, 4);
    ;
  }

  long unpack_int16(int offset) {
    return this->unpack16(offset, 2);
    ;
  }

  long unpack16(int offset, int size) {
    long value = 0;  // four bytes on most Arduinos

    for (int k = 0; k < size; ++k) {
      value <<= 8;
      value |= (0xFF & this->payload[offset + 2 - k - 1]);
    }
    return value;
  }

  long unpack32(int offset, int size) {
    long value = 0;  // four bytes on most Arduinos

    for (int k = 0; k < size; ++k) {
      value <<= 8;
      value |= (0xFF & this->payload[offset + 4 - k - 1]);
    }
    return value;
  }

  void handle_NAV_RELPOSNED(unsigned long iTOW,
                            long relPosN,
                            long relPosE,
                            long relPosD,
                            long relPosL,
                            long relPosH,
                            int  relPosHPD,
                            int  relPosHPL,
                            uint32_t relPosFlags) {
    ubxData.iTOW = iTOW;
    ubxData.baseRelN   = (double)relPosN * 0.01;
    ubxData.baseRelE   = (double)relPosE * 0.01;
    ubxData.baseRelHPD = (double)relPosHPD * 0.01;
    ubxData.baseRelD   = (double)relPosD + ubxData.baseRelHPD;
    ubxData.baseRelHPL = (double)relPosHPL * 0.01;
    ubxData.baseRelL   = (double)relPosL + ubxData.baseRelHPL;

    ubxData.baseRelH = (double)relPosH * 0.0001 + 900;    // ubx default heading is Right ant -> Left ant
                                                          // turn the heading +90.0 degrees for dual ant in side-to-side (roll) setup
    if (ubxData.baseRelH >= 3600) ubxData.baseRelH -= 3600;
    if (ubxData.baseRelH < 0) ubxData.baseRelH += 3600;
    ubxData.baseRelH *= 0.1;

    ubxData.baseRelFlags = relPosFlags;

    if (debug) {
      Serial.print("\r\n"); Serial.print(millis());
      Serial.printf(" REL update (%i)", relMissed);
      Serial.print(millis() - prevRelposnedMsgTime);
      Serial.print(" "); Serial.print((ubxData.iTOW % 1000) / 10);
      Serial.print(" "); Serial.print(ubxData.baseRelH, 2);
    }
    msgPeriod = millis() - prevRelposnedMsgTime;
    prevRelposnedMsgTime = millis();
    relPosTimer = 0;
    if (!useDual) firstHeadingDetected = 1;
    relPosNedRcvd = true;
    useDual = true;             // set true for the rest of runtime


    bool gnssFixOk = ubxData.baseRelFlags & 1;
    bool diffSoln = ubxData.baseRelFlags & 2;
    bool relPosValid = ubxData.baseRelFlags & 4;
    ubxData.carrSoln = (ubxData.baseRelFlags & 24) >> 3;
    /*bool isMoving = ubxData.baseRelFlags & (32);
    bool refPosMiss = ubxData.baseRelFlags & (64);
    bool refObsMiss = ubxData.baseRelFlags & (128);
    bool refPosHeadingValid = ubxData.baseRelFlags & (256);
    bool relPosNormalized = ubxData.baseRelFlags & (512);*/

    /*Serial.print("\r\ngnssFixOk: "); Serial.print(gnssFixOk);
    Serial.print("\r\ndiffSoln: "); Serial.print(diffSoln);
    Serial.print("\r\nrelPosValid: "); Serial.print(relPosValid);
    Serial.print("\r\ncarrSoln: "); Serial.print(ubxData.carrSoln);
    Serial.print("\r\nisMoving: "); Serial.print(isMoving);
    Serial.print("\r\nrefPosMiss: "); Serial.print(refPosMiss);
    Serial.print("\r\nrefObsMiss: "); Serial.print(refObsMiss);
    Serial.print("\r\nrefPosHeadingValid: "); Serial.print(refPosHeadingValid);
    Serial.print("\r\nrelPosNormalized: "); Serial.print(relPosNormalized);*/

    //must be all ok
    if (!gnssFixOk) { Serial.print("\r\n*** gnssFixOk! "); Serial.print(gnssFixOk); Serial.print(" ***"); }
    if (!diffSoln) { Serial.print("\r\n*** diffSoln! "); Serial.print(diffSoln); Serial.print(" ***"); }
    if (!relPosValid) { Serial.print("\r\n*** relPosValid! "); Serial.print(relPosValid); Serial.print(" ***"); }
    if (!gnssFixOk || !diffSoln || !relPosValid) return;

    if (ubxData.carrSoln > 1) {        // 1 - float, 2 - full RTK
      if (ubxData.baseRelL == 0) ubxData.baseRelL += 0.01;    // to prevent 0 division error
      ubxData.baseRelRoll = (asin(ubxData.baseRelD / ubxData.baseRelL)) * -RAD_TO_DEG;
      relPosNedReady = true;      // RelPos ready is true so PAOGI will send when the GGA is also ready
    } else {
      if (!debug) Serial.print("\r\n");
      Serial.print("    carrSoln: "); Serial.print(ubxData.carrSoln);
      ubxData.baseRelRoll *= 0.9;     // "level off" dual roll
      relPosNedReady = true;         
    }

    static double relRollOld;
    if (debug) {
      Serial.print(" "); Serial.print(ubxData.baseRelRoll, 2);
      if (ubxData.baseRelRoll <= relRollOld - 0.5 || ubxData.baseRelRoll >= relRollOld + 0.5) {
        Serial.printf("\r\n\n******************* ROLL JUMP %6.2f -> %6.2f ************************\r\n\n", relRollOld, ubxData.baseRelRoll);
      }
    }
    relRollOld = ubxData.baseRelRoll;
  }

  void handle_NAV_PVT(unsigned long iTOW,
                      unsigned short year,
                      unsigned char month,
                      unsigned char day,
                      unsigned char hour,
                      unsigned char min,
                      unsigned char sec,
                      unsigned char vaild,
                      unsigned char fixType,
                      unsigned char flags,
                      unsigned char flags2,
                      unsigned char numSV,
                      signed long lon,
                      signed long lat,
                      signed long height,
                      signed long hMSL,
                      signed long velN,
                      signed long velE,
                      signed long velD,
                      signed long gSpeed,
                      signed long headMot,
                      signed long headVeh) {
    ubxData.numSats = (byte)numSV;
    ubxData.lat = (double)lat * 0.0000001;
    ubxData.lon = (double)lon * 0.0000001;
    ubxData.alt = (double)height * 0.001;
    if (debug) {
      Serial.print("\r\n"); Serial.print(millis()); Serial.print(" PVT update "); Serial.print(millis() - prevPvtMsgTime);
      Serial.print(" "); Serial.print((double)iTOW / 1000.0, 2);
    }
    prevPvtMsgTime = millis();
    pvtTimer = 0;
  }

  void reportUnhandled(char msgid) {
    Serial.print("\r\nGot message ID:");
    Serial.print(msgid & 0xFF, HEX);
    Serial.print(" cls:");
    Serial.print(msgclass & 0xFF, HEX);
  }

public:

  struct UBX_Data {
    double baseRelN, baseRelE, baseRelD;   // moving base NED in meters
    double baseRelL, baseRelH;             // moving base length in meter, heading in degrees
    double baseRelHPD, baseRelHPL;         // high precision component of D & L
    uint32_t iTOW;                        // time of week from relposned
    byte numSats;                         // number of Sats from PVT
    double lat, lon, alt;                  // position from PVT
    uint32_t baseRelFlags;                // moving base flags (gnssFixOk, diffSoln, relPosValid, carrSoln etc)
    double baseRelRoll;                    // calc from baseRelD & baseRelL
    int carrSoln;                         // from relposned
  };
  UBX_Data ubxData;

  bool relPosNedReady, useDual, relPosNedRcvd, debug, firstHeadingDetected;
  uint32_t msgPeriod, msgReadTime;
  elapsedMillis relPosTimer, pvtTimer;
  uint16_t relMissed;

  /**
          * Constructs a UBX parser. 
          */
  UBX_Parser() {
    this->state = GOT_NONE;
    this->msgclass = -1;
    this->msgid = -1;
    this->msglen = -1;
    this->chka = -1;
    this->chkb = -1;
    this->count = -1;
  }

  /**
          * Parses a new byte from the GPS. Automatically calls handle_ methods when a new
          * message is successfully parsed.
          * @param b the byte
          */
  void parse(int b) {
    if (b == 0xB5 && this->count < 0) {
      //Serial.print(" cnt:"); Serial.print(this->count);
      this->state = GOT_SYNC1;
      startMsgTime = micros();
      if (debug) { Serial.print("\r\n"); Serial.print(millis()); Serial.print(" ubx 0xB5"); }
    }

    else if (b == 0x62 && this->state == GOT_SYNC1) {

      this->state = GOT_SYNC2;
      this->chka = 0;
      this->chkb = 0;
      if (debug) Serial.print(" 0x62");
    }

    else if (this->state == GOT_SYNC2) {

      this->state = GOT_CLASS;
      this->msgclass = b;
      this->addchk(b);
      if (debug) { Serial.print(" 0x"); Serial.print(b,HEX); }
    }

    else if (this->state == GOT_CLASS) {

      this->state = GOT_ID;
      this->msgid = b;
      this->addchk(b);
      if (debug) { Serial.print(" 0x"); Serial.print(b,HEX); }
    }

    else if (this->state == GOT_ID) {

      this->state = GOT_LENGTH1;
      this->msglen = b;
      this->addchk(b);
      if (debug) Serial.print(" L1");
    }

    else if (this->state == GOT_LENGTH1) {

      this->state = GOT_LENGTH2;
      this->msglen += (b << 8);
      this->count = 0;
      this->addchk(b);
      if (debug) Serial.print(" L2");
    }

    else if (this->state == GOT_LENGTH2) {

      this->addchk(b);
      this->payload[this->count] = b;
      this->count += 1;
      if (debug) { Serial.print(" "); Serial.print(b); }

      if (this->count == this->msglen) {

        this->state = GOT_PAYLOAD;
      }
    }

    else if (this->state == GOT_PAYLOAD) {

      if (b == this->chka) {
        this->state = GOT_CHKA;
        if (debug) Serial.print(" cA");
       } else {
        this->state = GOT_NONE;
        this->count = -1;
        Serial.print("\r\n"); Serial.print(millis());
        Serial.print(" ubx crcA failed");
       }
    }

    else if (this->state == GOT_CHKA) {

      if (b == this->chkb) {
        if (debug) Serial.print(" cB");
        this->dispatchMessage();
        this->count = -1;
      } else {
        this->state = GOT_NONE;
        this->count = -1;
        Serial.print("\r\n"); Serial.print(millis());
        Serial.print(" ubx crcB failed");
      }
    }
  }

  void clearCount() {
    this->count = -1;
  }
};
