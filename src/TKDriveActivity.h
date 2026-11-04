#pragma once
#include <stdint.h>
#include "DriveSound.h"

// Common timing for the Disk II PCM read/write textures and the VGA LEDs.
// All decisions are derived from the same per-drive Disk II event counters.
// No additional disk I/O, timer polling or ESP32 resources are required.
class TKDriveActivity {
public:
  enum : uint8_t { OFF=0, READ=1, WRITE=2 };
  // Keep the v0.34 sound envelopes byte-for-byte: 16kHz/25 samples for
  // reads and 16kHz/17 for writes. LED windows are rounded UP to the next
  // millisecond, as the GUI uses millis() rather than audio sample ticks.
  static const uint32_t readWindowDivisor=25u;
  static const uint32_t writeWindowDivisor=17u;
  static const uint32_t readMs=(1000u+readWindowDivisor-1u)/readWindowDivisor;
  static const uint32_t writeMs=(1000u+writeWindowDivisor-1u)/writeWindowDivisor;
  static const uint32_t impactMs=15u;
  static const uint32_t betweenStepsMs=12u;

  void arm(const TKDriveSoundState (&st)[2]) {
    for(int d=0;d<2;++d) {
      _reads[d]=st[d].reads;
      _writes[d]=st[d].writes;
      _steps[d]=st[d].steps;
      _motor[d]=st[d].motor;
      _readActive[d]=_writeActive[d]=_seekActive[d]=false;
      _readAt[d]=_writeAt[d]=_seekAt[d]=0;
      _seekLength[d]=0;
    }
    _armed=true;
  }

  // Called alongside TKPCMDriveAudio::update(), at the existing 2ms control
  // cadence, whether mechanical sound is ON or OFF. New events retrigger the
  // same envelopes that the PCM mixer starts when it sees updated counters.
  void update(const TKDriveSoundState (&st)[2], uint32_t nowMs) {
    if(!_armed) { arm(st); return; }
    for(int d=0;d<2;++d) {
      _motor[d]=st[d].motor;
      if(st[d].reads!=_reads[d]) {
        _reads[d]=st[d].reads;
        _readAt[d]=nowMs;
        _readActive[d]=true;
      }
      if(st[d].writes!=_writes[d]) {
        _writes[d]=st[d].writes;
        _writeAt[d]=nowMs;
        _writeActive[d]=true;
      }
      if(st[d].steps!=_steps[d]) {
        uint32_t steps=st[d].steps-_steps[d];
        _steps[d]=st[d].steps;
        if(steps>12u) steps=12u; // Same per-update clamp as PCM mixer.
        _seekAt[d]=nowMs;
        _seekLength[d]=(steps-1u)*betweenStepsMs+impactMs;
        _seekActive[d]=true;
      }
    }
  }

  uint8_t led(int d, uint32_t nowMs) const {
    if(d<0 || d>1 || !_armed) return OFF;
    // WRITE is dominant both visually and in the PCM read/write texture.
    if(_motor[d] && _writeActive[d] && (uint32_t)(nowMs-_writeAt[d])<writeMs)
      return WRITE;
    if((_motor[d] && _readActive[d] && (uint32_t)(nowMs-_readAt[d])<readMs) ||
       (_seekActive[d] && (uint32_t)(nowMs-_seekAt[d])<_seekLength[d]))
      return READ; // Head clicks also illuminate activity (green).
    return OFF; // Motor rumble alone never falsely signals a sector read.
  }

private:
  bool _armed=false;
  bool _motor[2]={};
  bool _readActive[2]={},_writeActive[2]={},_seekActive[2]={};
  uint32_t _reads[2]={},_writes[2]={},_steps[2]={};
  uint32_t _readAt[2]={},_writeAt[2]={},_seekAt[2]={};
  uint32_t _seekLength[2]={};
};
