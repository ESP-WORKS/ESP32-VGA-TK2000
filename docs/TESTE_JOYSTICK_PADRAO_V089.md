# Roteiro de teste: joystick padrao v0.89

1. Compile normalmente com `-DTK2000_GAME_JOYSTICK_COMPAT=0`.
2. Entre no BASIC; valide CONTROL esquerdo/direito + letras A..Z, teclado US/BR,
   NumLock ON/OFF, SHIFT, TAB (fire fisico) e F8 REPEAT.
3. Ative JOYSTICK ON (F4). Teste cursores, 1/3/7/9 do numpad (diagonais),
   5 neutro, 0/ESPAÇO (FIRE1) e decimal (FIRE2). Solte teclas em ordens diferentes.
4. Observe os paddles em um programa ou jogo que leia `$C064/$C065` apos `$C070`:
   UP e LEFT retornam extremo, soltura deve voltar ao centro (127).
5. Abra F1/F2/F3 e feche durante o jogo, alterne F4, mude fita CT2 e ejetar.
   Nenhuma tecla deve permanecer fisicamente pressionada.
6. Rode Montezuma, Moon Patrol e Missiles Defense. Registre as diferencas com
   o controle padrao; **nao** espere as compatibilidades particulares v0.88.
7. Para repetir com os atalhos antigos, mude somente
   `-DTK2000_GAME_JOYSTICK_COMPAT=1`, recompile e execute o mesmo roteiro.
8. Teste nas placas TTGO VGA32 e RobGO; nao pressuponha resultados iguais.

Notas: o controle K7 Turbo para Moon Patrol nao e desativado; a medicao K7 VGA
continua separada do perfil de joystick. Compile e valide fisicamente em ambas
as placas antes de aprovar esta variante.
