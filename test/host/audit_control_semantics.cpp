// v0.91: TK-2000 original control functions versus BASIC keyword chords.
// The scan layout is from Windows TK-2000 0.3 (src/tk_teclado.cpp).
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "Keyboard.h"
#include "TKModifierSides.h"
#include "TKHostKeyOwners.h"

static bool row(KeyboardTK &k,uint8_t r,uint8_t bit) {
  k.out=(uint8_t)(1u<<r);
  k.ctrl=false; // physical TK ROM clears CONTROL latch before scanning rows
  return (k.in() & bit)!=0;
}
static bool ctrl(KeyboardTK &k) {
  k.ctrl=true;
  return (k.in()&1)!=0;
}
int main(){
  // Every A..Z must be a separate matrix key and MUST NOT force SHIFT.
  // A...Z lookup positions match the source code of the Windows v0.3.
  struct Pos {char key;uint8_t r,bit;};
  const Pos positions[]={
    {'A',1,0x20},{'B',0,0x02},{'C',0,0x08},{'D',1,0x08},
    {'E',2,0x08},{'F',1,0x04},{'G',1,0x02},{'H',6,0x02},
    {'I',5,0x08},{'J',6,0x04},{'K',6,0x08},{'L',6,0x10},
    {'M',7,0x04},{'N',7,0x02},{'O',5,0x10},{'P',5,0x20},
    {'Q',2,0x20},{'R',2,0x04},{'S',1,0x10},{'T',2,0x02},
    {'U',5,0x04},{'V',0,0x04},{'W',2,0x10},{'X',0,0x10},
    {'Y',5,0x02},{'Z',0,0x20}
  };
  for(const Pos &p:positions){
    KeyboardTK k;
    assert(!ctrl(k));
    k.setPhysicalCtrl(true);
    k.key((uint8_t)p.key,true);
    assert(ctrl(k));
    // MAME apple/tk2000.cpp: $C05F adds CTRL as ROW8, never masks KBOUT.
    k.ctrl=true;k.out=(uint8_t)(1u<<p.r);
    assert(k.in()==(uint8_t)(0x01u|p.bit)); // v0.96: idle KBIN bit7=0 (MiSTer)
    assert(row(k,p.r,p.bit));
    assert(!row(k,0,0x01)); // CTRL + A..Z != token
    k.setShift(true);
    assert(row(k,0,0x01));  // CTRL + SHIFT + A..Z = token chord
    assert(ctrl(k));
    k.setShift(false);
    assert(!row(k,0,0x01));
    k.key((uint8_t)p.key,false);
    assert(!row(k,p.r,p.bit));
    assert(!row(k,0,0x01));
    k.setPhysicalCtrl(false);
    assert(!ctrl(k));
  }
  // The six documented CONTROL-only functions are never promoted into tokens.
  for(const char c:"CXGRSQ") {
    if (!c) break;
    KeyboardTK k;k.setPhysicalCtrl(true);k.key((uint8_t)c,true);
    assert(ctrl(k));assert(!row(k,0,1));
    k.key((uint8_t)c,false);
    assert(!row(k,0,1));
  }
  // Press/release in both orders; CTRL must never synthesize or latch SHIFT.
  {
    KeyboardTK k;
    k.key('C',true);k.setPhysicalCtrl(true);
    assert(!row(k,0,1));
    k.setShift(true);assert(row(k,0,1));
    k.setPhysicalCtrl(false);assert(row(k,0,1));
    k.setPhysicalCtrl(true);assert(row(k,0,1));
    k.key('C',false);assert(row(k,0,1));
    k.setShift(false);assert(!row(k,0,1));
    k.setPhysicalCtrl(false);assert(!ctrl(k));
    k.releaseInputs();assert(!row(k,0,1));
  }
  // Left/right physical modifiers are distinct owners of a single TK line.
  {
    TKModifierSides sides; KeyboardTK k;
    sides.setCtrl(false,true);sides.setCtrl(true,true);
    k.setPhysicalCtrl(sides.ctrl());k.key('Q',true);
    assert(ctrl(k));assert(!row(k,0,1));
    sides.setCtrl(false,false);k.setPhysicalCtrl(sides.ctrl());
    assert(ctrl(k));assert(!row(k,0,1));
    sides.setShift(false,true);sides.setShift(true,true);
    k.setShift(sides.shift());assert(row(k,0,1));
    sides.setShift(false,false);k.setShift(sides.shift());assert(row(k,0,1));
    sides.setShift(true,false);k.setShift(sides.shift());assert(!row(k,0,1));
    sides.setCtrl(true,false);k.setPhysicalCtrl(sides.ctrl());
    assert(!ctrl(k));k.key('Q',false);
  }
  // Legacy Windows symbol translation and forced shift are unchanged.
  {
    KeyboardTK k;
    k.key('+',true);assert(row(k,5,0x20));assert(row(k,0,1));
    k.key('+',false);assert(!row(k,0,1));
    k.key(':',true);assert(row(k,6,0x20));assert(!row(k,0,1));
    k.key(':',false);
  }
  puts("PASS v0.91 CONTROL-only versus SHIFT+CONTROL matrix (26 letters, both sides)");
}
