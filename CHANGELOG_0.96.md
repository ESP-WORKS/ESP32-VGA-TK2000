# TK-2000 TTGO VGA32 / RobGO, v0.96 (cumulativo; para teste físico)

## Revisão 3 (após teste físico da revisão 2)

1. **JOYSTICK ON = joystick Apple conectado também no PREAD.** `JSR $FB1E` devolve o eixo em Y somente com JOYSTICK ON (`TK2000_UNIVERSAL_PREAD=1`, condicionado em tempo real a F4). Com JOYSTICK OFF a ROM roda intacta e Y é preservado. Restaura o modo J (joystick) do Moon Patrol: validado, ↑ pula nos modos K e J.
2. **ESPAÇO volta a ser FIRE1 com JOYSTICK ON**, além da tecla ESPAÇO do TK (comportamento validado em hardware na v0.95 para Choplifter e H.E.R.O.). As setas continuam sem a ponte A/Z.
3. **Montezuma (pedido explícito):** com o CT2 do Montezuma montado (identificado por tamanho+CRC, `TKDetectTapeGameInput`), ↑ e ↓ do cursor, com joystick ON ou OFF, fecham também A e Z. Única regra por título; `TK2000_MONTEZUMA_AZ=0` desliga. Teste `audit_montezuma96.cpp`.
4. **Missiles Defense, ↓ indo na diagonal:** defeito no próprio CT2. Na tabela de teclas do jogo ($5957), a entrada de ↓ (`$F1`) tem deslocamento horizontal `$55` em vez de `$00`; o jogo soma isso à velocidade e limita a +6, gerando a diagonal para a direita em qualquer máquina. Fornecido CT2 corrigido à parte (1 byte e o checksum do bloco). Validado: ↓ passa a dar velocidade X = 0.
5. **CTRL+B / SHIFT de lados diferentes:** não reproduzido. Com CTRL ainda pressionado, SHIFT+letra gera os gráficos do conjunto CTRL+SHIFT (sem bipe), então o bipe indica que o modo gráfico não estava ligado. O trace (`-DTK2000_PS2_CTRL_TRACE=1`) agora registra todos os eventos de CTRL, SHIFT e letras com horário; basta ligar a flag.

A seção abaixo descreve a revisão 2; onde houver conflito, vale a revisão 3.


Base: v0.95. Três correções gerais de entrada, sem nenhuma regra por jogo, nome de CT2, CRC ou endereço de programa. Todas foram reproduzidas e validadas num emulador de testes que compila o código real do firmware e executa a ROM TK2000 original com os CT2/DSK enviados pelo usuário.

## 1. KBIN ($C010) em repouso: bit 7 = 0

KBIN = {entrada do cassete, BUSY da impressora, 6 colunas da matriz} (MiSTer `rtl/tk2000.v`). Sem fita e sem impressora, os dois bits superiores valem 0. O firmware devolvia sempre bit 7 = 1.

O driver de teclado do Choplifter TK detecta "tecla presente" com `LDA $C010 / CMP #$80`. Com bit 7 fixo em 1, o jogo via um botão permanentemente pressionado (na v0.95 a partida começava sozinha). A leitura de fita continua recebendo o nível real do CT2 no bit 7, e o leitor da ROM é independente de polaridade (`EOR $2F`).

## 2. Uma tecla do PC = um único contato do TK

A ROM do TK ($F043, SCAN1) varre as linhas 0 a 7 e devolve **somente o primeiro contato fechado**. A v0.95 fechava dois contatos por tecla:
- JOYSTICK ON, ↑: seta (linha 6) e A (linha 1). O SCAN1 devolvia "A" e o Moon Patrol não pulava.
- ↓: seta (linha 5) e Z (linha 0).
- ESPAÇO: ESPAÇO (linha 2) e FIRE `.` (linha 7). O SCAN1 devolvia ESPAÇO e o disparo `.` era ocultado.

Correção: ponte A/Z/ESPAÇO desligada (`TK2000_JOYSTICK_KEYBOARD_BRIDGE=0`) e ESPAÇO deixa de ser FIRE1. Com JOYSTICK ON: setas e teclado numérico = direções; KP0/Ins = FIRE1 (contato `.`); KP. = FIRE2 (contato `L`); TAB = FIRE; ESPAÇO = tecla ESPAÇO do TK. Igual ao MiSTer e ao emulador Windows 0.3.

Validado: Moon Patrol, modo KEYBOARD, JOYSTICK ON, ↑ ativa o pulo na v0.96; na v0.95, não. Teste de sistema: ↑ ↓ ← → FIRE1 FIRE2 chegam ao SCAN1 como `$F0 $F1 $88 $95 $AE $CC`.

## 3. JSR $FB1E preserva Y

Em `$FB1E` a ROM TK tem o final (`SEC; RTS`) do GETCHAR, não um PREAD. O hook "PREAD universal" passa a 0 (`TK2000_UNIVERSAL_PREAD=0`). Centro do eixo analógico do gameport FPGA: 128.

## Análise dos jogos enviados (sem defeito de emulação)

- **Choplifter (DSK):** a conversão TK substituiu o joystick por um manche virtual em `$079F` (X, 0..$FC, passo 2) e `$07F6` (Y, $30..$7C, passo 4), alterado apenas pelas setas e mantido ao soltar. O valor gravado no disco é `$079F=$00`, que o Choplifter traduz como aceleração máxima para a esquerda (ponto neutro: `$80..$8F`). Por isso o helicóptero decola indo para a esquerda em qualquer máquina. Segurar → por cerca de 4 s leva o manche ao centro.
- **Missiles Defense:** K = modo teclado, P = paddle (inexistente no TK). No modo K as setas dão **velocidade** à mira, que continua andando ao soltar (zerada pela tecla `.` ou na borda). O disparo é feito pelas teclas **1, 2 e 3** (uma por base); ESPAÇO não dispara neste jogo. Confirmado no teste: a velocidade persiste e 1/2 disparam.
- **Moon Patrol:** K = teclado (↑ pula, ← →, `.` atira), J = joystick analógico do Apple via `$FB1E`/`$C061`, não suportado pelo TK original.

## Pendente

- SHIFT esquerdo + letra após CTRL+B gera bipe, enquanto o direito funciona. O tratamento dos dois lados é idêntico no firmware e no FabGL; a causa não foi reproduzida. Diagnóstico: compilar com `-DTK2000_PS2_CTRL_TRACE=1` e enviar o log serial.
- Montezuma com JOYSTICK ON: sem a ponte, se o jogo usar A/Z para escadas, use essas teclas (como num TK real).

## Testes

`test/host/run.sh` completo passa. Com `TK2000_TEST_ROM=/caminho/TK2000.rom`, `audit_system96.cpp` verifica CTRL+B + SHIFT, Y preservado em `JSR $FB1E`, KBIN em repouso = `$00` e os seis contatos do joystick via SCAN1. Com a ponte ligada, o teste falha, como esperado.

## Testes físicos prioritários

1. Moon Patrol, modo K, JOYSTICK ON: ↑ pula, KP0 ou `.` atira.
2. Montezuma, JOYSTICK OFF e ON.
3. Choplifter: com bit 7 corrigido, a partida não deve iniciar sozinha.
4. Missiles Defense: modo K, `.` para a mira, 1/2/3 disparam.
