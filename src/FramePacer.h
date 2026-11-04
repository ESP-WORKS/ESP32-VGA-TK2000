#pragma once
#include <stdint.h>

// 60 Hz wall-clock scheduler for the video service. Do not count skipped
// deadlines as FPS. Handles micros() wrapping (unsigned 32-bit clock).
class TKFramePacer {
public:
  void reset() { armed_=false; phase_=0; nextUs_=0; }
  bool due(uint32_t now) {
    if (!armed_) {nextUs_=now;armed_=true;}
    if ((int32_t)(now-nextUs_) < 0) return false;
    // 1000000 / 60 = 16666 + 2/3 microseconds.
    nextUs_ += 16666u;
    phase_ += 2u;
    if (phase_ >= 3u) {++nextUs_;phase_-=3u;}
    // If a job ran too late, wait for the next service. Do not fake frames.
    if ((int32_t)(now-nextUs_) >= 0) {
      nextUs_ = now + 16667u;
      phase_=0;
    }
    return true;
  }
private:
  uint32_t nextUs_=0;
  uint8_t phase_=0;
  bool armed_=false;
};
