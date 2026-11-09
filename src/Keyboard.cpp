#include "Keyboard.h"
#include "TKJoystickMatrix.h"
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
  // Match the physical TK-2000 keyboard and the Windows 0.3 reference:
  // CONTROL and SHIFT are INDEPENDENT contacts. CTRL+letter must not
  // synthesize SHIFT: the TK ROM uses combinations such as CTRL+C/X/G/R/S/Q
  // for control functions. BASIC keyword tokens require a REAL simultaneous
  // CTRL+SHIFT+letter press. Other glyphs with a fixed TK shifted mapping
  // (such as + or @) still use forcedShiftCount as before.
  if ((physicalShift || forcedShiftCount) && !suppressedShiftCount)
    rows[0] |= 0x01;
  else
    rows[0] &= (uint8_t)~0x01;
}

void KeyboardTK::setShift(bool down) {
  physicalShift = down;
  updateShiftBit();
}

void KeyboardTK::setPhysicalCtrl(bool down) {
  ctrlDown = down;
  // The CTRL contact must not modify the original SHIFT row.
  updateShiftBit();
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
  if (action > 5) return;
  const uint8_t bit=(uint8_t)(1u << action);
  setJoystickMask(down?(uint8_t)(joyMask|bit):(uint8_t)(joyMask&~bit));
}
void KeyboardTK::clearJoystick() {
  joyMask=0;
}
void KeyboardTK::setJoystickMask(uint8_t mask) {
  // Fire sources are composed at KBIN read time. Never synthesize a
  // keyboard.key(' ') press: a physical SPACE key-up would otherwise
  // release a still-held joystick button, or vice versa.
  joyMask=(uint8_t)(mask & 0x3Fu);
}

uint8_t KeyboardTK::in() const {
  // KBIN = {cassette input, printer BUSY, six matrix columns} (MiSTer
  // rtl/tk2000.v: kbd_data_s = {cas_i, lpt_busy_i, kbd_cols_i}). With no
  // tape signal and no printer both upper lines read 0. v0.96: idle bit 7 is
  // 0, not 1. TK software tests "key present" with LDA $C010 / CMP #$80
  // (e.g. the TK Choplifter keyboard driver), so a constant bit 7 looked
  // like a permanently pressed key/button. The cassette path ORs the real
  // tape level into bit 7 separately (TKMachine::read), and the TK ROM
  // reader is polarity independent (EOR $2F).
  uint8_t value=0x00;
  // MAME tk2000.cpp kbin_r(): the $C05F CONTROL latch selects an
  // ADDITIONAL input row, independent of KBOUT.  The former v0.91 Windows
  // shortcut returned 0x81 immediately, discarding all selected lines.
  // This difference is observable to arbitrary guest keyboard scanners and
  // programs; use the actual matrix OR, not a synthetic ASCII code.
  if (ctrl && ctrlDown) value |= 0x01u;
  for (uint8_t row=0; row<8; ++row) {
    if (!(out & (1u << row))) continue;
    uint8_t physicalRows=rows[row];
    if (repeatSuppressed && repeatActive && held[repeatActive] &&
        heldRow[repeatActive]==row &&
        keyRefs[row][heldCol[repeatActive]]==1)
      physicalRows &= (uint8_t)~(1u << heldCol[repeatActive]);
    value |= physicalRows;
    // MAME confirms the joystick is wired to KBIN/KBOUT.  Windows 0.3
    // provides the tested row/column contacts and normalized thresholds.
    value |= TKJoystickMatrix::rowBits(joyMask,row,physicalFire);
#if TK2000_JOYSTICK_KEYBOARD_BRIDGE
    // Generic multi-interface keyboard bridge. Some 6502 programs poll
    // KBOUT directions, others use the native A/Z vertical keys or SPACE.
    // Expose the same host joystick state to all three existing contacts,
    // without any title/CRC/RAM-specific condition or key ownership hack.
    // The joystick is already cleared whenever JOYSTICK is OFF.
    if (row == 1 && (joyMask & 0x01u) && !(joyMask & 0x02u)) value |= 0x20u; // A/up
    if (row == 0 && (joyMask & 0x02u) && !(joyMask & 0x01u)) value |= 0x20u; // Z/down
    if (row == 2 && (joyMask & 0x10u)) value |= 0x01u; // SPACE/FIRE1
#endif
#if TK2000_MONTEZUMA_AZ
    if (montezumaAZ) {
      const bool up=((joyMask & 0x01u) && !(joyMask & 0x02u)) || (rows[6] & 0x01u);
      const bool down=((joyMask & 0x02u) && !(joyMask & 0x01u)) || (rows[5] & 0x01u);
      if (row == 1 && up && !down) value |= 0x20u;   // A
      if (row == 0 && down && !up) value |= 0x20u;   // Z
    }
#endif
#if TK2000_GAME_JOYSTICK_COMPAT
    // Legacy v0.88 per-tape matrix extensions, available only in the
    // explicitly opted-in historical A/B build (never in standard v0.94).
    if (gameInputProfile == TKGameInputProfile::MissilesDefense &&
        row == 2 && (joyMask & 0x10u)) value |= 0x01u;
    if (gameInputProfile == TKGameInputProfile::Montezuma) {
      if (row == 1 && (joyMask & 0x01u)) value |= 0x20u; // A
      if (row == 0 && (joyMask & 0x02u)) value |= 0x20u; // Z
      // The supplied game checks the TK SPACE matrix contact for JUMP
      // even in joystick mode. Keep the ordinary FIRE row as well, so
      // both joystick button and legacy SPACE-based game controls work.
      if (row == 2 && (joyMask & 0x10u)) value |= 0x01u; // SPACE
    }
#endif
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
  const TKGameInputProfile profile=gameInputProfile;
  clear();
  out=selectedRows;
  ctrl=controlLatch;
  // F1/F2/F3/F4 capture is a HOST-only key release: the CT2 profile
  // remains active while a game is running. Resetting it to Standard
  // silently disabled Montezuma/Missiles on every F4 toggle.
  gameInputProfile=profile;
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
