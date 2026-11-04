#include "SystemROM.h"
#include "Machine.h"
#include <Arduino.h>
#include <SD.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

namespace {
constexpr size_t kSystemROMSize=0x4000;
constexpr size_t kDiskROMSize=0x0100;
constexpr size_t kMaxSDPath=192;

bool asciiEqualIgnoreCase(const char *a,const char *b){
  if (!a || !b) return false;
  while (*a && *b) {
    unsigned char x=(unsigned char)*a++, y=(unsigned char)*b++;
    if (x>='A' && x<='Z') x=(unsigned char)(x+'a'-'A');
    if (y>='A' && y<='Z') y=(unsigned char)(y+'a'-'A');
    if (x!=y) return false;
  }
  return *a==0 && *b==0;
}

// Enumerate directory entries instead of trusting SD.exists() case behavior:
// different FAT implementations return different case for long file names.
bool findChild(const char *parent,const char *wanted,bool wantDirectory,
               char *found,size_t capacity){
  File dir=SD.open(parent,FILE_READ);
  if (!dir || !dir.isDirectory()) { if(dir)dir.close(); return false; }
  bool ok=false;
  File entry=dir.openNextFile();
  while (entry) {
    const char *raw=entry.name();
    if (raw) {
      const char *base=raw;
      for (const char *p=raw; *p; ++p)
        if (*p=='/' || *p=='\\') base=p+1;
      if (asciiEqualIgnoreCase(base,wanted) &&
          entry.isDirectory()==wantDirectory) {
        // Build the absolute pathname with the spelling returned by FAT.
        int n=snprintf(found,capacity,"%s%s%s",parent,
                       strcmp(parent,"/")==0 ? "" : "/",base);
        ok=n>0 && (size_t)n<capacity;
        entry.close();
        break;
      }
    }
    entry.close();
    entry=dir.openNextFile();
  }
  dir.close();
  return ok;
}
}

TKROMStatus TKLoadSystemROMFromSD(TKMachine &machine,
                                   char *resolvedPath,size_t resolvedCapacity){
  if (resolvedPath && resolvedCapacity) resolvedPath[0]='\0';
  char p1[kMaxSDPath],p2[kMaxSDPath],p3[kMaxSDPath];
  if (!findChild("/","TK-2000",true,p1,sizeof(p1)) ||
      !findChild(p1,"ROMS",true,p2,sizeof(p2))) return TKROMStatus::FOLDER_MISSING;
  if (!findChild(p2,"TK-2000.ROM",false,p3,sizeof(p3)))
    return TKROMStatus::FILE_MISSING;
  File file=SD.open(p3,FILE_READ);
  if (!file || file.isDirectory()) { if(file)file.close(); return TKROMStatus::OPEN_FAILED; }
  if (file.size()!=kSystemROMSize) { file.close(); return TKROMStatus::INVALID_SIZE; }
  // Prefer PSRAM to preserve scarce internal heap for FabGL and VGA output.
  uint8_t *data=(uint8_t*)ps_malloc(kSystemROMSize);
  if (!data) data=(uint8_t*)malloc(kSystemROMSize);
  if (!data) { file.close(); return TKROMStatus::OUT_OF_MEMORY; }
  size_t bytes=file.read(data,kSystemROMSize);
  file.close();
  if (bytes!=kSystemROMSize) { free(data); return TKROMStatus::READ_FAILED; }
  const uint16_t resetVector=(uint16_t)data[0x3FFC] | ((uint16_t)data[0x3FFD]<<8);
  // Reject obviously erased/empty or misaligned ROM dumps. The TK-2000
  // reset handler is in the system ROM, never in the disk interface window.
  if (resetVector<0xC200 || resetVector==0xFFFF ||
      data[resetVector-0xC000]==0xFF) {
    free(data);
    return TKROMStatus::INVALID_VECTOR;
  }
  if (!machine.installSystemROM(data,kSystemROMSize)) {
    free(data);
    return TKROMStatus::READ_FAILED;
  }
  if (resolvedPath && resolvedCapacity)
    snprintf(resolvedPath,resolvedCapacity,"%s",p3);
  return TKROMStatus::OK;
}

