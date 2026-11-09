#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <initializer_list>
#include "BuildConfig.h"
#include "Keyboard.h"
#include "TKInputEventPacing.h"

static bool contact(KeyboardTK &k,int row,uint8_t bit) {
  k.out=uint8_t(1u<<row);
  return (k.in() & bit)!=0;
}
int main() {
#if !TK2000_JOYSTICK_KEYBOARD_BRIDGE
#error "v0.94 regression must exercise the default universal bridge"
#endif
  // These are physical matrix contacts, not filename-sensitive remaps.
  for(TKGameInputProfile id: {TKGameInputProfile::Standard,
         TKGameInputProfile::Montezuma,TKGameInputProfile::MoonPatrol,
         TKGameInputProfile::MissilesDefense}) {
    KeyboardTK k; k.setGameInputProfile(id);
    assert(!contact(k,0,0x20u));assert(!contact(k,1,0x20u));
    assert(!contact(k,2,0x01u));
    k.setJoystickMask(1u); // UP reaches original joystick row and A
    assert(contact(k,6,1u));assert(contact(k,1,0x20u));
    assert(!contact(k,0,0x20u));
    k.setJoystickMask(2u); // DOWN reaches original joystick row and Z
    assert(contact(k,5,1u));assert(contact(k,0,0x20u));
    assert(!contact(k,1,0x20u));
    k.setJoystickMask(3u); // opposites cancel; no ghost letters
    assert(!contact(k,0,0x20u));assert(!contact(k,1,0x20u));
    assert(!contact(k,6,1u));assert(!contact(k,5,1u));
    k.setJoystickMask(4u); // LEFT is always a native cursor/joystick contact
    assert(contact(k,3,1u));assert(!contact(k,0,0x20u));
    k.setJoystickMask(8u); // RIGHT
    assert(contact(k,4,1u));
    k.setJoystickMask(0x10u); // FIRE1=physical TK button plus SPACE
    assert(contact(k,7,0x10u));assert(contact(k,2,1u));
    k.setJoystickMask(0x20u); // FIRE2 separate; never SPACE
    assert(contact(k,6,0x10u));assert(!contact(k,2,1u));
    k.setJoystickMask(0x10u);
    k.key(' ',true);k.setJoystickMask(0);assert(contact(k,2,1u));
    k.key(' ',false);assert(!contact(k,2,1u));
    k.key('A',true);k.setJoystickMask(1u);
    k.key('A',false);assert(contact(k,1,0x20u));
    k.setJoystickMask(0);assert(!contact(k,1,0x20u));
    k.setJoystickMask(0x04u);k.clearJoystick();
    assert(!contact(k,3,1u)); // OFF = no movement at first frame
    // CONTROL+B is ROW0 bit1 and SHIFT must stay independent.
    k.setPhysicalCtrl(true);k.ctrl=true;k.key('B',true);
    k.ctrl=false; // ROM clears CONTROL-row latch before scanning SHIFT + letters
    assert(contact(k,0,2u));assert(!contact(k,0,1u));
    k.setShift(true);assert(contact(k,0,1u));
    k.key('B',false);k.setShift(false);k.setPhysicalCtrl(false);
  }
  using namespace TKInputEventPacing;
  assert(slice(true,true,true,false,0)==4096u);
  assert(slice(false,true,true,false,0)==0u);
  assert(slice(true,false,true,false,0)==0u);
  assert(slice(true,true,false,false,0)==0u);
  assert(slice(true,true,true,true,0)==0u); // tape loading unchanged
  assert(slice(true,true,true,false,4096u)==4096u);
  assert(slice(true,true,true,false,8192u)==0u);
  // A queued CTRL, B, BREAK sequence must receive guest CPU time while
  // B is held, rather than being collapsed to only the final released state.
  struct Key {bool ctrl,b;};
  const Key events[]={{true,false},{true,true},{true,false},{false,false}};
  uint32_t advanced=0,cyclesWithCtrlB=0;
  uint32_t debt=0,guestCycles=0;
  for(unsigned i=0;i<4;++i) {
    const bool down=i<2;
    const uint32_t amount=slice(down,i+1<4,true,false,advanced);
    if(events[i].ctrl && events[i].b)cyclesWithCtrlB+=amount;
    advanced+=amount;debt+=amount;guestCycles+=amount;
  }
  assert(cyclesWithCtrlB==4096u);
  assert(advanced==8192u);
  uint32_t due=payBack(0,1024,debt);
  assert(due==0 && debt==7168u);
  due=payBack(due,2048,debt);
  assert(due==0 && debt==5120u);
  due=payBack(due,5120,debt);
  assert(due==0 && debt==0u);
  due=payBack(due,4096,debt);
  assert(due==4096u && debt==0u);
  assert(guestCycles+due==12288u);
  puts("PASS v0.94: universal joystick/keyboard contacts, CTRL+B, queued input interleave and cycle debt");
}
