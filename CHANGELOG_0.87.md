# v0.87 — Auditoria de teclado, joystick e saída VGA

Nova base oficial: `ESP32-VGA-TK2000-ROBGO-main.zip` (PlatformIO), não
migração invertida para a antiga base `.ino`.

- Corrigido o reset indevido de perfil de joystick dos CT2 ao usar F1/F2/F3/F4.
- Corrigida a ausência de sincronização de perfil ao montar/ejetar fitas.
- Eliminado conflito de tecla SPACE real com FIRE virtual em Missiles Defense.
- Novas diagonais 1/3/7/9 e segundo botão, de acordo com Windows 0.3.
- NumPad + - * / e vírgula decimal no modo teclado.
- Escrita VGA de áreas retangulares agrupada em spans de linhas cruas FabGL.
- PlatformIO usa explicitamente partições da base e tag FabGL v1.0.9.
- Mantidos os recursos anteriormente aprovados de menus, CTRL A–Z,
  Montezuma, Moon Patrol e gravação CT2 multifaixa.

Ver `docs/AUDITORIA_TECLADO_JOYSTICK_VGA_V087.md`.

**Status:** testes C++ host aprovados. Compile completo de PlatformIO e
confirmação de eliminação do chiado na tela dependem de testes físicos.