TKROMStatus TKLoadDiskROMFromSD(TKMachine &machine,
                                 char *resolvedPath,size_t resolvedCapacity){
  if (resolvedPath && resolvedCapacity) resolvedPath[0]='\0';
  char p1[kMaxSDPath],p2[kMaxSDPath],p3[kMaxSDPath];
  if (!findChild("/","TK-2000",true,p1,sizeof(p1)) ||
      !findChild(p1,"ROMS",true,p2,sizeof(p2))) return TKROMStatus::FOLDER_MISSING;
  if (!findChild(p2,"DISK-II.ROM",false,p3,sizeof(p3)))
    return TKROMStatus::FILE_MISSING;
  File file=SD.open(p3,FILE_READ);
  if (!file || file.isDirectory()) { if(file)file.close(); return TKROMStatus::OPEN_FAILED; }
  if (file.size()!=kDiskROMSize) { file.close(); return TKROMStatus::INVALID_SIZE; }
  uint8_t *data=(uint8_t*)malloc(kDiskROMSize);
  if (!data) { file.close(); return TKROMStatus::OUT_OF_MEMORY; }
  size_t bytes=file.read(data,kDiskROMSize);
  file.close();
  if (bytes!=kDiskROMSize) { free(data); return TKROMStatus::READ_FAILED; }
  // Reject obviously blank or erased dumps; do not reject alternate firmwares
  // merely because their CRC differs from the known TK-2000 AUX controller.
  bool anyNot00=false, anyNotFF=false;
  for (size_t i=0; i<kDiskROMSize; ++i) {
    anyNot00 |= data[i]!=0;
    anyNotFF |= data[i]!=0xFF;
  }
  if (!anyNot00 || !anyNotFF) { free(data); return TKROMStatus::INVALID_CONTENT; }
  if (!machine.installDiskROM(data,kDiskROMSize)) {
    free(data);
    return TKROMStatus::READ_FAILED;
  }
  if (resolvedPath && resolvedCapacity)
    snprintf(resolvedPath,resolvedCapacity,"%s",p3);
  return TKROMStatus::OK;
}

const char *TKDiskROMStatusText(TKROMStatus status){
  switch(status){
    case TKROMStatus::OK: return "DISK-II.ROM OK";
    case TKROMStatus::FOLDER_MISSING: return "PASTA TK-2000/ROMS NAO ENCONTRADA";
    case TKROMStatus::FILE_MISSING: return "ARQUIVO DISK-II.ROM NAO ENCONTRADO";
    case TKROMStatus::OPEN_FAILED: return "ERRO AO ABRIR DISK-II.ROM";
    case TKROMStatus::INVALID_SIZE: return "DISK-II.ROM DEVE TER 256 BYTES";
    case TKROMStatus::OUT_OF_MEMORY: return "SEM MEMORIA PARA ROM DE DISCO";
    case TKROMStatus::READ_FAILED: return "FALHA NA LEITURA DISK-II.ROM";
    case TKROMStatus::INVALID_CONTENT: return "DISK-II.ROM VAZIA OU APAGADA";
    default: return "ERRO DE ROM DISK-II";
  }
}

const char *TKROMStatusText(TKROMStatus status){
  switch(status){
    case TKROMStatus::OK: return "ROM OK";
    case TKROMStatus::FOLDER_MISSING: return "PASTA TK-2000/ROMS NAO ENCONTRADA";
    case TKROMStatus::FILE_MISSING: return "ARQUIVO TK-2000.ROM NAO ENCONTRADO";
    case TKROMStatus::OPEN_FAILED: return "ERRO AO ABRIR ROM";
    case TKROMStatus::INVALID_SIZE: return "ROM DEVE TER 16384 BYTES";
    case TKROMStatus::OUT_OF_MEMORY: return "SEM MEMORIA PARA ROM";
    case TKROMStatus::READ_FAILED: return "FALHA NA LEITURA DA ROM";
    case TKROMStatus::INVALID_VECTOR: return "ROM INVALIDA: VETOR DE RESET";
    case TKROMStatus::INVALID_CONTENT: return "ROM INVALIDA: CONTEUDO VAZIO";
    default: return "ERRO DE ROM";
  }
}
