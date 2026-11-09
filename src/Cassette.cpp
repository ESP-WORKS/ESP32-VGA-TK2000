#include "Cassette.h"
#include <stdlib.h>
#include <string.h>
#if defined(ESP32)
#include <esp_heap_caps.h>
#endif

namespace {
static uint16_t le16(const uint8_t* p) { return (uint16_t)(p[0] | (uint16_t(p[1]) << 8)); }
static bool isTag(const uint8_t* p, char a, char b) { return p[0]==(uint8_t)a && p[1]==(uint8_t)b; }
static void* tapeAlloc(size_t n) {
#if defined(ESP32)
  return heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
  return malloc(n);
#endif
}
static void tapeFree(void* p) {
#if defined(ESP32)
  heap_caps_free(p);
#else
  free(p);
#endif
}
}
CassetteCT2::~CassetteCT2() { eject(); tapeFree(edges_); }
bool CassetteCT2::valid(const uint8_t* b, size_t size) {
  if (!b || size < 4 || memcmp(b,"CTK2",4)) return false;
  size_t p=4;
  while (p<size) {
    if (size-p<4) return false;
    const uint16_t n=le16(b+p+2);
    if (!(isTag(b+p,'C','A') || isTag(b+p,'C','B') || isTag(b+p,'D','A'))) return false;
    if (!isTag(b+p,'D','A') && n) return false;
    p+=4;
    if (n>size-p) return false;
    p+=n;
  }
  return true;
}
bool CassetteCT2::mount(uint8_t* b, size_t n) {
  if (!valid(b,n)) return false;
  eject(); image_=b; imageSize_=n;
  gameInputProfile_=TKDetectTapeGameInput(b,n);
  // Apple-compatible SAVEA/LOADA records contain a 4-byte start/end header
  // rather than the TK six-character/name + block-number 13-byte header.
  appleFormat_=false;
  for(size_t pos=4;pos+4<=n;){
    const uint16_t len=le16(b+pos+2);
    if(isTag(b+pos,'D','A')) {appleFormat_=(len==4);break;}
    pos+=4u+len;
  }
  resetTransport();
  totalPulseCycles_=0;
  for(size_t pos=4;pos+4<=n;){
    const uint16_t len=le16(b+pos+2);
    if(isTag(b+pos,'C','A')) {
      if(totalBlocks_<65535) ++totalBlocks_;
      totalPulseCycles_ += appleFormat_ ? (16384ull*652u+195u+250u)
                                       : (9600ull*500u);
    } else if(isTag(b+pos,'C','B')) {
      totalPulseCycles_ += 464u+65u*679u+199u+250u;
    } else if(isTag(b+pos,'D','A')) {
      // Each data bit is represented by two equal 250/500-cycle pulses.
      // A byte contributes 4000 cycles plus 500 for each set bit.
      for(size_t j=0;j<len;++j){
        uint8_t v=b[pos+4u+j];
        totalPulseCycles_ += 4000u+500u*(uint32_t)__builtin_popcount((unsigned)v);
      }
    }
    pos+=4u+len;
  }
  return true;
}
void CassetteCT2::eject() {
  if (image_) tapeFree(image_);
  image_=nullptr; imageSize_=0; appleFormat_=false;
  gameInputProfile_=TKGameInputProfile::Standard;
  resetTransport();totalBlocks_=0;totalPulseCycles_=0;
}
void CassetteCT2::resetTransport() {
  playing_=false;finished_=false;request_=None;
  autoPaused_=manualPaused_=everStarted_=false;
  pendingEdgeCycles_=0;currentBlock_=0;advancedPulseCycles_=0;
  probePC_=customReaderPC_=probeCount_=0;probeLastCycle_=0;
  chunkAt_=recordAt_=4; chunkSize_=0;edgeInChunk_=edgesThisChunk_=0;
  chunkKind_=ChunkNone;cachedByteIndex_=(size_t)-1;cachedDataByte_=0;
  nextEdgeCycle_=0;lastInputCycle_=0;edgeDuration_=0;level_=0x80;recording_=false;
}
void CassetteCT2::beginPlay() {
  if (!mounted()) return;
  resetTransport();playing_=true;everStarted_=true;edgeDuration_=nextDuration();
  if (!edgeDuration_) { playing_=false; finished_=true; }
}
bool CassetteCT2::nextChunk() {
  while(recordAt_<imageSize_) {
    const size_t p=recordAt_;
    if(imageSize_-p<4)return false;
    chunkSize_=le16(image_+p+2);
    chunkAt_=p+4;recordAt_=chunkAt_+chunkSize_;
    edgeInChunk_=0;
    cachedByteIndex_=(size_t)-1;
    if(isTag(image_+p,'C','A')) {
      chunkKind_=ChunkLeader;
      if(currentBlock_<totalBlocks_) ++currentBlock_;
      edgesThisChunk_=appleFormat_?16386u:9600u; // SAVEA / SAVET leaders
    }
    else if(isTag(image_+p,'C','B')) {
      chunkKind_=ChunkSync;edgesThisChunk_=68;
    }
    else if(isTag(image_+p,'D','A')) {
      chunkKind_=ChunkData;edgesThisChunk_=(uint32_t)chunkSize_*16u;
    }
    else { chunkKind_=ChunkNone;return false; }
    if(edgesThisChunk_)return true;
  }
  return false;
}
uint16_t CassetteCT2::nextDuration() {
  if(!image_)return 0;
  while(edgeInChunk_>=edgesThisChunk_)
    if(!nextChunk())return 0;
  const uint32_t edge=edgeInChunk_++;
  uint16_t duration=0;
  if(chunkKind_==ChunkLeader) {
    duration=!appleFormat_?500:(edge<16384u?652:(edge==16384u?195:250));
  } else if(chunkKind_==ChunkSync) {
    if(edge==0)duration=464;
    else if(edge==1)duration=679;
    else if(edge<66)duration=679;
    else duration=edge==66?199:250;
  } else {
    const size_t byteIndex=edge/16u;
    if(byteIndex!=cachedByteIndex_){
      cachedByteIndex_=byteIndex;
      cachedDataByte_=image_[chunkAt_+byteIndex];
    }
    const uint8_t bit=(uint8_t)(7u-(edge%16u)/2u);
    duration=(cachedDataByte_ & (1u<<bit)) ? 500 : 250;
  }
  advancedPulseCycles_+=duration;
  return duration;
}
// Preserve the unfinished half-wave at the most recent sampled cycle,
// NOT at the wall-clock instant when the watchdog notices inactivity.
void CassetteCT2::suspend(bool manual) {
  if(playing_) {
    pendingEdgeCycles_=nextEdgeCycle_ && nextEdgeCycle_>lastInputCycle_ ?
                       nextEdgeCycle_-lastInputCycle_ : (edgeDuration_?edgeDuration_:1u);
  }
  playing_=false;
  if(manual) { manualPaused_=true;autoPaused_=false; }
  else if(!manualPaused_) autoPaused_=true;
  customReaderPC_=0;probeCount_=0;
}
void CassetteCT2::resume(uint64_t cycles) {
  if(!mounted() || finished_ || !everStarted_)return;
  playing_=true;autoPaused_=manualPaused_=false;
  nextEdgeCycle_=cycles+(pendingEdgeCycles_?pendingEdgeCycles_:edgeDuration_);
  pendingEdgeCycles_=0;
  lastInputCycle_=cycles;
}
void CassetteCT2::service(uint64_t cycles) {
  if(playing_ && lastInputCycle_ && cycles>lastInputCycle_ &&
     cycles-lastInputCycle_>kIdleCycles) suspend(false);
}
void CassetteCT2::togglePause(uint64_t cycles) {
  if(recording_ || !mounted())return;
  if(playing_) { suspend(true);return; }
  if(paused()) { resume(cycles);return; }
  if(finished_ || !everStarted_) { beginPlay();return; }
}
void CassetteCT2::rewind() { if(!recording_)resetTransport(); }
uint8_t CassetteCT2::progress() const {
  if(!image_ || !totalPulseCycles_)return 0;
  if(finished_)return 100;
  // Real cassette progress is proportional to the pulse-time already decoded,
  // not the CT2 byte offset: early CA pilot leaders consume several seconds
  // but virtually no bytes. This avoids the misleading 0% on Karateka.
  const uint64_t pct=(advancedPulseCycles_/totalPulseCycles_)*100u +
      ((advancedPulseCycles_%totalPulseCycles_)*100u)/totalPulseCycles_;
  return (uint8_t)(pct>99u?99u:pct);
}
bool CassetteCT2::qualifiesInput(uint16_t pc, uint64_t cycles) {
  if(pc==0xFD01u)return true; // original TK cassette ROM, LOADT/LOADA
  if(!mounted() || recording_ || manualPaused_ || finished_ || !everStarted_)
    return false;
  // Ignore occasional matrix scans and distinguish a cassette bit loop
  // from keyboard row scanning. Each KBOUT write invalidates this probe.
  if(cycles<probeLastCycle_ || cycles-probeLastCycle_>512u){
    probeCount_=1;customReaderPC_=0;
  }else if(probeCount_<32u)++probeCount_;
  probePC_=pc;probeLastCycle_=cycles;
  if(probeCount_>=24u)customReaderPC_=pc;
  return customReaderPC_!=0 && probeCount_>=24u;
}
uint8_t CassetteCT2::read(uint64_t cycles) {
#if TK2000_K7_MOON_PROFILE
  if(gameInputProfile_==TKGameInputProfile::MoonPatrol) ++diagnosticReadSamples_;
#endif
  // A tight guest tape reader can reappear without a host service() call.
  // Detect the gap here as well, and resume without consuming skipped edges.
  if(playing_ && lastInputCycle_ && cycles>lastInputCycle_ &&
     cycles-lastInputCycle_>kIdleCycles) suspend(false);
  if(manualPaused_)return level_;
  if(autoPaused_)resume(cycles);
  lastInputCycle_=cycles;
  if(!playing_)return level_;
  if(!nextEdgeCycle_)nextEdgeCycle_=cycles+edgeDuration_;
  // Catch up after guest-cycle scheduling gaps without losing transitions.
  // The CPU normally samples every ~10 cycles in the ROM reader.
  unsigned iterations=0;
  while(playing_ && cycles>=nextEdgeCycle_ && iterations++<8192) {
#if TK2000_K7_MOON_PROFILE
    if(gameInputProfile_==TKGameInputProfile::MoonPatrol) ++diagnosticDecodedEdges_;
#endif
    level_^=0x80;
    const uint16_t d=nextDuration();
    if(!d){playing_=false;finished_=true;autoPaused_=manualPaused_=false;break;}
    edgeDuration_=d; // current half-wave is also the audible tape frequency
    nextEdgeCycle_+=d;
  }
  return level_;
}
bool CassetteCT2::reserveEdge() {
  if(edgesCount_<edgesCapacity_)return true;
  if(edgesCapacity_>=1024u*1024u)return false;
  size_t newCap=edgesCapacity_?edgesCapacity_*2u:4096u;
  uint16_t* n=(uint16_t*)tapeAlloc(newCap*sizeof(uint16_t));
  if(!n)return false;
  if(edgesCount_)memcpy(n,edges_,edgesCount_*sizeof(uint16_t));
  tapeFree(edges_);edges_=n;edgesCapacity_=newCap;return true;
}
void CassetteCT2::beginRecord() {
  playing_=false;autoPaused_=manualPaused_=false;recording_=true;request_=None;
  // A failed/aborted SAVE must not leave the disk slot disconnected because
  // an earlier playback was only partially completed.
  everStarted_=false;finished_=false;
  edgesCount_=0;lastOutputCycle_=0;
}
void CassetteCT2::outputToggle(uint64_t cycles) {
  if(!recording_)return;
  if(lastOutputCycle_) {
    const uint64_t delta=cycles-lastOutputCycle_;
    if(!reserveEdge()) {recording_=false;return;}
    edges_[edgesCount_++]=(uint16_t)(delta>65535u?65535u:delta);
  }
  lastOutputCycle_=cycles;
}
bool CassetteCT2::recordFinished(uint64_t cycles) const {
  return recording_ && edgesCount_>128 && lastOutputCycle_ &&
         cycles-lastOutputCycle_>3000000u; // ~2.5s guest time, across packet gaps
}

