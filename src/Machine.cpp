#include <Arduino.h>
#include "Bus.h"
#include "Machine.h"
#include "BuildConfig.h"
#include "TKTapeROMFast.h"
#include <string.h>
#include <stdlib.h>
#if defined(ESP32)
#include <devdrivers/soundgen.h>  // FabGL 1.0.9: audio header lives under src/devdrivers/.
#include "TKPCMDriveAudio.h"
namespace {
class TKPCMChannel : public fabgl::WaveformGenerator {
public:
  TKPCMDriveAudio mixer;
  void setFrequency(int) override {} // Event-driven PCM, not a tone oscillator.
  int getSample() override { return mixer.nextSample(); }
};
// FabGL VGA uses its video engine; a single SoundGenerator sends all audio
// through GPIO25 DAC. Never enable LEDC on the same pin at the same time.
static fabgl::SoundGenerator tkSoundGenerator(16000, GPIO_NUM_25, fabgl::SoundGenMethod::DAC);
static TKPCMChannel tkPCMChannel;
}
#endif

#ifndef TK2000_BOOT_TRACE
#define TK2000_BOOT_TRACE 0
#endif

TKMachine::~TKMachine(){
  free(ram);
  free(_systemROM);
  free(_diskROM);
}

bool TKMachine::installSystemROM(uint8_t *data, size_t bytes){
  if (!data || bytes != 0x4000) return false;
  // The checksum helps confirm which SD ROM was used without enforcing a
  // single dump: alternate legitimate 16 KiB TK-2000 revisions may work.
  uint32_t crc=0xFFFFFFFFu;
  for (size_t i=0; i<bytes; ++i) {
    crc ^= data[i];
    for (int j=0; j<8; ++j)
      crc = (crc >> 1) ^ ((crc & 1u) ? 0xEDB88320u : 0u);
  }
  if (_systemROM != data) free(_systemROM);
  _systemROM=data;
  // A retry can replace the ROM while an already-attached CPU exists.
  // Refresh the fast window immediately to avoid a stale/freed ROM pointer.
  if (ram && cpu.bus) cpu.attachFastMemory(ram,_systemROM,&romSelected,&videoDirty);
  // This ROM places a JMP to its initialization routine at $FB2E, whereas
  // a legacy TKDOS bootstrap uses the Apple II entry $FB2F. The CPU has a
  // tightly scoped guest-call alias, enabled only for this exact opcode.
  cpu.tkdosLegacyBootAlias = (data[0x3B2E]==0x4C &&
                              data[0x3B2F]==0x17 && data[0x3B30]==0xF1);
  _verifiedTapeROM=TKTapeROMFast::supported(data,bytes);
  _systemROMCRC32=crc ^ 0xFFFFFFFFu;
  return true;
}

bool TKMachine::installDiskROM(uint8_t *data, size_t bytes){
  if (!data || bytes != 0x0100) return false;
  // The interface ROM can have legitimate alternate revisions. Record a CRC
  // for diagnostics, but do not require one fixed copyrighted ROM dump.
  uint32_t crc=0xFFFFFFFFu;
  for (size_t i=0; i<bytes; ++i) {
    crc ^= data[i];
    for (int j=0; j<8; ++j)
      crc = (crc >> 1) ^ ((crc & 1u) ? 0xEDB88320u : 0u);
  }
  if (_diskROM != data) free(_diskROM);
  _diskROM=data;
  _diskROMCRC32=crc ^ 0xFFFFFFFFu;
  return true;
}

bool TKMachine::prepareRAM(){
  if (ram) return saturn.allocate();
  // ESP32 PSRAM memory is fine for CPU/video emulation, but not for the
  // FabGL I2S DMA framebuffer. Keep the latter in internal RAM.
  ram=(uint8_t*)ps_malloc(65536u);
  if (!ram) ram=(uint8_t*)malloc(65536u);
  if (!ram || !saturn.allocate()) return false;
  memset(ram,0,65536u);
  return true;
}

