#!/bin/sh
set -eu
cd "$(dirname "$0")"
# Compile in a fresh temporary directory; do not put host binaries in ZIP.
workdir=$(mktemp -d)
trap 'rm -rf "$workdir"' EXIT HUP INT TERM
CXX=${CXX:-g++}
BASE="-std=c++11 -O2 -Wall -Wextra -Werror -I. -I../../src"
# clang does not support GCC optimize("O3") on CPU6502::step.
case "$CXX" in *clang*) BASE="$BASE -DTK2000_CPU_HOT_O3=0";; esac
# v0.88 compatibility regression remains available when flag is restored.
"$CXX" $BASE -DTK2000_GAME_JOYSTICK_COMPAT=1 audit_input_video.cpp ../../src/Keyboard.cpp -o "$workdir/audit_v088"
"$workdir/audit_v088"
# Independent actual KeyboardTK + PaddleJoystick tests in both configurations.
for mode in 0 1; do
  "$CXX" $BASE -DTK2000_JOYSTICK_KEYBOARD_BRIDGE=0 -DTK2000_GAME_JOYSTICK_COMPAT=$mode audit_game_joystick_toggle.cpp ../../src/Keyboard.cpp -o "$workdir/joystick_$mode"
  "$workdir/joystick_$mode"
done

# Universal compatibility and DMA scanline word writes (no title/CRC checks).
for mode in 0 1; do
  "$CXX" $BASE -DTK2000_JOYSTICK_KEYBOARD_BRIDGE=0 -DTK2000_GAME_JOYSTICK_COMPAT=$mode -DTK2000_VGA_WORD_STORES=1 audit_universal_controls.cpp ../../src/Keyboard.cpp -o "$workdir/universal_$mode"
  "$workdir/universal_$mode"
done

# Run a real 6502 JSR to the TK PREAD stub (no ROM image distributed).
"$CXX" $BASE -DTK2000_GAME_JOYSTICK_COMPAT=0 audit_cpu_pread.cpp ../../src/CPU6502.cpp -o "$workdir/pread_cpu"
"$workdir/pread_cpu"
# Optional A/B synthetic PREAD remains compilable and guarded.
"$CXX" $BASE -DTK2000_GAME_JOYSTICK_COMPAT=0 -DTK2000_UNIVERSAL_PREAD=0 audit_cpu_pread.cpp ../../src/CPU6502.cpp -o "$workdir/pread_cpu_ab"
"$workdir/pread_cpu_ab"

# Additional global STANDARD-mode build, with game profiles still OFF.
"$CXX" $BASE -DTK2000_JOYSTICK_KEYBOARD_BRIDGE=0 -DTK2000_GAME_JOYSTICK_COMPAT=0 -DTK2000_JOYSTICK_STANDARD_MODE=1 audit_universal_controls.cpp ../../src/Keyboard.cpp -o "$workdir/universal_standard"
"$workdir/universal_standard"

# v0.91 keyboard semantic regression, physical left/right CTRL/SHIFT, and
# 26 matrix contacts against the Windows v0.3 table. No ROM required.
for mode in 0 1; do
  "$CXX" $BASE -DTK2000_GAME_JOYSTICK_COMPAT=$mode audit_control_semantics.cpp ../../src/Keyboard.cpp -o "$workdir/control_$mode"
  "$workdir/control_$mode"
done

# Optional authentic monitor-ROM regression: the ROM is supplied out-of-band,
# never copied into the source distribution or embedded in the test binary.
"$CXX" $BASE audit_montezuma96.cpp ../../src/Keyboard.cpp -o "$workdir/montezuma96"
"$workdir/montezuma96"
if [ -n "${TK2000_TEST_ROM:-}" ]; then
  "$CXX" $BASE audit_control_rom.cpp ../../src/Keyboard.cpp ../../src/CPU6502.cpp -o "$workdir/control_rom"
  "$workdir/control_rom" "$TK2000_TEST_ROM"
  # v0.96 full system: real TKMachine + MMIO + gameport against the ROM.
  SYS="../../src/Machine.cpp ../../src/Bus.cpp ../../src/CPU6502.cpp ../../src/Keyboard.cpp ../../src/Cassette.cpp ../../src/DiskController.cpp ../../src/Video.cpp stubs/host_disk_stub.cpp"
  "$CXX" -std=c++11 -O2 -Istubs -I../../src audit_system96.cpp $SYS -o "$workdir/system96"
  "$workdir/system96" "$TK2000_TEST_ROM"
fi

# v0.92 MAME matrix / Windows joystick exhaustive scan.
for mode in 0 1; do
  "$CXX" $BASE -DTK2000_JOYSTICK_KEYBOARD_BRIDGE=0 -DTK2000_GAME_JOYSTICK_COMPAT=$mode audit_mame_keyboard_joystick.cpp ../../src/Keyboard.cpp -o "$workdir/mame_input_$mode"
  "$workdir/mame_input_$mode"
done

# v0.93: analog inputs before first C070; PS/2 transient poll protection,
# both CTRL letters and independent modifier-side releases.
"$CXX" $BASE audit_reference_93.cpp ../../src/Keyboard.cpp -o "$workdir/reference_93"
"$workdir/reference_93"

# v0.94 combined standard physical inputs, without title/CRC hacks;
# preserve backwards-compatible pure joystick matrix tests above.
"$CXX" $BASE -DTK2000_GAME_JOYSTICK_COMPAT=0 -DTK2000_JOYSTICK_KEYBOARD_BRIDGE=1 audit_input94.cpp ../../src/Keyboard.cpp -o "$workdir/input_94"
"$workdir/input_94"

# v0.95 FPGA address decode: all 16 C06x/C07x aliases, both fire buttons,
# analogue neutral including JOYSTICK OFF, and CTRL+B independent of SHIFT.
"$CXX" $BASE -DTK2000_GAME_JOYSTICK_COMPAT=0 audit_fpga95.cpp ../../src/Keyboard.cpp -o "$workdir/fpga_95"
"$workdir/fpga_95"

# v0.97: cassette PCM controlled by actual CT2 / C020 edges, separate mute
# from original C030 speaker and Disk II mechanics, ten-row F1 geometry.
"$CXX" $BASE audit_tape_audio97.cpp ../../src/Cassette.cpp -o "$workdir/tape_audio97"
"$workdir/tape_audio97"

# Actual VGA layout rendering with a small host-only FabGL framebuffer stub.
"$CXX" -std=c++11 -O2 -Istubs -I../../src audit_f1_97.cpp ../../src/VGA.cpp -o "$workdir/f1_97"
"$workdir/f1_97"
