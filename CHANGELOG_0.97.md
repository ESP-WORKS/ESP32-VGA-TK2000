# TK-2000 v0.97 — Som FITAS K7 (base oficial v0.96r3)

## Implementação

- A **base é exatamente o ZIP oficial e estável `TK2000_v0_96r3_CUMULATIVO.zip`**, com modificações cumulativas nesta versão; não há retorno à v0.95.
- Novo item **SOM FITAS K7** no menu F1, entre `SOM DOS DRIVES` e `COLD RESET`, com seleção **ON/OFF**. O estado após ligar o ESP32 é **ON**. Alterar a opção não reinicia a fita, o CPU, nem modifica CT2 ou SD.
- F1 aumentado de 9 para **10 linhas**, preservando passo de 13 px, fonte 5×7 e repintura somente das linhas alteradas. Diálogo deslocado de `(125,15,390,170)` para `(125,7,390,186)` de modo que a última opção permaneça acima do separador e o rodapé caiba nos 200 pixels de vídeo.
- Áudio K7 independente, misturado ao **DAC FabGL já existente GPIO25 / 16 kHz**, sem ocupar pinos ou alterar o flip-flop virtual `$C030`, jogos, teclado, joystick ou áudio Disk II.
- **Leitura**: volume e tonalidade da voz K7 derivam do sinal de entrada **real** do leitor CT2 (bit 7 `$C010`) e da duração atual de cada meia-onda (CA/CB/DA, especialmente 250 e 500 ciclos). Não é uma trilha ou ruído reproduzido sem uma fita ativa. O som inicia com a reprodução e é silenciado em pausa, EOF, ejeção e menus.
- **Gravação**: o áudio deriva das transições **reais** de `$C020–$C02F` e dos intervalos de ciclos entre elas, somente durante gravação ativa. O tratamento de CT2/multifaixa e as condições de parada permanecem inalterados.
- Independência completa: `SOM FITAS K7=OFF` silencia **somente a voz de monitor K7**, sem afetar a gravação/leitura, o alto-falante C030 ou o SOM DOS DRIVES. A escolha pode ser alterada mesmo com fita montada (o F1 pausa a CPU).
- `TKTapePCM97.h` usa um sintetizador quadrado em 16 kHz sincronizado à fase/intervalos reportados pelo K7 e uma tabela de passos pré-calculados para os comprimentos de pulso do formato CT2; ISR sem leitura SD, alocação de memória ou divisão.
- O comportamento segue o princípio de monitoramento de meia-onda observado em `TK2000-Java.gz`, `Tape.java` (`C010`/`C020` e sinal de monitor) e `Sound.java` (toggle `$C030` e produção PCM). O código de síntese não foi copiado: foi adaptado à arquitetura e ao DAC do ESP32, usando um canal próprio para preservar o som do TK.

## Arquivos alterados

- `src/main.cpp`: menu F1, escolha ON por padrão e comutação independente.
- `src/VGA.cpp`, `src/VGA.h` e `src/TKMainMenu97.h`: layout F1 de dez linhas.
- `src/Machine.cpp`, `src/Machine.h`: eventos de monitor de leitura/gravação e flag de áudio K7.
- `src/Cassette.cpp`, `src/Cassette.h`: exposição da duração da meia-onda efetivamente atual; decodificação e tempo de K7 preservados.
- `src/TKPCMDriveAudio.h` e novo `src/TKTapePCM97.h`: voz PCM independente.
- `src/BuildConfig.h`, `platformio.ini`, `README.md`, `test/host/run.sh`, testes de host v0.97 e avisos de referências.

## Verificações executadas

- Suíte cumulativa `test/host/run.sh` com GCC e Clang: **PASS**.
- Suíte com ROM original TK-2000 (fornecida apenas no ambiente de teste, não incluída no ZIP): **PASS**.
- Novos testes de fonte CT2 sintética, variação de frequência 250/500 ciclos, captura de SAVE, pausa, retomada, ON/OFF, coexistência com C030, geometria e pintura de 10 opções F1: **PASS**.
- ASAN e UBSAN nos novos testes: **PASS**.
- Não foi possível compilar firmware ESP32/PlatformIO/FabGL nem validar o som em hardware nesta sessão. O volume e o timbre deverão ser aprovados no TTGO/RobGO.

## Roteiro de validação física

1. Iniciar o TK-2000 e abrir F1: confirmar `SOM FITAS K7  ON`, dez linhas inteiras e rodapé visível. Navegar até `REINICIAR ESP32` sem piscadas.
2. Inserir uma fita CT2, sair do menu e executar `LOADT`/`LOADA` apropriado. O chiado/sinal de carga deve começar junto das meias-ondas K7 e cessar após o fim/pausa.
3. Abrir F1 e escolher `SOM FITAS K7 OFF`: repetir a mesma operação e confirmar **carga idêntica e silenciosa**. Com ON novamente o monitor de fita deve voltar.
4. Executar um SAVE com fita e confirmar o ruído durante a escrita e ausência após finalizar; repetir em OFF para verificar gravação intacta.
5. Testar `SOM DOS DRIVES` ON/OFF e som de jogo C030, independentes de `SOM FITAS K7`.
6. Confirmar Moon Patrol, Montezuma, teclado, VGA sem chiado, CT2 multifaixa e MultiLoader a partir da base v0.96r3.

**Nota de fidelidade:** a voz PCM emprega resíntese de pulsos do sinal CT2; segue duração/fase dos pulsos da fita, mas não equivale a uma captura analógica do cabeçote K7 nem garante amostragem subciclo sob K7 Turbo 8×.
