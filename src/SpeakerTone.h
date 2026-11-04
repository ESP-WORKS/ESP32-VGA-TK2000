#pragma once
#include <stdint.h>

// The TK-2000 speaker is a 1-bit flip-flop toggled by $C030-$C03F.
// The reference tk_som.cpp dates its edges in 6502 cycles and reconstructs
// a wave.  On ESP32 the LEDC peripheral is used for stable sustained tones:
// frequency is inferred from consistent successive edge intervals, never
// from the host's variable loop() scheduling.  Individual clicks and noise
// waveforms remain an approximation of the analog speaker.
class TKSpeakerTone {
public:
  static constexpr uint32_t kCPUHz = 1023000u;
  void reset() { lastCycle=0; period=0; streak=0; valid=false; hz=0; }
  void edge(uint64_t cycle) {
    // Several MMIO accesses inside a single instruction share the same
    // step-level cycle stamp. They toggle the 1-bit latch but a zero-length
    // interval must not erase an already stable note's period estimate.
    if(valid && cycle==lastCycle) return;
    if(valid && cycle<lastCycle) reset(); // defensive timestamp rewind
    if(valid) {
      const uint64_t dt=cycle-lastCycle;
      if(dt>=60u && dt<=12000u) {
        const uint32_t d=(uint32_t)dt;
        const uint32_t tolerance=period/4u+2u;
        if(period && d>=period-tolerance && d<=period+tolerance) {
          period=(period*3u+d+2u)/4u;
          if(streak<255) ++streak;
        } else {period=d;streak=1;}
        if(streak>=3) {
          const uint32_t f=kCPUHz/(2u*period);
          hz=(uint16_t)(f>6000u?6000u:f);
        } else hz=0;
      } else {period=0;streak=0;hz=0;}
    }
    lastCycle=cycle;valid=true;
  }
  uint16_t frequency(uint64_t cycle) {
    if(!valid || cycle<lastCycle) {reset();return 0;}
    // Muting after inactivity prevents a held square wave / DC output.
    // A longer half-period gets proportionally more release time.
    const uint64_t timeout=(period*4u>12276u)?period*4u:12276u;
    if(cycle-lastCycle>timeout){hz=0;period=0;streak=0;valid=false;}
    return hz;
  }
  bool active() const {return valid;}
  uint64_t lastEdgeCycle() const {return lastCycle;}
private:
  uint64_t lastCycle=0;
  uint32_t period=0;
  uint8_t streak=0;
  uint16_t hz=0;
  bool valid=false;
};
