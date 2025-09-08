// main.cpp - Updated section with motor driver testing
#include "Arduino.h"
#include <QNEthernet.h>
#include "QNetworkBase.h"
#include "QNEthernetUDPHandler.h"
#include "ConfigManager.h"
#include "HardwareManager.h"
#include "SerialManager.h"
#include "SerialGlobals.h"
#include "GNSSProcessor.h"
#include "IMUProcessor.h" // Add this include
#include "NAVProcessor.h"
#include "I2CManager.h"
#include "CANManager.h"
#include "ADProcessor.h"
#include "PWMProcessor.h"
#include "MotorDriverInterface.h"
#include "MotorDriverManager.h"
#include "CANGlobals.h"
#include "AutosteerProcessor.h"
#include "EncoderProcessor.h"
#include "KeyaCANDriver.h"
#include "LEDManagerFSM.h"
#include "MachineProcessor.h"
// SubnetManager functionality moved to QNetworkBase
#include "EventLogger.h"
#include "CommandHandler.h"
#include "PGNProcessor.h"
#include "RTCMProcessor.h"
#include "SimpleWebManager.h"
#include "Version.h"
#include "LittleDawnInterface.h"

// Flash ID for OTA verification - must match FLASH_ID in FlashTxx.h
const char* flash_id = "fw_teensy41";

// Global objects (no more pointers)
ConfigManager configManager;
HardwareManager hardwareManager;
SerialManager serialManager;
I2CManager i2cManager;
CANManager canManager;
GNSSProcessor gnssProcessor;
GNSSProcessor* gnssProcessorPtr = &gnssProcessor;  // Global pointer for external access
IMUProcessor imuProcessor;
ADProcessor adProcessor;
PWMProcessor pwmProcessor;
// LEDManagerFSM ledManagerFSM; // Global instance already defined in LEDManagerFSM.cpp
SimpleWebManager webManager;
MotorDriverInterface *motorPTR = nullptr; // Motor driver still uses factory pattern

// Loop timing diagnostics
volatile bool loopTimingEnabled = false;
uint32_t loopCount = 0;
elapsedMillis timingPeriod;  // Teensy's auto-incrementing millisecond timer

// Function to toggle loop timing
void toggleLoopTiming() {
  loopTimingEnabled = !loopTimingEnabled;
  if (loopTimingEnabled) {
    // Reset counters when enabling
    loopCount = 0;
    timingPeriod = 0;
    LOG_INFO(EventSource::SYSTEM, "Loop timing diagnostics ENABLED - first report in 30 seconds");
  } else {
    LOG_INFO(EventSource::SYSTEM, "Loop timing diagnostics DISABLED");
  }
}

