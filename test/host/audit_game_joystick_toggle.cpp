#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "BuildConfig.h"
#include "Keyboard.h"
#include "JoystickKeys.h"
#include "PaddleJoystick.h"
#include "TKTapeGameControls.h"

static bool contact(KeyboardTK &kb, unsigned row, uint8_t mask) {
  kb.out=static_cast<uint8_t>(1u<<row);
  return (kb.in() & mask)!=0;
}

int main() {
  static_assert(TK2000_GAME_JOYSTICK_COMPAT==0 || TK2000_GAME_JOYSTICK_COMPAT==1,
                "Use 0 or 1");
  const TKGameInputProfile profiles[] = {
    TKGameInputProfile::Montezuma, TKGameInputProfile::MoonPatrol,
    TKGameInputProfile::MissilesDefense
  };
  for (TKGameInputProfile detected : profiles) {
    const TKGameInputProfile effective=TKJoystickProfile(detected);
    KeyboardTK kb;
    kb.setGameInputProfile(detected);
#if TK2000_GAME_JOYSTICK_COMPAT
    assert(effective==detected);
    assert(kb.inputProfile()==detected);
#else
    assert(effective==TKGameInputProfile::Standard);
    assert(kb.inputProfile()==TKGameInputProfile::Standard);
#endif
    kb.setJoystickMask(0x01u); // UP, no synthetic A in default mode
    assert(contact(kb,6,0x01u)); // native UP
#if TK2000_GAME_JOYSTICK_COMPAT
    assert(contact(kb,1,0x20u)==(detected==TKGameInputProfile::Montezuma));
#else
    // v0.96: Montezuma-only A/Z is the single user-requested title rule.
    assert(contact(kb,1,0x20u)==(TK2000_MONTEZUMA_AZ && detected==TKGameInputProfile::Montezuma));
#endif
    kb.setJoystickMask(0x02u); // DOWN, no synthetic Z in default mode
    assert(contact(kb,5,0x01u));
#if TK2000_GAME_JOYSTICK_COMPAT
    assert(contact(kb,0,0x20u)==(detected==TKGameInputProfile::Montezuma));
#else
    assert(contact(kb,0,0x20u)==(TK2000_MONTEZUMA_AZ && detected==TKGameInputProfile::Montezuma));
#endif
    kb.setJoystickMask(0x10u); // FIRE1, no synthetic SPACE in default mode
    assert(contact(kb,7,0x10u)); // native FIRE1
#if TK2000_GAME_JOYSTICK_COMPAT
    assert(contact(kb,2,0x01u)==(detected==TKGameInputProfile::Montezuma ||
                                  detected==TKGameInputProfile::MissilesDefense));
#else
    assert(!contact(kb,2,0x01u));
#endif
    kb.key(' ',true); // physical guest space always preserved
    assert(contact(kb,2,0x01u));
    kb.setJoystickMask(0);
    assert(contact(kb,2,0x01u));
    kb.key(' ',false);
    assert(!contact(kb,2,0x01u));
    kb.setJoystickMask(0x20u); // FIRE2 is independent and generic
    assert(contact(kb,6,0x10u));
    kb.setJoystickMask(0);
    kb.releaseInputs();
    assert(kb.inputProfile()==effective);
  }
  // Default analog semantics are centering for ANY CT2 when profiles OFF.
  TKPaddleJoystick paddle;
  paddle.setEnabled(true);
  for (TKGameInputProfile detected: profiles) {
    TKPaddleJoystick::Mode mode=(TKJoystickProfile(detected)==TKGameInputProfile::MissilesDefense)?
      TKPaddleJoystick::Mode::Standard:TKPaddleJoystick::Mode::Centering;
    paddle.setMode(mode);
    paddle.setMask(0x04u);assert(paddle.positionForAxis(0)==0);
    paddle.setMask(0);
#if TK2000_GAME_JOYSTICK_COMPAT
    assert(paddle.positionForAxis(0)==(detected==TKGameInputProfile::MissilesDefense?0u:(unsigned)TKPaddleJoystick::kCenter));
#else
    assert(paddle.positionForAxis(0)==TKPaddleJoystick::kCenter);
#endif
  }
  // Generic keyboard & joystick capabilities are identical in both builds.
  KeyboardTK kb;
  kb.setGameInputProfile(TKGameInputProfile::Standard);
  kb.setJoystickMask(0x09u); // up + right = diagonal
  assert(contact(kb,6,0x01u) && contact(kb,4,0x01u));
  kb.setJoystickMask(0x30u); // two buttons
  assert(contact(kb,7,0x10u) && contact(kb,6,0x10u));
  kb.setJoystickMask(0);
  for(int c='A';c<='Z';++c) {
    kb.setPhysicalCtrl(true);kb.key(static_cast<uint8_t>(c),true);
    assert(!contact(kb,0,1));
    kb.setShift(true);assert(contact(kb,0,1));
    kb.setShift(false);assert(!contact(kb,0,1));
    kb.key(static_cast<uint8_t>(c),false);kb.setPhysicalCtrl(false);
  }
  assert(TKJoystickConsumesKey(fabgl::VK_KP_5));
  assert(TKJoystickActionMask(fabgl::VK_KP_7)==0x05);
  assert(TKJoystickActionMask(fabgl::VK_KP_PERIOD)==0x20);
  printf("PASS v0.89 joystick A/B switch = %d, all controls verified\n",TK2000_GAME_JOYSTICK_COMPAT);
}
