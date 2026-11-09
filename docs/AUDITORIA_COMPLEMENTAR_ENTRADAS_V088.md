# TK-2000 RobGO/TTGO — auditoria complementar v0.88

Data: 04/11/2026. Base cumulativa **v0.87 RobGO/PlatformIO**, NÃO o projeto antigo Arduino `.ino`. Referência fornecida pelo usuário: `tk2000-emulator-master-0.3.zip` (Windows), especialmente `src/tk_joystick.cpp`, `src/tk_teclado.cpp` e `src/tk_memoria.cpp`.

## Diagnóstico e correções efetivamente aplicadas

### Alta: comportamento analógico Windows STANDARD/CENTERING

A função Windows 0.3 `JoyProcessKey()` atualiza as coordenadas com `MODE_CENTERING` a cada KEYDOWN/KEYUP. Em `MODE_STANDARD`, recalcula apenas em um novo KEYDOWN (não autorepeat), preservando a última posição depois da soltura. A v0.87 implementava apenas a centralização normal e um ponteiro próprio para Missiles.

A classe `TKPaddleJoystick` agora implementa explicitamente os modos `Centering` (padrão; preserva o Moon Patrol já aprovado) e `Standard` (coordenada retida após KEYUP). `TKMachine::syncTapeInputProfile()` escolhe `Standard` para o CT2 específico de Missiles Defense e `Centering` para o restante, sem novas opções de menu e sem alterar o emulador de joystick original do TK. Leituras de `$C064/$C065` e a rotina PREAD de Missiles passam a usar o **mesmo ponteiro absoluto**. O ponteiro é atualizado ao longo dos ciclos da CPU, e o intervalo antes de um KEYDOWN é contabilizado com a direção anterior para evitar saltos artificiais.

**Limitação:** o comportamento final dentro de Missiles Defense continua pendente de teste no hardware; o reconhecimento permanece restrito ao CT2 identificado (9975 bytes, CRC32 `43B19501`). Não se pode deduzir compatibilidade com variantes desconhecidas sem seus arquivos.

### Alta: soltura de teclas, estado PS/2 e teclado simultâneo

`TKHostKeyOwners.h` mantém uma lista fixa de VKs físicos ativos e combina fontes distintas que enviam o mesmo código guest (ex.: tecla `1` principal + numpad `1`; BACKSPACE + LEFT no modo teclado). A matriz recebe somente transições da primeira pressão e última liberação. Eventos de autorepeat não adicionam novas pressões.

Um polling aproximadamente a cada 12 ms passa a **liberar** teclas guest, CTRL esquerdo/direito, SHIFT esquerdo/direito, TAB/FIRE e F8/REPEAT cujo KEYUP tenha sido perdido pela *fila* de eventos FabGL; somente eventos PS/2 reais podem criar novas pressões. O polling respeita aliases de letras maiúsculas/minúsculas e pares básicos de pontuação que mudam quando SHIFT é pressionado. A garantia depende de o estado físico `isVKDown()` ter recebido a soltura; se o próprio controlador PS/2 nunca receber o break, o firmware não pode inferir fisicamente que a tecla foi liberada.

Ao abrir/fechar F1/F2/F3, mudar F4/BR-US ou reiniciar o console, o novo registro de fontes físicas também é esvaziado junto com a matriz guest. Os registradores selecionados por software ($C000 e latch CTRL) e os perfis do CT2 permanecem preservados conforme a correção v0.87.

### Média: diagonais, neutro, 2 botões e NumLock

O `TKJoystickInput` v0.87 já implementava diagonais (numpad 1/3/7/9), numpad com NumLock ON/OFF, aliases simultâneos, FIRE1 (ESPAÇO/num0) e FIRE2 (numpad decimal), com os contatos originais da referência Windows 0.3. A v0.88 corrigiu uma lacuna: **numpad 5**, centro/neutro na referência Windows, estava vazando como o caractere `5` para o BASIC mesmo com JOYSTICK ON. Agora é consumido como controle neutro, sem injetar código TK.

`VK_KP_PLUS`, `MINUS`, `MULTIPLY`, `DIVIDE` e decimal continuam disponíveis para tradução alfanumérica quando JOYSTICK OFF. Com JOYSTICK ON, decimal é botão 2, conforme Windows; os operadores não são direções.

### Média: BR/US, símbolos e atalho BASIC

