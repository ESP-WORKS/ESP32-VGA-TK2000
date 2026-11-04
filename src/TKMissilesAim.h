#pragma once
#include <stdint.h>

// Missiles Defense CT2 uses Apple II PREAD as an *absolute* paddle pointer.
// Mapping LEFT/RIGHT directly to 0/255 and the release to 127 continuously
// steers the sight again after key-up. Instead, cursor keys move an emulated
// persistent paddle. Once released, the analog value stays at its last
// position. This is enabled only for the exact Missiles CT2 game profile.
// All movement follows 6502 guest cycles, independent of ESP32 loop jitter.
class TKMissilesAim {
public:
  void reset(uint64_t guestCycles=0) {
    _x=127;_y=127;_lastCycles=guestCycles;_remainder=0;
    _lastMask=0;
  }
  void advance(uint64_t guestCycles,uint8_t mask) {
    if (guestCycles<_lastCycles) { reset(guestCycles);return; }
    const uint64_t elapsed=guestCycles-_lastCycles;
    _lastCycles=guestCycles;
    const uint8_t dirs=(uint8_t)(mask&0x0Fu);
    if (!dirs || ((dirs&0x0Cu)==0x0Cu && (dirs&0x03u)==0x03u)) {
      _remainder=0; _lastMask=dirs; return;
    }
    if (dirs!=_lastMask) _remainder=0;
    _lastMask=dirs;
    // 12,000 guest cycles / 1 paddle increment: ~85 pixels/second at 1x.
    // A large interval cannot produce a sudden leap after an OS stall.
    const uint64_t credit=(elapsed>120000u?120000u:elapsed)+_remainder;
    const uint32_t steps=(uint32_t)(credit/12000u);
    _remainder=(uint16_t)(credit%12000u);
    if (!steps)return;
    if ((dirs&0x0Cu)!=0x0Cu) {
      if (dirs&0x04u) _x=clamp((int)_x-(int)steps);
      if (dirs&0x08u) _x=clamp((int)_x+(int)steps);
    }
    if ((dirs&0x03u)!=0x03u) {
      if (dirs&0x01u) _y=clamp((int)_y-(int)steps);
      if (dirs&0x02u) _y=clamp((int)_y+(int)steps);
    }
  }
  uint8_t positionForAxis(uint8_t axis) const {return axis==0?_x:_y;}
private:
  static uint8_t clamp(int value) { return (uint8_t)(value<4?4:(value>190?190:value)); }
  uint8_t _x=127,_y=127,_lastMask=0;
  uint16_t _remainder=0;
  uint64_t _lastCycles=0;
};
