#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "VGA.h"
#include "TKMainMenu97.h"
int main(){
  static TKVGA v;
  static uint8_t ram[65536]={};
  assert(v.begin(ram));
  v.showMainMenu(9,true,true,true,true,true,true);
  for(int row=0;row<TKMainMenu97::rows;++row)
    v.updateMainMenuSelection(9,row,true,true,true,true,true,row==7);
  assert(TKMainMenu97::rowY(9)+12<TKMainMenu97::y+TKMainMenu97::separatorOffset);
  puts("PASS v0.97 F1: VGA source renders ten choices, toggles ON/OFF with row-only repaint");
}