Conferidos em `KeyboardTK::mapKey()` e `main.cpp::translate()` os contatos da matriz para letras, SHIFT+CTRL+A..Z (mantendo a correção **já aprovada**), RETURN, BACKSPACE, cursores, pontuação e operadores; incluídos testes para 23 símbolos/contatos, supressão de Shift especial para `?` e `:` e liberação de SHIFT físico. O ABNT2 continua usando o layout PS/2 próprio; a ROM TK não tem contato nativo para cedilha nem permite formar letras acentuadas por composição, então **não** foram inventadas teclas nem novos tokens. Teclas sem equivalente genuíno continuam ignoradas.

**Limitação:** não houve um teclado PS/2 BR/US físico para conferir todas as teclas e marcas de teclado. A fidelidade de `VK` derivado de scancode por FabGL 1.0.9 exige testes na placa.

### Média: perfis de jogo, menus e CT2

Mantidas as correções da v0.87: perfis não somem ao entrar/sair de menus; seleção/ejeção de fita atualiza a matriz e agora também o modo do paddle analógico. No reset quente/frio, STANDARD retorna a um centro neutro, sem reter coordenadas antigas entre execuções. Dois botões independentes, SPACE físico e SPACE virtual combinados apenas na matriz, sem dupla injeção.

Os perfis são identificados conservadoramente por CRC do CT2 inteiro. Não foram criadas assinaturas abrangentes para jogos diferentes com a mesma rotina, pois isso poderia ativar um desvio 6502 indevido e causar regressões. Para ampliar o suporte a variantes, são necessários seus CT2 originais.

### RobGO e TTGO

O PlatformIO (`platformio.ini`), tabela de partições MultiLoader e FabGL 1.0.9 **não foram alterados** nesta versão; também não foram alterados mapeamentos físicos de GPIO, VGA, SD, áudio ou partições. O código segue o mesmo hardware da base RobGO oficial entregue pelo usuário; validação separada de PS/2 em cada placa ainda é necessária.

## Testes de computador executados

- `sh test/host/run.sh` — C++11 `-O2 -Wall -Wextra -Werror`: **PASS**.
- Recompilação com AddressSanitizer+UndefinedBehaviorSanitizer: **PASS**.
- Casos: 26 letras CTRL+A..Z, lados CTRL independentes, perfis Missiles/Montezuma, FIRE físico+virtual, 2 botões, eixos contrários, diagonais, evento NumLock, numpad 5 neutro, dois códigos físicos com mesmo contato, autorepeat, KEYUP ausente, alternância de modos de paddle e retenção STANDARD, posição PREAD/`$C064`, símbolos BASIC e 5.000 regiões VGA sem diferença de pixel da v0.87.
- CT2 fornecidos: Missiles Defense 9.975 bytes/CRC `43B19501`, Moon Patrol 35.547 bytes/CRC `C28A4F8D`.

**Não executados:** compilação completa ESP32/PlatformIO/FabGL (não instalados neste ambiente), nem teste físico RobGO/TTGO. Resultados host não garantem correção de Missiles, compatibilidade RobGO ou ausência de falhas PS/2 em hardware.

## Roteiro de validação na placa

1. **JOYSTICK OFF**: testar BASIC com `1` da fileira + `1` numpad pressionados juntos, soltando cada um em ordem inversa; repetir com BACKSPACE e seta esquerda; `?`, `:`, `;`, `@`, `^`, aspas, operadores e SHIFT BR/US. Testar SHIFT esquerdo/direito + CONTROL+A..Z e F8 REPEAT.
2. **JOYSTICK ON**: setas, numpad 1/3/7/9, 5 neutro (não deve digitar 5), NumLock ON/OFF, SPACE/0 e decimal (dois botões), combinações e liberações das setas.
3. Trocar **F4** repetidamente e entrar/sair de **F1/F2/F3** com teclas previamente pressionadas. Não pode surgir movimento travado, caractere residual ou retorno das piscadas dos menus.
4. Montar o CT2 de **Missiles Defense**, mover a mira em cada eixo e soltar; a mira deve ficar parada no último ponto, e SPACE deve disparar. Tentar com abertura/fechamento de F1/F3; repetir após RESET.
5. Regressão **Moon Patrol** (cursores com JOYSTICK ON), **Montezuma** (cursores e SPACE), CT2 multifaixa, menus sem piscadas, compatibilidade MultiLoader e VGA. Comparar nos dois hardwares.

Nenhuma nova otimização de Moon Patrol K7 ou novo diagnóstico de ruído de vídeo foi introduzida pela v0.88; estes são trabalhos separados.
