# Atualização v0.97 — Som FITAS K7

A partir da **base oficial v0.96r3**, F1 possui dez opções e uma nova escolha
`SOM FITAS K7  ON/OFF` (inicial ON). Reproduz os pulsos de leitura CT2 e de
salvamento C020 pelo DAC GPIO25, no mesmo misturador do speaker e drives, mas
com controle de áudio independente. Em OFF as operações K7 não são alteradas.
A janela F1 foi ampliada verticalmente para acomodar a opção. Ver
`CHANGELOG_0.97.md` para implementação, limitações e roteiro de testes.

---

# Versão v0.96 (revisão 3): entrada fiel ao hardware TK2000

KBIN em repouso com bit 7 = 0; setas sem ponte A/Z (a ROM lia "A" no lugar de "↑"); ESPAÇO = tecla ESPAÇO + FIRE1 com JOYSTICK ON; PREAD Apple em `JSR $FB1E` somente com JOYSTICK ON; A/Z nas setas apenas para o Montezuma. Consulte `CHANGELOG_0.96.md`.

> **v0.95:** espelhamento C06x de joystick e C07x de strobe de paddle (FPGA TK2000 II). Veja `CHANGELOG_0.95.md`.

> **v0.94 — auditoria geral de teclado e joystick:** corrigida a perda de eventos PS/2 quando MAKE e BREAK chegam na mesma fila (execução intercalada de 6502 com débito de ciclos compensado); adicionada ponte global opcional JOYSTICK ON -> A/Z e FIRE1/ESPAÇO, sem detectar jogos. CONTROL+B é o comando correto para modo gráfico. Veja `CHANGELOG_0.94.md` e `docs/CONTROLES_GERAIS_V094.md`. Moon Patrol aprovado fisicamente na v0.93; a v0.94 ainda exige testes no equipamento.

> **v0.93**: auditoria C17/Java, inicialização genérica do paddle e reforço na reconciliação PS/2. Veja `CHANGELOG_0.93.md`.

> **v0.91 — teclado original corrigido:** CONTROL+letra conserva os atalhos do TK-2000 (C/X/G/R/S/Q); somente CONTROL+SHIFT+letra gera tokens BASIC. A matriz e a prioridade do latch CONTROL foram conferidas com o emulador Windows 0.3 e a rotina real da ROM `$F043`. Consulte `CHANGELOG_0.91.md` e `docs/AUDITORIA_CTRL_SHIFT_V091.md`. Testes físicos e compilação PlatformIO ainda pendentes.

> **v0.90 — auditoria geral sem patches por jogo:** o joystick padrão recebeu PREAD universal com validação de chamada 6502, soltura robusta de teclas do teclado numérico, SPACE físico na matriz e no FIRE1 e escrita VGA em palavras de 32 bits. Duas variantes analógicas globais (CENTERING e STANDARD) são selecionáveis por opção de compilação, sem identificar os jogos. Consulte `CHANGELOG_0.90.md` e `docs/AUDITORIA_GERAL_CONTROLES_VGA_V090.md`. Compilação PlatformIO e testes físicos pendentes.

> **v0.89 — variante de joystick padrao:** no `platformio.ini`, `TK2000_GAME_JOYSTICK_COMPAT=0` desativa somente atalhos de controle especificos dos jogos. Para recuperar o comportamento v0.88, altere a flag para `1` e recompile. Consulte `CHANGELOG_0.89.md` e `docs/TESTE_JOYSTICK_PADRAO_V089.md`.

# TK-2000 ESP32 — base RobGO/TTGO, PlatformIO — v0.91

**Base do projeto:** `ESP32-VGA-TK2000-ROBGO-main.zip` fornecido pelo usuário.
**Referência de comportamento:** `tk2000-emulator-master-0.3.zip` (emulador TK-2000 para Windows, Fábio Belavenuto).

Este pacote contém o **código-fonte cumulativo** do emulador TK-2000 para ESP32,
com suporte de teclado PS/2, joystick virtual, vídeo VGA, Disk II, cassete CT2 e
expansão Saturn 128K. A estrutura `src/main.cpp`, `platformio.ini`, `src/`
e `partitions.csv` da base RobGO foi preservada. Não inclui ROMs, jogos, arquivos de discos ou binários compilados.

## Compilação com Visual Studio Code + PlatformIO

1. Extraia todo o ZIP, incluindo `platformio.ini`, `src/` e `partitions.csv`.
2. Abra a pasta `ESP32-VGA-TK2000-main` no Visual Studio Code com PlatformIO.
3. Execute **PlatformIO: Build** (ou `pio run -e esp32dev`).
4. O binário normal do PlatformIO, após uma compilação bem-sucedida, ficará em
   `.pio/build/esp32dev/firmware.bin`.

