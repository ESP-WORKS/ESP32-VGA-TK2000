#pragma once
#include <stdint.h>

// TK-2000 joystick is connected to the keyboard-matrix input (KBIN), not
// to an Apple II paddle connector.  MAME apple/tk2000.cpp documents this
// connection but explicitly lists joystick emulation as TODO.  The concrete
// row/column mapping and <30 / >225 digital thresholds come from the
// independently implemented Windows TK-2000 0.3 JoyVerificaTecla().
// The host keyboard and a real controller can share this one device model.
struct TKJoystickMatrix {
  static uint8_t axis(uint8_t negative, uint8_t positive) {
    return (!!negative) == (!!positive) ? 127u : (negative ? 0u : 255u);
  }
  // Bits: UP DOWN LEFT RIGHT FIRE1 FIRE2.  Multiple simultaneous/opposing
  // directions resolve to neutral *per axis* without fabricating a keypress.
  static uint8_t rowBits(uint8_t mask, uint8_t row, bool physicalFire) {
    const uint8_t x=axis(mask & 0x04u,mask & 0x08u);
    const uint8_t y=axis(mask & 0x01u,mask & 0x02u);
    uint8_t bits=0;
    if((row==3 && x<30u) || (row==4 && x>225u) ||
       (row==6 && y<30u) || (row==5 && y>225u))bits|=0x01u;
    if(row==7 && (physicalFire || (mask & 0x10u)))bits|=0x10u;
    if(row==6 && (mask & 0x20u))bits|=0x10u;
    return bits;
  }
};
