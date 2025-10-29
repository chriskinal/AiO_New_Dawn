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
#include "KickoutMonitor.h"
#include "LEDManagerFSM.h"
#include "MachineProcessor.h"
// SubnetManager functionality moved to QNetworkBase
#include "EventLogger.h"
#include "CommandHandler.h"
#include "PGNProcessor.h"
#include "RTCMProcessor.h"
#include "SimpleWebManager.h"
#include "Version.h"
#include "ESP32Interface.h"
#include "SimpleScheduler/SimpleScheduler.h"

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

// Process timing diagnostics
volatile bool processTimingEnabled = false;
struct ProcessTiming {
  const char* name;
  uint32_t totalTime;  // microseconds
  uint32_t count;
  uint32_t maxTime;
};

ProcessTiming processTiming[] = {
  {"Ethernet.loop", 0, 0, 0},
  {"QNetworkBase.poll", 0, 0, 0},
  {"UDPHandler.poll", 0, 0, 0},
  {"EventLogger.check", 0, 0, 0},
  {"CommandHandler", 0, 0, 0},
  {"IMUProcessor", 0, 0, 0},
  {"ADProcessor", 0, 0, 0},
  {"ESP32Interface", 0, 0, 0},
  {"NAVProcessor", 0, 0, 0},
  {"RTCMProcessor", 0, 0, 0},
  {"AutosteerProcessor", 0, 0, 0},
  {"MotorDriver", 0, 0, 0},
  {"EncoderProcessor", 0, 0, 0},
  {"MachineProcessor", 0, 0, 0},
  {"LED Update", 0, 0, 0},
  {"WebManager.handle", 0, 0, 0},
  {"WebManager.broadcast", 0, 0, 0},
  {"PWMProcessor", 0, 0, 0},
  {"GPS1 Serial", 0, 0, 0},
  {"GPS2 Serial", 0, 0, 0}
};

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

// Function to toggle process timing
void toggleProcessTiming() {
  processTimingEnabled = !processTimingEnabled;
  if (processTimingEnabled) {
    // Reset all timing data
    for (size_t i = 0; i < sizeof(processTiming)/sizeof(processTiming[0]); i++) {
      processTiming[i].totalTime = 0;
      processTiming[i].count = 0;
      processTiming[i].maxTime = 0;
    }
    LOG_INFO(EventSource::SYSTEM, "Process timing diagnostics ENABLED");
  } else {
    // Print final report directly to Serial to bypass EventLogger buffering
    Serial.println("\r\n=== Process Timing Report ===");

    float totalAvgTime = 0;
    uint32_t mainLoopCount = 0;

    // Find the loop count from a process that runs every iteration
    if (processTiming[0].count > 0) {
      mainLoopCount = processTiming[0].count;
    }

    for (size_t i = 0; i < sizeof(processTiming)/sizeof(processTiming[0]); i++) {
      if (processTiming[i].count > 0) {
        float avgTime = (float)processTiming[i].totalTime / (float)processTiming[i].count;

        // For accurate total, scale by actual execution frequency
        if (mainLoopCount > 0) {
          float scaledAvg = avgTime * ((float)processTiming[i].count / (float)mainLoopCount);
          totalAvgTime += scaledAvg;
        }

        Serial.printf("%2d. %-20s: avg=%6.1fus max=%6luus (n=%lu)\r\n",
                      i, processTiming[i].name, avgTime, processTiming[i].maxTime, processTiming[i].count);
      }
    }

    Serial.printf("============================\r\n");
    Serial.printf("Total avg time per loop: %.1f us\r\n", totalAvgTime);
    Serial.printf("Theoretical max frequency: %.1f kHz\r\n", 1000.0f / totalAvgTime);
    Serial.println("============================");

    LOG_INFO(EventSource::SYSTEM, "Process timing diagnostics DISABLED");
  }
}

// ============================================
// SimpleScheduler Task Wrapper Functions
// ============================================

// Every Loop Tasks (no timing check)
void taskEthernetLoop() {
  Ethernet.loop();  // REQUIRED for QNEthernet!
}

void taskQNetworkPoll() {
  QNetworkBase::poll();
}

void taskUDPPoll() {
  QNEthernetUDPHandler::poll();
}

void taskGPS1Serial() {
  if (SerialGPS1.available()) {
    char c = SerialGPS1.read();
    gnssProcessor.processNMEAChar(c);
  }
}

void taskGPS2Serial() {
  if (SerialGPS2.available()) {
    uint8_t b = SerialGPS2.read();
    gnssProcessor.processUBXByte(b);
  }
}

// 100Hz Tasks (10ms)
void taskAutosteer() {
  AutosteerProcessor::getInstance()->process();
}

void taskWebHandleClient() {
  webManager.handleClient();
}

void taskWebBroadcastTelemetry() {
  webManager.broadcastTelemetry();
}

// 50Hz Tasks (20ms)
void taskMotorDriver() {
  if (motorPTR) {
    motorPTR->process();
  }
}

// 10Hz Tasks (100ms)
void taskLEDUpdate() {
  ledManagerFSM.updateAll();
}

/*void taskBufferStats() {
  serialManager.updateBufferStats();
}*/

void taskNetworkCheck() {
  EventLogger::getInstance()->checkNetworkReady();
}

void taskNAVProcess() {
  NAVProcessor::getInstance()->process();
}

void taskKickoutSendPGN250() {
  KickoutMonitor::getInstance()->sendPGN250();
}

// 1Hz Tasks (1000ms)
// Reserved for future slow updates

// 0.2Hz Tasks (5000ms)
// Reserved for very slow status checks

