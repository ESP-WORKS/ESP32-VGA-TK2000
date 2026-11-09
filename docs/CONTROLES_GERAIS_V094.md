# Auditoria de entrada v0.94: causalidade, observabilidade e limites

## Antes — bug de fila

`main.cpp` consumia `CTRL make`, `B make`, `B break`, `CTRL break` até a fila FabGL ficar vazia. Só então executava o 6502. Portanto, a matriz já estava vazia quando o programa emulado consultava `$C010`. Isso explica por que testes que consultavam diretamente a matriz após um MAKE passavam no computador, mesmo com atalhos falhando no BASIC do equipamento.

## Agora — interleaving com orçamento corrigido

A máquina executa uma fatia limitada de ciclos entre eventos quando há MAKE e novos eventos pendentes, desde que não esteja carregando ou gravando fita nem com menu aberto. O número real de ciclos executados entra em `queuedInputCycleDebt`. A atualização normal de `cpuCycleBudget` usa `TKInputEventPacing::payBack`, que desconta a dívida do tempo decorrido, carregando dívida para iterações seguintes. Qualquer reinicialização da contagem de CPU também limpa a dívida. Assim, as teclas aparecem ao programa emulado sem introduzir um segundo relógio para a máquina.

**Limite da solução:** um jogo pode consultar os controles apenas uma vez por quadro de vídeo, ou pode manter seu próprio latch. Uma fatia de 4096 ciclos torna as transições mais observáveis mas não garante capturar uma pulsação curtíssima em qualquer código de jogo.

## Matriz física e ponte universal

O MAME documenta KBIN/KBOUT, mas marca o joystick como incompleto. Windows 0.3 fornece os contatos: linhas 3/4 esquerda/direita, 6/5 cima/baixo, linha 7 bit 4 FIRE1 e linha 6 bit 4 FIRE2. Todos continuam ativos. A ponte acrescenta apenas posições de teclado que diversos jogos usam em lugar dos contatos de joystick: A/Z nas direções verticais e ESPAÇO no FIRE1. Nada depende do software em execução.

**JOYSTICK OFF não injeta contatos da ponte.** Em JOYSTICK ON, a conexão A/Z é deliberadamente uma opção de compatibilidade, não uma característica reivindicada como equivalente ao circuito original; `TK2000_JOYSTICK_KEYBOARD_BRIDGE=0` permite comparar com o hardware estrito. Os velhos perfis particulares permanecem OFF pela opção de build `TK2000_GAME_JOYSTICK_COMPAT=0`.

## CONTROL+B

A matriz ROW0 bit 1 é B, sem SHIFT forçado. A rotina monitor `$F043` da ROM TK-2000 original (fornecida somente ao teste) retorna `$82` para CONTROL+B. O BASIC usa esse código para os caracteres gráficos, porém somente um teste de execução real no hardware pode validar a funcionalidade completa. A antiga referência textual a CONTROL+R no documento digitalizado foi uma interpretação incorreta; o comando correto informado pelo usuário é CONTROL+B.

## Regressões / limitações

GCC e Clang: `test/host/run.sh`. O teste antigo de joystick estrito permanece compilado com `TK2000_JOYSTICK_KEYBOARD_BRIDGE=0` para comparação com Windows/MAME; o novo teste roda com a ponte padrão ligada. Os testes não incluem VGA real, inicialização de jogos, CT2 de Choplifter ou compilação Xtensa. Não extrapolar testes de matriz para afirmar compatibilidade física.
