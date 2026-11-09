// v0.96: Montezuma-only A/Z on cursor UP/DOWN; every other title untouched.
#include <assert.h>
#include <stdio.h>
#include "Keyboard.h"
static uint8_t rowRead(KeyboardTK &k,uint8_t row){k.out=(uint8_t)(1u<<row);return k.in();}
int main(){
  KeyboardTK k; k.clear();
  // Standard tape: joystick UP closes only the UP arrow contact.
  k.setGameInputProfile(TKGameInputProfile::Standard);
  k.setJoystickMask(0x01);
  assert((rowRead(k,6)&0x01)==0x01 && (rowRead(k,1)&0x20)==0);
  // Montezuma: joystick UP also closes A, DOWN also Z.
  k.setGameInputProfile(TKGameInputProfile::Montezuma);
  assert((rowRead(k,1)&0x20)==0x20 && (rowRead(k,0)&0x20)==0);
  k.setJoystickMask(0x02);
  assert((rowRead(k,0)&0x20)==0x20 && (rowRead(k,1)&0x20)==0);
  k.setJoystickMask(0x03); // opposing = neutral
  assert((rowRead(k,0)&0x20)==0 && (rowRead(k,1)&0x20)==0);
  k.setJoystickMask(0);
  // JOYSTICK OFF: physical cursor keys (codes 0x0B up, 0x0A down).
  k.key(0x0B,true); assert((rowRead(k,6)&1) && (rowRead(k,1)&0x20)==0x20); k.key(0x0B,false);
  k.key(0x0A,true); assert((rowRead(k,5)&1) && (rowRead(k,0)&0x20)==0x20); k.key(0x0A,false);
  assert((rowRead(k,0)&0x20)==0 && (rowRead(k,1)&0x20)==0);
  // Any other profile: rule absent.
  k.setGameInputProfile(TKGameInputProfile::MoonPatrol); k.setJoystickMask(0x01);
  assert((rowRead(k,1)&0x20)==0);
  puts("PASS v0.96 Montezuma-only A/Z on cursor UP/DOWN (joystick and keyboard), other titles untouched");
}
