#pragma once
#include <stdint.h>

// Separate cassette monitor channel. Java Tape.java toggles the Apple-like
// C030 speaker on every CASIN half-wave; on ESP32 an independent PCM voice
// avoids modifying the *guest* C030 flip-flop, music or CPU/tape timing.
// The selected half-period is the actual CT2 half-wave (or the previous
// $C020 recording-edge delta), never a background beeper/timer.
class TKTapePCM97 {
public:
  static const uint32_t kGuestHz=1023000u;
  static const uint32_t kSampleHz=16000u;
  static const int kAmplitude=22;
  void setEnabled(bool on) { _enabled=on?1:0; }
  bool enabled() const { return _enabled!=0; }
  void setPaused(bool paused) { _paused=paused?1:0; }
  void stop() { _active=0; _step=0; _lastHalfCycles=0; }
  // 8-bit level is the *real* cassette input (or the recording-edge parity).
  // Snapshot fields are only single-word volatile values shared with DAC ISR.
  void signal(bool active,uint8_t level,uint32_t halfCycles) {
    if(!active || halfCycles<30u || halfCycles>20000u) { stop(); return; }
    const uint8_t bit=(level&0x80u)?1u:0u;
    if(halfCycles!=_lastHalfCycles) {
      _lastHalfCycles=halfCycles;
      // CT2 waveforms use just a handful of half-periods. Avoid costly
      // 64-bit division on every 250/500-cycle transition during 8x K7.
      switch(halfCycles) {
        case 250u: _step=549218942u; break;
        case 500u: _step=274609471u; break;
        case 652u: _step=210590085u; break;
        case 679u: _step=202216105u; break;
        case 464u: _step=295915378u; break;
        case 195u: _step=704126849u; break;
        case 199u: _step=689973546u; break;
        default: {
          const uint64_t den=(uint64_t)halfCycles*2u*kSampleHz;
          const uint64_t rate=(uint64_t(kGuestHz)<<32)/den;
          // Nyquist guard for atypically short recorded pulses.
          _step=(uint32_t)(rate>0x80000000ull?0x80000000ull:rate);
        }
      }
    }
    _level=bit;
    _active=1;
  }
  int nextSample() { // DAC ISR, no divisions, malloc or guest memory access.
    if(!_enabled || !_active || _paused || !_step) {
      _lastLevel=2;
      _phase=0;
      return 0;
    }
    const uint8_t level=_level;
    if(level!=_lastLevel) {
      _phase=level?0x80000000u:0u; // lock phase to actual CASIN/CASOUT edge
      _lastLevel=level;
    } else {
      _phase+=_step;
    }
    return (_phase&0x80000000u)?kAmplitude:-kAmplitude;
  }
private:
  volatile uint32_t _step=0;
  volatile uint8_t _level=0,_active=0,_enabled=1,_paused=0;
  uint32_t _lastHalfCycles=0; // guest-core only
  uint32_t _phase=0;          // DAC ISR only
  uint8_t _lastLevel=2;       // DAC ISR only
};
