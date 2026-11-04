#pragma once
#include <stdint.h>
#include "TKTapeGameControls.h"

// The TK-2000 keyboard is a scanned 8-row matrix, not an ASCII FIFO.
// Bits selected by KBOUT ($C000-$C00F) are ORed into the KBIN value
// ($C010-$C01F), with bit 7 set as on the original machine.
class KeyboardTK {
public:
  uint8_t out = 0;
  bool ctrl = false;       // C05E/C05F control latch
  bool ctrlDown = false;   // physical Ctrl key
  void key(uint8_t code, bool down);
  void setShift(bool down);
  // PC CTRL+A..Z generates BASIC token shortcuts on the TK matrix by
  // supplying the original SHIFT+CTRL+letter combination.
  void setPhysicalCtrl(bool down);
  // Physical TK-2000 FIRE button: dedicated TAB input, independent of joystick ON/OFF.
  void setFire(bool down) { physicalFire = down; }
  // F8 (TK REPEAT): pulse the currently held guest key on the scanned matrix.
  // No ASCII FIFO or additional PS/2 typematic injection.
  void setRepeat(bool down);
  void serviceRepeat(uint32_t nowMs);
  void clear();
  // Release host keys without changing the guest KBOUT row select or CTRL latch.
  // Opening F1/F2 must not electrically reset the running TK keyboard hardware.
  void releaseInputs();
  // Joystick directions use the TK-2000 keyboard matrix, like MiSTer RTL.
  // Codes: 0=UP, 1=DOWN, 2=LEFT, 3=RIGHT, 4=BUTTON (SPACE).
  void setJoystick(uint8_t action, bool down);
  void clearJoystick();
  void setJoystickMask(uint8_t mask);
  void setGameInputProfile(TKGameInputProfile profile) {
    if (gameInputProfile!=profile) {
      if (gameInputProfile==TKGameInputProfile::MissilesDefense) key(' ',false);
      gameInputProfile=profile;
      if (gameInputProfile==TKGameInputProfile::MissilesDefense && (joyMask&0x10u))
        key(' ',true);
    }
  }
  uint8_t joystickMask() const { return joyMask; }
  uint8_t in() const;

private:
  uint8_t rows[8] = {};
  uint8_t keyRefs[8][8] = {};
  bool held[256] = {};
  uint8_t heldRow[256] = {};
  uint8_t heldCol[256] = {};
  bool heldForcedShift[256] = {};
  uint8_t forcedShiftCount = 0;
  uint8_t suppressedShiftCount = 0;
  bool heldSuppressShift[256] = {};
  bool physicalShift = false;
  bool physicalFire = false;
  bool repeatHeld = false;
  bool repeatSuppressed = false;
  uint8_t repeatCandidate = 0;
  uint8_t repeatActive = 0;
  uint32_t repeatStartMs = 0;
  uint8_t joyMask = 0;
  TKGameInputProfile gameInputProfile = TKGameInputProfile::Standard;

  void updateShiftBit();
  static bool mapKey(uint8_t code, uint8_t &row, uint8_t &col, bool &forceShift);
};
