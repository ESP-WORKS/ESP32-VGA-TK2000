/* SPDX-License-Identifier: MIT
 * Adapted from MC-1000 v0.12 src/fps_eps_meter.h.
 * TK-2000: 6502 target 1,023,000 cycles/s, 60 emulated frames/s.
 * One video-service event per sample, independent of changed VRAM pixels.
 */
#pragma once
#include <stdint.h>

class TKFPS_EPS_Meter {
public:
  struct Result { uint16_t fps; uint16_t eps; };
  static constexpr uint64_t kSampleWindowUs = 2000000ull;
  static constexpr uint32_t kGuestHz = 1023000u;
  static constexpr uint32_t kFramesPerGuestSecond = 60u;

  void Reset(uint64_t nowUs, uint64_t totalCycles) {
    beginUs_ = nowUs;
    beginCycles_ = totalCycles;
    frames_ = 0;
  }

  // Call only after each completed periodic video service; no per-pixel counts.
  // A backward clock or guest CPU reset discards a partial sampling window.
  bool Sample(uint64_t nowUs, uint64_t totalCycles, Result *out) {
    if (!out) return false;
    if (nowUs < beginUs_ || totalCycles < beginCycles_) {
      Reset(nowUs, totalCycles);
      return false;
    }
    if (frames_ != UINT32_MAX) ++frames_;
    const uint64_t elapsedUs = nowUs - beginUs_;
    if (elapsedUs < kSampleWindowUs) return false;
    const uint64_t elapsedCycles = totalCycles - beginCycles_;
    // Round half up exactly once, before clamping to the 60Hz guest ceiling.
    const uint64_t fps = (uint64_t(frames_) * 1000000ull + elapsedUs / 2) / elapsedUs;
    const uint64_t denominator = uint64_t(kGuestHz) * elapsedUs;
    const uint64_t eps =
        (elapsedCycles * kFramesPerGuestSecond * 1000000ull + denominator / 2) / denominator;
    out->fps = uint16_t(fps > 60 ? 60 : fps);
    out->eps = uint16_t(eps > 60 ? 60 : eps);
    Reset(nowUs, totalCycles);
    return true;
  }
private:
  uint64_t beginUs_ = 0;
  uint64_t beginCycles_ = 0;
  uint32_t frames_ = 0;
};
