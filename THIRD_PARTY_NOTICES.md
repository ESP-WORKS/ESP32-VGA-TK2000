# Créditos e referências

- **TK-2000 emulator (Windows)**: referência pública de Fábio Belavenuto,
  baseada em trabalhos de AppleWin. Usada para conferência de mapa de portas,
  matriz de teclado e formato de imagem K7 CTK2; o código de referência e suas
  ROMs não são redistribuídos neste pacote.
- **FabGL**: biblioteca externa necessária à compilação (versão 1.0.9),
  utilizada para vídeo VGA, teclado PS/2 e áudio.
- **ESP32 Arduino Core**: dependência externa da plataforma ESP32 (2.0.17).


Este projeto distribui seu código-fonte sob GPL-3.0-ou-posterior, conforme
`LICENSE`. As ROMs, jogos e imagens de disquete não são fornecidos e devem
ser obtidos legitimamente pelo usuário.

- **TK2000 Java (2008, Gabriel Velo)**: `Tape.java` / `Sound.java` consultados
  como referência funcional para o monitor de áudio K7 sincronizado às
  transições do sinal. O projeto v0.97 possui implementação PCM independente;
  o código-fonte Java e sua ROM não são redistribuídos neste ZIP.
