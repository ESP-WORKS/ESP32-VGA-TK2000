#pragma once
#include <stdint.h>

// Two real PC SHIFT and CTRL contacts per single TK-2000 modifier.
// A KEYUP on the left must not release the right, and vice versa.
// This is independent of the US/BR keyboard layout translation.
class TKModifierSides {
public:
  void setShift(bool right, bool down) { change(shiftMask, right, down); misses[right?1:0]=0; }
  void setCtrl(bool right, bool down) { change(ctrlMask, right, down); misses[right?3:2]=0; }
  bool shift() const { return shiftMask!=0; }
  bool ctrl() const { return ctrlMask!=0; }
  void clear() { shiftMask=ctrlMask=0; for (unsigned i=0;i<4;++i) misses[i]=0; }
  // Only release sides proven absent by FabGL. A missing PS/2 break must
  // not keep SHIFT/CTRL held forever, and the other side remains independent.
  void releaseMissing(bool leftShift,bool rightShift,bool leftCtrl,bool rightCtrl) {
    releaseSide(shiftMask,1u,leftShift,misses[0]);
    releaseSide(shiftMask,2u,rightShift,misses[1]);
    releaseSide(ctrlMask,1u,leftCtrl,misses[2]);
    releaseSide(ctrlMask,2u,rightCtrl,misses[3]);
  }
private:
  uint8_t shiftMask=0, ctrlMask=0;
  uint8_t misses[4]={};
  static void releaseSide(uint8_t &mask,uint8_t bit,bool physicallyDown,uint8_t &miss) {
    if (physicallyDown || !(mask&bit)) { miss=0; return; }
    if (++miss>=3) { mask&=(uint8_t)~bit; miss=0; }
  }
  static void change(uint8_t &mask, bool right, bool down) {
    const uint8_t bit=right?2u:1u;
    if (down) mask|=bit;
    else mask&=(uint8_t)~bit;
  }
};
