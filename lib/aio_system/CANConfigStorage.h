// CANConfigStorage.h
// Manages CAN configuration JSON storage in LittleFS flash memory

#ifndef CAN_CONFIG_STORAGE_H
#define CAN_CONFIG_STORAGE_H

#include <Arduino.h>
#include <LittleFS.h>
#include "EventLogger.h"

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
            LOG_INFO(EventSource::CONFIG, "readCustomConfig: No custom config file exists");
            return String();
        }

        File file = getFS().open("/can_config.json", FILE_READ);
        if (!file) {
            LOG_ERROR(EventSource::CONFIG, "readCustomConfig: Failed to open file");
            return String();
        }

        size_t fileSize = file.size();
        LOG_INFO(EventSource::CONFIG, "readCustomConfig: File opened, size = %d bytes", fileSize);

        // Use the file's readString() method - simplest approach
        String content = file.readString();
        file.close();

        LOG_INFO(EventSource::CONFIG, "readCustomConfig: Read complete, String length = %d bytes", content.length());

        return content;
    }

    // Write custom configuration to flash
    static bool writeCustomConfig(const String& jsonContent) {
        LOG_INFO(EventSource::CONFIG, "writeCustomConfig: Starting write of %d bytes", jsonContent.length());

        // Delete existing file first to ensure we don't append
        if (hasCustomConfig()) {
            LOG_INFO(EventSource::CONFIG, "writeCustomConfig: Deleting existing file");
            getFS().remove("/can_config.json");
        }

        // Now open for writing (creates new file)
        File file = getFS().open("/can_config.json", FILE_WRITE);
        if (!file) {
            LOG_ERROR(EventSource::CONFIG, "writeCustomConfig: Failed to open file for writing");
            return false;
        }

        // Use write() instead of print() to avoid any character interpretation
        size_t written = file.write((const uint8_t*)jsonContent.c_str(), jsonContent.length());
        file.close();

        LOG_INFO(EventSource::CONFIG, "writeCustomConfig: Wrote %d bytes, expected %d bytes", written, jsonContent.length());

        // Verify by reading back the file size
        size_t verifySize = getCustomConfigSize();
        LOG_INFO(EventSource::CONFIG, "writeCustomConfig: Verified file size on disk = %d bytes", verifySize);

        bool success = (written == jsonContent.length());
        if (!success) {
            LOG_ERROR(EventSource::CONFIG, "writeCustomConfig: Write size mismatch!");
        }

        return success;
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
