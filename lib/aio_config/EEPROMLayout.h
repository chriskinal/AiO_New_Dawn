// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

#ifndef EEPROM_LAYOUT_H
#define EEPROM_LAYOUT_H

// EEPROM Version - increment this when EEPROM layout changes
#define EEPROM_VERSION 111  // Added JD PWM encoder configuration

// EEPROM Address Map
#define EE_VERSION_ADDR      1      // Version number (2 bytes)
#define NETWORK_CONFIG_ADDR  100    // Network configuration (100-199)
#define STEER_CONFIG_ADDR    200    // Steer configuration (200-299)
#define STEER_SETTINGS_ADDR  300    // Steer settings (300-399)
#define GPS_CONFIG_ADDR      400    // GPS configuration (400-499)
#define MACHINE_CONFIG_ADDR  500    // Machine configuration (500-599)
#define KWAS_CONFIG_ADDR     600    // KWAS configuration (600-699)
#define INS_CONFIG_ADDR      700    // INS configuration (700-799)
#define EVENT_CONFIG_ADDR    800    // EventLogger configuration (800-899)
#define WEB_CONFIG_ADDR      900    // Web interface configuration (900-999)
#define TURN_SENSOR_CONFIG_ADDR 1000 // Turn sensor configuration (1000-1099)
#define ANALOG_WORK_SWITCH_ADDR 1100 // Analog work switch configuration (1100-1199)
#define MISC_CONFIG_ADDR        1200 // Miscellaneous settings (1200-1299)

#endif // EEPROM_LAYOUT_H