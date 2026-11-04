#pragma once
#include <stdint.h>
#include <stddef.h>

// Optional, conservative game-specific input compatibility. Only the exact
// user-verified CT2 images below may activate these profiles. Renamed copies
// work; unrelated tapes (even those containing similar names) remain normal.
// These fingerprints are checked ONCE when mounting a valid CTK2 file.
enum class TKGameInputProfile : uint8_t { Standard=0, Montezuma=1, MoonPatrol=2, MissilesDefense=3 };

static inline TKGameInputProfile TKDetectTapeGameInput(const uint8_t *data,size_t size) {
  if (!data || (size!=47437u && size!=35547u && size!=9975u)) return TKGameInputProfile::Standard;
  uint32_t crc=0xFFFFFFFFu;
  for(size_t i=0;i<size;++i) {
    crc^=data[i];
    for(unsigned bit=0;bit<8;++bit)
      crc=(crc>>1)^((crc&1u)?0xEDB88320u:0u);
  }
  crc^=0xFFFFFFFFu;
  if(size==47437u && crc==0xC6FEAA76u) return TKGameInputProfile::Montezuma;
  if(size==35547u && crc==0xC28A4F8Du) return TKGameInputProfile::MoonPatrol;
  if(size==9975u && crc==0x43B19501u) return TKGameInputProfile::MissilesDefense;
  return TKGameInputProfile::Standard;
}
