#!/usr/bin/env python3
"""Build ONLY ota_0 firmware.bin for ESP-WORKS ESP32_Bootloader. Never flashes ESP32."""
import argparse
import hashlib
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
from verify_bootloader import verify_artifacts, VerifyError

ROOT=Path(__file__).resolve().parents[1]
BUILD=ROOT/'build'; STAGING=BUILD/'_arduino'; OUTPUT=BUILD/'bootloader_sd'/'TK2000'
FQBN='esp32:esp32:esp32wrover:PartitionScheme=huge_app'

def version():
    m=re.search(r'#define\s+TK2000_FW_VERSION\s+"([^"\n]+)"',(ROOT/'src/BuildConfig.h').read_text())
    if not m: raise RuntimeError('versao ausente')
    return m.group(1)

def find_cli(requested):
    candidate=Path(requested).expanduser()
    search=[candidate] if candidate.is_absolute() or candidate.parent != Path('.') else []
    located=shutil.which(requested)
    if located: search.append(Path(located))
    name='arduino-cli.exe' if os.name=='nt' else 'arduino-cli'
    search.extend([ROOT.parent/name, ROOT/name,ROOT.parent/'arduino-cli'/name])
    if os.name=='nt': search.extend([Path('C:/Progs/arduino-cli/arduino-cli.exe'),Path('C:/arduino-cli/arduino-cli.exe')])
    for p in search:
        if p.is_file(): return str(p.resolve())
    raise RuntimeError('arduino-cli nao encontrado; use COMPILAR.bat --cli "C:\\caminho\\arduino-cli.exe"')

def clear():
    for p in (OUTPUT,STAGING,BUILD/'standalone'):
        if p.exists(): shutil.rmtree(p)

def one(pattern):
    found=list(STAGING.glob(pattern))
    if len(found)!=1: raise RuntimeError('esperado 1 %s, encontrados %d'%(pattern,len(found)))
    return found[0]

def build(args):
    BUILD.mkdir(exist_ok=True)
    clear()
    if args.fqbn != FQBN: raise RuntimeError('FQBN inesperado (exigido %s)'%FQBN)
    cli=find_cli(args.cli)
    STAGING.mkdir(parents=True)
    # Sketch-local partitions.csv overrides huge_app, as done by the MC-1000 project.
    command=[cli,'compile','--fqbn',FQBN,'--output-dir',str(STAGING),str(ROOT)]
    print('Compilando TK-2000 ESP-WORKS ota_0 (sem gravar flash):\n '+' '.join(command),flush=True)
    subprocess.run(command,check=True)
    app=one('*.ino.bin').read_bytes()
    part=one('*.partitions.bin').read_bytes()
    result=verify_artifacts(app,part,(ROOT/'partitions.csv').read_text())
    OUTPUT.mkdir(parents=True)
    (OUTPUT/'firmware.bin').write_bytes(app)
    (OUTPUT/'version.txt').write_text('TK2000_'+version()+'\n',encoding='ascii')
    (OUTPUT/'VERIFICACAO.txt').write_text(
      'ESP-WORKS firmware TK2000 '+version()+'\n'
      'Para SD: TK2000/firmware.bin + TK2000/version.txt\n'
      'OTA0: 0x130000 / 0x2C0000 bytes; SOMENTE firmware.bin\n'
      'Nao gravar bootloader, particoes ou otadata por USB.\n'
      'Imagem ESP32 verificada localmente; execucao na placa nao validada.\n'
      'Bytes: %d\nSHA256: %s\n'%(result['application'],hashlib.sha256(app).hexdigest()),encoding='utf-8')
    print('APROVADO: %s (%d / %d bytes)'%(OUTPUT/'firmware.bin',len(app),0x2C0000))
    print('Nenhuma operacao de gravacao na flash foi executada.')

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--cli',default=os.environ.get('ARDUINO_CLI','arduino-cli'))
    p.add_argument('--fqbn',default=FQBN)
    args=p.parse_args()
    try: build(args);return 0
    except (RuntimeError,VerifyError,subprocess.CalledProcessError,OSError) as err:
        clear();print('BUILD CANCELADO / NAO USE BINARIOS ANTIGOS: '+str(err),file=sys.stderr);return 1
if __name__=='__main__':sys.exit(main())
