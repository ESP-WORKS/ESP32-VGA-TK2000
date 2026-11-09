# TK-2000 RobGO / TTGO VGA32 — v0.91 (04/11/2026)

Base cumulativa: v0.90 (`ESP32-VGA-TK2000-ROBGO-main`, PlatformIO).
Referência de teclado: `tk2000-emulator-master-0.3.zip`, especialmente
`src/tk_teclado.cpp`, e imagem documental `Comandos Control.JPG`.

## Correção funcional principal

**CONTROL + A..Z não deve sintetizar SHIFT.** A v0.84 havia acrescentado
SHIFT virtual sempre que CONTROL e uma letra eram mantidos pressionados,
transformando as funções originais de CONTROL em atalhos de tokens BASIC.

Na v0.91, `KeyboardTK::updateShiftBit()` passa a gerar a linha SHIFT apenas
se SHIFT físico estiver pressionado ou quando o caráter TK mapeado exigir
SHIFT próprio (ex.: `+`, `@`, aspas). Pressionar CONTROL não modifica SHIFT.

- **CONTROL + letra**: entrega somente CONTROL e a tecla real, permitindo
  ao programa/ROM interpretar suas funções específicas.
- **CONTROL + SHIFT + letra**: entrega os três contatos da matriz para os
  comandos BASIC da ROM.
- Ambos os CONTROL e SHIFT físicos (esquerdo/direito) permanecem independentes.
- Liberação em qualquer ordem não deve deixar modificadores pressionados.
- Nenhum caractere ou comando BASIC é injetado artificialmente em RAM.

## Prioridade de leitura do CONTROL

A versão Windows 0.3 em `KeybKBIN()` retorna `0x81` quando a seleção
CONTROL (`$C05F`) está ativa e CONTROL físico está pressionado. A v0.91
replica essa prioridade: a ROM lê CONTROL, seleciona `$C05E` e então
varre as linhas alfanuméricas/SHIFT normalmente. Isto evita misturar
contatos na leitura reservada à linha CONTROL.

## Funções do manual (CONTROL sozinho, sem SHIFT)

- CONTROL+C — interromper execução ou listagem.
- CONTROL+X — cancelar a linha digitada.
- CONTROL+G — gerar beep.
- CONTROL+R — entrar/sair do modo gráfico.
- CONTROL+S — pausar listagem BASIC.
- CONTROL+Q — continuar listagem BASIC.

Estas são funções do BASIC original; o emulador apresenta os contatos
corretos, mas não inventa handlers separados para esses comandos.

## Comparação com Windows 0.3

- Mesma distribuição física de 26 letras nos 8 registros KBOUT/KBIN.
- Shift na linha 0, bit 0; CTRL em linha selecionável por C05E/C05F.
- A tabela de pontuação e códigos especiais permanece preservada.
- Não houve alteração na leitura de joystick, nos botões, menus, CT2,
  cassete, VGA, partições MultiLoader, PlatformIO ou suporte RobGO/TTGO.

## Testes no computador

1. 26 letras × CONTROL, SHIFT+CONTROL, ambas as ordens de KEYUP;
2. CONTROL esquerdo e direito, SHIFT esquerdo e direito simultâneos;
3. prioridade de seleção de CONTROL sobre contatos da matriz;
4. símbolos e SHIFT forçado já existentes;
5. 6502 real executando a rotina de teclado ROM `$F043` (com ROM fornecida
   separadamente). No ROM de referência, por exemplo, CONTROL+C retorna
   `$83`, enquanto CONTROL+SHIFT+C retorna `$1A`;
6. regressões anteriores de joystick, teclado, PREAD, vídeo e menus com
   C++11 GCC; parte dos testes também executada em Clang.

O teste da ROM exige apontar o arquivo local de ROM, por exemplo:

```sh
TK2000_TEST_ROM=/caminho/TK2000.rom sh test/host/run.sh
```

A ROM não é copiada para este pacote.

**Limites da validação:** execução real das ações BASIC (parar LIST,
beep, modo gráfico), compilação final ESP32/PlatformIO/FabGL e teste físico
nas placas RobGO e TTGO ainda estão pendentes. A correção garante a
sequência da matriz e os códigos da ROM no teste, não a reação visual
final desses programas em hardware.
