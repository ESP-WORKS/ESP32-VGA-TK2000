#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "Keyboard.h"
#include "JoystickInput.h"
#include "PaddleJoystick.h"
#include "TKUniversalPREAD.h"
#include "TKVGARawSpan.h"

static uint8_t ram[65536];
static uint8_t rom[16384];

int main() {
  // $FB1E stub must be SEC/RTS in the exact ROM window, and the caller
  // must really be JSR $FB1E. No CRC or game profile enters this test.
  rom[0x3B1E]=0x38;rom[0x3B1F]=0x60;
  ram[0x4000]=0x20;ram[0x4001]=0x1E;ram[0x4002]=0xFB;
  ram[0x01FC]=0x02;ram[0x01FD]=0x40;
  assert(TKUniversalPREADCall(ram,rom,true,0xFB1E,0,0xFB));
  assert(TKUniversalPREADCall(ram,rom,true,0xFB1E,1,0xFB));
  assert(!TKUniversalPREADCall(ram,rom,true,0xFB1E,2,0xFB));
  assert(!TKUniversalPREADCall(ram,rom,false,0xFB1E,0,0xFB));
  assert(!TKUniversalPREADCall(ram,rom,true,0xFB1F,0,0xFB));
  rom[0x3B1E]=0xEA;
  assert(!TKUniversalPREADCall(ram,rom,true,0xFB1E,0,0xFB));
  rom[0x3B1E]=0x38;
  ram[0x4000]=0x4C;
  assert(!TKUniversalPREADCall(ram,rom,true,0xFB1E,0,0xFB));
  ram[0x4000]=0x20;
  // The same call sequence at any ordinary game RAM address works.
  ram[0x6000]=0x20;ram[0x6001]=0x1E;ram[0x6002]=0xFB;
  ram[0x01FC]=0x02;ram[0x01FD]=0x60;
  assert(TKUniversalPREADCall(ram,rom,true,0xFB1E,0,0xFB));

  // Normalize physical keypad identities: changing NumLock between make
  // and break must not leave an UP+LEFT diagonal stuck indefinitely.
  TKJoystickInput input;
  input.event(fabgl::VK_KP_7,true);
  assert(input.mask()==(0x01|0x04));
  input.event(fabgl::VK_KP_HOME,false);
  assert(input.mask()==0);
  input.event(fabgl::VK_KP_END,true);
  assert(input.mask()==(0x02|0x04));
  input.event(fabgl::VK_KP_1,false);
  assert(input.mask()==0);
  input.event(fabgl::VK_KP_8,true);
  input.event(fabgl::VK_UP,true); // real arrow independent of physical KP8
  input.event(fabgl::VK_KP_UP,false);
  assert(input.mask()==0x01);
  input.event(fabgl::VK_UP,false);
  assert(input.mask()==0);
  input.event(fabgl::VK_SPACE,true);
  input.event(fabgl::VK_KP_0,true);
  input.event(fabgl::VK_KP_INSERT,false);
  assert(input.mask()==0x10);
  input.event(fabgl::VK_SPACE,false);
  assert(input.mask()==0);
  input.event(fabgl::VK_KP_DELETE,true);
  assert(input.mask()==0x20);
  input.event(fabgl::VK_KP_PERIOD,false);
  assert(input.mask()==0);

  // Missing KEYUP releases only actual held contacts; a phantom poll
  // alone never causes a key press.
  input.releaseMissing([](fabgl::VirtualKey){return true;});
  assert(input.mask()==0);
  input.event(fabgl::VK_KP_9,true);
  input.releaseMissing([](fabgl::VirtualKey){return false;});
  assert(input.mask()==(0x01|0x08));
  input.releaseMissing([](fabgl::VirtualKey){return false;});
  assert(input.mask()==(0x01|0x08));
  input.releaseMissing([](fabgl::VirtualKey){return false;});
  assert(input.mask()==0);

  assert(TKPaddleModeForProfile(TKGameInputProfile::Standard)==
#if TK2000_JOYSTICK_STANDARD_MODE
    TKPaddleJoystick::Mode::Standard
#else
    TKPaddleJoystick::Mode::Centering
#endif
  );
  TKPaddleJoystick p;
  p.setEnabled(true);
  p.setMask(0x04);assert(p.positionForAxis(0)==0);
  p.setMask(0);assert(p.positionForAxis(0)==TKPaddleJoystick::kCenter);
  p.setMask(0x08);assert(p.positionForAxis(0)==255);
  p.setMask(0);assert(p.positionForAxis(0)==TKPaddleJoystick::kCenter);
  p.setEnabled(false);p.setMask(0x04);
  assert(p.positionForAxis(0)==TKPaddleJoystick::kCenter); // no left drift when OFF
  assert(p.positionForAxis(1)==TKPaddleJoystick::kCenter);

  // Even when a physical SPACE drives FIRE1, it owns the original TK
  // keyboard SPACE contact independently from other fire sources.
  KeyboardTK kb;
  kb.key(' ',true);kb.setJoystickMask(0x10);kb.out=1u<<2;
  assert(kb.in()&1u);
  kb.setJoystickMask(0);assert(kb.in()&1u);
  kb.key(' ',false);assert(!(kb.in()&1u));

  // Compare the packed row path against a byte-for-byte RGB/sync copy
  // and an intentionally unaligned fallback.
  uint8_t src[564];uint8_t dst[570];
  for(unsigned i=0;i<sizeof(src);++i)src[i]=(uint8_t)(i*37+9);
  memset(dst,0,sizeof(dst));
  TKVGACopyPackedRow(dst+4,src,560);
  assert(memcmp(dst+4,src,560)==0);
  memset(dst,0,sizeof(dst));
  TKVGACopyPackedRow(dst+1,src,560);
  assert(memcmp(dst+1,src,560)==0);
  printf("PASS v0.90: universal PREAD, NumLock and KEYUP, physical SPACE, DMA row stores\n");
}
