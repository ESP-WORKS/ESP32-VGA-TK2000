#pragma once
#include <stdint.h>
// During the *verified* Moon Patrol CT2 transport, FabGL keeps outputting
// every VGA scanline in hardware. Only the expensive 6502 bitmap transfer
// is reduced to 15 fps: tape leaders and data do not require 60 fresh image
// copies/second. Normal game and all other tapes remain at 60 fps.
namespace TKMoonTurboPolicy {
static inline uint8_t videoDivider(bool moonTransport) {
  return moonTransport ? 4u : 1u;
}
static inline bool videoService(bool moonTransport,uint32_t frameSequence) {
  return !moonTransport || ((frameSequence & 3u)==0u);
}
static inline uint32_t maxBurstMicroseconds(bool moonTransport) {
  return moonTransport ? 5500u : 2500u;
}
}
