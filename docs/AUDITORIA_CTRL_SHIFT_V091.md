# Auditoria complementar do teclado — v0.91

## Fontes confrontadas

1. `tk2000-emulator-master-0.3/src/tk_teclado.cpp`:
   `CONFTECLA`, `KeybQueueKeypress()`, `KeybCTRL0`, `KeybCTRL1`, `KeybKBIN`.
2. Imagem documental `Comandos Control.JPG` enviada pelo usuário.
3. Base ESP32 RobGO/TTGO PlatformIO v0.90: `src/Keyboard.h/.cpp`,
   `src/main.cpp`, `src/TKModifierSides.h`, `src/TKHostKeyOwners.h`.
4. Rotina real `$F043` das ROMs do TK-2000 fornecidas como referências.

## Defeito confirmado

A v0.90 repetia a lógica introduzida na v0.84 para facilitar tokens:

```cpp
bool tokenShift = false;
if (ctrlDown) {
  for (uint8_t c='A';c<='Z';++c)
    if (held[c]) { tokenShift=true; break; }
}
```

Esse `tokenShift` era combinado com `physicalShift`, provocando SHIFT
virtual com **qualquer** CONTROL+letra. A rotina da ROM recebia uma
combinação diferente da do teclado original, impossibilitando distinguir
CONTROL+C de CONTROL+SHIFT+C, por exemplo.

## Comportamento corrigido

| Combinação | Linhas TK / intenção |
|---|---|
| C | C sem CTRL nem SHIFT |
| CONTROL+C | CONTROL e C, SHIFT desligado |
| CONTROL+SHIFT+C | CONTROL, SHIFT e C |
| CONTROL+X/G/R/S/Q | CONTROL e respectiva letra, SHIFT desligado |
| CONTROL+SHIFT+ A..Z | CTRL + SHIFT + tecla alfabética; tokens da ROM |

As seis ações citadas na imagem são listadas no CHANGELOG_0.91.md.
Os códigos não são reescritos na ROM nem substituídos por atalhos do host.

Na referência Windows 0.3, CONTROL selecionado por `$C05F` bloqueia
os outros bits naquele acesso (`0x81`); em seguida a rotina libera a
seleção via `$C05E` e faz a leitura dos contatos das teclas. Agora a
v0.91 segue essa precedência.

## Validação com código original 6502

O teste `test/host/audit_control_rom.cpp` chama realmente `$F043`,
sem reproduzir a rotina em C++ e sem distribuir ROM. Os resultados abaixo
foram lidos da ROM de referência. Os valores são códigos da rotina de
teclado, **não** uma prova de que o BASIC concluiu a ação (LIST/RUN etc.).

| Tecla | Letra | CONTROL+letra | CONTROL+SHIFT+letra |
|---|---|---|---|
| C | C3 | 83 | 1A |
| X | D8 | 98 | 17 |
| G | C7 | 87 | 10 |
| R | D2 | 92 | 0B |
| S | D3 | 93 | 15 |
| Q | D1 | 91 | 08 |
| P | D0 | 90 | 44 |

As leituras foram iguais comparando o modelo OR antigo e o retorno
prioritário da referência Windows, pois a rotina faz leituras separadas
das linhas CONTROL e alfanuméricas. A precedência nova melhora fidelidade
sem alterar a rotina de leitura demonstrada.

## Roteiro de testes físicos, sem monitor serial

1. Em BASIC, digitar uma linha e pressionar `CONTROL+X`: deve cancelá-la.
2. `CONTROL+G`: confirmar emissão de beep (áudio ativo).
3. `CONTROL+B`: alternar modo gráfico e voltar. (Retificação v0.94: leitura do manual em baixa qualidade confundiu B com R.)
4. Durante `LIST` extenso, testar `CONTROL+S` (pausa), depois
   `CONTROL+Q` (retoma).
5. Durante execução/listagem, `CONTROL+C` deve interromper.
6. Usar `CONTROL+SHIFT+P` para token `PRINT`; repetir com outras letras.
7. Repetir com CONTROL e SHIFT de ambos os lados (incluindo segurar
   CTRL direito e liberar CTRL esquerdo, e vice-versa).
8. Testar 20 caracteres BR/US, acentos ignorados e pontuação;
   verificar o joystick ON/OFF, os menus F1/F2/F3 e CT2 multifaixa.

Não foi possível compilar para ESP32 com PlatformIO/FabGL nem testar
na RobGO ou TTGO durante esta revisão. A v0.91 deve ser considerada
candidata a testes físicos, sem afirmar estabilidade prévia.
