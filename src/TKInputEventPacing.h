#pragma once
#include <stdint.h>

// Device-wide PS/2 event pacing: a MAKE immediately followed by BREAK in a
// FabGL queue must not be consumed atomically without guest 6502 execution.
// Gate interleaving to live emulation (no menu, no cassette and no boot hold).
// Count guest *cycles*, not wall-clock microseconds, then repay every one
// through the normal scheduler so queued input does not overclock the TK.
namespace TKInputEventPacing {
static const uint32_t kGuestSliceCycles=4096u;
static const uint32_t kMaxAdvancePerLoop=8192u;

static inline uint32_t slice(bool keyDown,bool moreQueued,bool guestRunning,
                             bool tapeActive,uint32_t alreadyAdvanced) {
  if (!keyDown || !moreQueued || !guestRunning || tapeActive ||
      alreadyAdvanced>=kMaxAdvancePerLoop) return 0;
  const uint32_t remaining=kMaxAdvancePerLoop-alreadyAdvanced;
  return remaining<kGuestSliceCycles?remaining:kGuestSliceCycles;
}
// Carry any outstanding debt into later loop iterations rather than
// silently accelerating emulation if a short input burst runs ahead of time.
static inline uint32_t payBack(uint32_t carried,uint32_t earned,
                                uint32_t &advanceDebt) {
  const uint64_t total=(uint64_t)carried+(uint64_t)earned;
  const uint32_t used=advanceDebt>total?(uint32_t)total:advanceDebt;
  advanceDebt-=used;
  const uint64_t result=total-used;
  return result>UINT32_MAX?UINT32_MAX:(uint32_t)result;
}
}
