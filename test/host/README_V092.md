# TK-2000 v0.92 input audit (MAME + Windows 0.3)

Run `bash run.sh` from this folder, optionally with `TK2000_TEST_ROM=/path/to/TK2000.rom`.
The new `audit_mame_keyboard_joystick.cpp` checks all 64 joystick-bit combinations, both button positions, neutral/opposed directions, release and the independent CONTROL row from MAME. The 6502 `$F043` regression proves that SHIFT+CONTROL letter and CONTROL-only are distinct codes, not that the BASIC runtime actually performs the six requested actions on physical boards.
MAME `tk2000.cpp` explicitly lists joystick port emulation as TODO; Windows 0.3 supplies the joystick row/column/threshold logic. No per-title patch is enabled by default.