bool TKMachine::init(){
  // Load both 16 KiB system and 256-byte disk ROMs before any 6502 reset.
  if (!_systemROM || !_diskROM) return false;
  if (!prepareRAM()) return false;
  memset(ram,0,65536u);
  videoDirty.reset();
  if (!cpu.bus) {
    TKBus *b = new TKBus();
    b->m=this;
    cpu.attach(b);
  }
  // Refresh pointers on every init/reload (the ROM can be replaced from SD).
  cpu.attachFastMemory(ram,_systemROM,&romSelected,&videoDirty);
  cpu.attachSaturn(&saturn);
  disk.begin();
  if (!reset()) return false;
  // The SD disks are mounted just AFTER init() by the sketch. Arm the first
  // ROM startup probe regardless of current D1 status; when it reaches the
  // Disk II entry, we will then know whether a disk was actually mounted.
  _powerOnBootProbe=true;
  return true;
}

bool TKMachine::reset(){
  return resetState(false);
}

bool TKMachine::coldReset(){
  return resetState(true);
}

bool TKMachine::resetState(bool cold){
  if (!_systemROM || !_diskROM || !ram) return false;
  // Never erase guest RAM before pending DSK/NIB writes are committed.
  // An unsuccessful flush leaves CPU, RAM, keyboard and mounted disks intact.
  if (cold) {
    if (!disk.coldReset()) return false;
    // MC-1000 F10-style cold start: clear all writable guest memory, including
    // the 6502 stack, zero page and both possible video memory pages.
    memset(ram, 0, 65536u);
    videoDirty.reset();
    cpu.A=0; cpu.X=0; cpu.Y=0; cpu.pageCrossed=false;
    frameCycles=0;
    // Stop and rewind any in-progress cassette, without affecting SD media.
    tape.resetTransport();
  } else {
    // F12 behaves like the machine's physical RESET: preserve the BASIC RAM
    // and do not mechanically home the disk heads.
    if (!disk.flush()) return false;
    disk.reset();
    if (_waitingForBootReset && _systemROM &&
        _systemROM[0x3A7Eu]==0xADu && _systemROM[0x3A7Fu]==0xF3u &&
        _systemROM[0x3A80u]==0x03u && _systemROM[0x3A81u]==0x49u &&
        _systemROM[0x3A82u]==0xA5u && _systemROM[0x3A83u]==0xCDu &&
        _systemROM[0x3A84u]==0xF4u && _systemROM[0x3A85u]==0x03u) {
      // Only the validated TK-2000 ROM's warm-start signature is modified.
      // The ROM wrote a warm-start signature at $03F3/$03F4 BEFORE entering
      // the disk bootstrap. With no disk, BASIC initialization has NOT yet
      // finished: honoring that signature here can jump into a half-initialized
      // interpreter, leaving the screen blank. Invalidate only its checksum
      // upon the first physical-equivalent RESET, so the *original ROM* runs
      // the correct BASIC cold-entry path. Do not erase guest RAM or touch
      // this signature for subsequent F12 resets while BASIC/games run.
      ram[0x03F4u] ^= 0x01u;
    }
  }
  // Power-on and COLD RESET re-arm the original no-disk title/RESET wait.
  // F12 is the physical-equivalent warm RESET that exits the wait into BASIC.
  _powerOnBootProbe=cold;
  _waitingForBootReset=false;
  videoDirty.reset(); // invalidate cached screen even after a warm RESET
  romSelected=true;
  saturn.resetSwitches();
  keyboard.clear();
  keyboard.setGameInputProfile(tape.gameInputProfile());
  _paddleJoystick.setMask(0);
  _missilesAim.reset(cpu.cycles);
  cpu.irq_pending=false;
  cpu.nmi_pending=false;
  kbout=0; sound=0; speakerLastToggleCycle=0; speakerActive=false;
  _speakerTone.reset();
  _speakerHz=0;
  TKDriveSoundState resetDrives[2]; readDriveSoundState(resetDrives);
  _driveActivity.arm(resetDrives);
#if defined(ESP32)
  if (_audioReady) {
    tkPCMChannel.mixer.setSpeakerHz(0);
    TKDriveSoundState st[2]; readDriveSoundState(st);
    tkPCMChannel.mixer.arm(st);
  }
#endif
  video.page=1; video.color=false; video.inverse=false; // Clean power-on text; C050 enables artifact color
  cpu.reset();
  _bootTraceRemaining=48;
  _switchTraceRemaining=16;
  return true;
}

