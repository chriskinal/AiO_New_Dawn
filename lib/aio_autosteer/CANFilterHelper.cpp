// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

// CANFilterHelper.cpp
// Hardware mailbox filtering implementation for CAN buses

#include "CANFilterHelper.h"
#include "CANGlobals.h"
#include "EventLogger.h"

void CANFilterHelper::applyFilters(uint8_t busNum, uint8_t brand, uint8_t functions) {
    // Brand mapping: 0=None, 1=Fendt, 2=JD, 3=CaseIH, 4=NH, 5=MF, 6=Valtra, 7=Claas, 8=Same/Deutz, 9=Kubota

    switch (brand) {
        case 1:
            setupFendtFilters(busNum, functions);
            break;
        case 2:
            setupJohnDeereFilters(busNum, functions);
            break;
        case 3:
            setupCaseIHFilters(busNum, functions);
            break;
        case 4:
            setupNewHollandFilters(busNum, functions);
            break;
        case 5:
            setupMasseyFergusonFilters(busNum, functions);
            break;
        case 6:
            setupValtraFilters(busNum, functions);
            break;
        case 7:
            setupClaasFilters(busNum, functions);
            break;
        case 8:
            setupSameDeutzFilters(busNum, functions);
            break;
        case 9:
            setupKubotaFilters(busNum, functions);
            break;
        default:
            LOG_INFO(EventSource::CAN, "CAN%d: No brand filtering (brand=%d)", busNum, brand);
            break;
    }
}

void CANFilterHelper::setupFendtFilters(uint8_t busNum, uint8_t functions) {
    switch (busNum) {
        case 1:
            if (functions & (uint8_t)CANFunction::STEERING) {
                globalCAN1.setMBFilter(MB0, 0x0CEF2CF0);  // Valve status from steering controller
            }
            if (functions & (uint8_t)CANFunction::MACHINE_DATA) {
                globalCAN1.setMBFilter(MB1, 0x0CF00400);  // Wheel-based speed
            }
            LOG_INFO(EventSource::CAN, "CAN1: Fendt filters applied (functions=0x%02X)", functions);
            break;
        case 2:
            if (functions & (uint8_t)CANFunction::IMPLEMENTS) {
                globalCAN2.setMBFilter(MB0, 0x0CFEFF00);  // Hitch position
            }
            LOG_INFO(EventSource::CAN, "CAN2: Fendt filters applied (functions=0x%02X)", functions);
            break;
        case 3:
            if (functions & (uint8_t)CANFunction::MACHINE_DATA) {
                globalCAN3.setMBFilter(MB0, 0x18FEF100);  // Ground-based speed
            }
            LOG_INFO(EventSource::CAN, "CAN3: Fendt filters applied (functions=0x%02X)", functions);
            break;
    }
}

void CANFilterHelper::setupJohnDeereFilters(uint8_t busNum, uint8_t functions) {
    switch (busNum) {
        case 1:
            if (functions & (uint8_t)CANFunction::STEERING) {
                globalCAN1.setMBFilter(MB0, 0x0CF02C00);  // Steering valve command
            }
            if (functions & (uint8_t)CANFunction::MACHINE_DATA) {
                globalCAN1.setMBFilter(MB1, 0x0CFE6CEE);  // Wheel speed
            }
            LOG_INFO(EventSource::CAN, "CAN1: John Deere filters applied (functions=0x%02X)", functions);
            break;
        case 2:
            if (functions & (uint8_t)CANFunction::IMPLEMENTS) {
                globalCAN2.setMBFilter(MB0, 0x0CFE4800);  // Hitch position
            }
            LOG_INFO(EventSource::CAN, "CAN2: John Deere filters applied (functions=0x%02X)", functions);
            break;
        case 3:
            if (functions & (uint8_t)CANFunction::MACHINE_DATA) {
                globalCAN3.setMBFilter(MB0, 0x18FEF119);  // Ground speed
            }
            LOG_INFO(EventSource::CAN, "CAN3: John Deere filters applied (functions=0x%02X)", functions);
            break;
    }
}

