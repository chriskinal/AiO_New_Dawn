// CANConfigStorage.h
// Manages CAN configuration JSON storage in LittleFS flash memory

#ifndef CAN_CONFIG_STORAGE_H
#define CAN_CONFIG_STORAGE_H

#include <Arduino.h>
#include <LittleFS.h>

class CANConfigStorage {
public:
    // Get or create the filesystem instance
    static LittleFS_Program& getFS() {
        static LittleFS_Program fs;
        return fs;
    }

    // Initialize LittleFS (call once at startup)
    static bool init() {
        static bool initialized = false;
        if (!initialized) {
            if (!getFS().begin(1024 * 1024)) {  // 1MB for program flash
                return false;
            }
            initialized = true;
        }
        return true;
    }

    // Check if custom configuration exists
    static bool hasCustomConfig() {
        return getFS().exists("/can_config.json");
    }

    // Read custom configuration from flash
    // Returns empty string if no custom config exists
    static String readCustomConfig() {
        if (!hasCustomConfig()) {
            return String();
        }

        File file = getFS().open("/can_config.json", FILE_READ);
        if (!file) {
            return String();
        }

        // Get file size and allocate buffer
        size_t fileSize = file.size();

        // Allocate buffer with extra space
        char* buffer = (char*)malloc(fileSize + 1);
        if (!buffer) {
            file.close();
            return String();
        }

        // Binary read into buffer
        size_t bytesRead = file.read((uint8_t*)buffer, fileSize);
        file.close();

        // Null terminate
        buffer[bytesRead] = '\0';

        // Create string from buffer with explicit copy
        String content;
        content.reserve(bytesRead);
        content = buffer;

        free(buffer);

        return content;
    }

    // Write custom configuration to flash
    static bool writeCustomConfig(const String& jsonContent) {
        // Delete existing file first to ensure we don't append
        if (hasCustomConfig()) {
            getFS().remove("/can_config.json");
        }

        // Now open for writing (creates new file)
        File file = getFS().open("/can_config.json", FILE_WRITE);
        if (!file) {
            return false;
        }

        // Use write() instead of print() to avoid any character interpretation
        size_t written = file.write((const uint8_t*)jsonContent.c_str(), jsonContent.length());
        file.close();

        return (written == jsonContent.length());
    }

    // Delete custom configuration (restore to default)
    static bool deleteCustomConfig() {
        if (!hasCustomConfig()) {
            return true; // Already deleted
        }

        return getFS().remove("/can_config.json");
    }

    // Get size of custom configuration file
    static size_t getCustomConfigSize() {
        if (!hasCustomConfig()) {
            return 0;
        }

        File file = getFS().open("/can_config.json", FILE_READ);
        if (!file) {
            return 0;
        }

        size_t size = file.size();
        file.close();

        return size;
    }
};

#endif // CAN_CONFIG_STORAGE_H
