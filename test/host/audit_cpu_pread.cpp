#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "BuildConfig.h"
#include "CPU6502.h"
#include "Bus.h"

static uint8_t memory[65536];
static uint8_t rom[16384];
uint8_t TKBus::read(uint16_t a){return a>=0xC000?rom[a-0xC000]:memory[a];}
void TKBus::write(uint16_t a,uint8_t v){memory[a]=v;}
static void run(uint8_t x,uint8_t expectedY,bool selected=true,bool active=true) {
  memset(memory,0,sizeof(memory));
  memory[0x4000]=0x20;memory[0x4001]=0x1E;memory[0x4002]=0xFB;
  memory[0x4003]=0xEA;
  memory[0xFB1E]=0x38;memory[0xFB1F]=0x60;
  bool overlay=selected;
  TKBus bus;
  CPU6502 cpu;
  cpu.attach(&bus);
  cpu.attachFastMemory(memory,rom,&overlay,nullptr);
  cpu.PC=0x4000;cpu.S=0xFD;cpu.X=x;cpu.Y=77;
  cpu.universalPreadX=13;cpu.universalPreadY=221;cpu.universalPreadActive=active;
  assert(cpu.step()==6); // authentic JSR pushes guest return address
  assert(cpu.PC==0xFB1E && cpu.S==0xFB);
  assert(cpu.step()==2); // SEC; universal PREAD updates Y only
  assert(cpu.Y==expectedY);
  assert((cpu.P&1u)!=0); // preserve original ROM carry semantics
  assert(cpu.step()==6); // RTS
  assert(cpu.PC==0x4003 && cpu.S==0xFD);
}
int main(){
  rom[0x3B1E]=0x38;rom[0x3B1F]=0x60;
  // JOYSTICK OFF (any build): genuine TK ROM, Y preserved for all axes.
  run(0,77,true,false); run(1,77,true,false); run(2,77,true,false);
#if TK2000_UNIVERSAL_PREAD
  // JOYSTICK ON: attached Apple-style joystick answers PREAD axes 0/1.
  run(0,13); run(1,221); run(2,77);
  run(0,77,false); // RAM overlay must not be overridden
  puts("PASS v0.96 JSR $FB1E: OFF keeps Y (ROM SEC/RTS); ON = PREAD axes; RAM overlay guarded");
#else
  run(0,77); run(1,77);
  puts("PASS v0.96 pure-ROM build: JSR $FB1E never alters Y");
#endif
}