uint8_t TKMachine::read(uint16_t a){
  // The overwhelming majority of 6502 accesses are direct RAM or $C200+
  // ROM fetches. Dispatch both before evaluating slow MMIO ranges.
  if (a < 0xC000u) return ram[a];
  if (a >= 0xC200u) {
    if (a >= 0xD000u && romSelected && saturn.readEnabled()) return saturn.read(a);
    return romSelected ? (_systemROM ? _systemROM[a-0xC000u] : 0xFFu) : ram[a];
  }
  // Independent AUX peripheral decode: MiSTer rtl/tk2000.v explicitly
  // prioritizes the expansion ROM at $C100-$C1FF whenever the disk
  // interface is installed, independent of $C05A/$C05B RAM/ROM bank state.
  // A selected CT2 does NOT unplug the Disk II expansion interface. Hide
  // its C100 ROM / C090 I/O only while the cassette operation is in flight.
  // Multi-stage loaders keep it hidden during auto-pauses; reaching 100%
  // reconnects it without F3/eject, as does completion of a tape recording.
  const bool diskSlotActive = _diskROM && !tape.operationActive();
  if(a>=0xC100 && a<=0xC1FF && diskSlotActive) {
    // On the stock TK-2000 ROM the power-on code at $FABE probes a disk
    // interface signature, then unconditionally jumps to $C100. At initial
    // power-on/COLD RESET we deliberately expose the genuine Disk II slot:
    // if D1 is empty, the ROM will show its real TK2000 header and our run()
    // loop waits at the first $C100 entry until F12 (warm RESET). After F12
    // only, make THIS PARTICULAR BIOS signature probe see an empty slot, so
    // the original ROM enters its BASIC fallback. Mounting D1 with F2 and
    // issuing DSK still executes the unmodified disk firmware. Alternate
    // system ROMs do not get an unsafe signature override.
    // See tests/test_dsk_late_mount_v032_optional.cpp.
    if (a==0xC107u && !_powerOnBootProbe && !disk.mounted(0) && _systemROM && ram &&
        cpu.PC==0xFAC0u && cpu.Y==7u && ram[0]==0 && ram[1]==0xC1u &&
        _systemROM[0x3ABEu]==0xB1u && _systemROM[0x3ABFu]==0x00u &&
        _systemROM[0x3AC0u]==0xD9u && _systemROM[0x3AC1u]==0x8Fu &&
        _systemROM[0x3AC2u]==0xF5u && _systemROM[0x3AC3u]==0xD0u &&
        _systemROM[0x3AC4u]==0xCFu)
      return 0xFFu; // fail the signature probe without faking a disk read

    return _diskROM[a-0xC100];
  }
  if(a>=0xC100)
    return romSelected ? (_systemROM ? _systemROM[a-0xC000] : 0xFF) : ram[a];

  // Keyboard output latch is mirrored across C000-C00F.
  if(a>=0xC000 && a<=0xC00F) return keyboard.out;

  // Keyboard input is mirrored across C010-C01F. Reading does not consume
  // a character: software selects one or more rows via KBOUT and scans them.
  if(a>=0xC010 && a<=0xC01F) {
    // $FCFE: LDA $C010, $FD01: EOR $2F. The TK cassette ROM detects
    // polarity changes through the high bit of the keyboard INPUT port.
    // Never route tape through $C070: it is the independent paddle strobe.
    const uint8_t matrix=keyboard.in();
    if(tape.qualifiesInput(cpu.PC,cpu.cycles)) {
      if(!tape.mounted() && !tape.recording()) tape.requestLoad();
      if(tape.mounted() && (tape.mayRestartRead(cpu.cycles) ||
                            (!tape.playing() && !tape.paused() && !tape.finished())))
        tape.beginPlay();
      return (uint8_t)((matrix & 0x7Fu) | tape.read(cpu.cycles));
    }
    return matrix;
  }

  // Original ROM's $FCE5 uses LDY $C020 to TOGGLE cassette output, not
  // merely read a floating bus. The save dialog is triggered by the precise
  // ROM call site; unrelated programs polling these addresses are unaffected.
  if(a>=0xC020 && a<=0xC02F) {
    // LOADA runs a short tape-output calibration *before* its input loop.
    // The original ROM calls $FCE5 from $FF05 in that case (stacked RTS
    // address $FF04); do NOT misinterpret that calibration as SAVEA.
    const uint8_t st=cpu.S;
    bool loadaCalibration=false;
    if(cpu.PC==0xFCE8u && st<=0xFAu){
      // The $FF04 return can be either the immediate caller or one
      // level deeper while the original ROM emits its calibration pulses.
      for(unsigned level=1;level<=5;level+=2){
        if(ram[0x100u+(uint8_t)(st+level)]==0x04u &&
           ram[0x100u+(uint8_t)(st+level+1u)]==0xFFu){
          loadaCalibration=true;break;
        }
      }
    }
    if(cpu.PC==0xFCE8u && !tape.recording() && !loadaCalibration)
      tape.requestSave();
    tape.outputToggle(cpu.cycles);
    return 0xFF;
  }
  if(a>=0xC030 && a<=0xC03F){
    sound^=0x80; speakerLastToggleCycle=cpu.cycles; speakerActive=true;
    _speakerTone.edge(cpu.cycles);
    // The speaker soft-switch is side-effect-only. The TK2000 master
    // reference returns open bus/high bits (0xFF), NOT our DAC state.
    return 0xFF;
  }

  if(a>=0xC090 && a<=0xC09F) return diskSlotActive ? disk.read(a,cpu.cycles) : 0x7Fu;
  if(a>=0xC0B0 && a<=0xC0BF) { saturn.touch(a); return 0x7Fu; }
  if(a==0xC070) {
    // Preserve the TK cassette result, and retrigger the passive PDL timers
    // even with F1 JOYSTICK OFF.  Apple-II-style games sample C064/C065
    // after C070: returning open-bus 0x7F unconditionally makes PREAD=0
    // (hard LEFT).  A disconnected joystick must report the centered value,
    // not a permanently discharged analog timer.  No input is accepted OFF.
    _paddleJoystick.trigger(cpu.cycles);
    return 0x7Fu;
  }
  if(a>=0xC061 && a<=0xC067)
    return _paddleJoystick.read(a,cpu.cycles);
  if(a==0xC071) return 0x7F;

  if(a>=0xC050 && a<=0xC05F){
    video.writeSwitch(a);
    if(a==0xC05A) romSelected=true;  // MiSTer: bit 5=0 -> ROM overlay
    if(a==0xC05B) romSelected=false; // MiSTer: bit 5=1 -> RAM overlay
    if(a==0xC05A || a==0xC05B) traceSwitch(a);
    if(a==0xC05E) keyboard.ctrl=false;
    if(a==0xC05F) keyboard.ctrl=true;
    return 0x7F;
  }

  if(a>=0xC000 && a<0xC100) return 0x7F;
  return ram[a];
}

