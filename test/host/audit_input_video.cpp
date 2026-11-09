#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <set>
#include <random>
#include "Keyboard.h"
#include "JoystickInput.h"
#include "TKHostKeyOwners.h"
#include "PaddleJoystick.h"
#include "TKMissilesAim.h"
#include "TKVGARawSpan.h"
#include "TKModifierSides.h"
static void row(KeyboardTK &k,uint8_t select) {k.out=uint8_t(1u<<select);}
static bool pressed(KeyboardTK &k,uint8_t selected,uint8_t bit) {
 row(k,selected);return (k.in() & bit)!=0;
}
int main() {
 KeyboardTK k;
 // v0.91: CONTROL+letter must preserve the original CONTROL functions.
 // Only a physically held SHIFT can enable the BASIC token combination.
 for(int c='A';c<='Z';++c) {
   k.setPhysicalCtrl(true);k.key((uint8_t)c,true);
   assert(!pressed(k,0,1));
   k.setShift(true);assert(pressed(k,0,1));
   k.setShift(false);assert(!pressed(k,0,1));
   k.key((uint8_t)c,false);assert(!pressed(k,0,1));
   k.setPhysicalCtrl(false);
 }
 TKModifierSides sides; sides.setCtrl(false,true); sides.setCtrl(true,true);
 sides.setCtrl(false,false); assert(sides.ctrl());sides.setCtrl(true,false);assert(!sides.ctrl());
 // Mount profile -> menu release -> F4 toggle must NOT drop identity.
 k.setGameInputProfile(TKGameInputProfile::MissilesDefense);
 assert(k.inputProfile()==TKGameInputProfile::MissilesDefense);
 k.releaseInputs();assert(k.inputProfile()==TKGameInputProfile::MissilesDefense);
 // Guest SPACE contact is the OR of physical and virtual owners.
 k.setJoystickMask(0x10);assert(pressed(k,2,0x01));
 k.key(' ',true);k.setJoystickMask(0);assert(pressed(k,2,0x01));
 k.setJoystickMask(0x10);k.key(' ',false);assert(pressed(k,2,0x01));
 k.setJoystickMask(0);assert(!pressed(k,2,0x01));
 k.setGameInputProfile(TKGameInputProfile::Montezuma);
 k.setJoystickMask(0x11);assert(pressed(k,1,0x20));assert(pressed(k,2,0x01));
 k.releaseInputs();assert(k.inputProfile()==TKGameInputProfile::Montezuma);
 k.setJoystickMask(0x12);assert(pressed(k,0,0x20));assert(pressed(k,2,0x01));
 // TK Windows 0.3 joybutton[1] = row 6 bit 4; button[0] = row 7 bit 4.
 k.setGameInputProfile(TKGameInputProfile::Standard);
 k.setJoystickMask(0x30);
 assert(pressed(k,6,0x10));assert(pressed(k,7,0x10));
 k.setJoystickMask(0x00);assert(!pressed(k,6,0x10));assert(!pressed(k,7,0x10));
 TKPaddleJoystick pd;pd.setEnabled(true);pd.setMask(0x30);
 assert(pd.read(0xC061,0)==0xFF);assert(pd.read(0xC062,0)==0xFF);
 pd.setMask(0);assert(pd.read(0xC061,0)==0x7F);assert(pd.read(0xC062,0)==0x7F);
 // Directions map to genuine matrix rows and opposing directions neutralize.
 k.setJoystickMask(0x01);assert(pressed(k,6,0x01));
 k.setJoystickMask(0x02);assert(pressed(k,5,0x01));
 k.setJoystickMask(0x04);assert(pressed(k,3,0x01));
 k.setJoystickMask(0x08);assert(pressed(k,4,0x01));
 k.setJoystickMask(0x0C);assert(!pressed(k,3,0x01));assert(!pressed(k,4,0x01));
 // Real PS/2 events create state; stale state polls cannot create presses.
 TKJoystickInput joys;
 assert(TKJoystickActionMask(fabgl::VK_KP_7)==(0x01|0x04));
 assert(TKJoystickActionMask(fabgl::VK_KP_END)==(0x02|0x04));
 assert(TKJoystickActionMask(fabgl::VK_KP_PERIOD)==0x20);
 std::set<fabgl::VirtualKey> physicallyDown;
 joys.event(fabgl::VK_KP_7,true);physicallyDown.insert(fabgl::VK_KP_7);
 assert(joys.mask()==(0x01|0x04));
 joys.releaseMissing([&](fabgl::VirtualKey vk){return physicallyDown.count(vk)!=0;});
 assert(joys.mask()==(0x01|0x04));
 physicallyDown.clear();
 for (int poll=0;poll<2;++poll) {
   joys.releaseMissing([&](fabgl::VirtualKey vk){return physicallyDown.count(vk)!=0;});
   assert(joys.mask()==(0x01|0x04));
 }
 joys.releaseMissing([&](fabgl::VirtualKey vk){return physicallyDown.count(vk)!=0;});
 assert(joys.mask()==0);
 joys.event(fabgl::VK_KP_PERIOD,true);assert(joys.mask()==0x20);
 joys.event(fabgl::VK_KP_PERIOD,false);assert(joys.mask()==0);
 joys.event(fabgl::VK_SPACE,true);assert(joys.mask()==0x10);
 joys.event(fabgl::VK_KP_0,true);joys.event(fabgl::VK_SPACE,false);
 assert(joys.mask()==0x10);
 joys.event(fabgl::VK_KP_0,false);assert(joys.mask()==0);
 // Missiles aim preserves absolute X position on release.
 TKMissilesAim aim;aim.reset();aim.advance(100000,0x04);
 assert(aim.positionForAxis(0)<127);
 int stable=aim.positionForAxis(0);
 aim.advance(900000,0); assert(aim.positionForAxis(0)==stable);
 // v0.88: two distinct physical keys owning one guest matrix contact.
 TKHostKeyOwners owners;KeyboardTK guest;
 const auto emit=[&](uint8_t key,bool down){guest.key(key,down);};
 owners.event(fabgl::VK_KP_1,true,'1',emit);
 assert(pressed(guest,3,0x20));
 owners.event(fabgl::VK_KP_1,true,'1',emit); // typematic cannot double own
 assert(owners.active()==1);
 owners.event(fabgl::VK_KP_2,true,'1',emit); // model another physical key -> 1
 assert(owners.active()==2);
 owners.event(fabgl::VK_KP_1,false,'1',emit);
 assert(pressed(guest,3,0x20)); // other owner is still holding it
 owners.event(fabgl::VK_KP_2,false,'1',emit);
 assert(!pressed(guest,3,0x20));
 // PS/2 event-loss recovery only RELEASES; a stale positive poll cannot
 // inject a phantom KEYDOWN on a key which never had one.
 owners.event(fabgl::VK_KP_3,true,'A',emit);
 assert(pressed(guest,1,0x20));
 for (int poll=0;poll<2;++poll) {
   owners.releaseMissing([](fabgl::VirtualKey){return false;},emit);
   assert(pressed(guest,1,0x20));
 }
 owners.releaseMissing([](fabgl::VirtualKey){return false;},emit);
 assert(!pressed(guest,1,0x20));
 assert(owners.active()==0);
 owners.releaseMissing([](fabgl::VirtualKey){return true;},emit);
 assert(owners.active()==0);
 // Unmatched VK release can match a translated physical contact.
 owners.event(fabgl::VK_KP_2,true,'B',emit);
 owners.event(fabgl::VK_KP_3,false,'B',emit);
 assert(!pressed(guest,0,0x02));
 // Keypad 5 is joystick NEUTRAL: must NOT type ASCII '5' in BASIC.
 assert(TKJoystickActionMask(fabgl::VK_KP_5)==0);
 assert(TKJoystickConsumesKey(fabgl::VK_KP_5));
 assert(!TKJoystickConsumesKey(fabgl::VK_NONE));
 // Modifier recovery is per SIDE; releasing LCTRL cannot clear RCTRL.
 sides.clear();sides.setCtrl(false,true);sides.setCtrl(true,true);
 sides.releaseMissing(false,false,false,true);assert(sides.ctrl());
 for (int poll=0;poll<3;++poll) sides.releaseMissing(false,false,false,false);
 assert(!sides.ctrl());
 // Emulated analog CENTERING vs STANDARD modes from Windows 0.3.
 TKPaddleJoystick analog;analog.setEnabled(true);
 analog.setMask(0x04);assert(analog.positionForAxis(0)==0);
 analog.setMask(0);assert(analog.positionForAxis(0)==TKPaddleJoystick::kCenter);
 analog.setMode(TKPaddleJoystick::Mode::Standard);
 analog.setMask(0x08);assert(analog.positionForAxis(0)==255);
 analog.setMask(0);assert(analog.positionForAxis(0)==255);
 analog.setMask(0x04);analog.setMask(0);
 assert(analog.positionForAxis(0)==0);
 analog.setMask(0x05);assert(analog.positionForAxis(1)==0);
 analog.setMask(0x04);assert(analog.positionForAxis(1)==0); // STANDARD holds on KEYUP
 analog.setMask(0x0C);assert(analog.positionForAxis(0)==TKPaddleJoystick::kCenter); // opposed axes average
 analog.setMask(0);assert(analog.positionForAxis(0)==TKPaddleJoystick::kCenter);
 analog.setAbsoluteOverride(true,53,145);
 assert(analog.positionForAxis(0)==53 && analog.positionForAxis(1)==145);
 analog.trigger(12345);assert(analog.read(0xC064,12345)==0xFF);
 assert(analog.read(0xC064,12345+54*11)==0x7F);
 analog.setAbsoluteOverride(false);
 analog.setMode(TKPaddleJoystick::Mode::Centering);
 assert(analog.positionForAxis(0)==TKPaddleJoystick::kCenter);
 analog.setEnabled(false);analog.setMask(0x04);
 assert(analog.positionForAxis(0)==TKPaddleJoystick::kCenter);
 // Original game profiles and fire owner composition survive menu release.
 guest.setGameInputProfile(TKGameInputProfile::Montezuma);
 guest.releaseInputs();assert(guest.inputProfile()==TKGameInputProfile::Montezuma);
 guest.setJoystickMask(0x10);assert(pressed(guest,2,0x01));
 guest.setJoystickMask(0);assert(!pressed(guest,2,0x01));
 // All 18 translated TK punctuation contacts, including synthetic shift.
 struct CharacterCase {uint8_t code;uint8_t row;uint8_t bit;bool tkShift;};
 const CharacterCase chars[]={
   {'?',7,0x20,false},{'/',7,0x20,true},{'.',7,0x10,false},{'>',7,0x10,true},
   {',',7,0x08,false},{'<',7,0x08,true},{':',6,0x20,false},{';',6,0x20,true},
   {'@',6,0x10,true},{'^',6,0x08,true},{'+',5,0x20,true},{'=',5,0x10,true},
   {'-',5,0x08,true},{'*',4,0x20,true},{'\'',4,0x04,true},{'"',3,0x10,true},
   {'&',4,0x02,true},{'!',3,0x20,true},{'#',3,0x08,true},{'$',3,0x04,true},
   {'%',3,0x02,true},{'(',4,0x08,true},{')',4,0x10,true}
 };
 for (const auto &tc:chars) {
   KeyboardTK symbols;
   symbols.key(tc.code,true);
   assert(pressed(symbols,tc.row,tc.bit));
   assert(pressed(symbols,0,0x01)==tc.tkShift);
   symbols.key(tc.code,false);
   assert(!pressed(symbols,tc.row,tc.bit));
   assert(!pressed(symbols,0,0x01));
 }
 // On ABNT2, ? and : are unshifted in the TK matrix even though the PC
 // layout may require a physical SHIFT to generate those symbols.
 KeyboardTK abnt;abnt.setShift(true);abnt.key('?',true);
 assert(!pressed(abnt,0,0x01));
 abnt.key('?',false);assert(pressed(abnt,0,0x01));
 abnt.key(':',true);assert(!pressed(abnt,0,0x01));
 abnt.key(':',false);assert(pressed(abnt,0,0x01));
 abnt.setShift(false);assert(!pressed(abnt,0,0x01));
 // Simultaneous TAB FIRE and virtual FIRE1 are independent owners.
 KeyboardTK fireTest;fireTest.setFire(true);fireTest.setJoystickMask(0x10);
 assert(pressed(fireTest,7,0x10));
 fireTest.setFire(false);assert(pressed(fireTest,7,0x10));
 fireTest.setJoystickMask(0);assert(!pressed(fireTest,7,0x10));
 // Raw packed spans MUST be pixel identical to the reference FabGL
 // setRawPixel path for arbitrary rectangles and repeated clears.
 uint8_t fast[640]={}, slow[640]={};
 std::mt19937 gen(20261104);
 for(int trial=0;trial<5000;trial++) {
   int x=gen()%640;
   int end=x+(gen()%(641-x));
   uint8_t raw=(uint8_t)(gen()%256);
   TKVGAFillPackedSpan(fast,x,end,raw);
   for(int i=x;i<end;i++)slow[i^2]=raw;
   assert(memcmp(fast,slow,sizeof(fast))==0);
 }
 puts("PASS v0.88: host multi-owner PS2, lost KEYUP recovery, modifiers, keypad5, CENTERING/STANDARD, CT2 profiles, Missiles, 5000 VGA spans");
}
