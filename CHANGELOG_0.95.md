# TK-2000 TTGO VGA32 / RobGO — v0.95 (cumulativo; para teste físico)

Base: v0.94, na qual o usuário validou **Moon Patrol e Montezuma**.

## Diferenças concretas encontradas no código FPGA TK2000 II / Saturn 128K

- `source/tk2000.vhd`, decoder `GAMEPORT_SELECT`, recebe **toda a faixa `$C060–$C06F`**, e a entrada do multiplexador depende somente de `A(2 downto 0)`. A v0.94 reconhecia apenas `$C061–$C067` e não os espelhos `$C068–$C06F`. A v0.95 corrige o espelhamento para todos os programas.
- `source/tk2000.vhd`, decoder `PDL_STROBE`, recebe **toda a faixa `$C070–$C07F`**. A v0.94 acionava o temporizador somente em `$C070` e retornava valor flutuante em `$C071`. A v0.95 dispara o temporizador nas dezesseis posições, em leitura ou escrita.
- `source/tk2000/keyboard.vhd` confirma B na matriz **linha 0 coluna 1**, SHIFT na linha 0 coluna 0, CONTROL em uma linha independente. A v0.94 já possuía essas três posições corretamente. Não foram introduzidos remapeamentos ou caracteres sintéticos.

## Preservação e limites

- Preservados todos os mapeamentos v0.94, incluindo ponte geral A/Z/ESPAÇO, janela de ciclos PS/2, PREAD universal, joystick neutro, vídeo VGA sem chiado, menus, carga/salvamento CT2, Cassette K7 Turbo, remoção do OSD de velocidade de Moon Patrol e Multiloader.
- Sem detecção de cartuchos, nomes de CT2, endereços de código dos jogos ou adaptações específicas para Choplifter e Missiles Defense.
- A FPGA é uma implementação variante TK2000 II, com expansão Saturn 128K. O espelhamento é um comportamento de **seu decodificador**; sua existência no equipamento original de todos os modelos TK2000 não foi comprovada independentemente.
- CONTROL+B: varredura com ROM TK2000 retorna `$82`, como na versão anterior. Ainda não foi possível provar no teste de integração que a rotina BASIC altera o estado de caracteres gráficos. Por isso esta versão **não afirma ter corrigido o modo gráfico**; é necessário teste físico e, se falhar, rastrear a rotina completa da ROM.
- Os problemas de movimento inicial do Choplifter e disparo/mira do Missiles Defense **não foram comprovadamente solucionados**. Espelhos MMIO são correções gerais justificadas pela nova referência, mas apenas teste físico dirá se esses jogos dependem deles.

## Testes

`test/host/run.sh` testa as regressões desde v0.88, teclado e joystick em dois modos, PREAD com 6502, e o novo `audit_fpga95.cpp` para os 16 espelhos de `$C06x`, 16 strobes de `$C07x`, temporização analógica, FIRE1/FIRE2, controle neutro OFF e CONTROL+B. Com uma ROM TK2000 obtida separadamente, executar `TK2000_TEST_ROM=/caminho/TK2000.rom test/host/run.sh` para testar a rotina ROM `$F043`. ROM, jogos e CT2 não estão incluídos no ZIP.
