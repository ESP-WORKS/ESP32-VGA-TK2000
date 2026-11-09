#include <cassert>
#include <cstdint>
#include <cstdio>
#include "PaddleJoystick.h"
#include "JoystickInput.h"
#include "TKHostKeyOwners.h"
#include "TKModifierSides.h"
#include "Keyboard.h"
int main(){
  TKPaddleJoystick pad;
  pad.setEnabled(false);
  // On reset, even WITHOUT C070 strobe, first sampled paddle must be neutral.
  assert(pad.read(0xC064,1000000)==0xFF);
  assert(pad.read(0xC064,1001408)==0x7F);
  pad.trigger(2000000);
  assert(pad.read(0xC064,2000000)==0xFF);
  assert(pad.read(0xC064,2001408)==0x7F);
  pad.setEnabled(true); pad.setMask(0x04);
  assert(pad.read(0xC064,3000000)==0x7F); // left = 0
  pad.setMask(0x08); pad.trigger(3100000);
  assert(pad.read(0xC064,3100000)==0xFF);
  assert(pad.read(0xC064,3102810)==0x7F);
  pad.setMask(0); pad.trigger(3200000);
  assert(pad.positionForAxis(0)==TKPaddleJoystick::kCenter);
  TKJoystickInput js;
  js.event(fabgl::VK_SPACE,true);
  assert(js.mask()==0x10);
  auto absent=[](fabgl::VirtualKey){return false;};
  js.releaseMissing(absent); assert(js.mask()==0x10);
  js.releaseMissing(absent); assert(js.mask()==0x10);
  js.releaseMissing(absent); assert(js.mask()==0);
  js.event(fabgl::VK_LEFT,true);js.event(fabgl::VK_KP_7,true);
  assert((js.mask()&0x05)==0x05);
  js.event(fabgl::VK_KP_HOME,false); // aliases canonical
  assert((js.mask()&0x05)==0x04);
  js.event(fabgl::VK_LEFT,false);assert(js.mask()==0);
  TKHostKeyOwners owns;
  int plus=0,minus=0;
  auto em=[&](uint8_t c,bool d){assert(c==' ');if(d)++plus;else ++minus;};
  owns.event(fabgl::VK_SPACE,true,' ',em);
  owns.releaseMissing(absent,em);owns.releaseMissing(absent,em);
  assert(plus==1 && minus==0);
  owns.releaseMissing(absent,em);assert(minus==1);
  owns.event(fabgl::VK_SPACE,true,' ',em);
  owns.event(fabgl::VK_SPACE,false,' ',em);assert(minus==2);
  TKModifierSides m;
  m.setCtrl(false,true);m.setCtrl(true,true);assert(m.ctrl());
  m.releaseMissing(false,false,false,true);
  assert(m.ctrl());m.releaseMissing(false,false,false,true);
  assert(m.ctrl());m.releaseMissing(false,false,false,true);
  assert(m.ctrl()); // right still pressed
  m.setCtrl(true,false);assert(!m.ctrl());
  KeyboardTK k;
  k.clear();k.out=1u<<0; k.key('B',true); k.setPhysicalCtrl(true);
  assert((k.in()&0x02)!=0); k.key('B',false);
  k.key('R',true); k.out=1u<<2;assert((k.in()&0x04)!=0);k.key('R',false);
  k.setShift(true);k.out=1u<<0;assert((k.in()&1)!=0);
  k.setPhysicalCtrl(false);
  std::puts("PASS: paddle no-strobe, strobe timing, fire, 3-poll PS2, arrows, CTRL+B/R matrix");
}
