# Testes v0.91 — teclado original e ROM

`sh test/host/run.sh` executa os testes prévios mais `audit_control_semantics.cpp`
(GCC C++11) para as 26 letras, modificadores esquerdo/direito, prioridade do
latch CONTROL, símbolos e liberação em diferentes ordens.

Para testar com a ROM original fornecida separadamente (sem redistribuí-la):

```
TK2000_TEST_ROM=/caminho/TK2000.rom sh test/host/run.sh
```

`audit_control_rom.cpp` executa o 6502 real na rotina `$F043`, comparando
CONTROL+C/X/G/R/S/Q versus CONTROL+SHIFT+letra. É uma verificação de
códigos de teclado, não do funcionamento integral de comandos BASIC.

**v0.92:** `audit_control_semantics.cpp` now verifies the MAME KBIN row OR instead of the Windows 0.3 early-return convention. See `audit_mame_keyboard_joystick.cpp`.
