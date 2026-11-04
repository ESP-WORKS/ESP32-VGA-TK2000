#include "DiskImage.h"
#include <SD.h>
#include <string.h>
#include <stdlib.h>
#include <esp_heap_caps.h>

namespace {
constexpr size_t kTrackBytes = 6656;
constexpr size_t kDskTrackBytes = 6384;
constexpr size_t kSectorBytes = 256;
constexpr uint8_t kDosOrder[16] = {
  0, 7, 14, 6, 13, 5, 12, 4, 11, 3, 10, 2, 9, 1, 8, 15
};
constexpr uint8_t kGcr[64] = {
  0x96,0x97,0x9A,0x9B,0x9D,0x9E,0x9F,0xA6,0xA7,0xAB,0xAC,0xAD,0xAE,0xAF,0xB2,0xB3,
  0xB4,0xB5,0xB6,0xB7,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF,0xCB,0xCD,0xCE,0xCF,0xD3,
  0xD6,0xD7,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF,0xE5,0xE6,0xE7,0xE9,0xEA,0xEB,0xEC,
  0xED,0xEE,0xEF,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF
};
// Preserve the byte ordering used in the supplied Fabio Belavenuto
// tk_imagem.cpp: 86 low-bit groups, followed by 256 high-bit groups.
inline uint8_t swap2(uint8_t x) { return (uint8_t)(((x & 1) << 1) | ((x & 2) >> 1)); }
inline uint8_t odd44(uint8_t x) { return (uint8_t)(0xAA | ((x >> 1) & 0x55)); }
inline uint8_t even44(uint8_t x) { return (uint8_t)(0xAA | (x & 0x55)); }
inline uint8_t decode44(uint8_t odd, uint8_t even) {
  return (uint8_t)(((odd & 0x55) << 1) | (even & 0x55));
}
// A FAT rename of the primary image to .bak may finish immediately before
// power loss. Recover that *old*, complete primary at the next mount. Never
// promote an unverified .tmp: it might have been only partly written.
bool recoverMissingPrimary(const char *path) {
  if (SD.exists(path)) return true;
  const String backup=String(path)+".bak";
  if (!SD.exists(backup.c_str())) return false;
  File file=SD.open(backup.c_str(),FILE_READ);
  const size_t bytes=file ? file.size() : 0;
  file.close();
  if (bytes!=143360u && bytes!=232960u) return false;
  return SD.rename(backup.c_str(),path);
}

// After a crash *following* .tmp promotion, the new primary and the old
// transient .bak can coexist. Only retire .bak after the primary is fully
// readable and the permanent, independent .original.bak is already present.
// If either check fails, keep the recovery image and refuse to overwrite it.
bool retireCompletedJournal(const char *path,size_t bytes) {
  const String backup=String(path)+".bak";
  if (!SD.exists(backup.c_str())) return true;
  const String permanent=String(path)+".original.bak";
  File original=SD.open(permanent.c_str(),FILE_READ);
  const bool hasPermanent=(bool)original && original.size()==bytes;
  original.close();
  if (!hasPermanent) return false;
  File primary=SD.open(path,FILE_READ);
  if (!primary || primary.size()!=bytes) {primary.close();return false;}
  uint8_t scratch[512];
  size_t left=bytes;
  while(left) {
    const size_t n=left<sizeof(scratch)?left:sizeof(scratch);
    if(primary.read(scratch,n)!=n) {primary.close();return false;}
    left-=n;
  }
  primary.close();
  return SD.remove(backup.c_str());
}
inline uint8_t wrapRead(const uint8_t *track, size_t pos) { return track[pos % kDskTrackBytes]; }
inline bool markAt(const uint8_t *track, size_t pos, uint8_t a, uint8_t b, uint8_t c) {
  return wrapRead(track, pos) == a && wrapRead(track, pos+1) == b && wrapRead(track, pos+2) == c;
}
void encode62(const uint8_t *src, uint8_t *dest) {
  uint8_t raw[342];
  uint8_t offset = 0xAC;
  for (int i = 0; i < 86; ++i) {
    uint8_t packed = 0;
    for (int n = 0; n < 3; ++n) {
      packed = (uint8_t)((packed << 2) | swap2(src[offset] & 3));
      offset = (uint8_t)(offset - (n == 2 ? 0x53 : 0x56));
    }
    raw[i] = (uint8_t)(packed << 2);
  }
  raw[84] &= 0x3F;
  raw[85] &= 0x3F;
  for (int i = 0; i < 256; ++i) raw[86+i] = src[i];
  uint8_t previous = 0;
  for (int i = 0; i < 342; ++i) {
    uint8_t x = (uint8_t)(previous ^ raw[i]);
    dest[i] = kGcr[x >> 2];
    previous = raw[i];
  }
  dest[342] = kGcr[previous >> 2];
}
bool decode62(const uint8_t *track, size_t start, uint8_t *dst) {
  // The encoded stream is circular; a sector is allowed to straddle the
  // physical end of the NIB track.
  uint8_t raw[342];
  uint8_t previous = 0;
  for (int i = 0; i < 342; ++i) {
    const uint8_t gcr = wrapRead(track, start+i);
    int decoded = -1;
    for (int j = 0; j < 64; ++j) if (kGcr[j] == gcr) { decoded=j; break; }
    if (decoded < 0) return false;
    previous = (uint8_t)(previous ^ (decoded << 2));
    raw[i] = previous;
  }
  int checksum=-1;
  for(int j=0;j<64;++j) if(kGcr[j]==wrapRead(track,start+342)) {checksum=j;break;}
  if (checksum<0 || (uint8_t)(checksum<<2)!=previous) return false;
  for (int i=0;i<256;++i) dst[i]=(uint8_t)(raw[86+i]&0xFC);
  uint8_t offset=0xAC;
  for (int i=0;i<86;++i) {
    uint8_t bits=raw[i];
    for (int n=0;n<3;++n) {
      dst[offset] |= swap2((uint8_t)((bits >> (6-2*n))&3));
      offset=(uint8_t)(offset - (n==2 ? 0x53 : 0x56));
    }
  }
  return true;
}
}


