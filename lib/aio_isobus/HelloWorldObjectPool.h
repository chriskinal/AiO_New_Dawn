// HelloWorldObjectPool.h
// Simple Hello World object pool for ISOBUS VT demonstration

#ifndef HELLO_WORLD_OBJECT_POOL_H
#define HELLO_WORLD_OBJECT_POOL_H

#include <Arduino.h>

// Object IDs for our simple pool
enum HelloWorldObjectIDs {
    HelloWorld_WorkingSet = 0,
    HelloWorld_DataMask = 1000,
    HelloWorld_Container = 2000,
    HelloWorld_SoftKeyMask = 3000,
    HelloWorld_OutputString = 4000,
    HelloWorld_OutputNumber = 4001,
    HelloWorld_SoftKey_Exit = 5000,
    HelloWorld_SoftKey_Increment = 5001,
    HelloWorld_StringVariable = 7000,
    HelloWorld_NumberVariable = 7001
};

// Binary object pool for Hello World VT display
// This creates a simple screen with "Hello World" text and a counter
// Note: Multi-byte values are little-endian as per ISO 11783
const uint8_t HelloWorldObjectPool[] PROGMEM = {
    // Working Set Object (ID: 0)
    0x00, 0x00,                    // Object ID: 0
    0x00,                          // Object Type: Working Set
    0xF0,                          // Background color: White
    0x01,                          // Selectable
    0xE8, 0x03,                    // Active Mask ID: 1000 (DataMask)
    0x06, 0x00,                    // Number of object references: 6
    0xB8, 0x0B, 0x1E, 0x00,        // Ref 1: SoftKeyMask (3000)
    0xE8, 0x03, 0x1D, 0x00,        // Ref 2: DataMask (1000)
    0xD0, 0x07, 0x00, 0x00,        // Ref 3: Container (2000)
    0xA0, 0x0F, 0x12, 0x00,        // Ref 4: OutputString (4000)
    0xA1, 0x0F, 0x0C, 0x00,        // Ref 5: OutputNumber (4001)
    0x88, 0x13, 0x06, 0x00,        // Ref 6: SoftKey Exit (5000)
    0x89, 0x13, 0x06, 0x00,        // Ref 7: SoftKey Increment (5001)
    
    // Data Mask Object (ID: 1000)
    0xE8, 0x03,                    // Object ID: 1000
    0x01,                          // Object Type: Data Mask
    0xF0,                          // Background color: White
    0xB8, 0x0B,                    // Soft Key Mask ID: 3000
    0x01, 0x00,                    // Number of object refs: 1
    0xD0, 0x07, 0x00, 0x00,        // Container ID: 2000
    0x01, 0x00,                    // Number of macro refs: 1
    0xFF,                          // Macro command: Hide/Show Container
    0xD0, 0x07,                    // Container ID: 2000
    0x01,                          // Show
    
    // Container Object (ID: 2000)
    0xD0, 0x07,                    // Object ID: 2000
    0x00,                          // Object Type: Container
    0x00, 0x01,                    // Width: 256
    0x00, 0x01,                    // Height: 256
    0x01,                          // Hidden: No
    0x02, 0x00,                    // Number of object refs: 2
    0xA0, 0x0F, 0x00, 0x00,        // OutputString ID: 4000, X: 0
    0x00, 0x00,                    // Y: 0
    0xA1, 0x0F, 0x00, 0x00,        // OutputNumber ID: 4001, X: 0
    0x50, 0x00,                    // Y: 80
    
    // Soft Key Mask Object (ID: 3000)
    0xB8, 0x0B,                    // Object ID: 3000
    0x02,                          // Object Type: Soft Key Mask
    0xF0,                          // Background color: White
    0x02, 0x00,                    // Number of object refs: 2
    0x88, 0x13,                    // SoftKey Exit: 5000
    0x89, 0x13,                    // SoftKey Increment: 5001
    
    // Output String Object (ID: 4000)
    0xA0, 0x0F,                    // Object ID: 4000
    0x12,                          // Object Type: Output String
    0x00, 0x01,                    // Width: 256
    0x50, 0x00,                    // Height: 80
    0xF0,                          // Background color: White
    0x00, 0x00,                    // Font attributes ID: 0 (default)
    0x01,                          // Options: Transparent
    0x58, 0x1B,                    // Variable Reference: 7000
    0x01,                          // Justification: Left
    0x0C, 0x00,                    // String length: 12
    'H', 0x00, 'e', 0x00, 'l', 0x00, 'l', 0x00, 'o', 0x00, ' ', 0x00,
    'W', 0x00, 'o', 0x00, 'r', 0x00, 'l', 0x00, 'd', 0x00, '!', 0x00,
    
    // Output Number Object (ID: 4001)
    0xA1, 0x0F,                    // Object ID: 4001
    0x0C,                          // Object Type: Output Number
    0x00, 0x01,                    // Width: 256
    0x50, 0x00,                    // Height: 80
    0xF0,                          // Background color: White
    0x00, 0x00,                    // Font attributes ID: 0
    0x01,                          // Options: Transparent
    0x59, 0x1B,                    // Variable Reference: 7001
    0x00, 0x00, 0x00, 0x00,        // Value: 0
    0x00, 0x00, 0x00, 0x00,        // Offset: 0
    0x00,                          // Scale: 0
    0x00,                          // Number of decimals: 0
    0x01,                          // Format: Fixed
    0x01,                          // Justification: Left
    
    // Soft Key Object - Exit (ID: 5000)
    0x88, 0x13,                    // Object ID: 5000
    0x06,                          // Object Type: Soft Key
    0xF0,                          // Background color: White
    0x00,                          // Key code: 0
    0x00, 0x00,                    // Object ID of label: 0 (none)
    0x04, 0x00,                    // String length: 4
    'E', 0x00, 'x', 0x00, 'i', 0x00, 't', 0x00,
    
    // Soft Key Object - Increment (ID: 5001)
    0x89, 0x13,                    // Object ID: 5001
    0x06,                          // Object Type: Soft Key
    0xF0,                          // Background color: White
    0x01,                          // Key code: 1
    0x00, 0x00,                    // Object ID of label: 0 (none)
    0x02, 0x00,                    // String length: 2
    '+', 0x00, '1', 0x00
};

const uint16_t HelloWorldObjectPoolSize = sizeof(HelloWorldObjectPool);

#endif // HELLO_WORLD_OBJECT_POOL_H