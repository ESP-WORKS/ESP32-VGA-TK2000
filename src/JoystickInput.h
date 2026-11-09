#pragma once
#include "JoystickKeys.h"
#include <stdint.h>

// The FabGL virtual key assigned to ONE physical keypad switch can change
// when NumLock is toggled while it is held (KP_7 <-> KP_HOME, etc.). Keep
// one stable owner for each switch; a break under either spelling releases
// the same owner. Independent main cursor keys remain separate owners.
// This is generic PS/2 bookkeeping, with no CT2/game profile dependencies.
class TKJoystickInput {
public:
  static constexpr uint8_t kKeys=15;

  void clear() { for (unsigned i=0;i<kKeys;++i) { _down[i]=false; _missing[i]=0; } }

  static fabgl::VirtualKey canonical(fabgl::VirtualKey vk) {
    switch(vk) {
      case fabgl::VK_KP_HOME: return fabgl::VK_KP_7;
      case fabgl::VK_KP_UP: return fabgl::VK_KP_8;
      case fabgl::VK_KP_PAGEUP: return fabgl::VK_KP_9;
      case fabgl::VK_KP_LEFT: return fabgl::VK_KP_4;
      case fabgl::VK_KP_RIGHT: return fabgl::VK_KP_6;
      case fabgl::VK_KP_END: return fabgl::VK_KP_1;
      case fabgl::VK_KP_DOWN: return fabgl::VK_KP_2;
      case fabgl::VK_KP_PAGEDOWN: return fabgl::VK_KP_3;
      case fabgl::VK_KP_INSERT: return fabgl::VK_KP_0;
      case fabgl::VK_KP_DELETE: return fabgl::VK_KP_PERIOD;
      default: return vk;
    }
  }

  bool event(fabgl::VirtualKey vk,bool down) {
    const int slot=find(canonical(vk));
    if(slot<0)return false;
    _down[slot]=down; _missing[slot]=0;
    return true;
  }

  template<class IsDown> void releaseMissing(const IsDown &isDown) {
    for(unsigned i=0;i<kKeys;++i) {
      if (!_down[i]) continue;
      if (isDown(keys()[i])) { _missing[i]=0; continue; }
      // A single stale FabGL isVKDown() snapshot must not cancel FIRE or
      // a direction. Only an actual PS/2 KEYUP, or three consecutive
      // absent physical-state observations, may release the contact.
      if (++_missing[i]>=3) { _down[i]=false; _missing[i]=0; }
    }
  }

  uint8_t mask() const {
    uint8_t m=0;
    for(unsigned i=0;i<kKeys;++i)
      if(_down[i]) m|=TKJoystickActionMask(keys()[i]);
    return m;
  }

private:
  bool _down[kKeys]={};
  uint8_t _missing[kKeys]={};
  static const fabgl::VirtualKey *keys() {
    static const fabgl::VirtualKey k[kKeys]={
      fabgl::VK_UP, fabgl::VK_DOWN, fabgl::VK_LEFT, fabgl::VK_RIGHT,
      fabgl::VK_KP_7, fabgl::VK_KP_8, fabgl::VK_KP_9,
      fabgl::VK_KP_4, fabgl::VK_KP_6,
      fabgl::VK_KP_1, fabgl::VK_KP_2, fabgl::VK_KP_3,
      fabgl::VK_SPACE, fabgl::VK_KP_0, fabgl::VK_KP_PERIOD
    };
    return k;
  }
  static int find(fabgl::VirtualKey vk) {
    for(unsigned i=0;i<kKeys;++i)if(keys()[i]==vk)return (int)i;
    return -1;
  }
};
