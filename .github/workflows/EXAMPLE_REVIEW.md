# PR AI Code Review - Example Output

This document shows examples of what the AI code review workflow will look like when it analyzes pull requests.

## Example 1: Architectural Violation

### PR Changes
```cpp
// lib/aio_autosteer/AutosteerProcessor.cpp

void AutosteerProcessor::init() {
  // WRONG: Registering for PGN 200 (broadcast message)
  PGNProcessor::getInstance()->registerCallback(200, this);
  PGNProcessor::getInstance()->registerCallback(202, this);  // Also wrong
  PGNProcessor::getInstance()->registerCallback(251, this);  // OK
}
```

### AI Review Comment

**Severity**: CRITICAL

**File**: `lib/aio_autosteer/AutosteerProcessor.cpp` lines 45-48

**Issue**: Registering for broadcast PGNs 200 and 202 violates AiO New Dawn architecture.

**Recommendation**: Use `registerBroadcastCallback()` instead:
```cpp
void AutosteerProcessor::init() {
  // Use broadcast callback for PGN 200 and 202
  PGNProcessor::getInstance()->registerBroadcastCallback(this);
  PGNProcessor::getInstance()->registerCallback(251, this);  // OK - specific PGN
}
```

**Rationale**: Per CLAUDE.md architecture documentation:
> PGN 200 and 202 are broadcast messages - modules should NOT register for these
> Use `registerBroadcastCallback()` to receive all broadcast messages

Registering for broadcast PGNs can cause message routing conflicts and unpredictable behavior.

---

## Example 2: Hardware Safety Issue

### PR Changes
```cpp
// lib/aio_autosteer/sensors/WheelAngleSensor.cpp

void WheelAngleSensor::init() {
  pinMode(WAS_PIN, INPUT);  // Configure analog pin
  wasValue = analogRead(WAS_PIN);
}
```

### AI Review Comment

**Severity**: CRITICAL

**File**: `lib/aio_autosteer/sensors/WheelAngleSensor.cpp` line 23

**Issue**: Using `INPUT` mode for analog pin. This is incorrect for Teensy ADC pins.

**Recommendation**: Use `INPUT_DISABLE` for analog pins:
```cpp
void WheelAngleSensor::init() {
  pinMode(WAS_PIN, INPUT_DISABLE);  // Correct for analog pins
  wasValue = analogRead(WAS_PIN);
}
```

**Rationale**: Per CLAUDE.md:
> Pin modes: Use `INPUT_DISABLE` for analog pins, not `INPUT`

Using `INPUT` mode on analog pins can cause incorrect readings and potential hardware conflicts on Teensy 4.1.

---

## Example 3: Real-Time Constraint Violation

### PR Changes
```cpp
// lib/aio_autosteer/AutosteerProcessor.cpp

void AutosteerProcessor::update() {
  // This runs at 100Hz - must complete in <10ms
  
  // Read sensor data
  currentAngle = wasReader->read();
  
  // PROBLEM: Blocking I2C read in critical loop
  for (int i = 0; i < 10; i++) {
    sensorData[i] = i2cDevice.readByte(0x10 + i);  // Can take 1ms each
  }
  
  // Calculate steering command
  steeringCommand = calculateSteering(currentAngle);
}
```

### AI Review Comment

**Severity**: CRITICAL

**File**: `lib/aio_autosteer/AutosteerProcessor.cpp` lines 156-164

**Issue**: Blocking I2C operations in 100Hz control loop can violate timing constraints.

**Recommendation**: Move I2C reads to a lower-priority task or use interrupt-driven I2C:
```cpp
void AutosteerProcessor::update() {
  // Critical path - must complete in <10ms
  currentAngle = wasReader->read();
  steeringCommand = calculateSteering(currentAngle);
}

// Separate method called at 10Hz or lower
void AutosteerProcessor::updateSlowSensors() {
  for (int i = 0; i < 10; i++) {
    sensorData[i] = i2cDevice.readByte(0x10 + i);
  }
}
```

**Rationale**: Per CLAUDE.md:
> AutosteerProcessor runs at 100Hz (10ms loop)

Each I2C transaction can take 0.5-2ms. Ten transactions could take 5-20ms, risking missed timing deadlines. This could cause steering instability.

---

## Example 4: Code Style Issue

### PR Changes
```cpp
// lib/aio_system/NetworkManager.cpp

bool NetworkManager::connect() {
  IPAddress static_ip(192, 168, 5, 126);
  IPAddress Gateway(192, 168, 5, 1);
  IPAddress subnet_mask(255, 255, 255, 0);
  
  Ethernet.begin(MacAddress, static_ip, Gateway, subnet_mask);
  return true;
}
```

### AI Review Comment

**Severity**: IMPORTANT

**File**: `lib/aio_system/NetworkManager.cpp` lines 34-40

**Issue**: Inconsistent naming conventions (PascalCase for local variables).

**Recommendation**: Use consistent naming per C++ conventions:
```cpp
bool NetworkManager::connect() {
  IPAddress staticIP(192, 168, 5, 126);
  IPAddress gateway(192, 168, 5, 1);
  IPAddress subnetMask(255, 255, 255, 0);

  Ethernet.begin(MacAddress, staticIP, gateway, subnetMask);
  return true;
}
```

