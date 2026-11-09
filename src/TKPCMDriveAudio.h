#pragma once
#include <stdint.h>
#include "TKTapePCM97.h"
#include "TKDriveActivity.h" // Share read/write envelope lengths with both drive LEDs.

// Integer-only, single-sample PCM synthesis for FabGL SoundGenerator DAC.
// Real Disk II media flux is not an audible tone. The main audible features
// are the head stepper/stop, resonances, motor and enclosure vibration.
// No heap, SD reads, trigonometric functions or host-clock polling in ISR.
// Exactly one shared audio output: GPIO25 DAC (guest 1-bit speaker + drives).
class TKPCMDriveAudio {
public:
  static const uint32_t sampleHz=16000;
  void reset() {
    _speakerStep=0;_speakerPhase=0;_enabled=0;_paused=0;
    _noise=0xDCA9129Bu;
    for(int d=0;d<2;++d) {
      _inputReads[d]=_inputWrites[d]=_inputSteps[d]=0;
      _seenReads[d]=_seenWrites[d]=_seenSteps[d]=0;
      _motor[d]=_motorSeen[d]=0;_stepsQueued[d]=0;
      _seekDelay[d]=_impactLeft[d]=0;_readLeft[d]=_writeLeft[d]=0;
      _motorStartLeft[d]=0;_noiseFiltered[d]=0;_ringPhase[d]=0;
      _motorPhase[d]=0;_ledPCM[d]=TKDriveActivity::OFF;
    }
  }
  TKPCMDriveAudio() { reset(); }
  void setSpeakerHz(uint16_t hz) {
    // Run on application core, not from FabGL's sample ISR.
    _speakerStep=hz?uint32_t((uint64_t(hz)<<32)/sampleHz):0;
  }
  void arm(const TKDriveSoundState (&st)[2]) {
    // Arm at current controller position: motor ON alone does not invent seeks.
    for(int d=0;d<2;++d) {
      _inputReads[d]=_seenReads[d]=st[d].reads;
      _inputWrites[d]=_seenWrites[d]=st[d].writes;
      _inputSteps[d]=_seenSteps[d]=st[d].steps;
      _motor[d]=_motorSeen[d]=st[d].motor?1:0;
      _stepsQueued[d]=0;_seekDelay[d]=_impactLeft[d]=0;
      _readLeft[d]=_writeLeft[d]=0;
      _motorStartLeft[d]=0;
    }
  }
  void update(const TKDriveSoundState (&st)[2]) {
    for(int d=0;d<2;++d) {
      _inputReads[d]=st[d].reads;
      _inputWrites[d]=st[d].writes;
      _inputSteps[d]=st[d].steps;
      _motor[d]=st[d].motor?1:0;
    }
  }
  void setDriveEnabled(bool enabled) { _enabled=enabled?1:0; }
  void setPaused(bool paused) { _paused=paused?1:0; _tape.setPaused(paused); }
  void setTapeSoundEnabled(bool on) { _tape.setEnabled(on); }
  void updateTapeSignal(bool active,uint8_t level,uint32_t halfCycles) {
    _tape.signal(active,level,halfCycles);
  }
  void stopTapeSignal() { _tape.stop(); }

  bool driveEnabled() const {return _enabled!=0;}
  // Snapshot of the ACTUAL 16kHz PCM envelopes, including queued head steps.
  uint8_t ledPCM(int drive) const {return drive>=0 && drive<2 ? _ledPCM[drive] : uint8_t(TKDriveActivity::OFF);}

