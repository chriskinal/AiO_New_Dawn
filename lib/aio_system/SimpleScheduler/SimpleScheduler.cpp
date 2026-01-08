// Firmware_Teensy_AiO-New-Dawn is copyright 2025 by the AOG Group
// Firmware_Teensy_AiO-New-Dawn is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// Firmware_Teensy_AiO-New-Dawn is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with Firmware_Teensy_AiO-New-Dawn. If not, see <https://www.gnu.org/licenses/>.
// Like most Arduino code, portions of this are based on other open source Arduino code with a compatiable license.

// SimpleScheduler.cpp
// Implementation of lightweight task scheduler

#include "SimpleScheduler.h"

// Optional global instance
SimpleScheduler scheduler;

SimpleScheduler::SimpleScheduler() : loopCount(0) {
    initializeGroups();
}

void SimpleScheduler::initializeGroups() {
    // Initialize frequency groups with names and intervals
    groups[EVERY_LOOP] = {.interval = 0, .lastRun = 0, .name = "EveryLoop", .enabled = true};
    groups[HZ_100] = {.interval = 10, .lastRun = 0, .name = "100Hz", .enabled = true};
    groups[HZ_50] = {.interval = 20, .lastRun = 0, .name = "50Hz", .enabled = true};
    groups[HZ_10] = {.interval = 100, .lastRun = 0, .name = "10Hz", .enabled = true};
    groups[HZ_5] = {.interval = 200, .lastRun = 0, .name = "5Hz", .enabled = true};
    groups[HZ_1] = {.interval = 1000, .lastRun = 0, .name = "1Hz", .enabled = true};
    groups[HZ_0_2] = {.interval = 5000, .lastRun = 0, .name = "0.2Hz", .enabled = true};

    // Initialize task counts
    for (uint8_t i = 0; i < NUM_GROUPS; i++) {
        groups[i].taskCount = 0;
    }
}

bool SimpleScheduler::addTask(uint8_t groupIndex, TaskFunction function, const char* name) {
    if (groupIndex >= NUM_GROUPS || function == nullptr) {
        return false;
    }

    FrequencyGroup& group = groups[groupIndex];
    if (group.taskCount >= MAX_TASKS_PER_GROUP) {
        return false;
    }

    Task& task = group.tasks[group.taskCount];
    task.function = function;
    task.name = name;
    task.enabled = true;

#ifdef SCHEDULER_TIMING_STATS
    task.stats = {0, 0, 0, 0};
#endif

    group.taskCount++;
    return true;
}

void SimpleScheduler::run() {
    uint32_t now = millis();
    loopCount++;

    // Always run EVERY_LOOP tasks first (no timing check needed)
    FrequencyGroup& everyLoop = groups[EVERY_LOOP];
    if (everyLoop.enabled) {
        for (uint8_t i = 0; i < everyLoop.taskCount; i++) {
            Task& task = everyLoop.tasks[i];
            if (task.enabled && task.function) {
#ifdef SCHEDULER_TIMING_STATS
                uint32_t startTime = micros();
                task.function();
                uint32_t elapsed = micros() - startTime;
                task.stats.runCount++;
                task.stats.totalTime += elapsed;
                task.stats.lastRunTime = elapsed;
                if (elapsed > task.stats.maxTime) {
                    task.stats.maxTime = elapsed;
                }
#else
                task.function();
#endif
            }
        }
    }

    // Check and run timed groups in priority order
    for (uint8_t g = 1; g < NUM_GROUPS; g++) {
        FrequencyGroup& group = groups[g];

        if (group.taskCount > 0 && group.isDue(now)) {
            group.lastRun = now;

            for (uint8_t i = 0; i < group.taskCount; i++) {
                Task& task = group.tasks[i];
                if (task.enabled && task.function) {
#ifdef SCHEDULER_TIMING_STATS
                    uint32_t startTime = micros();
                    task.function();
                    uint32_t elapsed = micros() - startTime;
                    task.stats.runCount++;
                    task.stats.totalTime += elapsed;
                    task.stats.lastRunTime = elapsed;
                    if (elapsed > task.stats.maxTime) {
                        task.stats.maxTime = elapsed;
                    }
#else
                    task.function();
#endif
                }
            }
        }
    }
}

