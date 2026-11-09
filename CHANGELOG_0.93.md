# TK-2000 v0.93 — Auditoria complementar de entradas (C17 e Java)

Base: v0.92 RobGO/TTGO, PlatformIO. Sem exceções de controles por jogo (flag `TK2000_GAME_JOYSTICK_COMPAT=0`).

## Alterações efetivas

1. **Paddle antes do primeiro `$C070`** — `TKPaddleJoystick` passa a armar o temporizador no primeiro acesso a `$C064`–`$C067`, caso nenhum strobe tenha ocorrido desde o reset. Antes o temporizador era zero, e uma primeira leitura após muitos ciclos parecia um paddle inteiramente descarregado (extremo esquerdo/superior), inclusive com JOYSTICK OFF. Após o primeiro acesso a curva continua obedecendo os ciclos da CPU; `$C070` continua sendo o strobe principal. O acionamento não altera nem injeta teclas.
2. **Contato de FIRE, cursores e teclas PS/2** — no mecanismo de reconciliação de estado a cada ~12 ms, três amostras consecutivas de `isVKDown()` ausentes são exigidas antes de liberar um contato. Isso evita perder uma tecla realmente pressionada devido a uma amostra transitória. O evento PS/2 KEYUP genuíno continua liberando o contato imediatamente. Aplicado de forma uniforme a `TKJoystickInput`, `TKHostKeyOwners` e `TKModifierSides` (CONTROL/SHIFT direito e esquerdo).
3. **Controle original preservado** — a matriz de teclado, PREAD universal, os dois botões, diagonais e modos CENTERING/STANDARD não receberam remapeamentos exclusivos para jogos. Permanecem, sem alterações, o tratamento CONTROL+SHIFT de tokens BASIC, as correções VGA e a ausência do OSD específico do Moon Patrol.

## Diferenças confirmadas nas referências

- C17 (SDL/C++), `src/Keyboard.cpp`: oito linhas de seis contatos de tecla, seleção KBOUT e contato CONTROL por `$C05F`; o estado é mantido enquanto a tecla está pressionada. C17 tem um caso de leitura especial para SHIFT quando `KBOUT==1`.
- TK2000-Java (`src/Keyboard.java`): matriz similar e prioridade especial CONTROL/SHIFT, mas exige igualdade exata da seleção da linha; não é uma referência completa para leituras de várias linhas simultâneas. `KeyboardBuffered.java` usa eventos limitados por contagem de ciclos, o que **não** equivale ao contato elétrico mantido enquanto a tecla está pressionada. Não foi transplantado esse buffer de eventos para o ESP32.
- Windows 0.3: único dos três que disponibiliza uma emulação de joystick com duas teclas de botão, limiares de eixo e diferentes modos de recentralização. C17 e Java não documentam uma implementação de joystick completa equivalente.
- **CONTROL+B x CONTROL+R**: a imagem/documentação de atalhos do TK-2000 recebida do usuário indica CONTROL+**R** para modo gráfico. C17/Java não justificam trocar o contato físico B por R. Portanto, CONTROL+B não foi artificialmente remapeado; testar também CONTROL+R antes de atribuir um defeito ao emulador.

## Validação

- `test/host/run.sh`: regressões existentes v0.88–0.92, mais `audit_reference_93.cpp`.
- O novo teste executa: leitura analógica sem `$C070`, leituras com strobe, esquerda/direita/neutro, tolerância a duas amostras PS/2 ausentes, liberação na terceira, KEYUP imediato, dois CONTROL independentes, matriz B/R.
- Código compilado localmente com GCC e Clang em C++11 e verificador AddressSanitizer/UndefinedBehaviorSanitizer.
- **Não foi possível compilar o firmware ESP32 com PlatformIO e FabGL neste ambiente.** Os resultados em jogos e na tela VGA exigem validação física nas placas.

## Critérios dos testes físicos

- Choplifter com JOYSTICK OFF e ON: verificar se deixou de apresentar desvio inicial à esquerda e se os comandos permanecem estáveis.
- Moon Patrol JOYSTICK ON: verificar disparo com ESPAÇO e com teclado numérico 0, comparando com TAB; a correção protege as teclas contra soltura prematura, mas não garante que o jogo use a mesma entrada.
- Montezuma e Missiles Defense: avaliar direções/disparo com joystick padrão; versões com comandos próprios podem continuar não compatíveis sem ajustes no programa original.
- BASIC: CONTROL+G, CONTROL+R, CONTROL+B e CONTROL+SHIFT+letra; não confundir atalho de R documentado com B.
- Sem OSD específico Moon Patrol; VGA sem chiado; menus F1/F2/F3; CT2 multifaixa.

## Retificação documental na v0.94

A digitação correta do comando de modo gráfico é **CONTROL+B**. O documento digitalizado foi inicialmente interpretado como CONTROL+R em baixa qualidade. A correção da leitura foi confirmada pelo usuário e conferida na rotina de teclado da ROM original ($F043 retorna $82).
