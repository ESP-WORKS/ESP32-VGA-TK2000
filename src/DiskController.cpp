#include "DiskController.h"

void TKDiskController::begin() {
  phaseMask=0;
  for (int d=0;d<2;++d) {
    head[d]=Head();
    observedWrites[d]=lastWriteMs[d]=lastAttemptMs[d]=0;
    attemptedAutoSave[d]=false;
  }
  reset();
}
void TKDiskController::reset() {
  // An electronic RESET does not mechanically home the stepper motor. The
  // preceding Machine::reset() has already flushed any pending writes.
  drive=0; dataLatch=0; motor=false; writeMode=false; phaseMask=0;
  for (int d=0; d<2; ++d) {
    Head &h=head[d];
    h.trackBuf=nullptr;
    if (disk[d].mounted() && !loadTrack(h,(uint8_t)d))writeFault=true;
  }
}
bool TKDiskController::coldReset() {
  // Treat inserted disks as physical media: do not eject them. All pending
  // writes must be durable before changing spindle or head position.
  if (!flush()) return false;
  for (int d=0; d<2; ++d) head[d]=Head();
  reset(); // controller latch, spindle and Q6/Q7 to their power-on state
  return true;
}
bool TKDiskController::loadTrack(Head &h,uint8_t d) {
  uint8_t *buf=disk[d].track((uint8_t)h.track);
  if(!buf) {h.trackBuf=nullptr;return false;}
  h.trackBuf=buf;
  if(h.nibPos>=disk[d].trackSize())h.nibPos=0;
  return true;
}
void TKDiskController::motorSwitch(bool on,uint64_t cycles) {
  (void)cycles;
  if(on==motor) return;
  motor=on;
  if(on) loadTrack(head[drive],drive);
  else {
    // Commit a complete write session when motor stops; all changed tracks
    // are validated before any new SD image is committed. Formatting an
    // incomplete track remains in PSRAM and the old DSK stays untouched.
    if(disk[drive].dirty() && !disk[drive].flush())writeFault=true;
    else if(!disk[drive].dirty())attemptedAutoSave[drive]=false;
  }
}

void TKDiskController::selectDrive(uint8_t d,uint64_t cycles) {
  (void)cycles;
  d&=1;
  if(d==drive)return;
  // Save neither track on selection: dirty nibble streams are retained in
  // separate PSRAM buffers, so an incomplete format cannot jam drive motion.
  Head &h=head[d];
  if(disk[d].mounted() && !loadTrack(h,d)){writeFault=true;return;}
  drive=d;
}
void TKDiskController::phaseSwitch(uint8_t p,bool on) {
  if(p>3)return;
  // MiSTer disk_ii.v tracks all four coil lines, including OFF transitions.
  // A falling edge does not step a head; the next rising adjacent coil does.
  if(on) phaseMask |= (uint8_t)(1u<<p);
  else   phaseMask &= (uint8_t)~(1u<<p);
  if(!on||!motor)return;
  Head &h=head[drive];
  int step=0;
  if(p==((h.phase+1)&3))step=+1;
  else if(p==((h.phase+3)&3))step=-1;
  // Do not treat a nonadjacent energized coil as a valid head phase.
  if(!step) return;
  int newHalf=h.halfTrack+step;
  if(newHalf<0)newHalf=0;
  if(newHalf>79)newHalf=79;
  const int newTrack=newHalf/2>=35?34:newHalf/2;
  if(newTrack!=h.track) {
    // Stage the next track before moving. Pending writes to the old track
    // are retained in its own buffer; they do not block head movement.
    uint8_t *next=disk[drive].track((uint8_t)newTrack);
    if(!next){writeFault=true;return;}
    h.halfTrack=newHalf;h.track=newTrack;h.nibPos=0;h.trackBuf=next;
  } else h.halfTrack=newHalf;
  h.phase=p;
  ++stepOp[drive]; // Real stepper transition, including a track-0 hard-stop click.
}
uint8_t TKDiskController::nextNibble(uint64_t cycles) {
  // AppleWin/TK2000 reference: enhancedisk=1 returns a consecutive nibble
  // for each Disk II Q6-low read.  Skipping bytes based on elapsed 6502
  // cycles causes lost prologues/data checksums during DOS second-stage boot.
  (void)cycles;
  if(!motor) return 0;
  Head &h=head[drive];
  if(!h.trackBuf) return 0xFF;
  const size_t size=disk[drive].trackSize();
  if(!size) return 0xFF;
  ++readOp[drive];
  const uint8_t value=h.trackBuf[h.nibPos];
  // Advance in O(1) without integer division on every controller read.
  // This is a very hot path during TKDOS boot and sector loading.
  if (++h.nibPos >= size) h.nibPos=0;
  return value;
}

