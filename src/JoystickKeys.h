#pragma once
#include <stdint.h>
#include "fabgl.h"

// Joystick virtual TK-2000 mapped to the genuine scanned keyboard matrix.
// Physical cursor keys and the numeric keypad have distinct FabGL VirtualKey
// values. The keypad also changes its VK codes when Num Lock changes.
// Actions: 0=UP 1=DOWN 2=LEFT 3=RIGHT 4=FIRE.
static inline int TKJoystickAction(fabgl::VirtualKey vk) {
  switch (vk) {
    case fabgl::VK_UP:
    case fabgl::VK_KP_UP:
    case fabgl::VK_KP_8: return 0;
    case fabgl::VK_DOWN:
    case fabgl::VK_KP_DOWN:
    case fabgl::VK_KP_2: return 1;
    case fabgl::VK_LEFT:
    case fabgl::VK_KP_LEFT:
    case fabgl::VK_KP_4: return 2;
    case fabgl::VK_RIGHT:
    case fabgl::VK_KP_RIGHT:
    case fabgl::VK_KP_6: return 3;
    case fabgl::VK_SPACE:
    case fabgl::VK_KP_0:
    case fabgl::VK_KP_INSERT: return 4;
    default: return -1;
  }
}

// Periodically sample the actual FabGL key state, independently of its queued
// press/release events. This repairs joystick states if a PS/2 break event is
// missed or a key changes VK alias with Num Lock. Pass a predicate equivalent
// to keyboard->isVKDown(vk). All directions are active-high in the TK matrix.
template<class IsDown>
static inline uint8_t TKJoystickMaskFromKeyboard(const IsDown &isDown) {
  uint8_t mask=0;
  if (isDown(fabgl::VK_UP) || isDown(fabgl::VK_KP_UP) || isDown(fabgl::VK_KP_8)) mask |= 1u<<0;
  if (isDown(fabgl::VK_DOWN) || isDown(fabgl::VK_KP_DOWN) || isDown(fabgl::VK_KP_2)) mask |= 1u<<1;
  if (isDown(fabgl::VK_LEFT) || isDown(fabgl::VK_KP_LEFT) || isDown(fabgl::VK_KP_4)) mask |= 1u<<2;
  if (isDown(fabgl::VK_RIGHT) || isDown(fabgl::VK_KP_RIGHT) || isDown(fabgl::VK_KP_6)) mask |= 1u<<3;
  if (isDown(fabgl::VK_SPACE) || isDown(fabgl::VK_KP_0) || isDown(fabgl::VK_KP_INSERT)) mask |= 1u<<4;
  return mask;
}
