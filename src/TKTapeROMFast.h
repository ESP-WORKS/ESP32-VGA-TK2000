#pragma once
#include <stdint.h>
#include <string.h>
#include "BuildConfig.h"
#include "CPU6502.h"
#include "Cassette.h"

// Accelerate *unchanged* 6502-ROM cassette busy-waits, not the CT2 signal.
// The original scheduler's 8x request alone could not improve wall-clock
// throughput when the ESP32 interpreter was already CPU-bound.
// Only exact, verified ROM instruction sequences may use these shortcuts.
// No ROM bytes are replaced and no game memory is injected.
namespace TKTapeROMFast {
static inline bool supported(const uint8_t *rom, size_t len) {
  if (!rom || len != 0x4000u) return false;
  // $FCFD: DEY, then $FCFE: LDA $C010 / EOR $2F / BPL $FCFD.
  // Only this variant (with DEY at FCFD) may be accelerated: the DEY
  // measures pulse width and cannot be omitted!
  static const uint8_t readWait[] = {
    0xAD,0x10,0xC0,0x45,0x2F,0x10,0xF8,0x45,0x2F,0x85,0x2F,0xC0,0x80,0x60
  };
  // $FCDB: DEY / BNE $FCDB. $FCE2: DEY / BNE $FCE2.
  static const uint8_t delay1[] = {0x88,0xD0,0xFD};
  // $FCA8 SEC/PHA, $FCAA SBC #1/BNE $FCAA: another ROM timing loop.
  static const uint8_t countDelay[] = {0x38,0x48,0xE9,0x01,0xD0,0xFC};
  return rom[0x3CFD]==0x88u &&
         memcmp(rom+0x3CFE,readWait,sizeof(readWait))==0 &&
         memcmp(rom+0x3CDB,delay1,sizeof(delay1))==0 &&
         memcmp(rom+0x3CE2,delay1,sizeof(delay1))==0 &&
         memcmp(rom+0x3CA8,countDelay,sizeof(countDelay))==0;
}

// Returns true only when an entire stretch of guest instructions was
// accounted for in cycles. The 6502 resumes at the original next opcode.
// The fast path is strictly bounded by the current scheduler slice.
static inline bool skip(CPU6502 &cpu, const CassetteCT2 &tape,
                        uint64_t target, bool readActive, bool saveActive) {
#if !TK2000_K7_FAST_ROM
  (void)cpu;(void)tape;(void)target;(void)readActive;(void)saveActive;
  return false;
#else
  if (cpu.nmi_pending || (cpu.irq_pending && !(cpu.P & 0x04u)) ||
      cpu.cycles >= target) return false;

  // The TK ROM measures a tape half-wave with:
  // $FCFD DEY (2), $FCFE LDA $C010 (4), $FD01 EOR $2F (3),
  // $FD03 BPL $FCFD (4: taken AND crosses $FD/$FC page).
  // Thus an old-level loop is 13 guest cycles.
  // **DEY is essential**: the ROM uses Y to discriminate 250 vs 500
  // cycle half-waves. Skip only reads that precede the next transition,
  // subtracting exactly one Y per skipped poll. The first changed-level
  // read is still executed by the original ROM and updates the latch.
  if (readActive && cpu.PC==0xFCFDu && !(cpu.P & 0x80u)) {
    const uint64_t edge=tape.nextEdgeDeadline();
    // The MMIO read is sampled immediately after DEY's 2 cycles, at
    // the *start* of the LDA instruction (CPU.step counts cycles at end).
    if (!edge || edge<=cpu.cycles+2u || target-cpu.cycles<13u) return false;
    const uint64_t until=edge-(cpu.cycles+2u);
    const uint64_t iterations=(until+12u)/13u;
    const uint64_t maxIterations=(target-cpu.cycles)/13u;
    const uint64_t count=iterations<maxIterations?iterations:maxIterations;
    if (!count) return false;
    cpu.cycles+=count*13u;
    cpu.Y=(uint8_t)(cpu.Y-(uint8_t)count);
    // The loop ends at the same PC with BPL taken. Its EOR restores N=0
    // and the other status flags (Z comes from the unchanged cassette bit).
    // A is overwritten on the next actual input read; no RAM is touched.
    return true;
  }

  // The same helper is called by the cassette read/calibration paths as
  // well as write; never require recording_ solely to accelerate it.
  // The ROM's delay helper also counts a positive accumulator down via
  // $FCAA SBC #1 / $FCAC BNE $FCAA. With carry set, every iteration
  // decrements A by one in binary mode; the final 1->0 leaves C=1,
  // Z=1, N=0, V=0. Decimal mode uses the original interpreter.
  // The PHA/PLA and the outer SBC/BNE are still interpreted normally.
  if ((saveActive || readActive) && cpu.PC==0xFCAAu && cpu.A &&
      (cpu.P & 0x01u) && !(cpu.P & 0x08u)) {
    const uint64_t spent=(uint64_t)cpu.A*5u-1u;
    if (spent>target-cpu.cycles) return false;
    cpu.cycles+=spent;
    cpu.A=0u;
    cpu.P=(uint8_t)((cpu.P | 0x03u) & ~(0x80u | 0x40u));
    cpu.PC=0xFCAEu;
    return true;
  }

  // $FCDB/$FCE2 are two-instruction DEY/BNE loops without I/O.
  // N iterations take 5*N-1 guest cycles (last BNE is not taken).
  // The C020 write/read itself is never skipped, retaining real pulse
  // lengths on SAVET, SAVEA and LOADA calibration.
  if ((saveActive || readActive) && (cpu.PC==0xFCDBu || cpu.PC==0xFCE2u)) {
    const uint64_t iterations=cpu.Y?cpu.Y:256u;
    const uint64_t spent=iterations*5u-1u;
    if (spent > target-cpu.cycles) return false;
    cpu.cycles+=spent;
    cpu.Y=0u;
    cpu.P=(uint8_t)((cpu.P | 0x02u) & ~0x80u); // Z=1, N=0; carry untouched.
    cpu.PC=(cpu.PC==0xFCDBu)?0xFCDEu:0xFCE5u;
    return true;
  }
  return false;
#endif
}
}