void CANFilterHelper::setupCaseIHFilters(uint8_t busNum, uint8_t functions) {
    switch (busNum) {
        case 1:
            if (functions & (uint8_t)CANFunction::STEERING) {
                globalCAN1.setMBFilter(MB0, 0x0CFF3D21);  // Steering command
            }
            if (functions & (uint8_t)CANFunction::MACHINE_DATA) {
                globalCAN1.setMBFilter(MB1, 0x18FEF100);  // Wheel speed
            }
            LOG_INFO(EventSource::CAN, "CAN1: Case IH filters applied (functions=0x%02X)", functions);
            break;
        case 2:
            if (functions & (uint8_t)CANFunction::IMPLEMENTS) {
                globalCAN2.setMBFilter(MB0, 0x0CFEFF00);  // Hitch status
            }
            LOG_INFO(EventSource::CAN, "CAN2: Case IH filters applied (functions=0x%02X)", functions);
            break;
        case 3:
            if (functions & (uint8_t)CANFunction::MACHINE_DATA) {
                globalCAN3.setMBFilter(MB0, 0x18FEEE00);  // Ground speed
            }
            LOG_INFO(EventSource::CAN, "CAN3: Case IH filters applied (functions=0x%02X)", functions);
            break;
    }
}

void CANFilterHelper::setupNewHollandFilters(uint8_t busNum, uint8_t functions) {
    // New Holland uses similar CAN IDs to Case IH (same parent company)
    switch (busNum) {
        case 1:
            if (functions & (uint8_t)CANFunction::STEERING) {
                globalCAN1.setMBFilter(MB0, 0x0CFF3D21);  // Steering command
            }
            if (functions & (uint8_t)CANFunction::MACHINE_DATA) {
                globalCAN1.setMBFilter(MB1, 0x18FEF100);  // Wheel speed
            }
            LOG_INFO(EventSource::CAN, "CAN1: New Holland filters applied (functions=0x%02X)", functions);
            break;
        case 2:
            if (functions & (uint8_t)CANFunction::IMPLEMENTS) {
                globalCAN2.setMBFilter(MB0, 0x0CFEFF00);  // Hitch status
            }
            LOG_INFO(EventSource::CAN, "CAN2: New Holland filters applied (functions=0x%02X)", functions);
            break;
        case 3:
            if (functions & (uint8_t)CANFunction::MACHINE_DATA) {
                globalCAN3.setMBFilter(MB0, 0x18FEEE00);  // Ground speed
            }
            LOG_INFO(EventSource::CAN, "CAN3: New Holland filters applied (functions=0x%02X)", functions);
            break;
    }
}

void CANFilterHelper::setupMasseyFergusonFilters(uint8_t busNum, uint8_t functions) {
    switch (busNum) {
        case 1:
            if (functions & (uint8_t)CANFunction::STEERING) {
                globalCAN1.setMBFilter(MB0, 0x0CEF1C03);  // Steering valve
            }
            if (functions & (uint8_t)CANFunction::MACHINE_DATA) {
                globalCAN1.setMBFilter(MB1, 0x18FEF100);  // Wheel speed
            }
            LOG_INFO(EventSource::CAN, "CAN1: Massey Ferguson filters applied (functions=0x%02X)", functions);
            break;
        case 2:
            if (functions & (uint8_t)CANFunction::IMPLEMENTS) {
                globalCAN2.setMBFilter(MB0, 0x0CFEFF00);  // Hitch position
            }
            LOG_INFO(EventSource::CAN, "CAN2: Massey Ferguson filters applied (functions=0x%02X)", functions);
            break;
        case 3:
            if (functions & (uint8_t)CANFunction::MACHINE_DATA) {
                globalCAN3.setMBFilter(MB0, 0x18FEEE00);  // Ground speed
            }
            LOG_INFO(EventSource::CAN, "CAN3: Massey Ferguson filters applied (functions=0x%02X)", functions);
            break;
    }
}

void CANFilterHelper::setupValtraFilters(uint8_t busNum, uint8_t functions) {
    switch (busNum) {
        case 1:
            if (functions & (uint8_t)CANFunction::STEERING) {
                globalCAN1.setMBFilter(MB0, 0x0CEF2C03);  // Steering command
            }
            if (functions & (uint8_t)CANFunction::MACHINE_DATA) {
                globalCAN1.setMBFilter(MB1, 0x18FEF100);  // Wheel speed
            }
            LOG_INFO(EventSource::CAN, "CAN1: Valtra filters applied (functions=0x%02X)", functions);
            break;
        case 2:
            if (functions & (uint8_t)CANFunction::IMPLEMENTS) {
                globalCAN2.setMBFilter(MB0, 0x0CFEFF00);  // Hitch status
            }
            LOG_INFO(EventSource::CAN, "CAN2: Valtra filters applied (functions=0x%02X)", functions);
            break;
        case 3:
            if (functions & (uint8_t)CANFunction::MACHINE_DATA) {
                globalCAN3.setMBFilter(MB0, 0x18FEEE00);  // Ground speed
            }
            LOG_INFO(EventSource::CAN, "CAN3: Valtra filters applied (functions=0x%02X)", functions);
            break;
    }
}

