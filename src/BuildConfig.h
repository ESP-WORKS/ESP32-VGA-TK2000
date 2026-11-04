#pragma once

#define TK2000_FW_VERSION "0.86"

// Optional PS/2 diagnostic for CTRL+A..Z on real TTGO VGA32.
// 0: no extra PS/2 queue processing or serial output (normal gameplay).
// 1: show raw FabGL key event, PS/2 scancodes, side and host modifier state
//    only while CTRL is held or CTRL itself is changing. Serial 115200.
#ifndef TK2000_PS2_CTRL_TRACE
#define TK2000_PS2_CTRL_TRACE 0
#endif


// Optional on-device Moon Patrol serial profiler (disabled for normal use).
// Enable only for advanced debugging; the normal build uses an on-screen speed display.
// Serial 115200: real 8x throughput, interpreted CPU instructions, ROM fast
// skip cycles, decoded CT2 half-waves, CPU/VGA time and scheduler saturation.
// Set to 0 for the production build after the source of slowdown is found.
#ifndef TK2000_K7_MOON_PROFILE
#define TK2000_K7_MOON_PROFILE 0
#endif

// The normal build shows Moon Patrol K7 speed on VGA; no USB serial
// monitor is needed. Enable to restore the v0.84 [K7 TURBO] log.
#ifndef TK2000_K7_SERIAL_TRACE
#define TK2000_K7_SERIAL_TRACE TK2000_K7_MOON_PROFILE
#endif

// TTGO VGA32 speaker DAC GPIO25 (FabGL SoundGenerator PCM). Override for custom boards.
#ifndef TK2000_AUDIO_DAC_PIN
#define TK2000_AUDIO_DAC_PIN 25
#endif

// v0.35: fast 6502 memory windows. 1 = bypass 6502->TKBus->TKMachine for
// ordinary RAM and ROM; MMIO/Disk II, banking and HGR dirty tracking remain
// exact. Set to 0 for diagnostic A/B builds with the original bus path.
#ifndef TK2000_CPU_FAST_MEMORY
#define TK2000_CPU_FAST_MEMORY 1
#endif

// MSX v38.9 uses O3 on selected hot paths while the Arduino ESP32 core
// builds the rest for size (-Os). Keep this scoped to the 6502 interpreter;
// disable with TK2000_CPU_HOT_O3=0 for device A/B diagnosis.
#ifndef TK2000_CPU_HOT_O3
#define TK2000_CPU_HOT_O3 1
#endif

// Native acceleration of the AUTHENTIC TK2000 cassette ROM loops.
// 1: verified ROM only; 0: compare against the original interpreter.
// The 8x guest-clock factor is configured in TKCassetteTurbo.h.
#ifndef TK2000_K7_FAST_ROM
#define TK2000_K7_FAST_ROM 1
#endif
