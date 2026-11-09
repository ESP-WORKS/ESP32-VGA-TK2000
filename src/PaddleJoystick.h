#pragma once
#include <stdint.h>
#include "TKTapeGameControls.h"

// Apple II PDL compatibility, with two keyboard-style operating modes from
// Windows TK-2000 0.3: CENTERING returns the analog axis to 127 after key-up;
// STANDARD preserves the last position on key-up. The default is CENTERING,
// preserving the already validated Moon Patrol behavior. Missiles Defense has
// its own cycle-driven analog sight and can override the axis coordinates.
class TKPaddleJoystick {
public:
  // Electrical rest position of an Apple-style 0..255 analog axis. v0.96:
  // 128, not 127. Apple-derived dead zones commonly start at $80 (e.g. the
  // original Choplifter X axis accepts $80..$8F as "no thrust"); 127 is
  // one step to the LEFT/UP of such zones and causes a permanent drift.
  static constexpr uint8_t kCenter=128u;
  enum class Mode : uint8_t { Centering=0, Standard=1 };
  void setEnabled(bool value) {
    _enabled=value;_mask=0;_triggerCycles=0;_armed=false;
    _x=_y=kCenter;_absolute=false;_rawMask=0;
  }
  bool enabled() const { return _enabled; }
  void setMode(Mode value) {
    if(_mode==value) return;
    const uint8_t current=_rawMask;
    _mode=value;_x=_y=kCenter;_absolute=false;_rawMask=0;
    setMask(current);
  }
  Mode mode() const {return _mode;}
  void setAbsoluteOverride(bool active,uint8_t x=kCenter,uint8_t y=kCenter) {
    _absolute=active;
    _absX=x;_absY=y;
  }
  void setMask(uint8_t mask) {
    const uint8_t nextRaw=(uint8_t)(mask & 0x0Fu);
    const bool newDirection=(nextRaw & (uint8_t)~_rawMask)!=0;
    _rawMask=nextRaw;
    _mask=(uint8_t)(mask & 0x3Fu);
    // Matrix contacts must never signal both opposed directions at once.
    if ((_mask & 0x0Cu)==0x0Cu) _mask &= (uint8_t)~0x0Cu;
    if ((_mask & 0x03u)==0x03u) _mask &= (uint8_t)~0x03u;
    // Windows JoyProcessKey(STANDARD) recalculates axes on key DOWN only;
    // releasing a direction retains the last analog coordinate. CENTERING
    // recalculates on both DOWN and UP. Key typematic changes nothing.
    if(_mode==Mode::Standard && !newDirection) return;
    _x=(nextRaw&0x04u) && !(nextRaw&0x08u)?0u:
       ((nextRaw&0x08u) && !(nextRaw&0x04u)?255u:kCenter);
    _y=(nextRaw&0x01u) && !(nextRaw&0x02u)?0u:
       ((nextRaw&0x02u) && !(nextRaw&0x01u)?255u:kCenter);
  }
  void trigger(uint64_t cycles) { _triggerCycles=cycles; _armed=true; }
  uint8_t positionForAxis(uint8_t axis) const {
    if(!_enabled) return kCenter;
    if (axis==0) return _absolute?_absX:_x;
    if (axis==1) return _absolute?_absY:_y;
    return kCenter;
  }
  uint8_t read(uint16_t address, uint64_t cycles) const {
    const uint8_t mask = _enabled ? _mask : 0u;
    if (address==0xC061u) return (mask&0x10u)?0xFFu:0x7Fu;
    if (address==0xC062u) return (mask&0x20u)?0xFFu:0x7Fu;
    if (address==0xC063u) return 0x7Fu;
    if (address<0xC064u || address>0xC067u) return 0x7Fu;
    const uint8_t position=positionForAxis((uint8_t)(address-0xC064u));
    // Some Apple-II-compatible programs sample a paddle before issuing their
    // first $C070 strobe. A default timer of zero has already expired by
    // the time the guest starts: that looks like hard LEFT/UP, even with
    // joystick OFF. Prime on the first sample, then retain the normal
    // cycle-accurate $C070 discharge curve for subsequent reads.
    if (!_armed) { _triggerCycles=cycles; _armed=true; }
    const uint64_t elapsed=cycles>=_triggerCycles?cycles-_triggerCycles:0;
    return elapsed<(uint32_t)position*11u?0xFFu:0x7Fu;
  }
private:
  bool _enabled=false,_absolute=false;
  Mode _mode=Mode::Centering;
  uint8_t _mask=0,_rawMask=0,_x=kCenter,_y=kCenter,_absX=kCenter,_absY=kCenter;
  mutable uint64_t _triggerCycles=0;
  mutable bool _armed=false;
};

// One device-wide, reversible mode for all games, like the two keyboard
// joystick types selectable in the Windows 0.3 emulator. In compatibility
// builds the old Missiles-only choice is retained for A/B regression.
static inline TKPaddleJoystick::Mode TKPaddleModeForProfile(TKGameInputProfile detected) {
#if TK2000_GAME_JOYSTICK_COMPAT
  if(detected==TKGameInputProfile::MissilesDefense)
    return TKPaddleJoystick::Mode::Standard;
#else
  (void)detected;
#endif
#if TK2000_JOYSTICK_STANDARD_MODE
  return TKPaddleJoystick::Mode::Standard;
#else
  return TKPaddleJoystick::Mode::Centering;
#endif
}