DiskImage::~DiskImage() { eject(false); }

void DiskImage::freeTrackBuffers() {
  for(int t=0;t<35;++t) {
    if(_tracks[t]) { free(_tracks[t]); _tracks[t]=nullptr; }
    _trackDirty[t]=false;
  }
  _cachedTrack=-1;
}

bool DiskImage::dirty() const {
  if(_dirty) return true;
  for(int t=0;t<35;++t) if(_trackDirty[t]) return true;
  return false;
}

bool DiskImage::mount(const char *path, bool writable) {
  if (!path || !*path) return false;
  // A same-file remount is common after INIT/SAVE or an F1 refresh. Flush
  // current dirty nibble tracks BEFORE reading the file again; otherwise
  // this mount loads a stale on-SD snapshot, then eject(true) commits newer
  // data and silently reattaches the stale bytes as the active image.
  if (_type != NONE && _path == path && !flush()) return false;
  // Load the new image before discarding mounted media. Never lose pending
  // formatted tracks on failed mount or an unreadable replacement.
  if (!recoverMissingPrimary(path)) return false;
  File f=SD.open(path,FILE_READ);
  if (!f) return false;
  const size_t sz=f.size();
  Type kind = sz==143360 ? DSK : (sz==232960 ? NIB : NONE);
  if (kind==NONE) {f.close();return false;}
  uint8_t *buffer=(uint8_t*)ps_malloc(sz);
  if (!buffer) buffer=(uint8_t*)malloc(sz);
  if (!buffer) {f.close();return false;}
  const size_t got=f.read(buffer,sz);f.close();
  if (got!=sz) {free(buffer);return false;}
  if (!eject(true)) {free(buffer);return false;}
  _image=buffer;_size=sz;_type=kind;_path=path;
  _writable=writable;_dirty=false;_cachedTrack=-1;
  return true;
}

bool DiskImage::eject(bool flushIt) {
  if(_type!=NONE && flushIt && !flush())return false;
  freeTrackBuffers();
  if(_image) {free(_image);_image=nullptr;}
  _type=NONE;_writable=false;_dirty=false;
  _size=0;_path="";
  return true;
}

uint8_t *DiskImage::track(uint8_t t) {
  if(t>=35 || _type==NONE || !loadTrack(t)) return nullptr;
  _cachedTrack=t;
  return _tracks[t];
}

