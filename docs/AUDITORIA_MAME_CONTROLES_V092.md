# Auditoria complementar do TK-2000: MAME versus Windows 0.3

## Matriz do teclado

- MAME `tk2000_state::kbout_w` grava seleção de linhas em $C000.
- MAME `tk2000_state::kbin_r` OR das ROW0..ROW7 selecionadas por KBOUT, OR da ROW8 (CONTROL físico), se a saída 7 de softlatch está ativa. O retorno ainda pode conter impressora ocupada bit6 e sinal do cassete bit7.
- O código anterior devolvia `0x81` antecipadamente quando CONTROL pressionado e latch selecionado, omitindo a matriz. A v0.92 usa OR de CTRL sem cortar as demais linhas. O bit 7 e o cassete permanecem com comportamento da v0.91 por preservação da lógica CT2 já validada.
- A prioridade usada pelo emulador Windows 0.3 é diferente; para o circuito digital, foi escolhida a combinação explícita do MAME. Isso deve ser verificado nos atalhos CTRL do BASIC.

## Joystick

- O MAME marca `TODO: emulate joystick port (wired to KBIN and KBOUT)`; ele não contém um joystick completo para transplante.
- O Windows 0.3 implementa `JoyVerificaTecla`: x <30 => ROW3 bit0 (esquerda); x >225 => ROW4 bit0 (direita); y <30 => ROW6 bit0 (cima); y >225 => ROW5 bit0 (baixo); FIRE1 => ROW7 bit4; FIRE2 => ROW6 bit4.
- A v0.92 centraliza esses contatos em `TKJoystickMatrix.h`, com eixos em 0/127/255 para setas de teclado, sem patches de jogos na configuração padrão.
- Cuidado: controles combinados de teclado e joystick dependem do programa que está rodando. Jogos que exigem especificamente teclas A/Z ou portas analógicas Apple II não se tornam automaticamente compatíveis com o joystick TK original. Não foram adicionados remapeamentos por título.

## OSD

- O indicador K7 específico Moon Patrol foi removido da margem direita, inclusive cache de caracteres, chamada e estado. EPS geral e status JOY seguem disponíveis.

## Verificação no hardware

1. No BASIC: CONTROL+G (beep), CONTROL+X, CONTROL+B; durante LIST, CONTROL+S seguido de CONTROL+Q; interromper programa com CONTROL+C.
2. Confirmar que SHIFT+CONTROL+P ainda produz PRINT.
3. Chopflifter: testar partida com JOYSTICK OFF sem pressionar nada; ON testar esquerda/direita após soltar.
4. Montezuma: OFF e ON, cursores e ESPAÇO; anotar se o jogo espera teclas A/Z em vez de joystick KBIN.
5. Missiles Defense: FIRE TAB, SPACE com ON e OFF, posição da mira depois de soltar setas.
6. Confirmar que não há mais OSD K7 do Moon Patrol; confirmar ausência de chiado VGA, menus sem piscadas e salvamento CT2 multifaixa.

## Limitação

Sem teste físico, os itens 1, 3, 4 e 5 ainda não estão comprovados. Não confundir passagem dos testes da matriz no host com compatibilidade completa dos jogos.
