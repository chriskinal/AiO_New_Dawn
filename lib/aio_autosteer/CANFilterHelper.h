// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

// CANFilterHelper.h
// Hardware mailbox filtering for CAN buses to reduce RX buffer requirements

#ifndef CAN_FILTER_HELPER_H
#define CAN_FILTER_HELPER_H

#include <stdint.h>

// CAN functions that can be assigned to buses
enum class CANFunction : uint8_t {
    NONE = 0,
    STEERING = 1,
    IMPLEMENTS = 2,
    MACHINE_DATA = 4
};

class CANFilterHelper {
public:
    // Apply filters based on brand and function configuration
    static void applyFilters(uint8_t busNum, uint8_t brand, uint8_t functions);

private:
    // Brand-specific filter setup (9 tractor brands)
    static void setupFendtFilters(uint8_t busNum, uint8_t functions);
    static void setupJohnDeereFilters(uint8_t busNum, uint8_t functions);
    static void setupCaseIHFilters(uint8_t busNum, uint8_t functions);
    static void setupNewHollandFilters(uint8_t busNum, uint8_t functions);
    static void setupMasseyFergusonFilters(uint8_t busNum, uint8_t functions);
    static void setupValtraFilters(uint8_t busNum, uint8_t functions);
    static void setupClaasFilters(uint8_t busNum, uint8_t functions);
    static void setupSameDeutzFilters(uint8_t busNum, uint8_t functions);
    static void setupKubotaFilters(uint8_t busNum, uint8_t functions);
};

#endif // CAN_FILTER_HELPER_H