bool DiskImage::loadTrack(uint8_t t) {
  if(t>=35 || _type==NONE) return false;
  if(_tracks[t]) {_cachedTrack=t;return true;}
  uint8_t *buf=(uint8_t*)ps_malloc(kTrackBytes);
  if(!buf)buf=(uint8_t*)malloc(kTrackBytes);
  if(!buf)return false; // never overwrite another cached/dirty track
  if(_type==NIB) {
    memcpy(buf,_image+(size_t)t*kTrackBytes,kTrackBytes);
  } else {
    memset(buf,0xFF,kTrackBytes);
    size_t pos=0;
    for(int physical=0;physical<16;++physical) {
      if(physical==0)for(int i=0;i<48;++i)buf[pos++]=0xFF;
      const uint8_t address[4]={254,t,(uint8_t)physical,(uint8_t)(254^t^physical)};
      buf[pos++]=0xD5;buf[pos++]=0xAA;buf[pos++]=0x96;
      for(int i=0;i<4;++i){buf[pos++]=odd44(address[i]);buf[pos++]=even44(address[i]);}
      buf[pos++]=0xDE;buf[pos++]=0xAA;buf[pos++]=0xEB;
      for(int i=0;i<6;++i)buf[pos++]=0xFF;
      buf[pos++]=0xD5;buf[pos++]=0xAA;buf[pos++]=0xAD;
      const uint8_t *sector=_image+((size_t)t*16+kDosOrder[physical])*kSectorBytes;
      encode62(sector,buf+pos);pos+=343;
      buf[pos++]=0xDE;buf[pos++]=0xAA;buf[pos++]=0xEB;
      for(int i=0;i<27;++i)buf[pos++]=0xFF;
    }
    if(pos!=kDskTrackBytes) {free(buf);return false;}
  }
  _tracks[t]=buf;_cachedTrack=t;
  return true;
}

bool DiskImage::saveTrack(uint8_t t) {
  if(_type==NONE || t>=35 || !_tracks[t])return false;
  if(!_trackDirty[t])return true;
  if(!_writable)return false;
  const uint8_t *buf=_tracks[t];
  if(_type==NIB) {
    memcpy(_image+(size_t)t*kTrackBytes,buf,kTrackBytes);
  } else {
    // Parse the complete nibble stream, requiring address checksums and
    // 6-and-2 data checksums for each of the 16 sectors. Decode all sectors
    // into a TEMPORARY track image before changing even one DSK byte.
    // Formatting can leave a track temporarily incomplete; do not commit
    // it, but keep the nibble track cached while the head moves elsewhere.
    // A 4 KiB stack array can overflow the Arduino loop task stack while
    // FabGL is active. Use transient PSRAM scratch instead.
    uint8_t *decodedTrack=(uint8_t*)ps_malloc(16u*256u);
    if(!decodedTrack) decodedTrack=(uint8_t*)malloc(16u*256u);
    if(!decodedTrack)return false;
    bool seen[16]={};
    unsigned count=0;
    for(size_t p=0;p<kDskTrackBytes;++p) {
      if(!markAt(buf,p,0xD5,0xAA,0x96))continue;
      // Common DOS writers use DE AA; EB is optional, and the initial
      // address gap is not fixed to the encoder's six FF bytes.
      if(wrapRead(buf,p+11)!=0xDE || wrapRead(buf,p+12)!=0xAA)continue;
      const uint8_t vol=decode44(wrapRead(buf,p+3),wrapRead(buf,p+4));
      const uint8_t tr=decode44(wrapRead(buf,p+5),wrapRead(buf,p+6));
      const uint8_t sec=decode44(wrapRead(buf,p+7),wrapRead(buf,p+8));
      const uint8_t chk=decode44(wrapRead(buf,p+9),wrapRead(buf,p+10));
      if(tr!=t || sec>=16 || (uint8_t)(vol^tr^sec)!=chk)continue;
      // Search up to the next address field (or one revolution). Formatting
      // routines may use different inter-field sync gap lengths.
      for(size_t gap=13;gap<kDskTrackBytes;++gap) {
        const size_t d=p+gap;
        if(markAt(buf,d,0xD5,0xAA,0x96))break;
        if(!markAt(buf,d,0xD5,0xAA,0xAD))continue;
        if(wrapRead(buf,d+346)!=0xDE || wrapRead(buf,d+347)!=0xAA)break;
        uint8_t sector[256];
        if(!decode62(buf,d+3,sector))break;
        // A track with two different valid copies of the same sector is
        // ambiguous (e.g. overwritten address fields); reject it safely.
        const unsigned target=kDosOrder[sec];
        if(seen[sec]) {
          if(memcmp(decodedTrack+target*256,sector,256)!=0){free(decodedTrack);return false;}
        } else {
          memcpy(decodedTrack+target*256,sector,256);
          seen[sec]=true;++count;
        }
        break;
      }
    }
    if(count!=16){free(decodedTrack);return false;}
    memcpy(_image+(size_t)t*4096,decodedTrack,4096);
    free(decodedTrack);
  }
  _trackDirty[t]=false;
  _dirty=true;
  return true;
}

