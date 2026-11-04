#include "Keyboard.h"
#include <string.h>

bool KeyboardTK::mapKey(uint8_t c, uint8_t &r, uint8_t &col, bool &forceShift) {
  forceShift = false;
  if (c >= 'a' && c <= 'z') c = (uint8_t)(c - 'a' + 'A');
  switch (c) {
    case '?': case '/': r=7; col=5; forceShift=(c=='/'); return true;
    case '.': case '>': r=7; col=4; forceShift=(c=='>'); return true;
    case ',': case '<': r=7; col=3; forceShift=(c=='<'); return true;
    case 'M': r=7; col=2; return true;
    case 'N': r=7; col=1; return true;
    case 13: r=7; col=0; return true;

    case ':': case ';': r=6; col=5; forceShift=(c==';'); return true;
    case 'L': case '@': r=6; col=4; forceShift=(c=='@'); return true;
    case 'K': case '^': r=6; col=3; forceShift=(c=='^'); return true;
    case 'J': r=6; col=2; return true;
    case 'H': r=6; col=1; return true;
    case 0x0B: r=6; col=0; return true; // cursor up

    case 'P': case '+': r=5; col=5; forceShift=(c=='+'); return true;
    case 'O': case '=': r=5; col=4; forceShift=(c=='='); return true;
    case 'I': case '-': r=5; col=3; forceShift=(c=='-'); return true;
    case 'U': r=5; col=2; return true;
    case 'Y': r=5; col=1; return true;
    case 0x0A: r=5; col=0; return true; // cursor down

    case '0': case '*': r=4; col=5; forceShift=(c=='*'); return true;
    case '9': case ')': r=4; col=4; forceShift=(c==')'); return true;
    case '8': case '(': r=4; col=3; forceShift=(c=='('); return true;
    case '7': case '\'': r=4; col=2; forceShift=(c=='\''); return true;
    case '6': case '&': r=4; col=1; forceShift=(c=='&'); return true;
    case 0x15: r=4; col=0; return true; // cursor right

    case '1': case '!': r=3; col=5; forceShift=(c=='!'); return true;
    case '2': case '"': r=3; col=4; forceShift=(c=='"'); return true;
    case '3': case '#': r=3; col=3; forceShift=(c=='#'); return true;
    case '4': case '$': r=3; col=2; forceShift=(c=='$'); return true;
    case '5': case '%': r=3; col=1; forceShift=(c=='%'); return true;
    case 8: r=3; col=0; return true; // backspace / cursor left

    case 'Q': r=2; col=5; return true;
    case 'W': r=2; col=4; return true;
    case 'E': r=2; col=3; return true;
    case 'R': r=2; col=2; return true;
    case 'T': r=2; col=1; return true;
    case ' ': r=2; col=0; return true;

    case 'A': r=1; col=5; return true;
    case 'S': r=1; col=4; return true;
    case 'D': r=1; col=3; return true;
    case 'F': r=1; col=2; return true;
    case 'G': r=1; col=1; return true;

    case 'Z': r=0; col=5; return true;
    case 'X': r=0; col=4; return true;
    case 'C': r=0; col=3; return true;
    case 'V': r=0; col=2; return true;
    case 'B': r=0; col=1; return true;
    default: return false;
  }
}

void KeyboardTK::updateShiftBit() {
  // TK-2000 BASIC keyword shortcuts use SHIFT+CTRL+letter ($F043 emits
  // bit 7 together with bit 6). PC users expect CTRL+letter alone to
  // produce the token. Keep the original physical SHIFT and CTRL inputs
  // independent, and add SHIFT only while at least one A..Z is physically
  // held together with CTRL. No ASCII/token byte is injected by the host.
  bool tokenShift = false;
  if (ctrlDown) {
    for (uint8_t c = 'A'; c <= 'Z'; ++c) {
      if (held[c]) { tokenShift = true; break; }
    }
  }
  if ((physicalShift || forcedShiftCount || tokenShift) && !suppressedShiftCount) rows[0] |= 0x01;
  else rows[0] &= (uint8_t)~0x01;
}

void KeyboardTK::setShift(bool down) {
  physicalShift = down;
  updateShiftBit();
}

void KeyboardTK::setPhysicalCtrl(bool down) {
  ctrlDown = down;
  updateShiftBit(); // also release synthetic SHIFT when CTRL is released first
}