void TKDiskController::writeNibble(uint64_t cycles) {
  (void)cycles;
  Head &h=head[drive];
  if(!motor||!h.trackBuf||!disk[drive].writable()) return;
  if(!(dataLatch&0x80)) return;
  // TK2000 reference DiskReadWrite(): advance exactly one nibble for each
  // Q6-low write, just as in enhanced read mode. Otherwise time gaps from
  // video rendering can cause skipped positions and corrupt the DSK track.
  ++writeOp[drive];
  h.trackBuf[h.nibPos]=dataLatch;
  disk[drive].markDirty();
  const size_t size=disk[drive].trackSize();
  if (++h.nibPos >= size) h.nibPos=0;
}

uint8_t TKDiskController::read(uint16_t a,uint64_t cycles) {
  const uint8_t sw=(uint8_t)(a-IO_BASE)&0x0F;
  switch(sw) {
    case 0:case 2:case 4:case 6:
      phaseSwitch((sw>>1)&3,false);return 0x7F;
    case 1:case 3:case 5:case 7: phaseSwitch((sw>>1)&3,true);return 0x7F;
    case 8: motorSwitch(false,cycles);return 0x7F;
    case 9: motorSwitch(true,cycles);return 0x7F;
    case 10:selectDrive(0,cycles);return 0x7F;
    case 11:selectDrive(1,cycles);return 0x7F;
    case 12:if(writeMode){writeNibble(cycles);return 0;}return nextNibble(cycles);
    case 13:return motor?dataLatch:0;
    case 14:writeMode=false;return motor?(disk[drive].writable()?0x7F:0xFF):0;
    case 15:writeMode=true;return motor?0xFF:0;
  }
  return 0x7F;
}
void TKDiskController::write(uint16_t a,uint8_t v,uint64_t cycles) {
  const uint8_t sw=(uint8_t)(a-IO_BASE)&0x0F;
  switch(sw) {
    case 0:case 2:case 4:case 6:phaseSwitch((sw>>1)&3,false);break;
    case 1:case 3:case 5:case 7:phaseSwitch((sw>>1)&3,true);break;
    case 8:motorSwitch(false,cycles);break;
    case 9:motorSwitch(true,cycles);break;
    case 10:selectDrive(0,cycles);break;
    case 11:selectDrive(1,cycles);break;
    case 12:if(writeMode)writeNibble(cycles);break;
    case 13:if(motor)dataLatch=v;break;
    case 14:writeMode=false;break;
    case 15:writeMode=true;break;
  }
}
void TKDiskController::serviceAutoSave(uint32_t nowMs) {
  static const uint32_t kWriteQuietMs=1500u;
  static const uint32_t kFailedRetryMs=5000u;
  for(int d=0; d<2; ++d) {
    // Observe actual writes without calling millis() for every Disk II nibble.
    const uint32_t writes=writeOp[d];
    if(writes!=observedWrites[d]) {
      observedWrites[d]=writes;
      lastWriteMs[d]=nowMs;
      attemptedAutoSave[d]=false;
    }
    if(!disk[d].dirty()) {attemptedAutoSave[d]=false;continue;}
    if(!disk[d].mounted() || !disk[d].writable()) continue;
    if(motor && drive==(uint8_t)d && writeMode)continue;
    if((uint32_t)(nowMs-lastWriteMs[d]) < kWriteQuietMs)continue;
    if(attemptedAutoSave[d] &&
       (uint32_t)(nowMs-lastAttemptMs[d]) < kFailedRetryMs)continue;
    attemptedAutoSave[d]=true;
    lastAttemptMs[d]=nowMs;
    if(disk[d].flush()) {
      attemptedAutoSave[d]=false;
      if(!disk[0].dirty() && !disk[1].dirty())writeFault=false;
    } else writeFault=true;
  }
}

bool TKDiskController::mount(int d,const char *p,bool w) {
  if(d<0||d>1)return false;
  if(!disk[d].mount(p,w))return false;
  observedWrites[d]=writeOp[d];
  lastWriteMs[d]=lastAttemptMs[d]=0;
  attemptedAutoSave[d]=false;
  // The F1 selector exchanges media while the CPU is paused. A disk that
  // was just formatted may have left the old head at track 34; carrying this
  // position into an unrelated mounted image makes the next DSK boot start
  // on the wrong track. Start every newly mounted virtual disk at track 0.
  head[d]=Head();
  if(d==drive) { phaseMask=0; dataLatch=0; writeMode=false; }
  return loadTrack(head[d],(uint8_t)d);
}
bool TKDiskController::eject(int d,bool f) {
  if(d<0||d>1)return false;
  if(!disk[d].eject(f))return false;
  observedWrites[d]=writeOp[d];
  lastWriteMs[d]=lastAttemptMs[d]=0;
  attemptedAutoSave[d]=false;
  head[d].trackBuf=nullptr;
  return true;
}
bool TKDiskController::flush() {
  bool ok=true;
  for(int d=0;d<2;++d) {
    if(!disk[d].flush()) {ok=false;writeFault=true;}
  }
  if (ok) {
    writeFault=false;
    for(int d=0;d<2;++d)attemptedAutoSave[d]=false;
  } // Fault is sticky until all disks flush cleanly.
  return ok;
}
