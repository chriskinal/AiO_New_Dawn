#ifndef TURN_SENSOR_TYPES_H
#define TURN_SENSOR_TYPES_H

#include <Arduino.h>

// Turn sensor types matching AgOpenGPS
enum class TurnSensorType : uint8_t {
    NONE = 0,
    ENCODER = 1,      // Rotary encoder (single or quadrature)
    PRESSURE = 2,     // Hydraulic pressure sensor
    CURRENT = 3,      // Motor current sensor
    JD_PWM = 4        // John Deere PWM encoder
};

// Encoder types
enum class EncoderType : uint8_t {
    SINGLE = 1,       // Single channel encoder
    QUADRATURE = 2    // Quadrature (dual channel) encoder
};

#endif // TURN_SENSOR_TYPES_H