void setup()
{
  delay(5000); // delay for time to start monitor
  Serial.begin(115200);

  Serial.print("\r\n\n=== Teensy 4.1 AiO-NG-v6 New Dawn v");
  Serial.print(FIRMWARE_VERSION);
  Serial.print(" ===\r\n");
  Serial.print("Initializing subsystems...");

  // Initialize ConfigManager FIRST - it has no dependencies
  configManager.init();
  Serial.print("\r\n- ConfigManager initialized\r\n");

  // Initialize EventLogger SECOND - so all subsequent messages are formatted
  EventLogger::init();
  Serial.print("\r\n- EventLogger initialized (startup mode)\r\n");
  delay(10);  // Small delay to ensure EventLogger is fully initialized

  // Initialize PGNProcessor (needed by QNetworkBase)
  PGNProcessor::init();
  LOG_INFO(EventSource::SYSTEM, "PGNProcessor initialized");

  // Network and communication setup
  QNetworkBase::init();
  
  // Wait for network speed to stabilize (some switches negotiate in steps)
  LOG_INFO(EventSource::NETWORK, "Waiting for network speed negotiation...");
  uint32_t startWait = millis();
  int lastSpeed = 0;
  
  // Wait up to 10 seconds for link speed to stabilize
  while (millis() - startWait < 10000) {
    int currentSpeed = Ethernet.linkSpeed();
    if (currentSpeed != lastSpeed) {
      LOG_INFO(EventSource::NETWORK, "Link speed changed: %d Mbps", currentSpeed);
      lastSpeed = currentSpeed;
      startWait = millis(); // Reset timer on speed change
    }
    
    // If we've been stable at 100Mbps for 2 seconds, we're good
    if (currentSpeed == 100 && millis() - startWait > 2000) {
      LOG_INFO(EventSource::NETWORK, "Link stable at 100 Mbps");
      break;
    }
    
    delay(100);
  }
  
  // Additional delay for network stack to stabilize
  LOG_INFO(EventSource::NETWORK, "Waiting for network stack to stabilize...");
  delay(2000);
  
  // Verify we have an IP address
  IPAddress localIP = Ethernet.localIP();
  if (localIP == IPAddress(0, 0, 0, 0)) {
    LOG_ERROR(EventSource::NETWORK, "No IP address assigned!");
  } else {
    LOG_INFO(EventSource::NETWORK, "IP ready: %d.%d.%d.%d", localIP[0], localIP[1], localIP[2], localIP[3]);
    LOG_INFO(EventSource::NETWORK, "Final link speed: %d Mbps", Ethernet.linkSpeed());
  }
  
  // Network stack is ready but don't initialize AsyncUDP yet
  LOG_INFO(EventSource::NETWORK, "Network stack initialized");
  
  // Initialize global CAN buses
  initializeGlobalCANBuses();
  
  // Initialize RTCMProcessor
  RTCMProcessor::init();
  LOG_INFO(EventSource::SYSTEM, "RTCMProcessor initialized");

  // Initialize HardwareManager
  if (hardwareManager.initializeHardware())
  {
    LOG_INFO(EventSource::SYSTEM, "HardwareManager initialized");
    
    // Quick buzzer beep to indicate hardware is ready
    hardwareManager.enableBuzzer();
    delay(100);
    hardwareManager.disableBuzzer();
  }
  else
  {
    LOG_ERROR(EventSource::SYSTEM, "HardwareManager FAILED");
  }

  // Initialize CANManager first (more critical than I2C)
  if (canManager.init())
  {
    LOG_INFO(EventSource::SYSTEM, "CANManager initialized");
  }
  else
  {
    LOG_ERROR(EventSource::SYSTEM, "CANManager FAILED");
  }

  // Initialize SerialManager
  if (serialManager.initializeSerial())
  {
    LOG_INFO(EventSource::SYSTEM, "SerialManager initialized");
  }
  else
  {
    LOG_ERROR(EventSource::SYSTEM, "SerialManager FAILED");
  }

  // Initialize GNSSProcessor
  if (gnssProcessor.setup(false, true)) // Disable debug, enable noise filter
  {
    LOG_INFO(EventSource::SYSTEM, "GNSSProcessor initialized");
  }
  else
  {
    LOG_ERROR(EventSource::SYSTEM, "GNSSProcessor FAILED");
  }

  // Add a delay before I2C to let network/interrupts stabilize
  delay(100);
  
  // Initialize I2CManager later in sequence after critical systems
  if (i2cManager.initializeI2C())
  {
    LOG_INFO(EventSource::SYSTEM, "I2CManager initialized");
  }
  else
  {
    LOG_ERROR(EventSource::SYSTEM, "I2CManager FAILED");
  }

  // Initialize LED Manager FSM (needs I2C)
  if (ledManagerFSM.init()) 
  {
    LOG_INFO(EventSource::SYSTEM, "LEDManagerFSM initialized");
    ledManagerFSM.setBrightness(configManager.getLEDBrightness());
  }
  else
  {
    LOG_ERROR(EventSource::SYSTEM, "LEDManagerFSM FAILED");
  }

  // Initialize IMUProcessor
  if (imuProcessor.initialize())
  {
    LOG_INFO(EventSource::SYSTEM, "IMUProcessor initialized");
    imuProcessor.registerPGNCallbacks();
  }
  else
  {
    LOG_ERROR(EventSource::SYSTEM, "IMUProcessor FAILED");
  }

  // Initialize ADProcessor
  // Set the instance pointer so getInstance() returns the correct object
  ADProcessor::instance = &adProcessor;
  if (adProcessor.init())
  {
    LOG_INFO(EventSource::SYSTEM, "ADProcessor initialized");
    adProcessor.process();
  }
  else
  {
    LOG_ERROR(EventSource::SYSTEM, "ADProcessor FAILED");
  }
  
  // Initialize Motor Driver BEFORE PWMProcessor to ensure correct PWM resolution
  motorPTR = MotorDriverManager::getInstance()->detectAndCreateMotorDriver(&hardwareManager, &canManager);
  
  if (motorPTR && motorPTR->init()) {
    LOG_INFO(EventSource::SYSTEM, "Motor driver initialized");
  } else {
    LOG_ERROR(EventSource::SYSTEM, "Motor driver FAILED");
  }

  // Initialize PWMProcessor AFTER motor driver (they conflict on PWM resolution)
  if (pwmProcessor.init())
  {
    LOG_INFO(EventSource::SYSTEM, "PWMProcessor initialized");
    pwmProcessor.setSpeedPulseDuty(0.5f);
    pwmProcessor.enableSpeedPulse(true);
    pwmProcessor.setPulsesPerMeter(130.0f);  // ISO 11786 standard
    //pwmProcessor.setSpeedKmh(10.0f);  // should not set this yet, we don't want a pulse output until we have a speed
  }
  else
  {
    LOG_ERROR(EventSource::SYSTEM, "PWMProcessor FAILED");
  }

  // Initialize NAVProcessor
  NAVProcessor::init();
  LOG_INFO(EventSource::SYSTEM, "NAVProcessor initialized");

  // NOW initialize AsyncUDP after ALL hardware is up
  LOG_INFO(EventSource::SYSTEM, "All hardware initialized, starting AsyncUDP");
  QNEthernetUDPHandler::init();
  LOG_INFO(EventSource::SYSTEM, "AsyncUDP handlers ready");

  // Initialize AutosteerProcessor
  AutosteerProcessor* autosteerPTR = AutosteerProcessor::getInstance();
  if (autosteerPTR->init()) {
    LOG_INFO(EventSource::SYSTEM, "AutosteerProcessor initialized");
  } else {
    LOG_ERROR(EventSource::SYSTEM, "AutosteerProcessor FAILED");
  }
  
  // Initialize EncoderProcessor
  EncoderProcessor* encoderPTR = EncoderProcessor::getInstance();
  if (encoderPTR->init()) {
    LOG_INFO(EventSource::SYSTEM, "EncoderProcessor initialized");
  } else {
    LOG_ERROR(EventSource::SYSTEM, "EncoderProcessor FAILED");
  }

  // Initialize MachineProcessor
  MachineProcessor* machinePTR = MachineProcessor::getInstance();
  if (machinePTR->initialize()) {
    LOG_INFO(EventSource::SYSTEM, "MachineProcessor initialized");
  } else {
    LOG_ERROR(EventSource::SYSTEM, "MachineProcessor FAILED");
  }

  // Initialize Little Dawn Interface
  littleDawnInterface.init();
  LOG_INFO(EventSource::SYSTEM, "LittleDawnInterface initialized");

  // PGN 201 handling is now done by QNetworkBase

  // Initialize CommandHandler
  CommandHandler::init();
  CommandHandler* cmdHandler = CommandHandler::getInstance();
  cmdHandler->setMachineProcessor(machinePTR);
  LOG_INFO(EventSource::SYSTEM, "CommandHandler initialized");

  // Initialize WebManager
  if (webManager.begin()) {
    LOG_INFO(EventSource::SYSTEM, "WebManager initialized");
  } else {
    LOG_ERROR(EventSource::SYSTEM, "WebManager FAILED");
  }

  // Exit startup mode - start enforcing configured log levels
  EventLogger::getInstance()->setStartupMode(false);
  
  // Mark web manager as ready for SSE updates
  webManager.setSystemReady(true);
  
  // Display access information
  localIP = Ethernet.localIP();  // Reuse existing variable
  Serial.println("\r\n");
  Serial.println("========================================");
  Serial.println("=== AiO New Dawn - System Ready ===");
  Serial.println("========================================");
  Serial.printf("IP Address: %d.%d.%d.%d\r\n", localIP[0], localIP[1], localIP[2], localIP[3]);
  Serial.printf("Web Interface: http://%d.%d.%d.%d\r\n", localIP[0], localIP[1], localIP[2], localIP[3]);
  Serial.println("DHCP Server: Enabled");
  Serial.println("========================================");
  Serial.println();
  
  LOG_INFO(EventSource::SYSTEM, "=== System Ready ===");
}