  // Called at exactly 16 kHz by FabGL's DAC mixer ISR. All cross-core
  // control fields are naturally aligned 32-bit or 8-bit scalars; an event
  // counter can be skipped between updates but never fabricates a direction.
  int nextSample() {
    const uint32_t speakerStep=_speakerStep;
    int speakerMix=0;
    if(speakerStep) {
      _speakerPhase+=speakerStep;
      speakerMix=(_speakerPhase&0x80000000u)?62:-62;
    }
    const int cassetteMix=_tape.nextSample();
    int driveMix=0;
    if(!_enabled || _paused) {
      _ledPCM[0]=_ledPCM[1]=TKDriveActivity::OFF;
      int mixed=speakerMix+cassetteMix;
      return mixed>127?127:(mixed<-127?-127:mixed);
    }
    _noise ^= _noise << 13;
    _noise ^= _noise >> 17;
    _noise ^= _noise << 5;
    const int randSample=(int8_t)(_noise>>24);
    for(int d=0;d<2;++d) {
      const uint32_t newSteps=_inputSteps[d];
      if(newSteps!=_seenSteps[d]) {
        uint32_t delta=newSteps-_seenSteps[d];
        _seenSteps[d]=newSteps;
        if(delta>12)delta=12;
        const uint32_t queued=(uint32_t)_stepsQueued[d]+delta;
        _stepsQueued[d]=uint8_t(queued>16?16:queued);
      }
      if(_inputReads[d]!=_seenReads[d]) {
        _seenReads[d]=_inputReads[d];
        _readLeft[d]=sampleHz/TKDriveActivity::readWindowDivisor;
      }
      if(_inputWrites[d]!=_seenWrites[d]) {
        _seenWrites[d]=_inputWrites[d];
        _writeLeft[d]=sampleHz/TKDriveActivity::writeWindowDivisor;
      }
      const bool running=_motor[d]!=0;
      if(running && !_motorSeen[d]) _motorStartLeft[d]=sampleHz/8u;
      _motorSeen[d]=running?1:0;
      if(_seekDelay[d]) --_seekDelay[d];
      if(!_seekDelay[d] && !_impactLeft[d] && _stepsQueued[d]) {
        --_stepsQueued[d];
        _impactLeft[d]=sampleHz/65u;  // ~15 ms percussive click
        _seekDelay[d]=sampleHz/82u;  // ~12 ms between steps
      }
      // Damped noise: attack from high-pass, resonance from a short
      // low register square/ring. No changing musical pitch per nibble.
      const int filtered=_noiseFiltered[d]+(randSample-_noiseFiltered[d])/6;
      _noiseFiltered[d]=(int16_t)filtered;
      const int high=randSample-filtered;
      if(_impactLeft[d]) {
        const uint16_t rem=--_impactLeft[d];
        _ringPhase[d]+=uint32_t((uint64_t(710+d*29)<<32)/sampleHz);
        const int ring=(_ringPhase[d]&0x80000000u)?1:-1;
        const int env=int(rem)*34/int(sampleHz/65u);
        driveMix+=(high*env)/140 + ring*(env/3);
      }
      if(running) {
        // Quiet motor bearing flutter (approximately 300 RPM) and low rumble.
        _motorPhase[d]+=uint32_t((uint64_t(78+d*6)<<32)/sampleHz);
        const int hum=(_motorPhase[d]&0x80000000u)?2:-2;
        const int startBump=_motorStartLeft[d] ? 2 : 0;
        driveMix+=hum+startBump+(filtered/48);
        if(_motorStartLeft[d])--_motorStartLeft[d];
        // Reading and writing should NOT produce long pitches; sound is
        // dominated by motor/head, with quieter paper-like texture.
        if(_writeLeft[d]) driveMix+=high/28;
        else if(_readLeft[d]) driveMix+=high/63;
      }
      // LED is sampled by the VGA task at 60Hz; use the real PCM
      // envelope rather than an independent millis() approximation.
      _ledPCM[d]=running && _writeLeft[d] ? TKDriveActivity::WRITE :
        ((running && _readLeft[d]) || _impactLeft[d] || _stepsQueued[d] || _seekDelay[d])
          ? TKDriveActivity::READ : TKDriveActivity::OFF;
      if(_readLeft[d])--_readLeft[d];
      if(_writeLeft[d])--_writeLeft[d];
    }
    // ~+1.94 dB on drive mechanics only, preserving every PCM envelope,
    // noise texture and head-step pitch of the physically validated v0.38.
    // Keep the guest C030 speaker's ORIGINAL mixing path bit-for-bit when
    // it is active, so drive gain cannot create additional clipped samples
    // during BASIC music or game sound. The no-speaker path is bounded to
    // FabGL's 8-bit DAC range; no global SoundGenerator gain was touched.
    if (!speakerStep) driveMix=(driveMix*5)/4;
    int mix=speakerMix+driveMix+cassetteMix;
    if(mix>127)mix=127;
    if(mix<-127)mix=-127;
    return mix;
  }
private:
  TKTapePCM97 _tape;
  volatile uint32_t _speakerStep=0;
  uint32_t _speakerPhase=0;
  volatile uint8_t _enabled=0,_paused=0;
  uint32_t _noise=0xDCA9129Bu;
  volatile uint32_t _inputReads[2]={},_inputWrites[2]={},_inputSteps[2]={};
  uint32_t _seenReads[2]={},_seenWrites[2]={},_seenSteps[2]={};
  volatile uint8_t _motor[2]={};
  volatile uint8_t _ledPCM[2]={};
  uint8_t _motorSeen[2]={},_stepsQueued[2]={};
  uint16_t _seekDelay[2]={},_impactLeft[2]={},_readLeft[2]={},_writeLeft[2]={};
  uint16_t _motorStartLeft[2]={};
  int16_t _noiseFiltered[2]={};
  uint32_t _ringPhase[2]={},_motorPhase[2]={};
};
