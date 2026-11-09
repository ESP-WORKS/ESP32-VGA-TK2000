# TK-2000 v0.94 — Auditoria geral da fila PS/2 e controles

**Base cumulativa:** v0.93 RobGO/TTGO, FabGL 1.0.9, ESP32 Arduino core compatível com PlatformIO, com partições MultiLoader preservadas. **Sem regras específicas para jogos na configuração padrão.**

## Defeito identificado no encaminhamento das teclas

No `loop()` da v0.93, todas as transições PS/2 da fila FabGL eram processadas em um `while(virtualKeyAvailable())` ANTES de qualquer execução do 6502. Quando MAKE e BREAK de CONTROL+B, de um cursor ou de ESPAÇO estavam ambos na fila, o estado final era tecla solta, e o 6502 jamais enxergava o intervalo pressionado.

A v0.94 introduz `TKInputEventPacing.h` e `interleaveQueuedInput()` no `main.cpp`. Após uma transição MAKE, quando existe outro evento na fila, o 6502 pode executar até 4096 ciclos, limitados a 8192 por iteração. Isso ocorre somente durante a execução efetiva da máquina, sem menus, reset pendente ou transporte K7 ativo. São contados os ciclos realmente executados e descontados **integralmente**, inclusive com dívida residual, do orçamento de tempo normal, evitando aceleração artificial do TK. Os caminhos de temporização do K7 turbo, VGA e cassete CT2 são preservados. `TK2000_INPUT_INTERLEAVE=0` reverte o comportamento para comparação.

## CONTROL+B — modo gráfico

O usuário confirmou que o manual original diz **CONTROL+B**, e NÃO CONTROL+R. Executado o monitor TK-2000 real na entrada `$F043`: CONTROL+B resulta em `$82`, B normal `$C2` e CONTROL+SHIFT+B `$12`. A posição na matriz estava correta; o defeito potencial de teclas curtas descartadas ANTES do 6502 foi corrigido no encaminhamento dos eventos. A mudança de modo gráfico BASIC ainda depende de teste físico na v0.94; o teste do monitor sozinho não comprova o estado gráfico.

## Compatibilidade geral entre interfaces do joystick

`KeyboardTK::in()` mantém sempre os contatos nativos do joystick em KBIN/KBOUT, conforme MAME/Windows 0.3. Com `TK2000_JOYSTICK_KEYBOARD_BRIDGE=1` (padrão), quando **JOYSTICK ON** os mesmos sinais também são expostos nas posições convencionais do teclado: UP=A (linha 1 bit 5), DOWN=Z (linha 0 bit 5) e FIRE1=ESPAÇO (linha 2 bit 0). LEFT/RIGHT já compartilham os contatos das setas nativas. Direções opostas são neutralizadas e FIRE2 continua separado. Nenhum contato físico é sintetizado via `key()`; a composição acontece somente na leitura da matriz e permite múltiplas fontes sem conflito de KEYUP. Quando JOYSTICK OFF, a máscara é zerada e a ponte não injeta nada.

O mesmo mapeamento se aplica a **todos** os softwares, independentemente de CT2, nome, CRC, RAM, endereço de execução ou perfil de jogo. Não há mudança de código/ROM dos jogos. `TK2000_JOYSTICK_KEYBOARD_BRIDGE=0` habilita o circuito estrito para comparação. A antiga compatibilidade particular da v0.88 somente compila como opção separada quando `TK2000_GAME_JOYSTICK_COMPAT=1`; permanece desativada na build padrão.

**Limitação:** a ponte genérica expõe A/Z enquanto os cursores verticais do joystick estão pressionados, mesmo para programas que usem essas letras para outra função. Portanto, testar novamente Moon Patrol, que foi aprovado fisicamente na v0.93. A tela VGA sem chiado, CT2 multifaixa, menus, OSD Moon removido e tokens SHIFT+CONTROL permanecem inalterados no código.

## Testes reproduzidos

- `TK2000_TEST_ROM=/caminho/TK-2000.ROM sh test/host/run.sh` — regressões antigas v0.88–v0.93 e nova v0.94 com GCC e Clang (ROM obtida fora deste ZIP).
- `test/host/audit_control_rom.cpp`: leitura da ROM autêntica para CONTROL+B, CONTROL+G e demais atalhos, com e sem SHIFT.
- `test/host/audit_input94.cpp`: quatro perfis de entrada, **sem tratamento por título**, estado neutro, duas direções opostas, A/Z, FIRE1/SPACE, FIRE2 independente, liberação, CONTROL+B, 4096/8192 ciclos de interleaving e amortização do débito.
- AddressSanitizer/UndefinedBehaviorSanitizer aplicado ao novo teste de matriz e escalonamento.

**Sem validação de compilação final ESP32/PlatformIO, FabGL em hardware, nem comprovação física de que Choplifter, Montezuma, Missiles Defense ou CONTROL+B estejam corrigidos.**

## Roteiro de teste físico

1. BASIC: CONTROL+G (beep), CONTROL+B (alternar gráfico), CONTROL+SHIFT+P (PRINT) e CONTROL+S/Q durante LIST. Não usar CONTROL+R como substituto de B.
2. Moon Patrol JOYSTICK ON: setas e ESPAÇO devem continuar funcionando; verificar que o OSD específico não reapareceu.
3. Choplifter JOYSTICK OFF: o helicóptero permanece parado na tela inicial? Repetir com JOYSTICK ON, soltando cada direção e testando FIRE.
4. Montezuma JOYSTICK ON: cima, baixo, esquerda, direita e ESPAÇO; depois JOYSTICK OFF com comandos tradicionais do teclado.
5. Missiles Defense: deslocamento, liberação das setas e FIRE com JOYSTICK ON; repetir OFF com teclas nativas. Registrar o que cada modo fez.
6. VGA sem chiado, menus F1/F2/F3 sem piscadas, CT2 multifaixa, SD/MultiLoader.

A imagem de Choplifter não foi fornecida nesta auditoria. Não há base para afirmar que o movimento autônomo inicial foi identificado ou eliminado.