void loop()
{
  // OTA updates are handled via web interface
  
  // Process Ethernet events - REQUIRED for QNEthernet!
  Ethernet.loop();
  
  QNetworkBase::poll();
  
  // Poll AsyncUDP for network diagnostics
  QNEthernetUDPHandler::poll();
  
  // AsyncUDP handles all UDP packet reception via callbacks
  // The poll() call above is just for diagnostics and status monitoring
  
  
  // Check network status and display system ready message when appropriate
  static uint32_t lastNetworkCheck = 0;
  if (millis() - lastNetworkCheck > 100) {  // Check every 100ms for responsiveness
    lastNetworkCheck = millis();
    EventLogger::getInstance()->checkNetworkReady();
  }

  // Process serial commands through CommandHandler
  CommandHandler::getInstance()->process();
  
  // Process IMU data
  imuProcessor.process();
  
  // Process A/D inputs
  adProcessor.process();
  
  // Process Little Dawn interface
  littleDawnInterface.process();

  // Process NAV messages
  NAVProcessor::getInstance()->process();
  
  // Process RTCM data from all sources (network and radio)
  RTCMProcessor::getInstance()->process();

  // CAN handling is done by motor drivers directly
  
  // Process autosteer FIRST - calculate new motor commands
  AutosteerProcessor::getInstance()->process();
  
  // Process motor driver AFTER autosteer has set new PWM values
  if (motorPTR)
  {
    motorPTR->process();
  }
  
  // Process encoder
  EncoderProcessor::getInstance()->process();
  
  // Process machine
  MachineProcessor::getInstance()->process();
  
  // Update LEDs
  static uint32_t lastLEDUpdate = 0;
  if (millis() - lastLEDUpdate > 100)  // Update every 100ms
  {
    lastLEDUpdate = millis();
    ledManagerFSM.updateAll();
  }
  
  // Handle WebSocket clients and broadcast telemetry
  webManager.handleClient();
  webManager.broadcastTelemetry();
    
  // Update PWM speed pulse from GPS
  static uint32_t lastSpeedUpdate = 0;
  
  if (millis() - lastSpeedUpdate > 200)  // Update every 200ms like V6-NG
  {
    lastSpeedUpdate = millis();
    
    if (pwmProcessor.isSpeedPulseEnabled())
    {
      float speedKmh = 0.0f;
      
      // Use actual GPS speed
      const auto &gpsData = gnssProcessor.getData();
      if (gpsData.hasVelocity)
      {
        // Convert knots to km/h
        speedKmh = gpsData.speedKnots * 1.852f;
      }
      else
      {
        // Fallback to PGN 254 speed if GPS velocity is not available
        // This is useful for systems that are running in SIM mode
        // - could also always use PGN254 speed as it returns GPS speed if available
        speedKmh = AutosteerProcessor::getInstance()->getVehicleSpeed();
      }

      //Serial.printf("Vehicle speed: %.1f km/h", speedKmh);
      
      // Set the speed
      pwmProcessor.setSpeedKmh(speedKmh);
    }
  }

  // Process GPS1 data if available - ONE byte per loop
  if (SerialGPS1.available())
  {
    char c = SerialGPS1.read();
    gnssProcessor.processNMEAChar(c);
  }
  
  // Process GPS2 data if available (for F9P dual RELPOSNED) - ONE byte per loop
  if (SerialGPS2.available())
  {
    uint8_t b = SerialGPS2.read();
    //gnssProcessor.processUBXByte(b);
    //SerialRS232.write(b);  // forward complete UM982 com2 to RS232 for GS3 2630 harvest documentation

    // Refactored GPS2 NMEA forwarding to handle decimal precision and checksum processing
    enum State { WAITING, IN_SENTENCE, COUNTING, CHECKSUM1, CHECKSUM2 };
    static State state = WAITING;
    static uint8_t decimalCount = 0;
    static uint8_t checkSum = 0;
    //static uint8_t buffer[128];
    //static uint8_t bufIndex = 0;

    switch (state) {
      case WAITING:
        if (b == '$') {
          state = IN_SENTENCE;
          checkSum = 0;
          //Serial.write(buffer, bufIndex);
          //bufIndex = 0;
        }
        SerialRS232.write(b);  // forward byte from UM982 com2 (GPS2) to RS232 for GS3 2630 harvest documentation
        //Serial.write(b);      // also echo to main serial for logging
        break;

      case IN_SENTENCE:
        if (b == '.') {
          state = COUNTING;
          decimalCount = 0;
          checkSum ^= b;
        } else if (b == '*') {
          state = CHECKSUM1;  // Start of checksum, ignore the next 1-2 bytes, upto '\n' or '\r'
        } else {
          checkSum ^= b;
        }
        SerialRS232.write(b);
        //Serial.write(b);      // also echo to main serial for logging
        break;

      case COUNTING:
        if (b >= '0' && b <= '9') {
          decimalCount++;
          if (decimalCount <= 6) {
            SerialRS232.write(b);
            //Serial.write(b);      // also echo to main serial for logging
            checkSum ^= b;
          }
          else {
            // do nothing, drop the digits
          }
        } else {  // byte is not a numerical digit so we go back to watching for the next '.'
          state = IN_SENTENCE;
          SerialRS232.write(b);
          //Serial.write(b);      // also echo to main serial for logging
          checkSum ^= b;
        }
        break;

      case CHECKSUM1:
        SerialRS232.print(checkSum, HEX);  // write the checksum character
        //Serial.print(checkSum, HEX);      // also echo to main serial for logging
        state = CHECKSUM2;
        break;

      case CHECKSUM2: // just to skip 2nd incoming checksum character
        state = WAITING;
        break;

      default:
        state = WAITING;
        //Serial.printf("Unexpected state in GPS2 NMEA forwarding precision processing state machine!\r\n");
    }

    //buffer[bufIndex++] = b;

  }

  // Loop timing - ultra lightweight, just increment counter
  if (loopTimingEnabled) {
    loopCount++;
    
    // Report every 30 seconds
    if (timingPeriod >= 30000) {
      // Calculate average from total time / total loops
      float avgLoopTime = (float)timingPeriod * 1000.0f / (float)loopCount;  // Convert ms to us
      float loopFreqKhz = (float)loopCount / (float)timingPeriod;  // loops per ms = kHz
      
      LOG_INFO(EventSource::SYSTEM, "Loop: %.2f kHz, Time: %.0f us (samples: %lu)", 
               loopFreqKhz, avgLoopTime, loopCount);
      
      // Reset for next period
      timingPeriod = 0;
      loopCount = 0;
    }
  }
}