// Preserve the pre-write original once per SD image. A successful FAT rename
// guarantees byte-atomic replacement, but it does NOT protect a user from a
// guest DOS that formats only some tracks before reporting I/O ERROR. This
// permanent independent .original.bak makes that situation recoverable.
bool DiskImage::preserveOriginalBackup() {
  const String backup=_path+".original.bak";
  if(SD.exists(backup.c_str())) {
    File existing=SD.open(backup.c_str(),FILE_READ);
    const bool valid=(bool)existing && existing.size()==_size;
    existing.close();
    return valid;
  }
  File source=SD.open(_path.c_str(),FILE_READ);
  if(!source || source.size()!=_size) {source.close();return false;}
  const String temp=backup+".tmp";
  // Arduino FILE_WRITE may append to a pre-existing file. A failed remove
  // must abort instead of silently concatenating two disk images.
  if (SD.exists(temp.c_str()) && !SD.remove(temp.c_str())) {
    source.close();return false;
  }
  File copy=SD.open(temp.c_str(),FILE_WRITE);
  if(!copy) {source.close();return false;}
  uint8_t chunk[512];
  size_t remaining=_size;
  uint32_t crc=0xFFFFFFFFu;
  bool ok=true;
  while(remaining) {
    const size_t n=remaining<sizeof(chunk)?remaining:sizeof(chunk);
    if(source.read(chunk,n)!=n || copy.write(chunk,n)!=n) {ok=false;break;}
    for(size_t i=0;i<n;++i) {
      crc^=chunk[i];
      for(int bit=0;bit<8;++bit)
        crc=(crc>>1)^((crc&1u)?0xEDB88320u:0u);
    }
    remaining-=n;
  }
  source.close(); copy.flush(); copy.close();
  if(!ok){SD.remove(temp.c_str());return false;}
  // Verify the backup bytes were really readable before allowing the DOS
  // image to be replaced. This runs only on the FIRST save of each image.
  File verify=SD.open(temp.c_str(),FILE_READ);
  if(!verify || verify.size()!=_size) {verify.close();SD.remove(temp.c_str());return false;}
  uint32_t check=0xFFFFFFFFu;
  remaining=_size;
  while(remaining) {
    const size_t n=remaining<sizeof(chunk)?remaining:sizeof(chunk);
    if(verify.read(chunk,n)!=n){ok=false;break;}
    for(size_t i=0;i<n;++i) {
      check^=chunk[i];
      for(int bit=0;bit<8;++bit)
        check=(check>>1)^((check&1u)?0xEDB88320u:0u);
    }
    remaining-=n;
  }
  verify.close();
  if(!ok || check!=crc){SD.remove(temp.c_str());return false;}
  if(!SD.rename(temp.c_str(),backup.c_str()))return false;
  return true;
}

bool DiskImage::flush() {
  if(_type==NONE || !_writable)return true;
  // All-or-nothing: if a formatted track is malformed, do not commit ANY
  // changed tracks from this session to the mounted SD image.
  for(int t=0;t<35;++t)
    if(_trackDirty[t] && !saveTrack((uint8_t)t))return false;
  if(!_dirty)return true;
  const String tmp=_path+".tmp", bak=_path+".bak";
  if (!recoverMissingPrimary(_path.c_str())) return false;
  if (!retireCompletedJournal(_path.c_str(),_size)) return false;
  if (!preserveOriginalBackup()) return false;
  if (SD.exists(tmp.c_str()) && !SD.remove(tmp.c_str())) return false;
  File f=SD.open(tmp.c_str(),FILE_WRITE);
  if(!f)return false;
  const size_t written=f.write(_image,_size);f.flush();f.close();
  if(written!=_size){SD.remove(tmp.c_str());return false;}
  // Reject a truncated/concatenated temporary file before ever renaming the
  // original SD image. Keep the source intact on failed validation.
  File verifyTmp=SD.open(tmp.c_str(),FILE_READ);
  const bool validTmp=(bool)verifyTmp && verifyTmp.size()==_size;
  verifyTmp.close();
  if(!validTmp){SD.remove(tmp.c_str());return false;}
  const bool hadOriginal=SD.exists(_path.c_str());
  if(hadOriginal && !SD.rename(_path.c_str(),bak.c_str()))return false;
  if(!SD.rename(tmp.c_str(),_path.c_str())) {
    if(hadOriginal)SD.rename(bak.c_str(),_path.c_str());
    return false;
  }
  if(hadOriginal)SD.remove(bak.c_str());
  _dirty=false;return true;
}

