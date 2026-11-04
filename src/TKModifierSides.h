#pragma once
#include <stdint.h>

// Two real PC SHIFT and CTRL contacts per single TK-2000 modifier.
// A KEYUP on the left must not release the right, and vice versa.
// This is independent of the US/BR keyboard layout translation.
class TKModifierSides {
public:
  void setShift(bool right, bool down) { change(shiftMask, right, down); }
  void setCtrl(bool right, bool down) { change(ctrlMask, right, down); }
  bool shift() const { return shiftMask!=0; }
  bool ctrl() const { return ctrlMask!=0; }
  void clear() { shiftMask=ctrlMask=0; }
private:
  uint8_t shiftMask=0, ctrlMask=0;
  static void change(uint8_t &mask, bool right, bool down) {
    const uint8_t bit=right?2u:1u;
    if (down) mask|=bit;
    else mask&=(uint8_t)~bit;
  }
};