void TKMachine::write(uint16_t a,uint8_t v){
  if (a < 0xC000u) {
    ram[a]=v;
    if ((a>=0x2000u && a<0x4000u) || (a>=0xA000u)) videoDirty.mark(a);
    return;
  }
  if (a >= 0xC200u) {
    if (a >= 0xD000u && romSelected && saturn.writeEnabled()) saturn.write(a,v);
    else if (!romSelected) ram[a]=v;
    return;
  }
  if(a>=0xC100 && a<=0xC1FF && _diskROM && !tape.operationActive())
    return; // physically connected expansion ROM only
  if(a>=0xC100){
    if(!romSelected) ram[a]=v;
    return;
  }

  // KBOUT is decoded on the whole C000-C00F range, not just C000.
  if(a>=0xC000 && a<=0xC00F){
    kbout=v; keyboard.out=v; tape.keyboardRowChanged(); return;
  }

  if(a>=0xC090 && a<=0xC09F){
    if(_diskROM && !tape.operationActive())
      disk.write(a,v,cpu.cycles);
    return;
  }
  if(a>=0xC0B0 && a<=0xC0BF){ saturn.touch(a); return; }
  if (a==0xC070) {
    // Reading/writing the trigger must behave identically in OFF and ON;
    // the TK cassette input on reads remains untouched.
    _paddleJoystick.trigger(cpu.cycles); return;
  }
  if(a>=0xC020 && a<=0xC02F) {
    // LOADA runs a short tape-output calibration *before* its input loop.
    // The original ROM calls $FCE5 from $FF05 in that case (stacked RTS
    // address $FF04); do NOT misinterpret that calibration as SAVEA.
    const uint8_t st=cpu.S;
    bool loadaCalibration=false;
    if(cpu.PC==0xFCE8u && st<=0xFAu){
      // The $FF04 return can be either the immediate caller or one
      // level deeper while the original ROM emits its calibration pulses.
      for(unsigned level=1;level<=5;level+=2){
        if(ram[0x100u+(uint8_t)(st+level)]==0x04u &&
           ram[0x100u+(uint8_t)(st+level+1u)]==0xFFu){
          loadaCalibration=true;break;
        }
      }
    }
    if(cpu.PC==0xFCE8u && !tape.recording() && !loadaCalibration)
      tape.requestSave();
    tape.outputToggle(cpu.cycles);return;
  }
  if(a>=0xC030 && a<=0xC03F){
    sound^=0x80; speakerLastToggleCycle=cpu.cycles; speakerActive=true;
    _speakerTone.edge(cpu.cycles); return;
  }

  if(a>=0xC050 && a<=0xC05F){
    video.writeSwitch(a);
    if(a==0xC05A) romSelected=true;  // MiSTer: bit 5=0 -> ROM overlay
    if(a==0xC05B) romSelected=false; // MiSTer: bit 5=1 -> RAM overlay
    if(a==0xC05A || a==0xC05B) traceSwitch(a);
    if(a==0xC05E) keyboard.ctrl=false;
    if(a==0xC05F) keyboard.ctrl=true;
    return;
  }

  if(a>=0xC000 && a<0xC100) return;
  ram[a]=v;
}

