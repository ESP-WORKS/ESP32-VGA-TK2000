#pragma once
#include <stdint.h>

// Generic Apple-II PREAD entry used by several TK-2000 software ports.
// The supplied TK monitor ROM has SEC;RTS at $FB1E, whereas those ports
// expect the analog paddle coordinate in Y, X=0/1 selecting the axis.
// Only honor a *real* guest JSR $FB1E with a matching stacked return and
// the known SEC/RTS ROM stub. No CT2 fingerprint, filename, or game code.
// This never patches RAM or ROM and does not alter 6502 carry/stack/cycles.
static inline bool TKUniversalPREADCall(const uint8_t *ram,const uint8_t *rom,
                                         bool romSelected,uint16_t pc,
                                         uint8_t x,uint8_t s) {
  if(pc!=0xFB1Eu || !ram || !rom || !romSelected || x>=2 ||
     rom[0x3B1Eu]!=0x38u || rom[0x3B1Fu]!=0x60u)return false;
  const uint16_t ret=(uint16_t)(ram[0x100u+(uint8_t)(s+1u)] |
                          (uint16_t)(ram[0x100u+(uint8_t)(s+2u)]<<8));
  // JSR pushes the address of the high target-operand byte. Ensure every
  // inspected instruction byte is in writable guest memory.
  if(ret<0x0202u || ret>=0xC000u)return false;
  return ram[ret-2u]==0x20u && ram[ret-1u]==0x1Eu && ram[ret]==0xFBu;
}
