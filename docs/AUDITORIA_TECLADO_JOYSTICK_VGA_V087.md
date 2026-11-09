# Auditoria técnica do TK-2000 — teclado, joystick e VGA (v0.87)

## Fontes efetivamente examinadas

- Nova base ESP32/RobGO fornecida: `src/Keyboard.cpp/.h`, `JoystickKeys.h`,
  `JoystickInput.h`, `PaddleJoystick.h`, `TKMissilesAim.h`, `main.cpp`,
  `Machine.cpp/.h`, `Cassette.cpp/.h`, `VGA.cpp/.h`, `TKVideoDirty.h`.
- Referência Windows 0.3 fornecida: `src/tk_teclado.cpp`, `src/tk_joystick.cpp`,
  `src/tk_memoria.cpp`, `src/tk_janela.cpp` e `src/tk_video.cpp`.
- CT2 originais entregues pelo usuário: Moon Patrol (35547 bytes, CRC32
  C28A4F8D) e Missiles Defense (9975 bytes, CRC32 43B19501).

**Escopo:** comparar sem copiar dependências Win32/DirectDraw. O emulador Windows
aceita joystick de PC e mouse além do teclado; nesta base ESP32 o dispositivo
virtual continua sendo o PS/2 (setas/teclado numérico). Não foi acrescentado
um joystick analógico físico à placa.

## 1. Matriz, CONTROL, SHIFT e teclas

| Item | Windows 0.3 | ESP32 base de entrada | v0.87 |
|---|---|---|---|
| Teclado TK | Matriz `matriz[8]`; colunas 0..5 em `KeybKBIN` | Matriz 8x6 em `KeyboardTK::in()` | Mantida |
| Leitura | `bKBOUT` e bit 7 em KBIN | `$C000/$C010` e bit 7 | Mantida |
| CONTROL | Chave seletora `$C05E/$C05F` | SHIFT virtual para tokens CTRL+A..Z | Preservado o atalho já testado |
| SHIFT | Linha 0, bit 0 | Shift físico, forçado ou sintético | Preservado |
| Letras | A..Z na mesma matriz | `VK_a..z` e `VK_A..Z` mapeadas | Preservado |
| Cursores | cima 0B, baixo 0A, direita 15, esquerda 08 | Mesmos contatos | Preservado |
| Símbolos | `Conversoes[]` por teclado Windows | `translate()` e `mapKey()` | Numpad + - * / e vírgula acrescentados |
| REPEAT | Não reproduzido como tecla F8 independente no trecho Win32 analisado | F8 gera pulsos de contato | Preservado |
| BR/US | Usa layout configurado no Windows | FabGL ABNT2/US por F1 | Preservado |

O Windows 0.3 libera modificadores por `GetKeyState` e usa uma conversão por
tecla; a implementação ESP32 conserva pressionar/soltar explicitamente para
ambos os lados de SHIFT/CONTROL. Uma diferença deliberada é que CTRL+A–Z
insere tokens BASIC no firmware ESP32 ao sintetizar SHIFT conforme a ROM;
esse comportamento já tinha aprovação física do usuário.

## 2. Joystick: matriz original e referência Windows

`JoyVerificaTecla()` em `tk_joystick.cpp` estabelece:

| Elemento | Leitura TK-2000 | Versão ESP32 v0.87 |
|---|---|---|
| UP | linha 6, bit 0x01 | UP / keypad 8, diagonais superiores |
| DOWN | linha 5, bit 0x01 | DOWN / keypad 2, diagonais inferiores |
| LEFT | linha 3, bit 0x01 | LEFT / keypad 4, diagonais esquerdas |
| RIGHT | linha 4, bit 0x01 | RIGHT / keypad 6, diagonais direitas |
| Botão 1 | linha 7, bit 0x10 | SPACE / keypad 0; TAB continua FIRE físico |
| Botão 2 | linha 6, bit 0x10 | keypad decimal (VK_KP_PERIOD/DELETE) |

No Windows 0.3, `JoyProcessKey()` aceita os nove pontos do teclado numérico
(1..9), calcula posição média para conflitos e possui modos STANDARD/CENTERING.
No ESP32, keypad 1/3/7/9 agora aciona **dois contatos** simultaneamente, tanto
com NumLock ON quanto OFF, usando os aliases FabGL. O keypad 5 é neutro.
O ESP32 cancela direções opostas do mesmo eixo; não habilita dois contatos
opostos ao mesmo tempo. Isso evita movimento involuntário em jogos que dão
prioridade a um lado e se aproxima do eixo centrado na referência.

A implementação de botão 2 alcança tanto a matriz (`linha 6/bit 0x10`)
quanto o botão analógico alternativo (`$C062`). O botão 1 continua em `$C061`.
Ambos são independentes; botão 2 não substitui SPACE/TAB.

## 3. Causa encontrada: perfil perdido ao abrir menu/F4

Na base entregue, `KeyboardTK::releaseInputs()` chamava `clear()`, que
zerava `gameInputProfile` para `Standard`. Os caminhos de F1/F2/F3 e
`toggleJoystick()` chamam `releaseInputs()` para esvaziar teclas PS/2.
Por isso, uma correção restrita a um CT2 podia funcionar em um teste
unitário e desaparecer durante o uso real ao abrir F1 ou ativar F4.

**Correção:** preservar `gameInputProfile` em `releaseInputs()` da mesma forma
que os registradores `$C000` e `$C05E/$C05F`; aplicar o perfil retornado por
`CassetteCT2::gameInputProfile()` na seleção de cada fita válida; retornar a
`Standard` ao ejetá-la. `clear()` completo continua resetando o perfil durante
reset de máquina normal, quando `Machine::reset()` o reconfigura.

