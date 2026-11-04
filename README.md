
Desenvolvido por André Chaves

> **v0.86 cumulativa para testes:** novo controle de mira e disparo em Missiles Defense; redução de atualização do bitmap durante K7 de Moon Patrol para liberar CPU. A velocidade real só será confirmada na TTGO. Consulte `CHANGELOG_0.86.md` e `TESTES_0.86.md`.

# TK-2000 para TTGO VGA32 — v0.86 MÍNIMA CUMULATIVA

Emulador do TK-2000 para ESP32-WROVER/TTGO VGA32 com FabGL 1.0.9,
CPU MOS 6502, ROM externa pelo SD, Disk II DSK/NIB, Saturn 128K e
cassete CT2. Esta é uma **distribuição mínima de código-fonte**, no
mesmo conceito de `MC-1000_0.7_MINIMO.zip`: não inclui ROMs, jogos,
disquetes, imagens K7, binários compilados, testes host ou versões históricas.

## Requisitos e compilação

- TTGO VGA32 com PSRAM (ESP32-WROVER), teclado PS/2 e cartão SD.
- Arduino CLI no Windows, core `esp32:esp32` 2.0.17 e biblioteca FabGL 1.0.9.
- `COMPILAR.bat` no Windows ou `COMPILAR.sh` no Linux/macOS.
- FQBN: `esp32:esp32:esp32wrover:PartitionScheme=huge_app`.
- O `partitions.csv` é exigido pelo esquema ESP-WORKS.
- O script compila **apenas** o `firmware.bin` de OTA0 para o bootloader
  ESP-WORKS e confere partições, limite de tamanho e checksum. Ele não
  realiza gravação direta de bootloader/partições/otadata na flash.
- Saída esperada: `build/bootloader_sd/TK2000/firmware.bin` e
  `build/bootloader_sd/TK2000/version.txt` (identificação `TK2000_0.86`).
- Não reutilize binários antigos se a compilação ou verificação falhar.

## Cartão SD

Conserve seus arquivos já utilizados pela v0.7; **não reformate o SD**.
A inicialização precisa de duas ROMs externas compatíveis, cujos bytes não
estão neste ZIP:

```
/TK-2000/ROMS/TK-2000.ROM   (16 KiB)
/TK-2000/ROMS/DISK-II.ROM  (256 bytes)
/TK-2000/DISKS/            (disquetes .DSK/.NIB)
/TK-2000/CT2/              (fitas .CT2; nomes sem distinguir caixa)
```

O F2 seleciona discos independentes D1/D2; o F3 seleciona uma fita por vez,
cria novos arquivos e confirma **S/A/N** antes de gravar sobre um nome existente. A recuperação
`.bak` de uma gravação CT2 interrompida acontece ao abrir F3 se o arquivo
principal estiver ausente e o backup for estruturalmente válido. Nunca
promove `.tmp` incompleto. Discos também possuem salvamento transacional.

## Teclas e comandos

| Tecla no PC | Ação |
|---|---|
| F1 | Menu de opções |
| F2 | Seletor de disquetes |
| F3 | Seletor de fita CT2 |
| F4 | Emulação do joystick ON/OFF, OSD discreto |
| F8 | REPEAT físico do TK-2000 |
| TAB | FIRE físico do TK-2000, mesmo com joystick OFF |
| SHIFT e CONTROL | Ambos os lados, layouts US e BR |
| F12 | RESET físico equivalente, sem apagar RAM |

No F3: `P` pausa/continua, `R` rebobina, `N` cria CT2, `E` ejeta,
`ENTER` seleciona, `ESC` fecha. Quando o nome já existe no diálogo de
**salvamento**, pressione `S` para substituir, `A` para acrescentar uma
nova faixa ao final do CT2 (mantendo as anteriores) ou `N`/`ESC` para
cancelar. O CT2 é validado e atualizado via `.tmp` + `.bak`; gravações
misturando formatos SAVEA/SAVET ou ultrapassando 8 MiB são recusadas. Para arquivos de fita multietapas,
reprodução com pausa automática e retomada preserva o pulso pendente;
K7 Turbo possui alvo de **8×** apenas durante leitura/gravação ativas.
A velocidade física real depende do processamento disponível na TTGO.

No BASIC, `LOADT` / `LOADA` leem a fita; `SAVET` / `SAVEA` gravam.
Quando necessário, o seletor F3 é aberto pelo próprio comando. Durante
carga e gravação de arquivos K7 multietapas, observe o estado PLAY/PAUSA
AUTO/PAUSA MANUAL no menu F3.

A expansão Saturn 128K é instalada automaticamente, com oito bancos 16 KiB,
registradores no slot 3 `$C0B0–$C0BF`; a ROM BASIC permanece visível ao ligar.
O menu inicia com indicador de desempenho ligado; o OSD mostra **somente o
número de EPS** no canto superior direito, sem texto FPS/EPS ou contador FPS.
Artefatos coloridos HGR permanecem ON. Som PCM dos drives e LEDs D1/D2
preservam a sincronização já validada.

