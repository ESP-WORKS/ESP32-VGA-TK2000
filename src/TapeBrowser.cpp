#include "TapeBrowser.h"
#include "TKTapeJournal.h"
#include <Arduino.h>
#include <SD.h>
#include <ctype.h>
#include <esp_heap_caps.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {
static int ci(const char* a,const char* b) {
  while(*a && *b) {
    int x=toupper((unsigned char)*a),y=toupper((unsigned char)*b);
    if(x!=y)return x<y?-1:1;
    ++a;++b;
  }
  return (*a==*b)?0:(*a?1:-1);
}
static const char* base(const char* p) {
  if(!p)return "";
  const char* s=strrchr(p,'/');return s?s+1:p;
}
static bool findChild(File& dir,const char* parent,const char* wanted,char* out,size_t n) {
  File child=dir.openNextFile();
  unsigned checks=0;
  while(child) {
    if(child.isDirectory() && ci(base(child.name()),wanted)==0) {
      const char* actual=child.name();
      // Depending on the ESP32 FS backend, File::name() can be a full
      // path, a root-relative name ("/CT2"), or just "CT2". Preserve the
      // actual case while joining it to the discovered parent directory.
      const size_t plen=strlen(parent);
      const bool absoluteChild=actual[0]=='/' &&
          (strcmp(parent,"/")==0 ||
           (strncmp(actual,parent,plen)==0 && actual[plen]=='/'));
      const int written=absoluteChild?
          snprintf(out,n,"%s",actual):
          snprintf(out,n,"%s%s%s",strcmp(parent,"/")==0?"":parent,
                   "/",base(actual));
      child.close();return written>0 && (size_t)written<n;
    }
    child.close();
    if((++checks & 31u)==0)delay(1);
    child=dir.openNextFile();
  }
  return false;
}
}
TKTapeBrowser::~TKTapeBrowser(){clear();}
void TKTapeBrowser::clear(){
  if(entries_)heap_caps_free(entries_);
  entries_=nullptr;count_=capacity_=0;directoryFound_=partialList_=false;
  directory_[0]=0;
}
bool TKTapeBrowser::reserveOne(){
  if(count_<capacity_)return true;
  if(capacity_>INT_MAX/2)return false;
  const int cap=capacity_?capacity_*2:64;
  char (*next)[kPathBytes]=(char (*)[kPathBytes])heap_caps_malloc(
      (size_t)cap*kPathBytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  if(!next)return false;
  if(count_)memcpy(next,entries_,(size_t)count_*kPathBytes);
  if(entries_)heap_caps_free(entries_);
  entries_=next;capacity_=cap;return true;
}
int TKTapeBrowser::compareNames(const void* a,const void* b){
  return ci((const char*)a,(const char*)b);
}
int TKTapeBrowser::scrollForSelection(int s,int scroll,int total){
  if(total<=kVisibleRows)return 0;
  if(s<scroll)scroll=s;
  if(s>=scroll+kVisibleRows)scroll=s-kVisibleRows+1;
  if(scroll<0)scroll=0;
  if(scroll>total-kVisibleRows)scroll=total-kVisibleRows;
  return scroll;
}
const char* TKTapeBrowser::pathAt(int i)const{
  return i>=0 && i<count_ && entries_?entries_[i]:nullptr;
}
bool TKTapeBrowser::hasCT2Extension(const char* name){
  if(!name)return false;
  const size_t n=strlen(name);
  return n>4 && ci(name+n-4,".CT2")==0;
}
bool TKTapeBrowser::safeName(const char* name){
  if(!name || !*name || name[0]=='.')return false;
  for(const unsigned char* p=(const unsigned char*)name;*p;++p){
    if(!((*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z')||
         (*p>='0'&&*p<='9')||*p=='_'||*p=='-'||*p==' '||*p=='.'))return false;
  }
  return true;
}
bool TKTapeBrowser::newPath(const char* name,char* out,size_t n)const{
  if(!directoryFound_ || !safeName(name) || !out || !n)return false;
  const char* dot=strrchr(name,'.');
  if(dot && ci(dot,".CT2")!=0)return false;
  const int size=snprintf(out,n,"%s/%s%s",directory_,name,dot?"":".CT2");
  return size>0 && (size_t)size<n;
}
int TKTapeBrowser::findPath(const char* path)const{
  if(!path)return -1;
  for(int i=0;i<count_;++i)
    if(ci(entries_[i],path)==0)return i;
  return -1;
}
bool TKTapeBrowser::refresh(){
  count_=0;directoryFound_=partialList_=false;directory_[0]=0;
  // Read FAT's real spelling instead of assuming upper-case directory names.
  File root=SD.open("/",FILE_READ);
  char tk[kPathBytes]={};
  bool found=false;
  if(root && root.isDirectory())found=findChild(root,"/","TK-2000",tk,sizeof(tk));
  if(root)root.close();
  if(!found)snprintf(tk,sizeof(tk),"/TK-2000");
  File parent=SD.open(tk,FILE_READ);
  if(!parent || !parent.isDirectory()) {
    if(parent)parent.close();
    if(!SD.mkdir(tk))return false;
    parent=SD.open(tk,FILE_READ);
    if(!parent || !parent.isDirectory()){if(parent)parent.close();return false;}
  }
  char ct[kPathBytes]={};
  found=findChild(parent,tk,"CT2",ct,sizeof(ct));
  parent.close();
  if(!found) {
    const int len=snprintf(ct,sizeof(ct),"%s/CT2",tk);
    if(len<=0 || (size_t)len>=sizeof(ct))return false;
  }
  File dir=SD.open(ct,FILE_READ);
  if(!dir || !dir.isDirectory()) {
    if(dir)dir.close();
    if(!SD.mkdir(ct))return false;
    dir=SD.open(ct,FILE_READ);
    if(!dir || !dir.isDirectory()){if(dir)dir.close();return false;}
  }
  directoryFound_=true;
  strncpy(directory_,ct,sizeof(directory_)-1);
  directory_[sizeof(directory_)-1]=0;
  // Recover complete CT2 journals in a separate pass. Renaming a file
  // while walking a FAT directory can reorder the iterator, skipping or
  // duplicating other tapes. Restart this pass after each restored backup,
  // then build the visible F3 index from a fresh, stable directory scan.
  // No heap allocation is needed for recovery, and no .tmp is promoted.
  for(;;){
    bool recoveredOne=false;
    File candidate=dir.openNextFile();
    unsigned inspected=0;
    while(candidate){
      char name[kPathBytes];
      snprintf(name,sizeof(name),"%s",base(candidate.name()));
      const bool isFile=!candidate.isDirectory();
      candidate.close();
      const size_t len=strlen(name);
      if(isFile && len>8 && ci(name+len-4,".bak")==0){
        char primary[kPathBytes];
        const int size=snprintf(primary,sizeof(primary),"%s/%.*s",ct,
                                (int)(len-4),name);
        if(size>0 && (size_t)size<sizeof(primary) &&
           hasCT2Extension(base(primary)) && !SD.exists(primary) &&
           TKTapeJournal::restoreMissing(primary)){
          recoveredOne=true;
          break;
        }
      }
      if((++inspected & 31u)==0)delay(1);
      candidate=dir.openNextFile();
    }
    if(candidate)candidate.close();
    if(!recoveredOne)break;
    // Reopen the directory: an old enumeration cannot be trusted after
    // renaming one of its entries. A successful rename removes one .bak,
    // so this loop terminates even with multiple crash journals.
    dir.close();
    dir=SD.open(ct,FILE_READ);
    if(!dir || !dir.isDirectory()){if(dir)dir.close();return false;}
  }
  dir.close();
  dir=SD.open(ct,FILE_READ);
  if(!dir || !dir.isDirectory()){if(dir)dir.close();return false;}
  File f=dir.openNextFile();unsigned scans=0;
  while(f){
    const char* filename=base(f.name());
    if(!f.isDirectory() && hasCT2Extension(filename) && f.size()>=4 &&
       f.size()<=8u*1024u*1024u) {
      char path[kPathBytes];
      const int n=snprintf(path,sizeof(path),"%s/%s",ct,filename);
      if(n>0 && (size_t)n<sizeof(path)){
        File input=SD.open(path,FILE_READ);
        uint8_t hdr[4]={};
        const bool valid=input && input.read(hdr,4)==4 && memcmp(hdr,"CTK2",4)==0;
        if(input)input.close();
        if(valid){
          if(!reserveOne()){partialList_=true;f.close();break;}
          memcpy(entries_[count_++],path,(size_t)n+1u);
        }
      }
    }
    f.close();if((++scans &31u)==0)delay(1);
    f=dir.openNextFile();
  }
  if(f)f.close();
  dir.close();
  if(count_>1)qsort(entries_,(size_t)count_,kPathBytes,compareNames);
  return !partialList_;
}
