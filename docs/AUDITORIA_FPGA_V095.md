# Auditoria geral do FPGA TK2000 II + Saturn 128K — v0.95

**Referência analisada:** arquivo do usuário `tk2000-2e-fpga-rubemf-saturn128k(1).zip` (código VHDL, não incluído na distribuição). Referências anteriores: MAME TK2000, TK2000 Windows 0.3, projetos C17 e Java.

## Decodificação do joystick

No arquivo FPGA `source/tk2000.vhd` há um decodificador de endereços de periféricos. Ao selecionar `A(7 downto 4) = x"6"`, ele habilita `GAMEPORT_SELECT`. No mux de leitura, só `A(2 downto 0)` seleciona o bit do GAMEPORT. Portanto `C060..C067` e `C068..C06F` acessam os **mesmos 8 sinais**.

No emulador v0.95 a função `TKGameportDecode::canonicalGameport()` reduz o endereço espelhado a `C060..C067`, antes de chamar o controlador analógico já aprovado. Não se altera a escala do paddle (`127*11` ciclos no centro) nem a definição de FIRE1 e FIRE2.

## Disparo de paddle

O mesmo FPGA seleciona `PDL_STROBE` quando `A(7 downto 4) = x"7"`, cobrindo `C070..C07F`. Nas rotinas de leitura e escrita do emulador, qualquer endereço dessa faixa agora reativa o temporizador. `$C070` continua funcionando exatamente como antes. Não usar esse strobe como I/O da fita CT2: o caminho da fita fica isolado.

## CONTROL+B e semigráficos

O arquivo `source/tk2000/keyboard.vhd` posiciona a tecla B na linha 0/coluna 1, SHIFT na linha 0/coluna 0 e CONTROL na linha 8/coluna 0, tal qual o emulador v0.94. O código da ROM original retorna `0x82` para CONTROL+B, mas isso não demonstra, isoladamente, que o interpretador tenha entrado em modo gráfico. Não há indicação de erro na matriz que permita justificar troca de B por outra letra, ou injeção de caracteres em lugar da ROM. A falha relatada pelo usuário permanece sob investigação, não é dada como resolvida.

## Compatibilidade com os jogos

Na v0.94 o usuário confirmou Moon Patrol e Montezuma funcionando. A nova revisão preserva exatamente suas rotas de comandos, joystick e teclado. A imagem CT2 do Missiles Defense analisada anteriormente efetua chamadas a `$FB1E` e leituras de `$C010`; por isso alterações no paddle não bastam para inferir melhora no jogo. O deslocamento inicial do Choplifter também não foi reproduzido em teste físico nesta análise. Os testes na placa são imprescindíveis.

## Regressão sem títulos específicos

`test/host/audit_fpga95.cpp` valida o decodificador sem reconhecer nenhuma imagem ou nome de jogo. O modo original de comparação (`TK2000_GAME_JOYSTICK_COMPAT=0`) permanece inalterado e não instala exceções de joystick por programa.
