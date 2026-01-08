// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

#ifndef COMMANDHANDLER_H_
#define COMMANDHANDLER_H_

#include "Arduino.h"
#include "EventLogger.h"
#include "MachineProcessor.h"

// External function from main.cpp
extern void toggleLoopTiming();

class CommandHandler {
private:
    static CommandHandler* instance;
    
    // Pointers to system components
    MachineProcessor* machinePtr = nullptr;
    EventLogger* loggerPtr = nullptr;
    
    // Private constructor for singleton
    CommandHandler();
    
    // Command handler
    void handleCommand(char cmd);
    
    // Menu display function
    void showMenu();
    

public:
    ~CommandHandler();
    
    // Singleton access
    static CommandHandler* getInstance();
    static void init();
    
    // Set component pointers
    void setMachineProcessor(MachineProcessor* ptr) { machinePtr = ptr; }
    
    // Main process function - call this from loop()
    void process();
};

#endif // COMMANDHANDLER_H_