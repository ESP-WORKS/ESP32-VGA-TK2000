#pragma once
#include "JoystickKeys.h"
#include <stdint.h>

// Trusted, edge-driven PS/2 -> joystick adapter.
// FabGL's global isVKDown() map can contain stale keys after a lost BREAK or
// NumLock/Shift virtual-key alias transition. Its contents must NEVER create a
// new joystick press by themselves. Only a received key-down event can do so.
// Polling is used exclusively to release a previously witnessed key-down.
class TKJoystickInput {
public:
  void clear() {
    for (uint8_t a=0;a<5;++a)
      for (uint8_t v=0;v<3;++v) _down[a][v]=false;
  }

  bool event(fabgl::VirtualKey vk, bool down) {
    bool matched=false;
    for (uint8_t a=0;a<5;++a) {
      for (uint8_t v=0;v<3;++v) {
        if (alias(a,v)!=vk) continue;
        _down[a][v]=down;
        matched=true;
      }
    }
    return matched;
  }

  template<class IsDown> void releaseMissing(const IsDown &isDown) {
    for (uint8_t a=0;a<5;++a)
      for (uint8_t v=0;v<3;++v)
        if (_down[a][v] && !isDown(alias(a,v))) _down[a][v]=false;
  }

  uint8_t mask() const {
    uint8_t result=0;
    for (uint8_t a=0;a<5;++a)
      if (_down[a][0] || _down[a][1] || _down[a][2]) result |= (uint8_t)(1u<<a);
    return result;
  }

private:
  bool _down[5][3] = {};
  static fabgl::VirtualKey alias(uint8_t action, uint8_t variant) {
    static const fabgl::VirtualKey keys[5][3]={
      {fabgl::VK_UP, fabgl::VK_KP_UP, fabgl::VK_KP_8},
      {fabgl::VK_DOWN, fabgl::VK_KP_DOWN, fabgl::VK_KP_2},
      {fabgl::VK_LEFT, fabgl::VK_KP_LEFT, fabgl::VK_KP_4},
      {fabgl::VK_RIGHT, fabgl::VK_KP_RIGHT, fabgl::VK_KP_6},
      {fabgl::VK_SPACE, fabgl::VK_KP_0, fabgl::VK_KP_INSERT}
    };
    return keys[action][variant];
  }
};