void KeyboardTK::key(uint8_t code, bool down) {
  uint8_t r=0, c=0;
  bool force=false;
  if (!mapKey(code,r,c,force)) return;

  if (down) {
    if (held[code]) return; // ignore typematic repeats; key remains held
    held[code]=true;
    repeatCandidate=code;
    heldRow[code]=r;
    heldCol[code]=c;
    heldForcedShift[code]=force;
    // The original TK matrix has ? and : unshifted, while PC ABNT2
    // physically uses SHIFT for both characters. Suppress only the
    // emulated Shift while that character is held, without affecting
    // other keys or the FabGL US scan-code layout.
    heldSuppressShift[code]=(code=='?' || code==':');
    if ((code=='?' || code==':') && suppressedShiftCount!=0xFF) ++suppressedShiftCount;
    if (keyRefs[r][c] != 0xFF) ++keyRefs[r][c];
    rows[r] |= (uint8_t)(1u << c);
    if (force && forcedShiftCount != 0xFF) ++forcedShiftCount;
  } else {
    // A modifier can change FabGL's derived VK (for example VK_a -> VK_A)
    // between key-down and key-up. Release by physical matrix position as a
    // fallback so such a transition cannot leave a key stuck.
    uint8_t heldCode=code;
    if (!held[heldCode]) {
      bool found=false;
      for (unsigned i=0;i<256;i++) {
        if (held[i] && heldRow[i]==r && heldCol[i]==c) {
          heldCode=(uint8_t)i;
          found=true;
          break;
        }
      }
      if (!found) return;
    }
    r=heldRow[heldCode]; c=heldCol[heldCode];
    if (keyRefs[r][c]) --keyRefs[r][c];
    if (!keyRefs[r][c]) rows[r] &= (uint8_t)~(1u << c);
    if (heldForcedShift[heldCode] && forcedShiftCount) --forcedShiftCount;
    if (heldSuppressShift[heldCode] && suppressedShiftCount) --suppressedShiftCount;
    heldSuppressShift[heldCode]=false;
    held[heldCode]=false;
    if (repeatCandidate==heldCode) {
      repeatCandidate=0;
      for(unsigned i=1;i<256;++i) if(held[i]) repeatCandidate=(uint8_t)i;
    }
    if (repeatActive==heldCode) { repeatActive=0; repeatSuppressed=false; }
    heldForcedShift[heldCode]=false;
  }
  updateShiftBit();
}

void KeyboardTK::setRepeat(bool down) {
  if (repeatHeld==down) return; // Ignore PS/2 typematic F8 keydowns.
  repeatHeld=down;
  repeatActive=0;
  repeatSuppressed=false;
}

void KeyboardTK::serviceRepeat(uint32_t nowMs) {
  if (!repeatHeld || !repeatCandidate || !held[repeatCandidate]) {
    repeatActive=0;
    repeatSuppressed=false;
    return;
  }
  if (repeatActive!=repeatCandidate) {
    repeatActive=repeatCandidate;
    repeatStartMs=nowMs;
    repeatSuppressed=false;
    return;
  }
  // Press F8 while holding the character. The first stroke is unchanged;
  // after 350ms, blank the matrix contact for 40ms every 140ms (~7/s).
  // BASIC sees distinct releases and re-presses, rather than a long hold.
  const uint32_t age=nowMs-repeatStartMs;
  repeatSuppressed=age>=350u && ((age-350u)%140u)<40u;
}

void KeyboardTK::setJoystick(uint8_t action, bool down) {
  if (action > 4) return;
  const uint8_t bit=(uint8_t)(1u << action);
  setJoystickMask(down?(uint8_t)(joyMask|bit):(uint8_t)(joyMask&~bit));
}
void KeyboardTK::clearJoystick() {
  joyMask=0;
  if (gameInputProfile==TKGameInputProfile::MissilesDefense) key(' ',false);
}
void KeyboardTK::setJoystickMask(uint8_t mask) {
  const uint8_t next=(uint8_t)(mask & 0x1Fu);
  if (gameInputProfile==TKGameInputProfile::MissilesDefense &&
      ((next^joyMask)&0x10u))
    key(' ',(next&0x10u)!=0u);
  joyMask=next;
}

