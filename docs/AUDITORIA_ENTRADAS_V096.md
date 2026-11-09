# Auditoria de teclado e joystick, v0.96

## Método

Os problemas foram reproduzidos num emulador de testes no computador que compila o código real do firmware (`TKMachine`, `CPU6502`, `KeyboardTK`, `CassetteCT2`, `TKPaddleJoystick`) com stubs mínimos de Arduino/SD, executando a ROM TK2000 original de 16 KiB. A ROM não acompanha o ZIP; o teste a recebe por `TK2000_TEST_ROM`. Nesse ambiente o BASIC inicia, o LOADT carrega CT2 reais e a tela HGR pode ser inspecionada.

Observação: a cópia `TK2000.rom` distribuída com o emulador Windows 0.3 é modificada (lê a fita por `$C070/$C071`). A ROM original lê a fita por `$C010` bit 7, como o firmware espera.

## Referências de hardware

- MiSTer `TK2000.sv` (linhas 480 a 491): joystick ligado à matriz do teclado; direções nos contatos das setas, botões nos contatos de `.` e `L`. `rtl/tk2000.v`: `$C060–$C07F` sem circuito; KBIN = `{cassete, impressora ocupada, colunas}`.
- Emulador Windows 0.3, `JoyVerificaTecla`: mesmas posições da matriz; `$C060–$C06F` sem função.
- ROM TK2000: `$FB02..$FB1F` é GETCHAR (espera de tecla com 5 leituras de confirmação e conversão pela tabela TABASC). `$FB1E/$FB1F` (`SEC; RTS`) são o fim dessa rotina.
- Choplifter (Apple II, engenharia reversa publicada): `handleJoystickX` faz `LDX eixo; JSR $FB1E; TYA; EOR #$FF; LSR x4; SBC #2; ...; SBC #5`. Zona neutra de Y: `$80..$8F`.

## Diagnóstico

| Sintoma | Causa | Correção |
|---|---|---|
| JSR $FB1E alterava Y (hipótese inicial para Choplifter/Missiles; a revisão 2 mostrou que esses jogos não usam esse caminho) | Hook "PREAD universal" sobrescrevia Y=127 | Hook desligado por padrão: Y preservado como no TK real |
| Gameport FPGA lido diretamente, eixo solto | Centro em 127, um passo abaixo de zonas que começam em `$80` | Centro 128 |
| CONTROL+B | Núcleo correto: CTRL+B alterna `$49`; semigráficos exigem SHIFT+tecla | Sem alteração de código; teste de sistema adicionado |

O disparo do Missiles Defense não foi reproduzido sem a fita. Se o jogo combinava a leitura do disparo com o valor devolvido em Y, a correção acima também o afeta; caso contrário, testar ESPAÇO, `.`, TAB e teclado numérico 0 e relatar qual tecla o jogo espera.

## Riscos

Moon Patrol chama `JSR $FB1E` (endereço `$0766`). No TK real isso também preserva Y, então a versão para TK deve funcionar sem o hook; ainda assim é a principal regressão a testar. A opção `-DTK2000_UNIVERSAL_PREAD=1` restaura o comportamento antigo para comparação.

## Revisão 2 (com os jogos do usuário)

Após o teste físico da primeira revisão, os CT2/DSK de Choplifter, Missiles Defense e Moon Patrol foram executados no emulador de testes (DiskImage real com shim de SD). Duas causas adicionais foram comprovadas e corrigidas: KBIN em repouso com bit 7 = 1 e a ponte que fechava dois contatos por tecla. Detalhes, medições e a análise do comportamento próprio de cada jogo estão em `CHANGELOG_0.96.md`.
