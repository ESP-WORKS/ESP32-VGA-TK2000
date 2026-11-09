#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "BuildConfig.h"
#include "TKGameportDecode.h"
#include "PaddleJoystick.h"
#include "Keyboard.h"

using namespace TKGameportDecode;

static uint8_t gameport(TKPaddleJoystick &pad, uint16_t addr,uint64_t cycle) {
  if (!isGameport(addr)) return 0u;
  const uint16_t port=canonicalGameport(addr);
  return port==0xC060u?0x7Fu:pad.read(port,cycle);
}
static uint8_t strobe(TKPaddleJoystick &pad,uint16_t addr,uint64_t cycle) {
  if (isPaddleStrobe(addr)) {pad.trigger(cycle);return 0x7Fu;}
  return 0u;
}
int main() {
  static_assert(sizeof(uint16_t)==2,"guest address is 16 bits");
  assert(!isGameport(0xC05Fu));assert(!isGameport(0xC070u));
  assert(!isPaddleStrobe(0xC06Fu));assert(!isPaddleStrobe(0xC080u));
  TKPaddleJoystick pad;pad.setEnabled(true);pad.setMask(0x10u);
  for(unsigned offset=0;offset<16u;++offset) {
    const uint16_t addr=uint16_t(0xC060u+offset);
    assert(isGameport(addr));
    const uint16_t canonical=uint16_t(0xC060u+(offset&7u));
    assert(canonicalGameport(addr)==canonical);
    assert(gameport(pad,addr,1000)==gameport(pad,canonical,1000));
    assert(isPaddleStrobe(uint16_t(0xC070u+offset)));
  }
  // Both regular and mirrored pushbuttons are independent active-high bit7.
  assert(gameport(pad,0xC061u,0)==0xFFu);
  assert(gameport(pad,0xC069u,0)==0xFFu);
  assert(gameport(pad,0xC062u,0)==0x7Fu);
  assert(gameport(pad,0xC06Au,0)==0x7Fu);
  pad.setMask(0x20u);
  assert(gameport(pad,0xC061u,0)==0x7Fu);
  assert(gameport(pad,0xC069u,0)==0x7Fu);
  assert(gameport(pad,0xC062u,0)==0xFFu);
  assert(gameport(pad,0xC06Au,0)==0xFFu);
  // All C07x aliases must recharge the same analog capacitor. The axis is
  // CENTERING 128 (v0.96), so its holdoff is exactly 128*11 guest CPU cycles.
  pad.setMask(0);
  for(unsigned alias=0;alias<16u;++alias) {
    const uint64_t cycle=uint64_t(10000u+alias*3000u);
    assert(strobe(pad,uint16_t(0xC070u+alias),cycle)==0x7Fu);
    assert(gameport(pad,0xC064u,cycle)==0xFFu);
    assert(gameport(pad,0xC06Cu,cycle+1407u)==0xFFu);
    assert(gameport(pad,0xC064u,cycle+1408u)==0x7Fu);
    assert(gameport(pad,0xC06Cu,cycle+1408u)==0x7Fu);
  }
  // JOYSTICK OFF still returns neutral; it must not hard-left at power-on.
  pad.setEnabled(false);
  pad.trigger(100000u);
  assert(gameport(pad,0xC064u,100000u)==0xFFu);
  assert(gameport(pad,0xC06Cu,101407u)==0xFFu);
  assert(gameport(pad,0xC064u,101408u)==0x7Fu);
  assert(gameport(pad,0xC061u,100000u)==0x7Fu);
  // Physical TK CONTROL+B lives at row8 bit0 + row0 bit1. SHIFT is a
  // separate row0 bit0. The graphics-mode BASIC action is NOT simulated in
  // this host test, it needs a running ROM + display integration test.
  KeyboardTK kb;
  kb.setPhysicalCtrl(true);
  kb.key('B',true);
  kb.ctrl=true;kb.out=1u;
  assert((kb.in()&0x03u)==0x03u);
  assert((kb.in()&0x04u)==0);
  kb.ctrl=false;
  assert((kb.in()&0x03u)==0x02u);
  kb.setShift(true);
  assert((kb.in()&0x03u)==0x03u);
  kb.setShift(false);kb.key('B',false);kb.setPhysicalCtrl(false);
  assert((kb.in()&0x03u)==0u);
  puts("PASS v0.95 FPGA C06x mirrors/C07x strobes, stable neutral, independent FIRE, CTRL+B matrix");
}
