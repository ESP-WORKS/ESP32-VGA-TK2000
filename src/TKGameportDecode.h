#pragma once
#include <stdint.h>

// FPGA TK2000 II: source/tk2000.vhd decodes the complete C06x nibble
// as GAMEPORT_SELECT and routes A(2 downto 0) to the port mux. C068..C06F
// therefore mirror C060..C067, for BOTH 6502 reads and bus selectors.
// Its PDL_STROBE is selected by the complete C07x nibble, not just C070.
// This is address decoding only; actual paddle/button states continue to
// come from TKPaddleJoystick, with no tape-name or program-specific logic.
namespace TKGameportDecode {
static inline bool isGameport(uint16_t address) {
  return (address & 0xFFF0u) == 0xC060u;
}
static inline uint16_t canonicalGameport(uint16_t address) {
  return (uint16_t)(0xC060u | (address & 0x0007u));
}
static inline bool isPaddleStrobe(uint16_t address) {
  return (address & 0xFFF0u) == 0xC070u;
}
}
