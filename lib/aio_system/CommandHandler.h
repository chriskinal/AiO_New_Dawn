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