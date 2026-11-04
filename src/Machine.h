#pragma once
#include <stdint.h>
#include <stddef.h>
#include "BuildConfig.h"
#include "CPU6502.h"
#include "Keyboard.h"
#include "Video.h"
#include "Cassette.h"
#include "DiskController.h"
#include "SpeakerTone.h"
#include "DriveSound.h"
#include "TKDriveActivity.h"
#include "PaddleJoystick.h"
#include "TKMissilesAim.h"
#include "TKVideoDirty.h"
#include "TKSaturn128K.h"
class TKMachine {
public:
 TKMachine() = default;
 ~TKMachine();
 TKMachine(const TKMachine&) = delete;
 TKMachine& operator=(const TKMachine&) = delete;
 TKVideoDirty videoDirty;
 // Always-installed Saturn 128K expansion, bank switched via $C0B0-$C0BF.
 TKSaturn128K saturn;
 uint8_t *ram=nullptr; CPU6502 cpu; KeyboardTK keyboard; VideoTK video; CassetteCT2 tape; TKDiskController disk; uint8_t kbout=0,sound=0; bool romSelected=true; uint64_t frameCycles=0;
 // The one-bit speaker transitions are measured in guest CPU cycles;
 // GPIO25 PWM is muted after inactivity (no DC level left on output).
 uint64_t speakerLastToggleCycle=0;
 bool speakerActive=false;
 void beginAudio();
 void setJoystick(uint8_t action, bool down){
   // OFF means no joystick events may reach native TK rows or PDL pins.
   if (!_paddleJoystick.enabled()) { clearJoystick(); return; }
   keyboard.setGameInputProfile(tape.gameInputProfile());
   keyboard.setJoystick(action,down);
   _paddleJoystick.setMask(keyboard.joystickMask());
 }
 void clearJoystick(){keyboard.clearJoystick(); _paddleJoystick.setMask(0);}
 void setJoystickMask(uint8_t mask){
   if (!_paddleJoystick.enabled()) mask=0;
   keyboard.setGameInputProfile(tape.gameInputProfile());
   keyboard.setJoystickMask(mask);
   _paddleJoystick.setMask(mask);
 }
 void setJoystickEnabled(bool enabled){
   keyboard.clearJoystick();
   keyboard.setGameInputProfile(tape.gameInputProfile());
   _paddleJoystick.setEnabled(enabled);
   // Center the passive PDL inputs on BOTH transitions.  OFF means no
   // physical joystick input, not an unplugged/fully-left PDL capacitor.
   _paddleJoystick.trigger(cpu.cycles);
   _missilesAim.reset(cpu.cycles);
 }
 bool joystickEnabled() const {return _paddleJoystick.enabled();}
 void serviceSpeaker();
 // FabGL SoundGenerator mixes guest C030 speaker and realistic drive PCM into
 // the single GPIO25 DAC output; no second sound chip or speaker pin is used.
 void serviceAudio(uint32_t nowMs, bool paused=false);
 // LED states mirror PCM activity envelopes; independent of DRIVE SOUND ON/OFF.
 uint8_t driveLED(int drive, uint32_t nowMs) const;
 void setDriveSoundEnabled(bool enabled);
 bool driveSoundEnabled() const {return _driveSoundEnabled;}
 uint16_t speakerFrequency() const {return _speakerHz;}
 // Takes ownership of a fully validated 16 KiB buffer (allocated with malloc/ps_malloc).
 // Failed loads never replace an already installed ROM.
 bool installSystemROM(uint8_t *data, size_t bytes);
 // Takes ownership of a fully validated 256-byte disk-interface ROM.
 bool installDiskROM(uint8_t *data, size_t bytes);
#if TK2000_K7_MOON_PROFILE
  struct K7MoonCPUStats {
    uint32_t executedInstructions=0;
    uint32_t nativeSkips=0;
    uint32_t romHiddenInstructions=0;
    uint64_t nativeSkippedCycles=0;
  } k7MoonCPUStats;
  K7MoonCPUStats takeK7MoonCPUStats() {
    const K7MoonCPUStats r=k7MoonCPUStats;
    k7MoonCPUStats=K7MoonCPUStats();
    return r;
  }
#endif
 bool hasDiskROM() const { return _diskROM != nullptr; }
 uint32_t diskROMCRC32() const { return _diskROMCRC32; }
 bool hasSystemROM() const { return _systemROM != nullptr; }

 uint32_t systemROMCRC32() const { return _systemROMCRC32; }
 // Reserve 64 KiB emulated RAM in external PSRAM when possible. Calling this
 // before VGA initialization keeps scarce internal DMA RAM available for FabGL.
 bool prepareRAM();
 bool init(); bool reset(); bool coldReset();
 // A real TK-2000 with an installed Disk II interface first displays its
 // ROM-drawn TK2000 title. If D1 is empty, it waits for the RESET key.
 // F12 (warm RESET) then follows the ROM's BASIC fallback; D1 boots normally.
 bool waitingForBootReset() const {return _waitingForBootReset;}
 bool mountDisk(int drive,const char *path,bool writable=true){return disk.mount(drive,path,writable);}
 void ejectDisk(int drive,bool flush=true){disk.eject(drive,flush);}
 bool flushDisks(){return disk.flush();} uint8_t read(uint16_t); void write(uint16_t,uint8_t); void run(uint32_t cycles); void key(uint8_t c, bool down){keyboard.key(c,down);} void setShift(bool down){keyboard.setShift(down);} void setCtrl(bool down){keyboard.setPhysicalCtrl(down);}
private:
 bool resetState(bool cold);
 bool _powerOnBootProbe = false;
 bool _verifiedTapeROM = false;
 bool _waitingForBootReset = false;
 TKSpeakerTone _speakerTone;
 uint16_t _speakerHz=0;
 TKPaddleJoystick _paddleJoystick;
 TKMissilesAim _missilesAim;
 bool _driveSoundEnabled=false;
 TKDriveActivity _driveActivity;
 uint16_t _pwmHz=0;
 uint8_t _pwmDuty=0;
 void setPWMTone(uint16_t hz,uint8_t duty);
 void readDriveSoundState(TKDriveSoundState (&state)[2]) const;
 bool _audioReady=false;
 uint8_t _bootTraceRemaining = 48;
 uint8_t _switchTraceRemaining = 16;
 void traceSwitch(uint16_t addr);
 uint8_t *_systemROM = nullptr;
 uint32_t _systemROMCRC32 = 0;
 uint8_t *_diskROM = nullptr;
 uint32_t _diskROMCRC32 = 0;
};
