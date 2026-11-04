#pragma once
#include <stdint.h>

// Apple II PDL compatibility for software probing C061/C064-C067.
// With F1 joystick ON, emulate left/right/up/down and button from keyboard;
// with OFF, remain centered and unpressed.  Returning 0x7F immediately for
// C064 when OFF used to make games treat the absent joystick as hard LEFT.
// TK cassette reads at C070 remain handled by the machine unchanged.
class TKPaddleJoystick {
public:
  void setEnabled(bool value) { _enabled=value; _mask=0; _triggerCycles=0; }
  bool enabled() const { return _enabled; }
  void setMask(uint8_t mask) {
    _mask=(uint8_t)(mask & 0x1Fu);
    if ((_mask & 0x0Cu)==0x0Cu) _mask &= (uint8_t)~0x0Cu;
    if ((_mask & 0x03u)==0x03u) _mask &= (uint8_t)~0x03u;
  }
  void trigger(uint64_t cycles) { _triggerCycles=cycles; }
  // Apple PREAD expects a returned Y coordinate (0..255). Reuse exactly
  // the same axis positions as the C064/C065 analog timers.
  uint8_t positionForAxis(uint8_t axis) const {
    const uint8_t mask = _enabled ? _mask : 0u;
    if (axis == 0) return (mask & 0x04u) ? 0u : ((mask & 0x08u) ? 255u : 127u);
    if (axis == 1) return (mask & 0x01u) ? 0u : ((mask & 0x02u) ? 255u : 127u);
    return 127u;
  }
  uint8_t read(uint16_t address, uint64_t cycles) const {
    // Hardware-read guard: no physical joystick input leaks into games OFF,
    // even if an asynchronous keyboard event arrives around an F1 toggle.
    const uint8_t mask = _enabled ? _mask : 0u;
    if (address == 0xC061u) return (mask & 0x10u) ? 0xFFu : 0x7Fu;
    if (address == 0xC062u || address == 0xC063u) return 0x7Fu;
    if (address < 0xC064u || address > 0xC067u) return 0x7Fu;
    // Paddle 0 = horizontal, 1 = vertical. Absent paddle 2/3 centered.
    const uint8_t position=positionForAxis((uint8_t)(address-0xC064u));
    // Apple II PREAD increments about once per 11 CPU cycles. At rest a
    // centered joystick must NOT return immediate zero (which is LEFT).
    const uint64_t elapsed=cycles>=_triggerCycles ? cycles-_triggerCycles : 0;
    return elapsed < (uint32_t)position*11u ? 0xFFu : 0x7Fu;
  }
private:
  bool _enabled=false;
  uint8_t _mask=0;
  uint64_t _triggerCycles=0;
};
