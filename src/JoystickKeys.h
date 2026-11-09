#pragma once
#include <stdint.h>
#include "fabgl.h"

// Joystick virtual TK-2000 mapped to the genuine scanned keyboard matrix.
// Physical cursor keys and the numeric keypad have distinct FabGL VirtualKey
// values. The keypad also changes its VK codes when Num Lock changes.
// Actions: 0=UP 1=DOWN 2=LEFT 3=RIGHT 4=FIRE1 5=FIRE2.
static inline uint8_t TKJoystickActionMask(fabgl::VirtualKey vk) {
  // Bits UP, DOWN, LEFT, RIGHT, FIRE1, FIRE2. The diagonal keypad
  // directions are two simultaneous original TK joystick contacts.
  switch (vk) {
    case fabgl::VK_UP: case fabgl::VK_KP_UP: case fabgl::VK_KP_8: return 0x01;
    case fabgl::VK_DOWN: case fabgl::VK_KP_DOWN: case fabgl::VK_KP_2: return 0x02;
    case fabgl::VK_LEFT: case fabgl::VK_KP_LEFT: case fabgl::VK_KP_4: return 0x04;
    case fabgl::VK_RIGHT: case fabgl::VK_KP_RIGHT: case fabgl::VK_KP_6: return 0x08;
    case fabgl::VK_KP_7: case fabgl::VK_KP_HOME: return 0x01|0x04;
    case fabgl::VK_KP_9: case fabgl::VK_KP_PAGEUP: return 0x01|0x08;
    case fabgl::VK_KP_1: case fabgl::VK_KP_END: return 0x02|0x04;
    case fabgl::VK_KP_3: case fabgl::VK_KP_PAGEDOWN: return 0x02|0x08;
    // SPACE = joystick FIRE1 ('.' contact) AND the TK SPACE key (main.cpp).
    // Validated on hardware: Choplifter and H.E.R.O. fire with SPACE.
    case fabgl::VK_SPACE: case fabgl::VK_KP_0: case fabgl::VK_KP_INSERT: return 0x10;
    case fabgl::VK_KP_PERIOD: case fabgl::VK_KP_DELETE: return 0x20;
    default: return 0;
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
  if (isDown(fabgl::VK_KP_7) || isDown(fabgl::VK_KP_HOME)) mask |= 0x01|0x04;
  if (isDown(fabgl::VK_KP_9) || isDown(fabgl::VK_KP_PAGEUP)) mask |= 0x01|0x08;
  if (isDown(fabgl::VK_KP_1) || isDown(fabgl::VK_KP_END)) mask |= 0x02|0x04;
  if (isDown(fabgl::VK_KP_3) || isDown(fabgl::VK_KP_PAGEDOWN)) mask |= 0x02|0x08;
  if (isDown(fabgl::VK_KP_PERIOD) || isDown(fabgl::VK_KP_DELETE)) mask |= 0x20;
  return mask;
}

// Windows 0.3 keypad 5 is the CENTER/NEUTRAL control, not ASCII digit 5.
// Consuming this VK while joystick mode is ON prevents BASIC keystroke leaks.
static inline bool TKJoystickConsumesKey(fabgl::VirtualKey vk) {
  return TKJoystickActionMask(vk)!=0 || vk==fabgl::VK_KP_5;
}
