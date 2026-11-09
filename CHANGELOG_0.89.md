# TK-2000 RobGO / TTGO — v0.89 (modo joystick padrao / A-B)

Base: **v0.88**, no projeto oficial RobGO/TTGO PlatformIO. Esta versao e uma
**variante de teste**, nao a declaracao de que todos os jogos agora funcionam.

## Mudanca principal

No `platformio.ini`, o build define `-DTK2000_GAME_JOYSTICK_COMPAT=0`.
Com este valor:

- JOYSTICK ON continua usando a matriz original do TK-2000 e paddles
  CENTERING (`$C061`/`$C062`, `$C064`/`$C065`, `$C070`).
- Cursores, diagonais do numpad, teclas FIRE1 (ESPAÇO / KP0), FIRE2
  (decimal), TAB (botao fisico), NumLock e liberacao de KEYUP permanecem.
- Os perfis especificos Montezuma/Moon Patrol/Missiles Defense sao ignorados
  **somente para controles**; nao ha A/Z ou SPACE sinteticos para jogos,
  shim de PREAD no 6502, mira persistente ou modo analogico STANDARD automatico.
- O codigo de reconhecimento de CT2 permanece, para **K7 Turbo** e outros
  tratamentos independentes de joystick. Nenhum arquivo de jogo/ROM e alterado.

## Reversao, sem recuperar outro ZIP

Mude **apenas uma linha** em `platformio.ini`:

```ini
-DTK2000_GAME_JOYSTICK_COMPAT=1
```

Recompile o mesmo projeto; este valor reativa as rotinas especificas da v0.88
com o mesmo reconhecimento de imagens CT2. Deixe `=0` para o comparativo padrao.
O valor configurado em PlatformIO prevalece sobre o padrao em `BuildConfig.h`.
Nao precisa mudar menus, ROMs ou arquivos CT2.

## Escopo da reversao

Apenas os atalhos especificos de joystick por jogo mudam. Continuam
inalterados: teclado BR/US, CONTROL+A..Z, LEDs, 60Hz, VGA, menus sem flicker,
CT2 multifaixa, suporte RobGO/TTGO, particoes MultiLoader e correcoes K7.

## Comportamento esperado ao testar

Moon Patrol e Missiles Defense podem deixar de aceitar cursores/paddles porque
seus CT2 usam `JSR $FB1E`, e a ROM TK-2000 nao implementa Apple PREAD nesse
endereco. Montezuma pode perder A/Z e pulo com ESPACO em modo joystick.
Isso e **esperado para a comparacao** com os controles padrao e nao indica
necessariamente regressao do mecanismo generico de teclado.

## Validacao

O `test/host/run.sh` executa o conjunto de regressao v0.88 com compatibilidade
reativada (`=1`) e os novos testes de ambos os perfis (`=0` e `=1`).
Os testes host nao substituem build completo nem verificacao nas placas.