// Reconstitute a CTK2 stream from actual $C020 toggles. The original ROM
// produces two equal half-waves for each bit. Pilot/sync edges are distinct
// from data half-waves; do not guess bytes when sync is absent.
bool CassetteCT2::encodeRecording(uint8_t** result,size_t* size) const {
  if(!result||!size){return false;} *result=nullptr;*size=0;
  if(edgesCount_<128)return false;
  // A legal TK CT2 can have many tiny CB+DA packets. The old 1/8-edge
  // estimate was too small for their headers and could overflow PSRAM.
  // Even 1 byte per 16 edges with a sync before each packet fits below 1/4.
  const size_t maxBytes=4u+edgesCount_/4u+8192u;
  uint8_t* out=(uint8_t*)tapeAlloc(maxBytes);
  if(!out)return false;
  memcpy(out,"CTK2",4);size_t used=4;
  // SAVEA (Apple monitor format) has two approximately 16K-edge 652-cycle
  // pilot leaders, WITHOUT TK's 64-edge CB synchronizer. Each is followed
  // immediately by bytes with 250/500-cycle half-waves. Preserve both
  // Apple blocks as CA+DA: the first DA contains 4 bytes of address range.
  bool apple=false;
  for(size_t p=0;p+8192<edgesCount_;++p){
    if(edges_[p]<585 || edges_[p]>790)continue;
    size_t run=0;
    while(p+run<edgesCount_ && edges_[p+run]>=585 && edges_[p+run]<=790)++run;
    if(run>=8192){apple=true;break;}
    p+=run;
  }
  if(apple){
    size_t i=0;unsigned blocks=0;
    while(i+8192<edgesCount_){
      if(edges_[i]<585 || edges_[i]>790){++i;continue;}
      size_t run=0;
      while(i+run<edgesCount_ && edges_[i+run]>=585 && edges_[i+run]<=790)++run;
      if(run<8192){i+=run;continue;}
      i+=run;
      // The Apple leader ends with a short 195/250-cycle synchronization
      // pair. It is not part of the first data bit.
      if(i+1<edgesCount_ && edges_[i]<220 && edges_[i+1]>=220 && edges_[i+1]<=330) i+=2;
      if(used+8u>maxBytes){tapeFree(out);return false;}
      const size_t start=used;
      out[used++]='C';out[used++]='A';out[used++]=0;out[used++]=0;
      const size_t header=used;used+=4;
      size_t n=0;
      while(i+15<edgesCount_ && n<65535){
        uint8_t byte=0;bool validByte=true;
        for(unsigned bit=0;bit<8;++bit){
          uint16_t a=edges_[i+bit*2],b=edges_[i+bit*2+1];
          if(a<140||b<140||a>560||b>560){validByte=false;break;}
          if(a>365 && b>365)byte|=(uint8_t)(0x80u>>bit);
        }
        if(!validByte)break;
        if(used>=maxBytes){tapeFree(out);return false;}
        out[used++]=byte;++n;i+=16;
      }
      if(!n){used=start;continue;}
      out[header]='D';out[header+1]='A';out[header+2]=(uint8_t)n;
      out[header+3]=(uint8_t)(n>>8);
      ++blocks;
    }
    if(blocks<2){tapeFree(out);return false;}
    *result=out;*size=used;return true;
  }
  // Sync's long 679-cycle half-waves identify each DA block. In practice
  // hardware clock variation makes exact comparisons inappropriate.
  size_t i=0;unsigned packets=0;
  while(i+70<edgesCount_) {
    size_t sync=edgesCount_;
    for(size_t j=i;j+35<edgesCount_;++j){
      unsigned n=0;
      while(n<35 && edges_[j+n]>=570 && edges_[j+n]<850)++n;
      if(n>=35){sync=j;break;}
    }
    if(sync==edgesCount_)break;
    if(used+(packets?8u:12u)>maxBytes){tapeFree(out);return false;}
    if(!packets){out[used++]='C';out[used++]='A';out[used++]=0;out[used++]=0;}
    out[used++]='C';out[used++]='B';out[used++]=0;out[used++]=0;
    // Skip the 64 long sync edges and the two terminator pulses.
    size_t data=sync;
    while(data<edgesCount_ && edges_[data]>=570 && edges_[data]<850)++data;
    data+=2;
    if(data>=edgesCount_)break;
    const size_t header=used;used+=4;
    size_t n=0;
    while(data+15<edgesCount_ && n<65535) {
      // End block at next sync (skip the initial 464/679 irregular pair).
      bool atSync=false;
      for(size_t off=0;off<3 && data+off+34<edgesCount_;++off){
        unsigned longEdges=0;
        while(longEdges<34 && edges_[data+off+longEdges]>=570 &&
              edges_[data+off+longEdges]<850)++longEdges;
        if(longEdges>=34){atSync=true;break;}
      }
      if(atSync)break;
      uint8_t value=0;
      bool validBit=true;
      for(unsigned bit=0;bit<8;bit++){
        const size_t ix=data+bit*2;
        uint16_t a=edges_[ix], b=edges_[ix+1];
        if(a>560||b>560 || a<140 || b<140){validBit=false;break;}
        if(a>=365 && b>=365)value|=(uint8_t)(0x80u>>bit);
      }
      if(!validBit)break;
      if(used>=maxBytes){tapeFree(out);return false;}
      out[used++]=value;n++;data+=16;
    }
    if(!n){used=header-8;i=sync+35;continue;}
    out[header]='D';out[header+1]='A';out[header+2]=(uint8_t)n;
    out[header+3]=(uint8_t)(n>>8);
    ++packets;i=data;
  }
  if(!packets){tapeFree(out);return false;}
  *result=out;*size=used;return true;
}