## Novidades da v0.86 (cumulativa, pendente de teste físico)

1. **Missiles Defense:** direções deslocam a posição analógica persistente da mira em vez de saltar 0/127/255; soltá-las mantém a última posição. ESPAÇO passa pela matriz como uma tecla normal e continua acendendo FIRE virtual. Somente CT2 com CRC exato é afetado.
2. **Moon Patrol K7:** enquanto a fita exata estiver em PLAY, a CPU atualiza o quadro HGR 15 vezes por segundo em vez de 60, sem desligar o scanout VGA; terminando a carga retorna a 60 Hz. CPU pode trabalhar até 5,5 ms por rodada K7. Indicador `K7` continua visível na tela, sem serial. A taxa física de 8x **não está comprovada**. Referência anterior na v0.85: 1,3x medida pelo usuário.
3. Preservados menus F1/F2/F3 sem flicker, CONTROL+A-Z, Moon joystick, Montezuma e CT2 multifaixa, todos validados fisicamente em versões anteriores.

Consulte `CHANGELOG_0.86.md` e `TESTES_0.86.md` para detalhes dos testes em host e pendências na TTGO.

## Histórico da v0.85

1. **Missiles Defense:** reconhecimento restrito ao CT2 enviado; a chamada `JSR $FB1E` recebe a posição atual dos eixos do joystick no registrador Y. Ao soltar as setas, a mira deve parar. O ESPAÇO com JOYSTICK ON aciona FIRE e também a matriz de teclado. Teste na TTGO pendente.
2. **F1/F2/F3:** as setas atualizam apenas a seleção ou a lista que mudou, em lugar de redesenhar repetidamente todo o diálogo. Verificar visualmente no VGA após compilar.
3. **Moon Patrol:** indicador `K7` e multiplicador efetivo de ciclos no canto direito do VGA durante PLAY, atualizado a cada aproximadamente dois segundos, **sem monitor serial**. O perfilador serial detalhado fica desligado (`TK2000_K7_MOON_PROFILE=0`) para reduzir trabalho adicional. A meta de 8x **não foi comprovada**; esta edição fornece medição na tela, não uma correção definitiva de velocidade.
4. **Preservação de funções aprovadas na TTGO:** CONTROL esquerdo/direito + A–Z, joystick ON e cursores de Moon Patrol, joystick de Montezuma e salvamento CT2 com múltiplas faixas.

Os testes locais estão em `TESTES_0.85.md`; sem compilação ESP32/FabGL nem testes físicos desta versão.

## O que mudou na v0.84 (cumulativa)

1. **CONTROL esquerdo ou direito + A–Z:** a ROM do TK-2000 insere
   atalhos BASIC via SHIFT+CONTROL+letra. A tradução PS/2 agora mantém
   os contatos A–Z e fornece SHIFT virtual apenas enquanto CONTROL e
   uma letra estão pressionados. CONTROL/SHIFT físicos e combinações
   não alfabéticas continuam independentes. A ROM confirmou 52
   combinações no teste de computador, **e o usuário confirmou funcionamento na TTGO na v0.84**.
2. **Moon Patrol, cursores F4 ON:** corrigido o endereço efetivo do
   `JSR $FB1E` para `$0766` e o retorno 6502 para `$0768`, verificados
   executando a instrução real do programa em teste host. A v0.83
   estava errada em `$0765`/`$0767` e sua proteção nunca disparava.
3. **K7 Turbo de Moon Patrol:** o perfilador `TK2000_K7_MOON_PROFILE=1`
   acrescenta `[K7 MOON PROFILE]` no serial 115200, junto ao
   `[K7 TURBO]` já existente, a cada ~5 s de PLAY: tempo CPU/VGA,
   instruções interpretadas, atalhos ROM, fila saturada/ciclos perdidos,
   leituras e bordas CT2. **Instrumentação para encontrar o gargalo,
   não correção comprovada da lentidão nem confirmação física de 8×.**
   Para comparar desempenho sem o custo do perfilador, compilar com
   `TK2000_K7_MOON_PROFILE=0` em `src/BuildConfig.h`.
4. **Preservação:** Montezuma (joystick e pulo), e gravação CT2 com
   múltiplas faixas permanecem intactos e foram validados pelo usuário
   em hardware na v0.83.

Consulte `CHANGELOG_0.84.md`, `TESTES_0.84.md`,
`DIAGNOSTICO_CONTROL_PS2.md` e `DIAGNOSTICO_K7_MOON.md`.

## O que mudou na v0.83 (histórico da versão anterior)

**Atenção:** as conclusões sobre o endereço `JSR` de Moon Patrol nesta seção
foram corrigidas na v0.84; consulte `CHANGELOG_0.84.md`.


