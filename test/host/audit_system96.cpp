// v0.96 full-system regression: the REAL TKMachine (CPU, MMIO, keyboard,
// gameport) runs the genuine TK2000 ROM supplied out-of-band. No ROM bytes
// are embedded or distributed. Verifies:
//  1. CONTROL+B then SHIFT+letter enters genuine TK semigraphic codes.
//  2. JSR $FB1E keeps Y (genuine SEC;RTS tail of GETCHAR), joystick ON/OFF.
//  3. Released analog axis rests at 128, inside Apple-style $80 dead zones.
#include "BuildConfig.h"
#include "Machine.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static TKMachine M;
static const uint32_t kFrame=17030u;
static void frames(int n){
  for(int i=0;i<n;++i){
    if(M.waitingForBootReset()) assert(M.reset());
    if(M.tape.request()!=CassetteCT2::None) M.tape.clearRequest();
    M.run(kFrame);
  }
}
static void tap(char c){M.key(c,true);frames(4);M.key(c,false);frames(4);}
// Run a tiny guest routine at $0300 that ends in a JMP * at $03F0.
static void runGuest(const uint8_t *code,size_t n){
  memcpy(M.ram+0x300,code,n);
  M.ram[0x3F0]=0x4C;M.ram[0x3F1]=0xF0;M.ram[0x3F2]=0x03; // JMP $03F0
  M.cpu.PC=0x0300; M.cpu.S=0xFD;
  for(int i=0;i<200000 && M.cpu.PC!=0x03F0;++i) M.cpu.step();
  assert(M.cpu.PC==0x03F0);
}
int main(int argc,char**argv){
  if(argc!=2){fprintf(stderr,"usage: %s TK2000.rom\n",argv[0]);return 2;}
  FILE*f=fopen(argv[1],"rb"); if(!f){perror(argv[1]);return 2;}
  uint8_t*rom=(uint8_t*)malloc(16384);
  if(fread(rom,1,16384,f)!=16384) return 3; fclose(f);
  uint8_t*disk=(uint8_t*)calloc(256,1);
  assert(M.installSystemROM(rom,16384) && M.installDiskROM(disk,256) && M.init());
  frames(200); // power-on title, RESET into BASIC (no D1)

  // 1. CONTROL+B, then SHIFT+A..D on the BASIC input line.
  M.setCtrl(true); frames(3); tap('B'); M.setCtrl(false); frames(10);
  assert(M.ram[0x49]==1);            // ROM $F7DC toggled graphic mode
  M.setShift(true); frames(3);
  tap('A'); tap('B'); tap('C'); tap('D');
  M.setShift(false); frames(10);
  // TABASC SHIFT codes: A=$2C B=$22 C=$2A D=$28, after the $82 marker.
  const uint8_t want[]={0x82,0x2C,0x22,0x2A,0x28};
  assert(memcmp(M.ram+0x200,want,sizeof want)==0);
  // Without CONTROL+B the same SHIFT+letter is rejected by the ROM.
  M.setCtrl(true); frames(3); tap('B'); M.setCtrl(false); frames(10);
  assert(M.ram[0x49]==0);

  // 2. Apple-style "LDX #axis; JSR $FB1E" must preserve Y on a TK2000.
  const uint8_t probe[]={0xA0,0x5A, 0xA2,0x00, 0x20,0x1E,0xFB, 0x8C,0xE0,0x03,
                         0xA0,0xA5, 0xA2,0x01, 0x20,0x1E,0xFB, 0x8C,0xE1,0x03,
                         0x4C,0xF0,0x03};
  // JOYSTICK OFF: genuine ROM, Y preserved.
  M.setJoystickEnabled(false); M.setJoystickMask(0);
  M.ram[0x3E0]=M.ram[0x3E1]=0; runGuest(probe,sizeof probe);
  assert(M.ram[0x3E0]==0x5A && M.ram[0x3E1]==0xA5);
#if TK2000_UNIVERSAL_PREAD
  // JOYSTICK ON: Apple-style joystick attached; released axes = 128.
  M.setJoystickEnabled(true); M.setJoystickMask(0);
  runGuest(probe,sizeof probe);
  M.run(1); runGuest(probe,sizeof probe);
  assert(M.ram[0x3E0]==TKPaddleJoystick::kCenter && M.ram[0x3E1]==TKPaddleJoystick::kCenter);
  M.setJoystickMask(0x01); M.run(1); runGuest(probe,sizeof probe); // UP
  assert(M.ram[0x3E1]==0);
  M.setJoystickMask(0);
#endif

  // 3. Apple PREAD loop on the FPGA gameport: released axis counts >= $80.
  // LDA $C070; LDY #0; L: LDA $C064; BPL done; INY; BNE L; done: STY $3E2
  const uint8_t pread[]={0xAD,0x70,0xC0, 0xA0,0x00, 0xAD,0x64,0xC0, 0x10,0x04,
                         0xC8, 0xD0,0xF8, 0xEA, 0x8C,0xE2,0x03, 0x4C,0xF0,0x03};
  for(int on=0;on<2;++on){
    M.setJoystickEnabled(on!=0); M.setJoystickMask(0);
    runGuest(pread,sizeof pread);
    assert(M.ram[0x3E2]>=0x80 && M.ram[0x3E2]<=0x8F);
  }
  M.setJoystickEnabled(true); M.setJoystickMask(0x04); // LEFT held
  runGuest(pread,sizeof pread); assert(M.ram[0x3E2]<0x10);
  M.setJoystickMask(0);                                  // released
  runGuest(pread,sizeof pread); assert(M.ram[0x3E2]>=0x80 && M.ram[0x3E2]<=0x8F);
  // 4. Idle KBIN: no key, no tape -> bit 7 = 0 (MiSTer cas_i=0). TK
  //    software tests "key present" with CMP #$80 (TK Choplifter driver).
  M.setJoystickEnabled(false); M.setJoystickMask(0);
  const uint8_t idle[]={0xA9,0xFF,0x8D,0x00,0xC0, 0xAD,0x10,0xC0, 0x8D,0xE3,0x03,
                        0x4C,0xF0,0x03};
  runGuest(idle,sizeof idle); assert(M.ram[0x3E3]==0x00);
  // 5. One host key = one TK contact: joystick UP must reach the ROM
  //    scanner as the UP key ($F0), not as an earlier-row bridge letter.
  //    JSR $F043 (SCAN1); BCC none; JSR $FB05; STA $3E4
  const uint8_t scan[]={0xA9,0x00,0x8D,0xE4,0x03, 0x20,0x43,0xF0, 0x90,0x06,
                        0x20,0x05,0xFB, 0x8D,0xE4,0x03, 0x4C,0xF0,0x03};
  M.setJoystickEnabled(true);
  const struct {uint8_t mask,ascii;} js[]={{0x01,0xF0},{0x02,0xF1},{0x04,0x88},
                                         {0x08,0x95},{0x10,0xAE},{0x20,0xCC}};
  for(const auto &j: js){
    M.setJoystickMask(j.mask); runGuest(scan,sizeof scan);
    assert(M.ram[0x3E4]==j.ascii);
  }
  M.setJoystickMask(0); runGuest(scan,sizeof scan); assert(M.ram[0x3E4]==0);
  puts("PASS v0.96 idle KBIN bit7=0; joystick UP/DOWN/LEFT/RIGHT/FIRE1/FIRE2 = $F0/$F1/$88/$95/$AE/$CC via ROM SCAN1");
  puts("PASS v0.96 real TKMachine + TK2000 ROM: CONTROL+B semigraphics, JSR $FB1E keeps Y when OFF, PREAD when ON, axis rest 128");
  return 0;
}