`platformio.ini` conserva a placa e os parâmetros originais da base RobGO,
seleciona explicitamente `partitions.csv` e fixa FabGL em `v1.0.9` para
reprodutibilidade. O arquivo de partições corresponde ao mapa histórico ESP-WORKS
(factory 0x10000, ota_0 0x130000). **Não use PlatformIO Upload sem antes
verificar o procedimento do bootloader da sua placa**: compilar não é o mesmo
que gravar somente a imagem OTA e não se deve substituir o MultiLoader.

As antigas ferramentas `tools/build_bootloader.py` e `tools/verify_bootloader.py`
foram conservadas da base recebida, mas não foram adaptadas para compilar
`src/main.cpp` com PlatformIO; utilize o fluxo `pio run`, não o script Arduino
legado, para gerar o binário desta versão.

O ambiente de execução desta auditoria não dispõe da cadeia ESP32/PlatformIO
nem de acesso à rede para instalá-la. Os testes C++ host foram executados;
**compilação final RobGO e TTGO e comportamento físico VGA ainda requerem confirmação**.

## Alterações de entrada e VGA — v0.87

- Perfil de jogo detectado pela identidade do CT2 é aplicado à matriz de teclado
  imediatamente ao montar a fita, e preservado quando um menu libera as teclas
  ou quando F4 alterna o joystick. Ejetar a fita restaura perfil padrão.
- O botão virtual SPACE é combinado na leitura da matriz; não é mais
  artificialmente pressionado como se fosse uma tecla física. Corrige conflitos
  de ordem de liberação do botão/tecla no Missiles Defense.
- Joystick via setas e teclado numérico: diagonais (1,3,7,9) com NumLock
  ligado/desligado e dois botões independentes (SPACE/0; decimal numérico).
  O botão 2 segue o Windows 0.3 em linha 6, bit 4; também disponível em C062.
- Teclado numérico fora do joystick reconhece +, -, *, / e vírgula decimal,
  no mapeamento do emulador de referência.
- Desenho de retângulos VGA agora grava sequências de pixels já empacotados
  para a FabGL, eliminando dezenas de milhares de escritas individuais na
  memória de varredura durante a limpeza/redesenho. O resultado é bit a bit
  equivalente ao anterior. Isso reduz uma possível causa de "chiado"/snow
  por atualização parcial, mas não elimina necessariamente interferência
  elétrica/ruído analógico presente no hardware, que exigirá teste físico.
- A resolução **640x200**, paleta HGR, menus F1/F2/F3 sem flicker, CTRL+A–Z,
  cursor Moon Patrol, Montezuma e CT2 multifaixa foram preservados.
  **Observação histórica:** a função automática CTRL+A..Z -> token descrita
  na v0.87 foi corrigida na v0.91; tokens exigem CTRL+SHIFT+A..Z.

Veja `docs/AUDITORIA_TECLADO_JOYSTICK_VGA_V087.md` para a comparação detalhada
com a versão Windows 0.3 e `test/host/run.sh` para os testes no computador.

## ROMs e arquivos no cartão SD

A base de código não distribui ROMs e jogos de terceiros. Mantenha os caminhos
já utilizados na instalação anterior, como `/TK-2000/ROMS/TK-2000.ROM`,
`/TK-2000/ROMS/DISK-II.ROM`, `/TK-2000/DISKS/` e `/TK-2000/CT2/`.

## Compatibilidade e testes físicos

A versão v0.91 **não foi testada fisicamente** nesta execução. Recomenda-se:

- BASIC: CONTROL+C/X/G/R/S/Q sem SHIFT; tokens com CONTROL+SHIFT+A–Z;
  SHIFT, símbolos BR/US e F8 REPEAT;
- Joystick ON: setas, diagonais 1/3/7/9, 0/SPACE, decimal numérico; OFF sem entrada residual;
- Missiles Defense CT2 original fornecido: movimentação, parada e disparo;
- Montezuma e Moon Patrol: repetir testes de controle já aprovados;
- Menus F1/F2/F3 sem piscadas, vídeo HGR nas duas páginas, bordas e áudio;
- RobGO e TTGO: conferir estabilidade de cores/sincronismo VGA e MultiLoader.

O desempenho 8x de Moon Patrol continua sendo uma questão separada; esta
revisão não promete mudança na velocidade de fita.

## Créditos e histórico

Os avisos de licenças originais permanecem em `LICENSE` e
`THIRD_PARTY_NOTICES.md`. O README da base anterior foi conservado em
`docs/README_HISTORICO_V086.md` apenas como documentação histórica, sem alterar
qual projeto deve ser usado como base atual.


## v0.92 (MAME / controles)

Matriz CTRL aditiva conforme o MAME, joystick genérico com limiares do Windows 0.3 e remoção do OSD de velocidade específico Moon Patrol. Veja [CHANGELOG_0.92.md](CHANGELOG_0.92.md) e [auditoria](docs/AUDITORIA_MAME_CONTROLES_V092.md). Recursos especiais por jogo continuam OFF por padrão. Build e teste físico pendentes.
