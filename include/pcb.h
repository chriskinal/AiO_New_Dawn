// Firmware_Teensy_AiO-NG-v6 is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-NG-v6 is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-NG-v6 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Foobar. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.
#include "Arduino.h"
#ifndef PCB_H_
#define PCB_H_
#define AIOv50a

const uint8_t encoderType = 1; // 1 - single input
                               // 2 - dual input (quadrature encoder), uses Kickout_A (Pressure) & Kickout_D (Remote) inputs
                               // 3 - variable duty cycle, for future updates

extern "C" uint32_t set_arm_clock(uint32_t frequency); // required prototype to set CPU speed

// ********* IO Defines *********
const uint8_t WAS_SENSOR_PIN = A15; // WAS input

const uint8_t SPEEDPULSE_PIN = 33;   // actual speed pulse output via optocoupler
const uint8_t SPEEDPULSE10_PIN = 37; // 1/10 speedpulse output, strictly for human visualization with onboard LED
//#include "misc.h"
//SpeedPulse speedPulse(SPEEDPULSE_PIN, SPEEDPULSE10_PIN); // misc.h

const uint8_t BUZZER = 36; // electromagnetic buzzer driven by NFET

// Cytron/DRV8701
#define SLEEP_PIN 4 // DRV8701-Cytron Sleep pin, LOCK output
#define PWM1_PIN 5  // DRV8701-Cytron PWM pin
#define PWM2_PIN 6  // DRV8701-Cytron PWM2 pin (prev Dir pin)

// Switches/Sensors
#define STEER_PIN 2       // Switch/btn input for autosteer engage/disengage
#define WORK_PIN A17      // Analog input, can also be used for Digital switches, see UI for settings
#define KICKOUT_D_PIN 3   // REMOTE, encoder or other digital disengage input
#define CURRENT_PIN A13   // CURRENT sense from on board DRV8701
#define KICKOUT_A_PIN A12 // Analog PRESSURE (can also be used for 2nd Quadrature encodeder input)

// ********* Serial Assignments *********
HardwareSerial *SerialIMU = &Serial4; // IMU BNO-085 in RVC serial mode
#define SerialRTK Serial3             // RTK radio
#define SerialGPS1 Serial5            // GPS1 UART (Right F9P, or UM982)
#define SerialGPS2 Serial8            // GPS2 UART (Left F9P)
#define SerialRS232 Serial7           // RS232 UART
#define SerialESP32 Serial2           // ESP32 UART (for ESP32 WiFi Bridge)

const int32_t baudGPS = 460800; // 921600;
const int32_t baudRTK = 115200; // most are using Xbee radios with default of 115200
const int32_t baudRS232 = 38400;
const int32_t baudESP32 = 460800;

// constexpr int buffer_size = 512;
constexpr int gpsBufferSz = 384;   // Need larger buffer to handle INS messages which are 305 bytes
uint8_t GPS1rxbuffer[gpsBufferSz]; // seems large enough
uint8_t GPS1txbuffer[gpsBufferSz]; // large enough for 256 byte AgIO NTRIP packet
uint8_t GPS2rxbuffer[gpsBufferSz]; // seems large enough
uint8_t GPS2txbuffer[gpsBufferSz]; // large enough for 256 byte AgIO NTRIP packet
uint8_t RTKrxbuffer[64];           // don't know what size is needed, larger buffer if GPS baud is lower then RTK radio baud

uint8_t RS232txbuffer[gpsBufferSz]; // large enough to hold a few NMEA sentences as ext terminal bauds are usually slow
// uint8_t RS232rxbuffer[gpsBufferSz]; // not needed unless custom rs232 rx code is added
uint8_t ESP32rxbuffer[256]; // don't know what size is needed, 128 is likely sufficient
uint8_t ESP32txbuffer[256]; // don't know what size is needed, 128 is likely sufficient

#endif // PCB_H_