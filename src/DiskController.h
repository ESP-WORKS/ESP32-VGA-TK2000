#pragma once
#include <Arduino.h>
#include "DiskImage.h"

// The TK-2000's AUX slot is wired like a Disk II *slot 1* interface:
// registers $C090..$C09F and 256-byte firmware at $C100..$C1FF.
// Reference: tk_memoria.cpp / tk_disco.cpp in apple-II/tk2000-emulator.
class TKDiskController {
public:
  static constexpr uint16_t IO_BASE=0xC090;
  void begin();
  void reset();
  // Reset the virtual drive mechanism to power-on position, retaining media.
  // Flush failure aborts without losing any dirty tracks.
  bool coldReset();
  uint8_t read(uint16_t a,uint64_t cycles);
  void write(uint16_t a,uint8_t v,uint64_t cycles);
  bool mount(int drive,const char *path,bool writable=true);
  bool eject(int drive,bool flush=true);
  bool flush();
  // Save each drive after write inactivity, including sessions where the
  // guest does not stop the motor. All existing DSK validation is retained.
  void serviceAutoSave(uint32_t nowMs);
  bool hasWriteFault() const { return writeFault; }
  uint32_t readOperations(int d) const { return (d>=0&&d<2)?readOp[d]:0; }
  uint32_t writeOperations(int d) const { return (d>=0&&d<2)?writeOp[d]:0; }
  uint32_t stepOperations(int d) const { return (d>=0&&d<2)?stepOp[d]:0; }
  bool motorRunning() const {return motor;}
  int selectedDrive() const {return drive;}
  int currentTrack(int d) const {return d>=0 && d<2 ? head[d].track : -1;}
  int currentHalfTrack(int d) const {return d>=0 && d<2 ? head[d].halfTrack : -1;}
  bool mounted(int drive) const {return drive>=0&&drive<2&&disk[drive].mounted();}
  const char *path(int drive) const {return drive>=0&&drive<2?disk[drive].path():"";}
private:
  struct Head {
    uint8_t phase=0;
    int halfTrack=0;
    int track=0;
    size_t nibPos=0;
    uint8_t *trackBuf=nullptr;
  };
  DiskImage disk[2];
  uint32_t readOp[2]={};
  uint32_t writeOp[2]={};
  uint32_t stepOp[2]={};
  Head head[2];
  uint8_t drive=0, dataLatch=0;
  bool motor=false, writeMode=false;
  bool writeFault=false;
  uint32_t observedWrites[2]={};
  uint32_t lastWriteMs[2]={};
  uint32_t lastAttemptMs[2]={};
  bool attemptedAutoSave[2]={};
  uint8_t phaseMask=0;
  void phaseSwitch(uint8_t phase,bool on);
  void motorSwitch(bool on,uint64_t cycles);
  void selectDrive(uint8_t d,uint64_t cycles);
  bool loadTrack(Head &h,uint8_t d);
  uint8_t nextNibble(uint64_t cycles);
  void writeNibble(uint64_t cycles);
};