void setup()
{
  //delay(5000); // delay for time to start monitor
  Serial.begin(115200);

  Serial.print("\r\n\n=== New Dawn v");
  Serial.print(FIRMWARE_VERSION);
  Serial.print(" === ");
  Serial.print(BOARD_VERSION);
  Serial.print("(Teensy 4.1 w/ETH) ===\r\n");
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
  
  // Set CAN bus speeds based on configuration
  CANSteerConfig canConfig = configManager.getCANSteerConfig();
  setCAN1Speed(canConfig.can1Speed == 1 ? 500000 : 250000);
  setCAN2Speed(canConfig.can2Speed == 1 ? 500000 : 250000);
  setCAN3Speed(canConfig.can3Speed == 1 ? 500000 : 250000);

  // Initialize global CAN buses
  initializeGlobalCANBuses();
  
  // Initialize RTCMProcessor
  RTCMProcessor::init(SerialGPS1, SerialRadio);
  LOG_INFO(EventSource::SYSTEM, "RTCMProcessor initialized");

  // Initialize HardwareManager
  if (hardwareManager.initializeHardware())
  {
    LOG_INFO(EventSource::SYSTEM, "HardwareManager initialized");
    
    // Quick buzzer beep to indicate hardware is ready
    hardwareManager.performBuzzerTest();
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
  //esp32Interface.init();
  //LOG_INFO(EventSource::SYSTEM, "ESP32Interface initialized");

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

  // ============================================
  // Initialize SimpleScheduler
  // ============================================
  LOG_INFO(EventSource::SYSTEM, "Initializing SimpleScheduler...");

  // Add EVERY_LOOP tasks (no timing)
  scheduler.addTask(SimpleScheduler::EVERY_LOOP, taskEthernetLoop, "Ethernet Loop");
  scheduler.addTask(SimpleScheduler::EVERY_LOOP, taskQNetworkPoll, "QNetwork Poll");
  scheduler.addTask(SimpleScheduler::EVERY_LOOP, taskUDPPoll, "UDP Poll");
  scheduler.addTask(SimpleScheduler::EVERY_LOOP, taskGPS1Serial, "GPS1 Serial");
  scheduler.addTask(SimpleScheduler::EVERY_LOOP, taskGPS2Serial, "GPS2 Serial");

  // Add these as EVERY_LOOP for now (they have no timing currently)
  scheduler.addTask(SimpleScheduler::EVERY_LOOP, []{ imuProcessor.process(); }, "IMU");
  scheduler.addTask(SimpleScheduler::EVERY_LOOP, []{ adProcessor.process(); }, "ADProcessor");
  scheduler.addTask(SimpleScheduler::EVERY_LOOP, []{ esp32Interface.process(); }, "ESP32");
  scheduler.addTask(SimpleScheduler::EVERY_LOOP, []{ RTCMProcessor::getInstance()->process(); }, "RTCM");
  scheduler.addTask(SimpleScheduler::EVERY_LOOP, []{ EncoderProcessor::getInstance()->process(); }, "Encoder");
  scheduler.addTask(SimpleScheduler::EVERY_LOOP, []{ MachineProcessor::getInstance()->process(); }, "Machine");
  scheduler.addTask(SimpleScheduler::EVERY_LOOP, []{ pwmProcessor.process(); }, "PWM");
  scheduler.addTask(SimpleScheduler::EVERY_LOOP, []{ KickoutMonitor::getInstance()->process(); }, "Kickout Monitor");

  // Add 100Hz tasks (critical timing)
  scheduler.addTask(SimpleScheduler::HZ_100, taskAutosteer, "Autosteer");
  scheduler.addTask(SimpleScheduler::HZ_100, taskWebHandleClient, "Web Client");
  scheduler.addTask(SimpleScheduler::HZ_100, taskWebBroadcastTelemetry, "Web Telemetry");

  // Add 50Hz tasks (motor control)
  scheduler.addTask(SimpleScheduler::HZ_50, taskMotorDriver, "Motor Driver");

  // Add 10Hz tasks (UI and status)
  scheduler.addTask(SimpleScheduler::HZ_10, taskLEDUpdate, "LED Update");
  scheduler.addTask(SimpleScheduler::HZ_10, taskNetworkCheck, "Network Check");
  scheduler.addTask(SimpleScheduler::HZ_10, taskNAVProcess, "NAV Process");
  scheduler.addTask(SimpleScheduler::HZ_10, taskKickoutSendPGN250, "PGN250 Send");
  scheduler.addTask(SimpleScheduler::HZ_10, []{ serialManager.updateBufferStats(); }, "Buffer Stats");
  scheduler.addTask(SimpleScheduler::HZ_10, []{ CommandHandler::getInstance()->process(); }, "CommandHandler");

  // the scheduler is counting tasks per group, why not just report that?
  LOG_INFO(EventSource::SYSTEM, "SimpleScheduler initialized with %d tasks",
           5 + 8 + 3 + 1 + 5); // EVERY_LOOP + 100Hz + 50Hz + 10Hz

  serialManager.startBufferMonitoring();

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

// Macro for timing a process
#define TIME_PROCESS(index, code) \
  if (processTimingEnabled) { \
    uint32_t start = micros(); \
    code; \
    uint32_t elapsed = micros() - start; \
    processTiming[index].totalTime += elapsed; \
    processTiming[index].count++; \
    if (elapsed > processTiming[index].maxTime) { \
      processTiming[index].maxTime = elapsed; \
    } \
  } else { \
    code; \
  }

void loop()
{
  // OTA updates are handled via web interface

  // ============================================
  // Run SimpleScheduler
  // ============================================
  scheduler.run();


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