void TKMachine::traceSwitch(uint16_t addr) {
#if defined(ESP32) && TK2000_BOOT_TRACE
  if (_switchTraceRemaining) {
    Serial.printf("[6502 MAP] switch=%04X => %s at PC=%04X\n",
                  addr, romSelected?"ROM":"RAM",cpu.PC);
    --_switchTraceRemaining;
  }
#else
  (void)addr;
#endif
}

void TKMachine::run(uint32_t cycles){
  // The supplied Moon Patrol CT2 calls Apple's PREAD at $FB1E from its
  // wrapper $0766. TK-2000 ROM $FB1E is only SEC/RTS, so Y never gets
  // updated. Enable a Y-coordinate compatibility shim only for this CT2
  // and with F4 ON. All other titles execute the untouched original ROM.
  cpu.moonPatrolPreadCompat=(tape.gameInputProfile()==TKGameInputProfile::MoonPatrol &&
                             _paddleJoystick.enabled());
  if (cpu.moonPatrolPreadCompat) {
    cpu.moonPatrolPreadX=_paddleJoystick.positionForAxis(0);
    cpu.moonPatrolPreadY=_paddleJoystick.positionForAxis(1);
  }
  cpu.missilesDefensePreadCompat=(tape.gameInputProfile()==TKGameInputProfile::MissilesDefense &&
                                 _paddleJoystick.enabled());
  if (cpu.missilesDefensePreadCompat) {
    // Persistent absolute analog aim, not full-left/full-right then auto-center.
    _missilesAim.advance(cpu.cycles,keyboard.joystickMask());
    cpu.missilesDefensePreadX=_missilesAim.positionForAxis(0);
    cpu.missilesDefensePreadY=_missilesAim.positionForAxis(1);
  }
  // A no-disk power-on stops just as the REAL ROM enters the Disk II slot,
  // AFTER drawing the TK2000 header but BEFORE starting a BASIC prompt.
  // This is a guest CPU wait, not a VGA popup; F1/F2/VGA/audio still work.
  // Crucially, the extra PC comparison is active only during early boot;
  // normal games retain the fast CPU loop and its 60 EPS performance work.
  if (_waitingForBootReset) return;
  const uint64_t target=cpu.cycles+cycles;
  if (_powerOnBootProbe) {
    while(cpu.cycles<target) {
      if (cpu.PC==0xC100u) {
        _powerOnBootProbe=false;
        if (!disk.mounted(0)) {
          _waitingForBootReset=true;
          return;
        }
        break; // Bootable D1: continue at $C100 in the normal fast loop.
      }
      cpu.step();
    }
  }
  // K7 fast ROM: account for the same guest cycles without interpreting
  // thousands of identical polling/delay instructions per tape byte.
  // Normal CPU operation, custom cassette readers and unknown ROMs retain
  // the original interpreter unchanged.
  if (_verifiedTapeROM && (tape.playing() || tape.recording())) {
#if TK2000_K7_MOON_PROFILE
    const bool profileMoon=tape.gameInputProfile()==TKGameInputProfile::MoonPatrol;
    uint32_t instructionCount=0,skipCount=0,romHiddenCount=0;
    uint64_t skippedCycles=0;
#endif
    while(cpu.cycles<target) {
#if TK2000_K7_MOON_PROFILE
      const uint64_t before=profileMoon?cpu.cycles:0u;
#endif
      if (!romSelected || !TKTapeROMFast::skip(cpu,tape,target,
                                               tape.playing(),tape.recording())) {
        cpu.step();
#if TK2000_K7_MOON_PROFILE
        if(profileMoon) {
          ++instructionCount;
          if(!romSelected) ++romHiddenCount;
        }
#endif
      }
#if TK2000_K7_MOON_PROFILE
      else if(profileMoon) {
        ++skipCount;
        skippedCycles+=cpu.cycles-before;
      }
#endif
    }
#if TK2000_K7_MOON_PROFILE
    if(profileMoon) {
      k7MoonCPUStats.executedInstructions+=instructionCount;
      k7MoonCPUStats.nativeSkips+=skipCount;
      k7MoonCPUStats.romHiddenInstructions+=romHiddenCount;
      k7MoonCPUStats.nativeSkippedCycles+=skippedCycles;
    }
#endif
    return;
  }
  while(cpu.cycles<target) {
#if defined(ESP32) && TK2000_BOOT_TRACE
    // Bounded first-instruction trace: no continual Serial I/O in the hot path.
    if (_bootTraceRemaining) {
      Serial.printf("[6502 BOOT] PC=%04X A=%02X X=%02X Y=%02X P=%02X SP=%02X ROM=%u cycles=%llu\n",
                    cpu.PC, cpu.A, cpu.X, cpu.Y, cpu.P, cpu.S,
                    romSelected ? 1u : 0u, (unsigned long long)cpu.cycles);
      --_bootTraceRemaining;
    }
#endif
    cpu.step();
  }
}