## 4. Causa encontrada: espaço físico vs botão virtual

Na versão anterior, `setJoystickMask()` para Missiles fazia
`KeyboardTK::key(' ',true/false)`. O `held[' ']` era compartilhado por essa
inserção e por eventos reais de tecla SPACE. A liberação por qualquer um dos
dois caminhos podia apagar a pressão do outro — típico sintoma de comando
que falha ou parece retido por um buffer.

**Correção:** remover essa inserção sintética. Na leitura KBIN, combinar
logicamente a tecla física com o botão 1 do joystick somente para o perfil
Missiles. Essa composição já era realizada por `KeyboardTK::in()`; assim, as
duas origens permanecem independentes. Montezuma continua com seu contato
SPACE próprio, e botão 1 na linha 7 permanece universal no joystick ON.

## 5. Eventos PS/2 e autorepetição

`TKJoystickInput` cria pressões apenas ao receber **KEYDOWN** via fila FabGL.
O `isVKDown()` é empregado de tempos em tempos só para **soltar** estados
cujo KEYUP foi perdido. Isso impede que um mapa de VKs obsoleto reative uma
direção fantasma. Foi estendido para diagonais e segundo botão. Não foi
introduzido buffer de repetição de botões. As teclas F1/F2/F3/F4/F12 mantêm
seus travamentos contra auto-repeat e os menus continuam com redesenho parcial.

## 6. Joystick analógico Apple/PREAD

O Windows 0.3 utiliza principalmente `JoyVerificaTecla()` sobre a matriz
original; o ESP32 mantém também `$C061..$C067/$C070` para software que espera
paddles Apple II e os shims restritos à identidade exata de CT2 em Moon Patrol
e Missiles. O eixo livre retorna 127, sem forçar esquerda. O segundo botão
passou a ser reportado em `$C062` como um complemento conservador.

A compatibilidade física de Missiles Defense ainda deve ser revalidada na
TTGO/RobGO; teste C++ do cursor e da matriz não substitui o jogo real.

## 7. Inspeção da geração de vídeo e hipótese do "chiado"

A base usa `VGA_640x200_70Hz`, paleta RGB222 e scanlines da FabGL.
A rotina HGR já constrói cada linha fora da área visível antes de um `memcpy`
único de 560 bytes, mas `TKVGA::rect()` realizava o desenho de áreas grandes
com **um `setRawPixel()` por pixel** (até 128.000 acessos para 640x200).
Como o controlador VGA transmite por DMA no mesmo framebuffer, esse caminho
estende a janela em que uma linha pode ser lida durante uma atualização,
aparentando ruído horizontal ou "snow".

**Mitigação implementada:** helper `TKVGAFillPackedSpan()` respeitando a
ordem crua de FabGL [2,3,0,1] por bloco de quatro pixels. Partes internas
são atualizadas em lote com `memset`, e bordas desalinhadas com poucos bytes.
Não muda cor, sincronismo, frequência, dimensão, HGR ou layout dos menus.
O teste comparou 5.000 faixas randômicas com o algoritmo de pixel individual,
sem divergências.

**Limitação honesta:** o framebuffer continua de buffer único, pois dois
quadros inteiros 640x200 consumiriam memória interna adicional importante no
ESP32. Portanto, não é possível afirmar que a alteração elimine todo tearing.
Chiado elétrico, cabo, aterramento, resistores VGA ou interferência de DAC
não pode ser diagnosticado só pelo código. Confirmar nas duas placas.

## 8. PlatformIO e preservação do MultiLoader

O arquivo `platformio.ini` da base RobGO já existia, mas a tabela
`partitions.csv` não estava configurada explicitamente no PlatformIO.
Incluído `board_build.partitions = partitions.csv` para utilizar o mapa
factory/ota_0 original. A dependência FabGL passou de branch móvel do Git
para tag `v1.0.9`. O projeto permanece usando `board = pico32` como entregue,
sem presumir GPIOs RobGO diferentes dos da base. Compilar **não autoriza
sobrescrever o bootloader** e o procedimento de upload deve ser conferido na placa.

## 9. Testes executados / pendentes

- **Passaram no host:** CTRL+letras, MOD CTRL esquerdo/direito, retenção do
  perfil ao liberar teclado, SPACE físico+virtual independentes, perfis
  Montezuma/Missiles, matriz de 2 botões, direções opostas, diagonais e
  eventos numpad, soltura de teclas, botão analógico 1/2, mira persistente
  de Missiles, **5.000 faixas VGA** equivalentes byte a byte.
- **Arquivos CT2 inspecionados:** dois CRCs conferem com perfis codificados.
- **Pendentes:** build completo PlatformIO/FabGL (não instalado no host),
  funcionamento físico VGA/PS2 nas placas TTGO/RobGO, verificação de
  Missiles, regressões em jogos e eventual chiado elétrico residual.

## 10. Plano recomendado de verificação na placa

1. Abrir o BASIC e conferir letras, SHIFT, CONTROL+A–Z, pontuação BR/US,
   REPEAT, RETURN, setas com joystick OFF.
2. Ligar JOYSTICK ON por F4; verificar as quatro setas, diagonais 1/3/7/9,
   FIRE1 por SPACE/0 e FIRE2 por keypad decimal; desligar e testar neutralidade.
3. Montar Missiles Defense CT2; após abrir F1/F3 e sair, confirmar joystick,
   mira que para ao soltar setas e disparo por SPACE.
4. Montar Montezuma e Moon Patrol para garantir não regressão dos controles.
5. Testar BASIC e gráficos HGR, menus F1/F2/F3 e transições de tela, observar
   se o ruído aparece só durante atualizações ou também numa tela estática.
6. Repetir vídeo/teclado em RobGO e TTGO e reportar qualquer diferença.