uint8_t KeyboardTK::in() const {
  uint8_t value=0x80;
  // CTRL is a separate matrix drive line selected by $C05E/$C05F.
  // It must be ORed with scanned letter contacts, not returned instead of
  // them: otherwise CTRL+A..Z appears as a lone CONTROL key (BASIC beep).
  if (ctrl && ctrlDown) value |= 0x01u;
  // Opposite directions on the same axis cancel to the neutral center,
  // matching the original emulator's keyboard joystick (average x/y = 127).
  // Do not turn both inputs ON: a game which gives LEFT precedence would then
  // move by itself while two keys are held.
  uint8_t activeJoy=joyMask;
  if ((activeJoy & 0x03u)==0x03u) activeJoy &= (uint8_t)~0x03u;
  if ((activeJoy & 0x0Cu)==0x0Cu) activeJoy &= (uint8_t)~0x0Cu;
  for (uint8_t row=0; row<8; ++row) {
    if (!(out & (1u << row))) continue;
    uint8_t physicalRows=rows[row];
    if (repeatSuppressed && repeatActive && held[repeatActive] &&
        heldRow[repeatActive]==row &&
        keyRefs[row][heldCol[repeatActive]]==1)
      physicalRows &= (uint8_t)~(1u << heldCol[repeatActive]);
    value |= physicalRows;
    // Joystick shares keyboard rows/columns, not analog Apple II paddle IO.
    // MiSTer joystick mapping: rows 6/5/4/3, column 0 for directions;
    // The reference JoyVerificaTecla() reports BUTTON1 at row 7, column 4.
    // SPACE is only the physical PC key used to trigger that virtual button.
    if ((row == 6 && (activeJoy & 0x01)) ||
        (row == 5 && (activeJoy & 0x02)) ||
        (row == 3 && (activeJoy & 0x04)) ||
        (row == 4 && (activeJoy & 0x08))) value |= 0x01;
    if (row == 7 && physicalFire) value |= 0x10; // TAB: original FIRE even if joystick is OFF
    if (row == 7 && (activeJoy & 0x10)) value |= 0x10;
    // This particular Montezuma CT2 translates A/Z into the VERTICAL axis
    // (see game code $7658); the ordinary physical TK joystick directions
    // alone cannot drive that axis. Add A/Z only for this exact tape and
    // only when virtual joystick is enabled. UP=A, DOWN=Z.
    // Missiles Defense's original program reads the TK keyboard matrix for
    // the trigger, even though it calls Apple PREAD for the aim coordinates.
    // Virtual SPACE must also appear at the TK SPACE matrix contact while
    // JOYSTICK ON; this is isolated by the exact tape CRC fingerprint.
    if (gameInputProfile == TKGameInputProfile::MissilesDefense &&
        row == 2 && (activeJoy & 0x10u)) value |= 0x01u;
    if (gameInputProfile == TKGameInputProfile::Montezuma) {
      if (row == 1 && (activeJoy & 0x01u)) value |= 0x20u; // A
      if (row == 0 && (activeJoy & 0x02u)) value |= 0x20u; // Z
      // The supplied game checks the TK SPACE matrix contact for JUMP
      // even in joystick mode. Keep the ordinary FIRE row as well, so
      // both joystick button and legacy SPACE-based game controls work.
      if (row == 2 && (activeJoy & 0x10u)) value |= 0x01u; // SPACE
    }
  }
  return value;
}

void KeyboardTK::releaseInputs() {
  // F1/F2 keyboard capture discards PS/2 key-up events. Release only the
  // physical keys and virtual joystick, preserving both hardware latches:
  // KBOUT ($C000, guest-selected scan rows) and CTRL ($C05E/$C05F).
  // Otherwise a game which preselected its scan row during initialization
  // loses joystick input until it reinitializes (often after cold reset).
  const uint8_t selectedRows=out;
  const bool controlLatch=ctrl;
  clear();
  out=selectedRows;
  ctrl=controlLatch;
}

void KeyboardTK::clear() {
  memset(rows,0,sizeof(rows));
  memset(keyRefs,0,sizeof(keyRefs));
  memset(held,0,sizeof(held));
  memset(heldRow,0,sizeof(heldRow));
  memset(heldCol,0,sizeof(heldCol));
  memset(heldForcedShift,0,sizeof(heldForcedShift));
  memset(heldSuppressShift,0,sizeof(heldSuppressShift));
  out=0; ctrl=false; ctrlDown=false; forcedShiftCount=0; suppressedShiftCount=0;
  physicalShift=false; physicalFire=false; joyMask=0;
  gameInputProfile=TKGameInputProfile::Standard;
  repeatHeld=false; repeatSuppressed=false; repeatCandidate=repeatActive=0; repeatStartMs=0;
}
