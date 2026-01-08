// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

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
    // NOTE: This loads the entire file into RAM - use with caution for large files
    static String readCustomConfig() {
        if (!hasCustomConfig()) {
            return String();
        }

        File file = getFS().open("/can_config.json", FILE_READ);
        if (!file) {
            return String();
        }

        String content = file.readString();
        file.close();

        return content;
    }

    // Write custom configuration to flash
    static bool writeCustomConfig(const String& jsonContent) {
        // Delete existing file first to ensure we don't append
        if (hasCustomConfig()) {
            getFS().remove("/can_config.json");
        }

        File file = getFS().open("/can_config.json", FILE_WRITE);
        if (!file) {
            return false;
        }

        // Write binary data to avoid character interpretation
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