1. **CONTROL + A–Z**: o controle elétrico separado ($C05E/$C05F)
   agora é mesclado aos contatos da matriz, em vez de suprimir a letra
   pressionada. Funciona tanto com CONTROL esquerdo quanto direito,
   mantendo o uso simultâneo dos dois modificadores e os layouts BR/US.
2. **Montezuma**: com F4 joystick ON, a barra ESPAÇO mantém o FIRE
   na matriz TK original e também aciona o contato ESPAÇO (linha 2, bit 0)
   utilizado por esta versão do jogo para pular. Só o CT2 exato é afetado.
3. **Moon Patrol**: na v0.82 a chamada JSR $FB1E foi identificada por um
   endereço errado ($0766). O código real começa em $0765 e empilha
   retorno $0767, não $0768. Corrigidos os três bytes da assinatura
   e o retorno na pilha; os guardas de ROM, F4 ON e identidade CT2 permanecem.
4. **K7 Turbo Moon Patrol**: o filme CT2 enviado foi comparado byte a byte
   com a imagem previamente reconhecida; são idênticos (35.547 bytes,
   CRC-32 C28A4F8D). O total de pulsos do arquivo corresponde a cerca de
   205 s virtuais, ou limite mínimo teórico de 25,6 s a 8x. O teto de
   execução para este jogo passa de 4 para 8 lotes de CPU e de 2,5 ms
   para 3,5 ms por passagem, sem alterar a waveform CT2. Os demais títulos
   mantêm os limites da v0.82. Um log `[K7 TURBO]` a cada ~5 s de operação
   indica a velocidade realmente alcançada (xN.NN), percentual e backlog.
   Isso é uma otimização de throughput, **não uma comprovação de 8x físico**.

Ver `CHANGELOG_0.83.md` e `TESTES_0.83.md` para detalhes e limites de validação.

## O que mudou na v0.82 (cumulativa)

1. **Slot Disk II durante K7:** montar, criar, rebobinar ou ejetar uma fita
   pelo F3 **não desliga** a interface de disco. A ROM da interface no
   `$C100–$C1FF` e os registradores `$C090–$C09F` ficam indisponíveis
   somente enquanto há operação de cassete; as pausas entre etapas de
   `LOADT`/`LOADA` contam como parte dela. O **fim real do CT2 (100%)**
   devolve a interface automaticamente, sem necessidade de ejetar. Ao
   completar `SAVET`/`SAVEA`, a interface também volta. ROM e imagens
   de discos permanecem montadas o tempo todo.
2. **Teclado:** layout **BR/ABNT2** ativado por padrão no boot. A troca
   BR/US pelo menu F1 continua disponível, inclusive ao vivo.
3. **F3 gravação:** pergunta de arquivo existente oferece `S` (substituir),
   `A` (adicionar faixa CT2) e `N`/`ESC` (cancelar). Append preserva todos
   os registros CA/CB/DA originais, adiciona os novos sem duplicar `CTK2`,
   faz validação integral e salvamento transacional.
4. **Karateka/K7 Turbo:** retomadas as otimizações adicionais descritas
   após v0.81: laços seguros da ROM acelerados também durante leitura,
   até quatro lotes de CPU consecutivos com limite de tempo e cache do
   tipo de bloco/byte de dados CT2 em PSRAM. Pulsos CT2 não foram alterados.
5. **Joystick F4:** preservadas as correções cumulativas de Montezuma
   (`↑/↓` também como `A/Z`) e Moon Patrol (leitura PREAD em `$FB1E`),
   ativadas somente pelos CT2 exatos analisados, conforme
   `CHANGELOG_0.82.md`.

A meta K7 Turbo de **8× permanece dependente de testes de velocidade na
TTGO VGA32**; resultado em computador não equivale a medição física.

## Funcionalidades anteriores mantidas

- F1/F2/F3/F4/F8/F12; Disk II DSK/NIB; Saturn 128K; áudio PCM dos drives;
  vídeo HGR em cores; OSD somente valor EPS; TAB/FIRE e repetição.
- CT2 multietapas, pausa automática e manual e progresso baseado na
  duração dos pulsos, incluindo tons-guia CA/CB.
- Interface de disco preservada na memória, com retirada **temporária**
  durante operações K7 para compatibilidade com Karateka.
- Recuperação de `.bak` válido e proteção contra `.tmp` incompleto.

Veja os históricos `CHANGELOG_0.81.md` a `CHANGELOG_0.86.md`,
`TESTES_0.86.md` e `THIRD_PARTY_NOTICES.md`.

**Validação histórica:** comparadas as formas de onda de três CT2 com a v0.81
(Montezuma, Moon Patrol, Karateka): idênticas; testes C++ de joystick,
rotinas de atraso da ROM e gravação transacional simulada aprovados. Não
foi possível compilar para o ESP32/FabGL nem medir 8× na placa neste
ambiente. ROMs, disquetes e jogos não acompanham esta distribuição mínima.
