# TK-2000 v0.90 — auditoria geral de teclado, joystick e VGA

Base: `ESP32-VGA-TK2000-ROBGO-main`, cumulativa sobre a v0.89, mantendo
`src/main.cpp`, `platformio.ini`, RobGO/TTGO VGA32 e as partições MultiLoader.

## Melhorias gerais, sem perfis específicos de jogos (opção padrão)

1. **Compatibilidade PREAD universal e reversível:** o TK2000.ROM que acompanha
   o emulador Windows 0.3 tem `SEC; RTS` em `$FB1E` (`38 60`). Alguns
   programas esperam o contrato de Apple II PREAD, com coordenada em `Y` e
   eixo em `X=0/1`. A nova verificação só fornece o valor quando há um
   `JSR $FB1E` genuíno identificado pela pilha, ROM ativa e stub esperado.
   Não consulta títulos, nomes de arquivo ou CRC de CT2. Com JOYSTICK OFF,
   as coordenadas ficam neutras em 127, evitando enviar esquerda por padrão
   aos programas que chamam essa função. Não altera carry, RTS ou tempo 6502.
2. **KEYUP e NumLock:** um mesmo teclado numérico pode mudar de `VK_KP_7`
   para `VK_KP_HOME` sem que a tecla física tenha sido solta. O joystick
   passa a identificar uma tecla física estável para as duas formas.
   Eventos de liberação em qualquer forma soltam corretamente a direção;
   controles do bloco de setas continuam independentes do teclado numérico.
3. **ESPAÇO físico com JOYSTICK ON:** passa simultaneamente pela matriz
   TK original e pelo contato FIRE1 do joystick, com liberação independente.
   O 0 do teclado numérico continua sendo só FIRE1, permitindo escolher
   uma fonte sem SPACE. Isso é um comportamento uniforme, não um patch de jogo.
4. **Dois modos analógicos universais:** CENTERING (padrão, soltar volta ao
   centro) e STANDARD (soltar mantém última coordenada), como no Windows 0.3.
   STANDARD é uma opção global de compilação, não ligada a nome de jogo.
5. **VGA:** desenho HGR continua pré-calculado em RAM local; cópia das
   linhas e preenchimento de retângulos agora usam escritas de 32 bits
   alinhadas quando possível, preservando a mesma imagem byte a byte.
   Objetivo: reduzir grupos de pixels lidos pela DMA no meio de mudanças.
   Isto **não elimina por definição** chiado de cabo, alimentação ou
   instabilidade de sincronismo; necessita teste físico nas duas placas.

## Configuração reversível (`platformio.ini`)

- `-DTK2000_GAME_JOYSTICK_COMPAT=0` padrão: desativa adaptações por jogo.
- `-DTK2000_UNIVERSAL_PREAD=1` padrão: PREAD universal com validação do JSR;
  `0` desativa esse comportamento.
- `-DTK2000_JOYSTICK_STANDARD_MODE=0` padrão: CENTERING; `1` seleciona
  STANDARD globalmente, sem exceções por jogo.
- `-DTK2000_VGA_WORD_STORES=1` padrão: escrita VGA agrupada; `0` restaura
  a estratégia anterior para comparação.
- `-DTK2000_GAME_JOYSTICK_COMPAT=1` reativa legado da v0.88; para comparação
  exata do PREAD antigo use também `-DTK2000_UNIVERSAL_PREAD=0`.

O firmware **não ativa mapeamento especial A/Z em Montezuma** no modo padrão.
Isso significa que a combinação JOYSTICK ON + cursores pode continuar sem
controlar um jogo que exige A/Z; a solução correta depende de identificar
um esquema de controles aceito pelo software, e não inventar teclas globais.

## Testes concluídos no computador

- Regressão v0.88, v0.89 nos modos de compatibilidade 0 e 1.
- 6502 genuíno: `JSR $FB1E`, `SEC` com Y atualizado, `RTS` preservando pilha
  e ciclos; proteção contra chamada falsa e ROM desabilitada.
- NumLock alternado, direções diagonais, dois botões, retorno à posição
  neutra, SHIFT/CTRL e liberação de tecla perdida.
- 5.000 retângulos comparados ao preenchimento de referência, mais cópia
  de scanline incluindo fallback desalinhado.
- GCC C++11, Clang em CPU6502 com O3 especializado desligado, ASAN/UBSAN
  nos testes gerais.

## Pendente

Compilação completa ESP32/PlatformIO/FabGL e testes físicos em RobGO e TTGO.
**Não** declarar resolvidos Chopflifter, Montezuma, Missiles Defense ou chiado
até verificar a v0.90 diretamente nas placas.