bool SimpleScheduler::enableTask(uint8_t groupIndex, const char* taskName) {
    if (groupIndex >= NUM_GROUPS || taskName == nullptr) {
        return false;
    }

    int taskIndex = findTaskIndex(groupIndex, taskName);
    if (taskIndex >= 0) {
        groups[groupIndex].tasks[taskIndex].enabled = true;
        return true;
    }
    return false;
}

bool SimpleScheduler::disableTask(uint8_t groupIndex, const char* taskName) {
    if (groupIndex >= NUM_GROUPS || taskName == nullptr) {
        return false;
    }

    int taskIndex = findTaskIndex(groupIndex, taskName);
    if (taskIndex >= 0) {
        groups[groupIndex].tasks[taskIndex].enabled = false;
        return true;
    }
    return false;
}

bool SimpleScheduler::enableGroup(uint8_t groupIndex) {
    if (groupIndex >= NUM_GROUPS) {
        return false;
    }
    groups[groupIndex].enabled = true;
    return true;
}

bool SimpleScheduler::disableGroup(uint8_t groupIndex) {
    if (groupIndex >= NUM_GROUPS) {
        return false;
    }
    groups[groupIndex].enabled = false;
    return true;
}

void SimpleScheduler::setGroupInterval(uint8_t groupIndex, uint32_t intervalMs) {
    if (groupIndex > 0 && groupIndex < NUM_GROUPS) {  // Can't change EVERY_LOOP interval
        groups[groupIndex].interval = intervalMs;
    }
}

int SimpleScheduler::findTaskIndex(uint8_t groupIndex, const char* taskName) {
    if (groupIndex >= NUM_GROUPS || taskName == nullptr) {
        return -1;
    }

    FrequencyGroup& group = groups[groupIndex];
    for (uint8_t i = 0; i < group.taskCount; i++) {
        if (group.tasks[i].name && strcmp(group.tasks[i].name, taskName) == 0) {
            return i;
        }
    }
    return -1;
}

void SimpleScheduler::printStatus() {
    Serial.println("\n=== SimpleScheduler Status ===");
    Serial.printf("Loop count: %lu\n", loopCount);
    Serial.println("\nGroup Status:");

    for (uint8_t g = 0; g < NUM_GROUPS; g++) {
        FrequencyGroup& group = groups[g];
        if (group.taskCount > 0) {
            Serial.printf("%s (%lums): %d tasks, %s\n",
                         group.name,
                         group.interval,
                         group.taskCount,
                         group.enabled ? "enabled" : "disabled");

            for (uint8_t i = 0; i < group.taskCount; i++) {
                Task& task = group.tasks[i];
                Serial.printf("  - %s: %s\n",
                             task.name ? task.name : "unnamed",
                             task.enabled ? "enabled" : "disabled");
            }
        }
    }
}

#ifdef SCHEDULER_TIMING_STATS

void SimpleScheduler::printStats() {
    Serial.println("\n=== SimpleScheduler Timing Stats ===");

    for (uint8_t g = 0; g < NUM_GROUPS; g++) {
        FrequencyGroup& group = groups[g];
        if (group.taskCount > 0) {
            Serial.printf("\n%s Group:\n", group.name);

            for (uint8_t i = 0; i < group.taskCount; i++) {
                Task& task = group.tasks[i];
                if (task.stats.runCount > 0) {
                    uint32_t avgTime = task.stats.totalTime / task.stats.runCount;
                    Serial.printf("  %s: runs=%lu, avg=%luus, max=%luus, last=%luus\n",
                                 task.name ? task.name : "unnamed",
                                 task.stats.runCount,
                                 avgTime,
                                 task.stats.maxTime,
                                 task.stats.lastRunTime);
                }
            }
        }
    }
}

void SimpleScheduler::resetStats() {
    for (uint8_t g = 0; g < NUM_GROUPS; g++) {
        FrequencyGroup& group = groups[g];
        for (uint8_t i = 0; i < group.taskCount; i++) {
            Task& task = group.tasks[i];
            task.stats = {0, 0, 0, 0};
        }
    }
}

SimpleScheduler::TaskStats* SimpleScheduler::getTaskStats(uint8_t groupIndex, uint8_t taskIndex) {
    if (groupIndex >= NUM_GROUPS || taskIndex >= groups[groupIndex].taskCount) {
        return nullptr;
    }
    return &groups[groupIndex].tasks[taskIndex].stats;
}

#endif // SCHEDULER_TIMING_STATS