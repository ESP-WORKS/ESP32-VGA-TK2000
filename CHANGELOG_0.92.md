# TK-2000 v0.92 - Auditoria MAME/Windows 0.3, controles e OSD K7

Base: v0.91 RobGO/TTGO PlatformIO. Esta versão é CANDIDATA para ensaios físicos.

## Alterações efetuadas

- `Keyboard.cpp`: leitura $C010 com CONTROL selecionado por $C05F soma o contato `ROW8.0` (0x01) ao resultado das linhas KBOUT, conforme `tk2000_state::kbin_r()` do MAME. A versão v0.91 devolvia 0x81 prematuramente e ocultava as outras linhas; isso podia quebrar softwares que verificam teclas/controlador enquanto o latch está ativo. Não foi criado atalho ASCII de CTRL; a ROM interpreta os códigos.
- `TKJoystickMatrix.h` (novo): mecanismo único de varredura do joystick pelo KBOUT/KBIN. Posição digital neutra 127, esquerda/cima 0, direita/baixo 255; limiares <30 ou >225 documentados em `JoyVerificaTecla()` do Windows 0.3. Direções opostas se neutralizam, contatos FIRE1 (ROW7/bit4) e FIRE2 (ROW6/bit4) mantidos. É genérico, não detecta jogos nem arquivos CT2.
- `VGA.cpp/.h`, `main.cpp`: remoção completa do OSD K7 específico Moon Patrol (rotina de desenho, chamada, cache e multiplicador de tela). Os modos de fita e otimizações de carregamento NÃO foram modificados; diagnósticos seriais opcionais permanecem desligados por padrão.
- `BuildConfig.h`: versão 0.92 e descrição da remoção do OSD.
- `test/host/audit_mame_keyboard_joystick.cpp`: varredura de 64 máscaras, duas teclas de disparo, controles opostos, soltura e CTRL somado ao KBOUT. Teste v0.91 adaptado para a regra elétrica do MAME. Testes reais da ROM $F043 continuam verificando CTRL+C/X/G/R/S/Q distintos de CTRL+SHIFT+C/X/G/R/S/Q.

## Fontes de referência

1. MAME (código oficial): https://raw.githubusercontent.com/mamedev/mame/master/src/mame/apple/tk2000.cpp — matriz de 8 linhas, ROW8 para CONTROL, KBOUT $C000 e KBIN $C010; joystick está explicitamente marcado como TODO, ligado ao par KBOUT/KBIN.
2. Emulador Windows 0.3 recebido do usuário: `src/tk_teclado.cpp`, `src/tk_joystick.cpp` — `JoyVerificaTecla()` e os limiares de coordenadas. A referência Windows usa uma prioridade curta diferente para CONTROL; nesta versão adotou-se a combinação de linhas do MAME.

## O que não foi alterado

- SHIFT+CONTROL+A-Z, já validado fisicamente pelo usuário; video framebuffer/paleta e correção do ruído VGA, também validados.
- Joystick por perfis/CRC permanece DESATIVADO por padrão (`TK2000_GAME_JOYSTICK_COMPAT=0`); reativação é reversível pela flag. `TK2000_UNIVERSAL_PREAD=1` e modo analógico CENTERING permanecem como v0.91.
- Drivers de cassete CT2, salvar multifaixas, discos, MultiLoader, partições e menus F1/F2/F3.

## Limites de validação

Não há evidência de que a execução física dos controles CTRL+C/X/G/R/S/Q ou dos jogos Chopflifter, Montezuma e Missiles Defense tenha sido corrigida. Os testes host cobrem a matriz e o retorno da rotina $F043 da ROM, mas não executam o BASIC completo nem reproduzem todos os jogos. É necessária compilação PlatformIO (FabGL 1.0.9) e teste na TTGO/RobGO para confirmar as correções. A versão MAME consultada ainda não emula a porta de joystick por completo e não pode ser tratada como referência funcional de jogos.
