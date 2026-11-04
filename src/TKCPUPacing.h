#pragma once
#include <stdint.h>

// Adapted from MC-1000 0.7's verified FreeRTOS pacing policy: periodically
// BLOCK (not merely taskYIELD) to let IDLE0/1 feed the task watchdog.
// A 20-ms blocking interval in v0.36 lost substantial CPU wall-clock time
// on ESP32 builds whose FreeRTOS tick is 10 ms.  Block every 250 ms instead;
// this is still comfortably below the seconds-long ESP32 task-WDT window.
// Video frames and input continue to be serviced at their original cadence.
namespace TKCPUPacing {
static const uint32_t kIdleBlockIntervalMs = 250u;
static inline bool idleBlockDue(uint32_t nowMs, uint32_t lastBlockedMs) {
  return (uint32_t)(nowMs - lastBlockedMs) >= kIdleBlockIntervalMs;
}
}
