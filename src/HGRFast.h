#pragma once
#include <stdint.h>

// Decode one 7-pixel HGR byte using a 9-bit neighbourhood. Unlike per-pixel
// TKHGRColor() this performs no x/7, x%7 or redundant neighbour lookups.
// Reference palette and phase semantics are UNCHANGED (HGRColor.h).
static inline void TKHGRFast7(const uint8_t row[40], int b, bool color, uint8_t out[7]) {
  const uint8_t byte = row[b];
  uint16_t bits = uint16_t(byte & 0x7Fu) << 1;
  if (b > 0) bits |= uint16_t((row[b-1] >> 6) & 1u);
  if (b < 39) bits |= uint16_t(row[b+1] & 1u) << 8;
  if (!color) {
    for (int i=0; i<7; ++i) out[i] = (bits & (uint16_t(2u) << i)) ? 7u : 0u;
    return;
  }
  // Indexes = GREEN,CYAN,BLUE,RED; byte bit 7 selects the artifact phase.
  static const uint8_t kColors[4] = {2,6,4,1};
  const unsigned phase = byte >> 7;
  for (int i=0; i<7; ++i) {
    const unsigned triple = (bits >> i) & 7u;
    const bool center = (triple & 2u) != 0;
    if (center && (triple & 5u)) { out[i]=7; continue; }
    if (!center && (triple & 5u) != 5u) { out[i]=0; continue; }
    const unsigned parity = (unsigned((b*7+i)&1) ^ unsigned(!center));
    out[i] = kColors[(parity << 1) | phase];
  }
}
