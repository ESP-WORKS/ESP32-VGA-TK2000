// v0.92: original TK-2000 KBOUT/KBIN matrix contract, based on
// MAME src/mame/apple/tk2000.cpp kbin_r() and Windows 0.3 JoyVerificaTecla().
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "Keyboard.h"
#include "TKJoystickMatrix.h"
static uint8_t sample(KeyboardTK &k,uint8_t selected) {
  k.out=selected;return k.in();
}
int main() {
  KeyboardTK k;
  // 0 when no row is selected; high bit is legacy idle CASSIN level.
  assert(sample(k,0x00)==0x00u); // v0.96 idle cassette line = 0
  for(unsigned r=0;r<8;r++)assert((sample(k,(uint8_t)(1u<<r)) & 0x3Fu)==0u);
  // $C05F is independent from KBOUT; it is never an ASCII shortcut.
  k.setPhysicalCtrl(true);k.ctrl=true;
  assert(sample(k,0x00)==0x01u);
  k.key('C',true);k.key('Q',true);
  assert((sample(k,0x01)&0x3Fu)==0x09u); // CTRL | C
  assert((sample(k,0x04)&0x3Fu)==0x21u); // CTRL | Q
  assert((sample(k,0x05)&0x3Fu)==0x29u); // CTRL | C | Q
  k.setShift(true);
  assert((sample(k,0x01)&0x3Fu)==0x09u); // CTRL | SHIFT | C
  k.ctrl=false;assert((sample(k,0x01)&0x3Fu)==0x09u);
  k.ctrl=true;k.setShift(false);k.key('C',false);k.key('Q',false);
  assert((sample(k,0x05)&0x3Fu)==0x01u);
  k.setPhysicalCtrl(false);assert((sample(k,0x05)&0x3Fu)==0);
  k.ctrl=false;
  // Exhaustive 6-bit joystick masks: Windows row thresholds and both fire
  // contacts; opposing directions must neutralize each other. No game IDs.
  for(unsigned m=0;m<64;++m){
    k.setJoystickMask((uint8_t)m);
    for(unsigned r=0;r<8;++r){
      uint8_t expected=0;
      bool up=!!(m&1u), down=!!(m&2u), left=!!(m&4u), right=!!(m&8u);
      if(r==6 && up && !down)expected|=1;
      if(r==5 && down && !up)expected|=1;
      if(r==3 && left && !right)expected|=1;
      if(r==4 && right && !left)expected|=1;
      if(r==7 && (m&16u))expected|=16;
      if(r==6 && (m&32u))expected|=16;
      assert((sample(k,(uint8_t)(1u<<r))&0x3Fu)==expected);
    }
  }
  // Key-up and mode switch release all contacts: no phantom 'LEFT'.
  k.setJoystickMask(0); k.out=0xff;
  assert((k.in()&0x3Fu)==0);
  k.setJoystickMask(0x04);assert((sample(k,0x08)&1)==1);
  k.clearJoystick();assert((sample(k,0x08)&1)==0);
  // TAB FIRE is an independent original keyboard input, including OFF.
  k.setFire(true);assert((sample(k,0x80)&0x10)==0x10);
  k.setJoystickMask(0x10);k.setFire(false);
  assert((sample(k,0x80)&0x10)==0x10);
  k.clearJoystick();assert((sample(k,0x80)&0x10)==0);
  // Original SPACE is row2 bit0, independent of joystick button 1.
  k.key(' ',true);assert(sample(k,0x04)&1);
  assert((sample(k,0x80)&0x10)==0);
  k.key(' ',false);assert((sample(k,0x04)&1)==0);
  puts("PASS v0.92: MAME CTRL additive KBIN and 64 joystick masks (OFF/ON, FIRE1/2, release)");
}
