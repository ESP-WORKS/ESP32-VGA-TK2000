#pragma once
#include <stdint.h>
class TKBus;
class TKVideoDirty;
class TKSaturn128K;
class CPU6502 {
public:
  uint8_t A=0,X=0,Y=0,P=0x24,S=0xFD; uint16_t PC=0; uint64_t cycles=0; TKBus* bus=nullptr;
  void attach(TKBus* b){bus=b;} void reset(); int step();
  // Direct memory windows avoid 6502 -> Bus -> Machine dispatch for ordinary
  // RAM and system-ROM accesses. MMIO and the independent disk slot still
  // pass through the original bus, preserving all side effects.
  void attachFastMemory(uint8_t *ram, const uint8_t *systemROM,
                        const bool *romOverlay, TKVideoDirty *dirty) {
    fastRAM=ram; fastROM=systemROM; fastROMSelected=romOverlay; fastVideoDirty=dirty;
  }
  void attachSaturn(TKSaturn128K *card) { fastSaturn=card; }
  bool irq_pending=false,nmi_pending=false; bool pageCrossed=false;
  bool tkdosLegacyBootAlias=false;
  // Exact Moon Patrol CT2 compatibility: Apple PREAD ($FB1E) returns a
  // coordinate in Y for axis X=0/1. Enabled by TKMachine only with F4 ON.
  uint8_t universalPreadX=128, universalPreadY=128;
  // v0.96: Apple-style PREAD answer only while a joystick is attached
  // (F4 ON). With JOYSTICK OFF, JSR $FB1E is the genuine ROM (Y kept).
  bool universalPreadActive=false;
  bool moonPatrolPreadCompat=false;
  uint8_t moonPatrolPreadX=127, moonPatrolPreadY=127;
  // Exact, isolated Missiles Defense CT2 PREAD shim. $41C3 is a real JSR
  // $FB1E in this image; the stock TK ROM stub does not return analog Y.
  bool missilesDefensePreadCompat=false;
  uint8_t missilesDefensePreadX=127, missilesDefensePreadY=127;
private:
  TKSaturn128K *fastSaturn=nullptr;
  uint8_t *fastRAM=nullptr;
  const uint8_t *fastROM=nullptr;
  const bool *fastROMSelected=nullptr;
  TKVideoDirty *fastVideoDirty=nullptr;
  uint8_t rd(uint16_t a); void wr(uint16_t a,uint8_t v); uint16_t rw(uint16_t a);
  void setZN(uint8_t v); uint8_t fetch(); uint16_t addr(int mode); void service(uint16_t vec,bool brk=false);
};
