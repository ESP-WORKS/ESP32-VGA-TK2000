# TK-2000 RobGO/TTGO PlatformIO v0.88 — cumulativa

**Base:** v0.87 RobGO/PlatformIO, com as funções já testadas pelo usuário preservadas.

- Teclas PS/2 com múltiplas origens: `TKHostKeyOwners.h` rastreia VKs físicos distintos e entrega uma única tecla guest até o último KEYUP. Evita o release prematuro de contatos duplicados e ignora autorepeat.
- Polling leve de solturas também para letras, modificadores L/R, TAB e REPEAT, sem gerar pressões falsas.
- Numpad 5 consumido como posição neutra quando joystick está ON (sem digitar 5 no BASIC).
- Implementados e comparados modos analógicos CENTERING e STANDARD do Windows 0.3, com comportamento específico de Missiles Defense; a leitura Apple de paddle e PREAD reporta a mesma mira.
- Mudança do perfil ao selecionar/ejetar CT2 ajusta o modo analógico; resets limpam a última posição STANDARD.
- Ampliados os testes host de pontuação BR/US, combinações, dois botões, diagonais, solturas, eventos duplicados e proteção de funcionalidades validadas.
- **Não compilado em ESP32/PlatformIO nem validado fisicamente.** Ver `docs/AUDITORIA_COMPLEMENTAR_ENTRADAS_V088.md`.
