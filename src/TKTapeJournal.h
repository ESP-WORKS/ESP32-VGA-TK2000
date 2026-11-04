#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <SD.h>

// Power-loss recovery for CT2 saves. A save first renames the complete old
// primary to .bak and then promotes a fully-written .tmp to the primary name.
// In between these operations the primary can be missing. Unlike disk
// images, CT2 files vary in size; validate their complete chunk structure
// before restoring a .bak. Never promote an incomplete .tmp.
class TKTapeJournal {
public:
  static bool valid(const char* path) {
    File f=SD.open(path,FILE_READ);
    if(!f)return false;
    const size_t length=f.size();
    if(length<4u || length>8u*1024u*1024u){f.close();return false;}
    uint8_t header[4];
    if(f.read(header,4)!=4 || memcmp(header,"CTK2",4)!=0){f.close();return false;}
    size_t left=length-4u;
    uint8_t buffer[256];
    while(left){
      if(left<4u || f.read(header,4)!=4){f.close();return false;}
      left-=4;
      const size_t bytes=(size_t)header[2] | ((size_t)header[3]<<8);
      const bool ca=header[0]=='C' && header[1]=='A';
      const bool cb=header[0]=='C' && header[1]=='B';
      const bool da=header[0]=='D' && header[1]=='A';
      if(!(ca || cb || da) || ((ca || cb) && bytes) || bytes>left) {
        f.close();return false;
      }
      size_t todo=bytes;
      while(todo){
        const size_t n=todo<sizeof(buffer)?todo:sizeof(buffer);
        if(f.read(buffer,n)!=n){f.close();return false;}
        todo-=n;
      }
      left-=bytes;
    }
    f.close();return true;
  }

  static bool backupPath(const char* path,char* out,size_t n) {
    if(!path || !*path || !out || !n)return false;
    const int written=snprintf(out,n,"%s.bak",path);
    return written>0 && (size_t)written<n;
  }

  static bool restoreMissing(const char* primary) {
    if(SD.exists(primary))return true;
    char backup[144];
    if(!backupPath(primary,backup,sizeof(backup)) || !SD.exists(backup))return false;
    // Do not recover a truncated or structurally corrupt old file.
    if(!valid(backup))return false;
    return SD.rename(backup,primary);
  }

  static bool safeToOverwrite(const char* primary) {
    char backup[144];
    if(!backupPath(primary,backup,sizeof(backup)))return false;
    if(!SD.exists(backup))return true;
    if(!SD.exists(primary))return restoreMissing(primary);
    // A stale backup may be discarded only if the primary is independently
    // verified intact. Otherwise preserve the backup for manual recovery.
    return valid(primary) && SD.remove(backup);
  }
};
