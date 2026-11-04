#pragma once
#include <stdint.h>

// K7 fast transport: run the *original* cassette ROM and its 6502 I/O loops
// at up to 8x guest cycles per real second. Do NOT shorten CT2 edge durations:
// the ROM decodes the physical pulse widths in guest CPU cycles and would
// corrupt LOADT/LOADA (and SAVET/SAVEA) if those durations were changed.
// Native busy-wait shortcuts in TKTapeROMFast.h reduce CPU host work for
// verified ROMs so 8x is no longer just a requested cycle rate.
// Factor remains a TARGET: real ESP32 speed needs measurement on hardware.
// All non-cassette activity retains the v0.7 1.023 MHz schedule.
#ifndef TK2000_K7_TURBO_FACTOR
#define TK2000_K7_TURBO_FACTOR 8
#endif
#if TK2000_K7_TURBO_FACTOR < 1 || TK2000_K7_TURBO_FACTOR > 8
#error "TK2000_K7_TURBO_FACTOR must be between 1 and 8"
#endif

namespace TKCassetteTurbo {
static constexpr uint8_t kFactor = TK2000_K7_TURBO_FACTOR;
static constexpr uint32_t kMaxPendingCycles = 65536u;
static inline uint8_t rate(bool playing, bool recording) {
  return (playing || recording) ? kFactor : 1u;
}
// Fractional precision matches the original v0.7 scheduler. uint64_t
// arithmetic avoids overflow for 50ms scheduling gaps at the 8x target.
static inline uint32_t cyclesFromUs(uint32_t elapsedUs, uint8_t rate,
                                    uint16_t& remainder) {
  const uint64_t scaled = (uint64_t)elapsedUs * 1023u * rate + remainder;
  remainder = (uint16_t)(scaled % 1000u);
  return (uint32_t)(scaled / 1000u);
}
// On slow hosts do not queue minutes of unwanted fast-forward work. A pending
// burst that outlives the K7 operation must not accelerate gameplay afterward.
static inline uint32_t addBounded(uint32_t pending, uint32_t earned) {
  const uint64_t total = (uint64_t)pending + earned;
  return total > kMaxPendingCycles ? kMaxPendingCycles : (uint32_t)total;
}
static inline uint32_t afterTransportStops(uint32_t pending,
                                            uint8_t previousRate,
                                            uint8_t currentRate) {
  return previousRate != 1u && currentRate == 1u ? 0u : pending;
}
}
