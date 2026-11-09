// Run the authentic TK-2000 monitor keyboard routine against the exact
#include <initializer_list>
// virtual keyboard matrix; no ROM bytes are embedded or distributed.
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "CPU6502.h"
#include "Bus.h"
#include "Keyboard.h"
static uint8_t ram[65536], rom[16384];
static KeyboardTK keys;
static int ioCtrl=0, reads=0, rowWrites=0;
static bool windowsCtrlPriority=false;
uint8_t TKBus::read(uint16_t a) {
  if(a>=0xC010 && a<=0xC01F) {++reads; if(windowsCtrlPriority && keys.ctrl && keys.ctrlDown) return 0x81u; return keys.in();}
  if(a==0xC05E) {keys.ctrl=false;ioCtrl=0;return 0;}
  if(a==0xC05F) {keys.ctrl=true;ioCtrl=1;return 0;}
  if(a>=0xC200) return rom[a-0xC000];
  return ram[a];
}
void TKBus::write(uint16_t a,uint8_t v) {
  if(a>=0xC000 && a<=0xC00F) {keys.out=v;++rowWrites;}
  else if(a==0xC05E) {keys.ctrl=false;ioCtrl=0;}
  else if(a==0xC05F) {keys.ctrl=true;ioCtrl=1;}
  else if(a<0xC000) ram[a]=v;
}
static uint8_t run(char c, bool ctrl, bool shift) {
  memset(ram,0,sizeof(ram)); keys.clear();reads=rowWrites=0;
  keys.setPhysicalCtrl(ctrl); keys.setShift(shift);keys.key(c,true);
  // JSR $F043 at $4000, followed by BRK; the C64? ROM returns via RTS.
  ram[0x4000]=0x20;ram[0x4001]=0x43;ram[0x4002]=0xF0;
  ram[0x4003]=0xEA;
  TKBus bus;CPU6502 cpu;cpu.attach(&bus);cpu.PC=0x4000;cpu.S=0xFD;
  int steps=0;while(cpu.PC!=0x4003 && steps++<120000) {
    cpu.step();
  }
  assert(cpu.PC==0x4003 && "the authentic keyboard monitor did not return");
  assert(reads>0 && rowWrites>0);
  return cpu.A;
}
int main(int argc,char** argv) {
 if(argc!=2) {fprintf(stderr,"usage: %s TK2000.rom\n",argv[0]);return 2;}
 FILE *f=fopen(argv[1],"rb");if(!f){perror(argv[1]);return 2;}
 if(fread(rom,1,sizeof(rom),f)!=sizeof(rom)) return 3;
 fclose(f);
 // These are the routine's returned scan codes (not BASIC-side effects).
 const struct {char key;uint8_t plain,control,token;} cases[]={
   {'B',0xC2,0x82,0x12}, // CTRL+B, authentic graphics-mode shortcut
   {'C',0xC3,0x83,0x1A},{'X',0xD8,0x98,0x17},
   {'G',0xC7,0x87,0x10},{'R',0xD2,0x92,0x0B},
   {'S',0xD3,0x93,0x15},{'Q',0xD1,0x91,0x08},
   {'P',0xD0,0x90,0x44},{'A',0xC1,0x81,0x14}
 };
 for(int mode=0;mode<2;++mode){
   windowsCtrlPriority=(mode!=0);
   for(const auto &tc:cases) {
     assert(run(tc.key,false,false)==tc.plain);
     assert(run(tc.key,true,false)==tc.control);
     assert(run(tc.key,true,true)==tc.token);
   }
 }
 puts("PASS v0.94 authentic TK2000 ROM $F043, CTRL+B and other native shortcuts vs BASIC tokens");
}
