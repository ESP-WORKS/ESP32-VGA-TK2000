# Testes adicionais v0.90

Execute a partir da raiz do projeto: `sh test/host/run.sh`.
Inclui regressões v0.88 e v0.89 e os testes novos:

- `audit_universal_controls.cpp`: guard universal PREAD, alias de NumLock,
  duas fontes de FIRE, retorno ao centro e cópia VGA byte-exata;
- `audit_cpu_pread.cpp`: CPU6502 real executando JSR/SEC/RTS com pilha real.

Nenhum ROM comercial ou CT2 é redistribuído ou usado na compilação.