**Rationale**: Project uses camelCase for local variables and parameters. `Gateway` and `subnet_mask` are inconsistent. Consistent naming improves code readability and maintainability.

---

## Example 5: Missing Error Handling

### PR Changes
```cpp
// lib/aio_navigation/GNSSProcessor.cpp

void GNSSProcessor::parseNMEA(const char* sentence) {
  // Parse NMEA sentence
  char* token = strtok((char*)sentence, ",");
  
  // Extract latitude
  token = strtok(NULL, ",");
  latitude = atof(token);
  
  // Extract longitude  
  token = strtok(NULL, ",");
  longitude = atof(token);
}
```

### AI Review Comment

**Severity**: IMPORTANT

**File**: `lib/aio_navigation/GNSSProcessor.cpp` lines 89-99

**Issue**: Missing null pointer checks for `strtok()` return values.

**Recommendation**: Add validation before dereferencing:
```cpp
void GNSSProcessor::parseNMEA(const char* sentence) {
  if (!sentence) return;
  
  char* token = strtok((char*)sentence, ",");
  if (!token) return;
  
  // Extract latitude
  token = strtok(NULL, ",");
  if (!token) {
    EventLogger::getInstance()->logError("Invalid NMEA: missing latitude");
    return;
  }
  latitude = atof(token);
  
  // Extract longitude
  token = strtok(NULL, ",");
  if (!token) {
    EventLogger::getInstance()->logError("Invalid NMEA: missing longitude");
    return;
  }
  longitude = atof(token);
}
```

**Rationale**: Malformed NMEA sentences (corrupted serial data, GPS startup messages) can cause `strtok()` to return NULL. Dereferencing NULL pointers causes crashes. Proper error handling ensures system stability even with bad input.

---

## Example 6: Good Code (No Issues)

### PR Changes
```cpp
// lib/aio_autosteer/MotorDriverManager.cpp

bool MotorDriverManager::init() {
  // Check pin ownership before configuring
  if (!PinOwnershipManager::getInstance()->requestPin(MOTOR_PWM_PIN, "MotorDriver")) {
    EventLogger::getInstance()->logError("Failed to claim motor PWM pin");
    return false;
  }
  
  // Initialize based on detected motor type
  switch (config.motorType) {
    case MOTOR_TYPE_CYTRON:
      driver = new PWMMotorDriver();
      break;
    case MOTOR_TYPE_KEYA:
      driver = new KeyaCANDriver();
      break;
    case MOTOR_TYPE_DANFOSS:
      driver = new DanfossMotorDriver();
      break;
    default:
      EventLogger::getInstance()->logError("Unknown motor type");
      return false;
  }
  
  return driver->init();
}
```

### AI Review Comment

✅ **No issues found**. Code changes follow project standards and architecture:

- ✅ Proper use of `PinOwnershipManager` before claiming hardware resources
- ✅ Appropriate error handling with `EventLogger`
- ✅ Follows motor driver abstraction pattern
- ✅ Consistent naming conventions (camelCase variables, PascalCase classes)
- ✅ Clear control flow with early returns

Good work following the AiO New Dawn architecture patterns!

---

## Example 7: Suggestion for Improvement

### PR Changes
```cpp
// lib/aio_system/CommandHandler.cpp

void CommandHandler::processCommand(char cmd) {
  if (cmd == 'e') {
    // Toggle event logging
    bool enabled = !EventLogger::getInstance()->isEnabled();
    EventLogger::getInstance()->setEnabled(enabled);
  } else if (cmd == 'u') {
    // Show current sensor readings
    printSensorData();
  } else if (cmd == 'd') {
    // Debug mode toggle
    debugMode = !debugMode;
  }
}
```

### AI Review Comment

**Severity**: SUGGESTION

**File**: `lib/aio_system/CommandHandler.cpp` lines 45-57

**Issue**: Long if-else chain could be more maintainable as a switch statement.

**Recommendation**: Consider using a switch statement:
```cpp
void CommandHandler::processCommand(char cmd) {
  switch (cmd) {
    case 'e':
      // Toggle event logging
      bool enabled = !EventLogger::getInstance()->isEnabled();
      EventLogger::getInstance()->setEnabled(enabled);
      break;
      
    case 'u':
      // Show current sensor readings
      printSensorData();
      break;
      
    case 'd':
      // Debug mode toggle
      debugMode = !debugMode;
      break;
      
    default:
      // Unknown command
      Serial.println("Unknown command. Press '?' for help.");
      break;
  }
}
```

**Rationale**: Switch statements are often clearer for command dispatching and make it easier to add new commands. The `default` case also handles unknown commands gracefully. Not critical, but improves code maintainability.

---

## Summary

The AI code review workflow provides:

1. **Critical Issues**: Must-fix problems (architecture violations, safety issues, timing problems)
2. **Important Issues**: Should-fix problems (style violations, missing error handling)
3. **Suggestions**: Nice-to-have improvements (refactoring, clarity)

Each review:
- References specific files and line numbers
- Explains the problem clearly
- Provides concrete fix recommendations
- Explains why it matters in the context of AiO New Dawn

The AI reviewer is a helpful tool but should complement (not replace) human code review, especially for safety-critical embedded systems code.
