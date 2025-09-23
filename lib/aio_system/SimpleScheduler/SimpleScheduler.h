// SimpleScheduler.h
// Lightweight task scheduler for AiO New Dawn
// Zero dynamic allocation, minimal overhead design

#ifndef SIMPLE_SCHEDULER_H
#define SIMPLE_SCHEDULER_H

#include <Arduino.h>

class SimpleScheduler {
public:
    // Configuration constants
    static constexpr uint8_t MAX_TASKS_PER_GROUP = 8;
    static constexpr uint8_t NUM_GROUPS = 7;

    // Group indices for direct access
    static constexpr uint8_t EVERY_LOOP = 0;
    static constexpr uint8_t HZ_100 = 1;
    static constexpr uint8_t HZ_50 = 2;
    static constexpr uint8_t HZ_10 = 3;
    static constexpr uint8_t HZ_5 = 4;
    static constexpr uint8_t HZ_1 = 5;
    static constexpr uint8_t HZ_0_2 = 6;

    // Task function type
    typedef void (*TaskFunction)(void);

    SimpleScheduler();

    // Add a task to a frequency group
    bool addTask(uint8_t groupIndex, TaskFunction function, const char* name = nullptr);

    // Main scheduler execution - call from loop()
    void run();

    // Task control
    bool enableTask(uint8_t groupIndex, const char* taskName);
    bool disableTask(uint8_t groupIndex, const char* taskName);
    bool enableGroup(uint8_t groupIndex);
    bool disableGroup(uint8_t groupIndex);

    // Runtime frequency adjustment
    void setGroupInterval(uint8_t groupIndex, uint32_t intervalMs);

    // Debug and statistics
    void printStatus();
    uint32_t getLoopCount() const { return loopCount; }

#ifdef SCHEDULER_TIMING_STATS
    struct TaskStats {
        uint32_t runCount;
        uint32_t totalTime;
        uint32_t maxTime;
        uint32_t lastRunTime;
    };

    void printStats();
    void resetStats();
    TaskStats* getTaskStats(uint8_t groupIndex, uint8_t taskIndex);
#endif

private:
    struct Task {
        TaskFunction function;
        const char* name;
        bool enabled;

#ifdef SCHEDULER_TIMING_STATS
        TaskStats stats;
#endif
    };

    struct FrequencyGroup {
        Task tasks[MAX_TASKS_PER_GROUP];
        uint8_t taskCount;
        uint32_t interval;      // 0 = every loop
        uint32_t lastRun;
        const char* name;
        bool enabled;

        // Inline for performance
        inline bool isDue(uint32_t now) {
            if (interval == 0) return true;  // EVERY_LOOP always runs
            if (!enabled) return false;
            return (now - lastRun) >= interval;
        }
    };

    FrequencyGroup groups[NUM_GROUPS];
    uint32_t loopCount;

    // Initialize group names and intervals
    void initializeGroups();

    // Find task by name in a group
    int findTaskIndex(uint8_t groupIndex, const char* taskName);
};

// Global instance (optional - can also create in main.cpp)
extern SimpleScheduler scheduler;

#endif // SIMPLE_SCHEDULER_H