void TKMachine::beginAudio() {
#if defined(ESP32)
  if (_audioReady) return;
  // FabGL I2S0 DAC, not the single-frequency LEDC oscillator. This supports
  // actual sampled noise with transients and mixes the TK C030 speaker into
  // the SAME GPIO25 output; it does not require extra wiring.
  tkPCMChannel.mixer.reset();
  tkSoundGenerator.attach(&tkPCMChannel);
  tkPCMChannel.enable(true);
  tkPCMChannel.setVolume(127);
  tkSoundGenerator.setVolume(127);
  tkSoundGenerator.play(true);
  if(!tkSoundGenerator.playing()) {
    Serial.println("[AUDIO] FabGL DAC unavailable; sound muted");
    _audioReady=false;
    return;
  }
#endif
  _audioReady=true;
}

void TKMachine::readDriveSoundState(TKDriveSoundState (&state)[2]) const {
  for(int d=0;d<2;++d) {
    state[d].reads=disk.readOperations(d);
    state[d].writes=disk.writeOperations(d);
    state[d].steps=disk.stepOperations(d);
    state[d].motor=disk.motorRunning() && disk.selectedDrive()==d;
  }
}

void TKMachine::setDriveSoundEnabled(bool enabled) {
  if(enabled==_driveSoundEnabled)return;
  _driveSoundEnabled=enabled;
  TKDriveSoundState st[2]; readDriveSoundState(st);
#if defined(ESP32)
  if(_audioReady) {
    tkPCMChannel.mixer.arm(st);
    tkPCMChannel.mixer.setDriveEnabled(enabled);
  }
#endif
}

void TKMachine::serviceSpeaker() {
  const uint16_t hz=_speakerTone.frequency(cpu.cycles);
  speakerActive=_speakerTone.active();
  if(hz==_speakerHz)return;
  _speakerHz=hz;
#if defined(ESP32)
  if(_audioReady)tkPCMChannel.mixer.setSpeakerHz(hz);
#endif
}

uint8_t TKMachine::driveLED(int drive, uint32_t nowMs) const {
#if defined(ESP32)
  // The audio ISR owns the real seek queue and sample envelopes. Poll its
  // current LED state when sound is running; otherwise retain the silent
  // counter-based LED implementation for DRIVE SOUND OFF.
  if (_audioReady && _driveSoundEnabled)
    return tkPCMChannel.mixer.ledPCM(drive);
#endif
  return _driveActivity.led(drive,nowMs);
}

void TKMachine::serviceAudio(uint32_t nowMs, bool paused) {
  serviceSpeaker();
  TKDriveSoundState st[2];
  readDriveSoundState(st);
  _driveActivity.update(st,nowMs);
#if defined(ESP32)
  if(!_audioReady)return;
  tkPCMChannel.mixer.update(st);
  tkPCMChannel.mixer.setPaused(paused);
#else
  (void)paused;
#endif
}
