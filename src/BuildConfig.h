#pragma once

#define TK2000_FW_VERSION "0.97"

// v0.90: universal joystick / PREAD compatibility, with legacy patches OFF.
// 0 (default): NO game-dependent input workarounds (Montezuma A/Z and SPACE,
//    Moon Patrol and Missiles Defense PREAD, Missiles analog sight or SPACE).
// 1: restore the exact v0.88 game-specific joystick behavior, reversibly.
// This flag does NOT affect tape identification, Moon Patrol K7 Turbo,
// general PS/2 keyboard handling, VGA, disk, or CT2 multi-track writing.
// PlatformIO sets this via -DTK2000_GAME_JOYSTICK_COMPAT=0 in platformio.ini.
#ifndef TK2000_GAME_JOYSTICK_COMPAT
#define TK2000_GAME_JOYSTICK_COMPAT 0
#endif
#if (TK2000_GAME_JOYSTICK_COMPAT != 0) && (TK2000_GAME_JOYSTICK_COMPAT != 1)
#error "TK2000_GAME_JOYSTICK_COMPAT must be 0 or 1"
#endif

// General compatibility between the TK joystick and keyboard controls.
// A virtual direction continues to close its original joystick contact;
// additionally UP/DOWN may close A/Z, and FIRE1 the keyboard SPACE contact.
// This is the same circuit-level bridge for ALL tapes and all keyboard
// sources. It never inspects tape names, CRCs, RAM or the guest PC.
// 1 = universal bridge when JOYSTICK ON (default); 0 = strict joystick rows.
// The historical per-game compatibility build keeps its original mappings.
// v0.96: OFF. The bridge closed a SECOND contact (A, Z or SPACE) together
// with each joystick contact. The TK ROM keyboard scanner returns only the
// first closed contact in row order (row0..row7), so A (row1) hid UP (row6)
// and SPACE (row2) hid FIRE '.' (row7). MiSTer/Windows 0.3: joystick uses
// only the arrow contacts and '.'/'L'. 1 = A/B diagnosis only.
// v0.96: Montezuma-only A/Z on cursor UP/DOWN (explicit user request).
#ifndef TK2000_MONTEZUMA_AZ
#define TK2000_MONTEZUMA_AZ 1
#endif

#ifndef TK2000_JOYSTICK_KEYBOARD_BRIDGE
#define TK2000_JOYSTICK_KEYBOARD_BRIDGE 0
#endif
#if (TK2000_JOYSTICK_KEYBOARD_BRIDGE != 0) && (TK2000_JOYSTICK_KEYBOARD_BRIDGE != 1)
#error "TK2000_JOYSTICK_KEYBOARD_BRIDGE must be 0 or 1"
#endif

// While the FabGL PS/2 queue contains both MAKE and BREAK, the 6502 must
// execute between these transitions or the guest never sees the keypress.
// This is device-wide and independent of which program is loaded.
#ifndef TK2000_INPUT_INTERLEAVE
#define TK2000_INPUT_INTERLEAVE 1
#endif

// Apple-II PREAD on JSR $FB1E, ONLY while JOYSTICK is ON (v0.96 rev 3).
// In the TK2000 ROM, $FB1E is the genuine SEC;RTS tail of GETCHAR, so with
// JOYSTICK OFF the ROM runs untouched and Y is preserved, like a real TK.
// With JOYSTICK ON the emulator models an attached Apple-compatible analog
// joystick (FPGA TK2000 II gameport): JSR $FB1E returns the axis in Y.
// Apple-derived programs (Moon Patrol J mode, Missiles Defense P mode) need
// this; TK-native code never calls $FB1E. 0 = never (pure ROM).
#ifndef TK2000_UNIVERSAL_PREAD
#define TK2000_UNIVERSAL_PREAD 1
#endif

// Global analog joystick mode, independent of game identity:
// 0=CENTERING (validated standard: returns to center on KEYUP),
// 1=STANDARD (Windows v0.3: holds last position on KEYUP).
// Legacy exact-game mode may still override when compatibility=1.
#ifndef TK2000_JOYSTICK_STANDARD_MODE
#define TK2000_JOYSTICK_STANDARD_MODE 0
#endif

// Atomic aligned word stores of fully prepared FabGL RGB/sync scanlines.
// Does not cure analog cable/power noise; avoids 4-pixel partial updates.
#ifndef TK2000_VGA_WORD_STORES
#define TK2000_VGA_WORD_STORES 1
#endif

// Optional PS/2 diagnostic for CONTROL and CTRL+SHIFT combinations.
// 0: no extra PS/2 queue processing or serial output (normal gameplay).
// 1: show raw FabGL key event, PS/2 scancodes, side and host modifier state
//    only while CTRL is held or CTRL itself is changing. Serial 115200.
#ifndef TK2000_PS2_CTRL_TRACE
#define TK2000_PS2_CTRL_TRACE 0
#endif


// Optional on-device Moon Patrol serial profiler (disabled for normal use).
// Enable only for advanced debugging; there is NO Moon Patrol speed OSD.
// Serial 115200: real 8x throughput, interpreted CPU instructions, ROM fast
// skip cycles, decoded CT2 half-waves, CPU/VGA time and scheduler saturation.
// Set to 0 for the normal production build.
#ifndef TK2000_K7_MOON_PROFILE
#define TK2000_K7_MOON_PROFILE 0
#endif

// Normal builds have no Moon Patrol rate OSD or serial diagnostics.
// Enable only to record advanced [K7 TURBO] logs.
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