void CANFilterHelper::setupClaasFilters(uint8_t busNum, uint8_t functions) {
    switch (busNum) {
        case 1:
            if (functions & (uint8_t)CANFunction::STEERING) {
                globalCAN1.setMBFilter(MB0, 0x0CFF2817);  // Steering valve
            }
            if (functions & (uint8_t)CANFunction::MACHINE_DATA) {
                globalCAN1.setMBFilter(MB1, 0x18FEF100);  // Wheel speed
            }
            LOG_INFO(EventSource::CAN, "CAN1: Claas filters applied (functions=0x%02X)", functions);
            break;
        case 2:
            if (functions & (uint8_t)CANFunction::IMPLEMENTS) {
                globalCAN2.setMBFilter(MB0, 0x0CFEFF00);  // Hitch position
            }
            LOG_INFO(EventSource::CAN, "CAN2: Claas filters applied (functions=0x%02X)", functions);
            break;
        case 3:
            if (functions & (uint8_t)CANFunction::MACHINE_DATA) {
                globalCAN3.setMBFilter(MB0, 0x18FEEE00);  // Ground speed
            }
            LOG_INFO(EventSource::CAN, "CAN3: Claas filters applied (functions=0x%02X)", functions);
            break;
    }
}

void CANFilterHelper::setupSameDeutzFilters(uint8_t busNum, uint8_t functions) {
    switch (busNum) {
        case 1:
            if (functions & (uint8_t)CANFunction::STEERING) {
                globalCAN1.setMBFilter(MB0, 0x0CEF1C17);  // Steering command
            }
            if (functions & (uint8_t)CANFunction::MACHINE_DATA) {
                globalCAN1.setMBFilter(MB1, 0x18FEF100);  // Wheel speed
            }
            LOG_INFO(EventSource::CAN, "CAN1: Same/Deutz-Fahr filters applied (functions=0x%02X)", functions);
            break;
        case 2:
            if (functions & (uint8_t)CANFunction::IMPLEMENTS) {
                globalCAN2.setMBFilter(MB0, 0x0CFEFF00);  // Hitch status
            }
            LOG_INFO(EventSource::CAN, "CAN2: Same/Deutz-Fahr filters applied (functions=0x%02X)", functions);
            break;
        case 3:
            if (functions & (uint8_t)CANFunction::MACHINE_DATA) {
                globalCAN3.setMBFilter(MB0, 0x18FEEE00);  // Ground speed
            }
            LOG_INFO(EventSource::CAN, "CAN3: Same/Deutz-Fahr filters applied (functions=0x%02X)", functions);
            break;
    }
}

void CANFilterHelper::setupKubotaFilters(uint8_t busNum, uint8_t functions) {
    switch (busNum) {
        case 1:
            if (functions & (uint8_t)CANFunction::STEERING) {
                globalCAN1.setMBFilter(MB0, 0x0CEF1C26);  // Steering valve
            }
            if (functions & (uint8_t)CANFunction::MACHINE_DATA) {
                globalCAN1.setMBFilter(MB1, 0x18FEF100);  // Wheel speed
            }
            LOG_INFO(EventSource::CAN, "CAN1: Kubota filters applied (functions=0x%02X)", functions);
            break;
        case 2:
            if (functions & (uint8_t)CANFunction::IMPLEMENTS) {
                globalCAN2.setMBFilter(MB0, 0x0CFEFF00);  // Hitch position
            }
            LOG_INFO(EventSource::CAN, "CAN2: Kubota filters applied (functions=0x%02X)", functions);
            break;
        case 3:
            if (functions & (uint8_t)CANFunction::MACHINE_DATA) {
                globalCAN3.setMBFilter(MB0, 0x18FEEE00);  // Ground speed
            }
            LOG_INFO(EventSource::CAN, "CAN3: Kubota filters applied (functions=0x%02X)", functions);
            break;
    }
}
