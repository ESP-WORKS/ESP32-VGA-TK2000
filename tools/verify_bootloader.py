#!/usr/bin/env python3
"""Strict ESP32 image+partition checks for ESP-WORKS, no flash operations."""
from pathlib import Path
import hashlib
import re
import struct
import sys
APP_SIZE=0x2C0000
EXPECTED={
 'nvs': (1,2,0x9000,0x5000),
 'otadata': (1,0,0xE000,0x2000),
 'factory': (0,0,0x10000,0x120000),
 'ota_0': (0,0x10,0x130000,0x2C0000),
 'spiffs': (1,0x82,0x3F0000,0x10000),
}
class VerifyError(ValueError):pass

def verify_csv(csv):
    names=[];expected_lines=[]
    for line in csv.splitlines():
        line=line.split('#',1)[0].strip()
        if not line:continue
        parts=[x.strip().lower() for x in line.split(',')]
        if len(parts)<5: raise VerifyError('CSV incompleto')
        n,typ,sub,addr,size=parts[:5]
        names.append(n)
        expected_lines.append((n,typ,sub,int(addr,0),int(size,0)))
    if names!=list(EXPECTED): raise VerifyError('partitions.csv incompatível: nomes ou ordem')
    typecodes={'app':0,'data':1}
    subcodes={'nvs':2,'ota':0,'factory':0,'ota_0':0x10,'spiffs':0x82}
    parsed={n:(typecodes[t],subcodes[sub],addr,size) for n,t,sub,addr,size in expected_lines}
    if parsed!=EXPECTED: raise VerifyError('partitions.csv divergente do ESP-WORKS')
    return parsed

def verify_partitions(data):
    if len(data) not in (0xC00,0x1000):raise VerifyError('partitions.bin tamanho incorreto')
    if len(data)==0xC00:data+=b'\xff'*0x400
    out={}; md5_seen=False; finished=False
    for pos in range(0,0x1000,32):
        record=data[pos:pos+32]
        if record[:2]==b'\xff\xff':
            if any(b!=0xff for b in data[pos:]):raise VerifyError('lixo na tabela')
            finished=True;break
        if record[:2]==b'\xeb\xeb':
            if md5_seen or record[2:16]!=b'\xff'*14 or record[16:]!=hashlib.md5(data[:pos]).digest():
                raise VerifyError('MD5 de particoes invalido')
            md5_seen=True;continue
        if record[:2]!=b'\xaa\x50' or md5_seen:raise VerifyError('registro de particao invalido')
        _,typ,sub,addr,size,raw,flags=struct.unpack('<HBBII16sI',record)
        name=raw.split(b'\0',1)[0].decode('ascii')
        if not name or name in out or flags or addr+size>0x400000:raise VerifyError('particao invalida')
        out[name]=(typ,sub,addr,size)
    if not finished or out!=EXPECTED:raise VerifyError('partitions.bin NAO corresponde ao ESP-WORKS: %s'%out)
    return out

def verify_image(data):
    if len(data)<64 or len(data)>APP_SIZE or data[0]!=0xe9:raise VerifyError('firmware ausente ou maior que ota_0')
    count=data[1]
    if not 1<=count<=16:raise VerifyError('segmentos ESP32 invalidos')
    cursor=24;checksum=0xef
    for _ in range(count):
        if cursor+8>len(data):raise VerifyError('segmento truncado')
        _,size=struct.unpack_from('<II',data,cursor);cursor+=8
        if size==0 or cursor+size>len(data):raise VerifyError('segmento invalido')
        for b in data[cursor:cursor+size]:checksum^=b
        cursor+=size
    end=(cursor+16)&~15
    if end>len(data) or data[end-1]!=checksum or any(data[cursor:end-1]):
        raise VerifyError('checksum ESP32 invalido')
    hashed=data[23]
    if hashed not in (0,1) or end+(32 if hashed else 0)!=len(data):
        raise VerifyError('rodape ESP32 invalido')
    if hashed and hashlib.sha256(data[:end]).digest()!=data[end:]:raise VerifyError('SHA256 incorreto')
    return len(data)

def verify_artifacts(app,part,csv):
    verify_csv(csv);verify_partitions(part)
    n=verify_image(app)
    if 0x130000+n>0x3F0000:raise VerifyError('firmware invadiria SPIFFS')
    return {'application':n}

if __name__=='__main__':
    if len(sys.argv)!=4:
        print('Uso: verify_bootloader.py firmware.bin partitions.bin partitions.csv');sys.exit(2)
    try:
        print('APROVADO:',verify_artifacts(Path(sys.argv[1]).read_bytes(),Path(sys.argv[2]).read_bytes(),Path(sys.argv[3]).read_text()))
    except (VerifyError,OSError) as err: print('REPROVADO:',err,file=sys.stderr);sys.exit(1)
