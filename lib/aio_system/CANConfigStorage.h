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

        size_t fileSize = file.size();
        String content;
        content.reserve(fileSize);

        // Read in 512-byte chunks to avoid String corruption
        const size_t CHUNK_SIZE = 512;
        char buffer[CHUNK_SIZE + 1];  // +1 for null terminator

        while (file.available()) {
            size_t bytesToRead = min((size_t)file.available(), CHUNK_SIZE);
            size_t bytesRead = file.read((uint8_t*)buffer, bytesToRead);
            buffer[bytesRead] = '\0';  // Null terminate
            content += buffer;
        }

        file.close();

        return content;
    }

    // Write custom configuration to flash
    static bool writeCustomConfig(const String& jsonContent) {
        File file = getFS().open("/can_config.json", FILE_WRITE);
        if (!file) {
            return false;
        }

        size_t written = file.print(jsonContent);
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
