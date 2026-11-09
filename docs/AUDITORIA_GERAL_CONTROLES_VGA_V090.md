# Auditoria complementar — Windows TK-2000 0.3 x RobGO v0.90

## Evidências do código de referência

- `tk_joystick.cpp`: `JoyProcessKey()` agrupa 9 direções/diagonais de um
  teclado numérico e oferece dois comportamentos (STANDARD/CENTERING).
  `JoyVerificaTecla()` envia a matriz de joystick: cima linha 6 bit 0,
  baixo linha 5 bit 0, direita linha 4 bit 0, esquerda linha 3 bit 0,
  botão 1 linha 7 bit 4 e botão 2 linha 6 bit 4. Os limiares de eixo são
  menos que 30 e maiores que 225, com centro 127.
- `tk_teclado.cpp`: `KeybKBIN()` combina teclado com `JoyVerificaTecla()`;
  há captura de teclas e conversões de símbolos. O emulador Windows 0.3
  não traz uma emulação equivalente da rotina PREAD para jogos Apple II.
- `Outros/TK2000.rom`: bytes em `$FB1E` são `38 60` (SEC/RTS), fato
  confirmado na ROM da referência (apenas examinada, não redistribuída).

## Resultados e limites

- As linhas nativas de joystick no ESP32 já correspondiam às da referência.
  Portanto, trocar as linhas ou inverter a esquerda seria regressão.
- O problema mais concreto de KEYUP é a mudança de alias do teclado
  numérico quando NumLock muda durante uma pressão; agora há um owner único
  para cada tecla do teclado numérico. O joystick sempre retorna a zero
  após os últimos KEYUPs, sem cache direcional residual.
- PREAD universal foi adicionado com verificação da chamada 6502, sem
  associar qualquer comportamento ao nome de Chopflifter, Montezuma ou
  Missiles Defense. Com joystick desligado retorna coordenada neutra.
- SPACE físico tem dois efeitos quando joystick está ligado: teclado SPACE
  e botão FIRE1. O teclado numérico zero aciona só FIRE1. Essa duplicidade
  é intencional e tem testes de liberação independentes.
- Um programa que lê exclusivamente A/Z, em vez dos contatos nativos de
  joystick ou da rotina PREAD, **não será automaticamente corrigido** pelas
  mudanças universais; converter toda seta em A/Z mudaria o funcionamento
  dos outros programas e não seria fiel ao TK2000 de referência.

## Chiado de VGA

A v0.89 já escrevia diretamente no buffer de varredura, com linhas HGR
calculadas previamente. A v0.90 reduz alterações parciais usando stores
32-bit alinhadas de grupos de quatro pixels e conservando o mesmo padrão
RGB e os mesmos bits de sincronismo por pixel. O método não implementa
um segundo framebuffer, que exigiria mais 128 KiB de RAM DMA interna em
640x200, e poderia impedir o boot ou desestabilizar a TTGO/RobGO.

Para distinguir software de hardware: observe uma tela estática do BASIC e
uma tela de jogo em movimento, com a mesma fonte/cabo/monitor. Se o chiado
persistir parado, examine primeiro cabo VGA, alimentação e sincronismo.
Se ocorrer apenas ao mudar pixels, a hipótese de escrita durante DMA ganha
força. **Não há confirmação física de eliminação do ruído nesta entrega.**

## Roteiro de verificação física (sem monitor serial)

1. F12, BASIC estático: observar se existe chiado sem atualizações HGR.
2. Com JOYSTICK OFF, Chopflifter: verificar se inicia neutro; testar as
   quatro setas e a barra de espaço conforme os comandos do próprio jogo.
3. JOYSTICK ON, Chopflifter: verificar direção e parada ao soltar.
4. Montezuma: JOYSTICK OFF (controle validado pelo usuário); depois ON,
   comparar setas, A/Z e ESPAÇO. Não há perfil de exceção habilitado.
5. Missiles Defense: experimentar JOYSTICK ON, soltar setas e disparar por
   ESPAÇO físico e 0 do teclado numérico separadamente; testar também OFF.
6. Digitar CTRL+A..Z, abrir/fechar menus F1/F2/F3, alternar F4, NumLock
   e diagonais sem nenhuma tecla ficar presa.
7. Repetir em RobGO e TTGO. Se precisar comparar modos analógicos,
   modificar somente `TK2000_JOYSTICK_STANDARD_MODE` no `platformio.ini`.

Não foram feitas modificações específicas por jogo em v0.90.
