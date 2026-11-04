#include <Arduino.h>
#include "BuildConfig.h"
#include <SPI.h>
#include <SD.h>
#include "fabgl.h"
#include "Machine.h"
#include "SystemROM.h"
#include "VGA.h"
#include "SDSupport.h"
#include "DiskBrowser.h"
#include "TapeBrowser.h"
#include "FpsEpsMeter.h"
#include "FramePacer.h"
#include "JoystickKeys.h"
#include "JoystickInput.h"
#include "TKCPUPacing.h"
#include "TKCassetteTurbo.h"
#include "TKMoonTurboPolicy.h"
#include "TKTapeJournal.h"
#include "TKKeyboardLayoutABNT2.h"
#include "TKModifierSides.h"
#include <esp_timer.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <ctype.h>
#include <string.h>

static fabgl::PS2Controller ps2;
static fabgl::Keyboard* kbd;
static TKMachine machine;
static TKVGA vga;
static TKFramePacer framePacer;
static uint32_t lastCpuMicros=0;
static uint16_t cpuCycleRemainder=0;
static uint32_t cpuCycleBudget=0;
static uint8_t lastCassetteRate=1;
// Diagnostics: report measured guest throughput during cassette activity.
// No periodic work outside K7; helps distinguish host CPU saturation from
// the ~8x target without drawing extra OSD elements over the game.
static uint32_t lastK7ReportMs=0;
static uint64_t lastK7ReportCycles=0;
static uint16_t measuredMoonTurboHundredths=0;
static uint32_t moonVideoFrameSequence=0;
#if TK2000_K7_MOON_PROFILE
struct TK2000MoonK7Profile {
  uint64_t cpuUs=0, vgaUs=0, droppedCycles=0;
  uint32_t queueLimitHits=0, burstLimitHits=0;
  uint32_t readSamples=0, decodedEdges=0;
  void reset() {
    cpuUs=vgaUs=droppedCycles=0;
    queueLimitHits=burstLimitHits=0;
  }
};
static TK2000MoonK7Profile moonK7Profile;
#endif
static TKFPS_EPS_Meter fpsMeter;
static bool fpsMeterActive=false;
static uint16_t fpsValue=0;
static uint16_t epsValue=0;
static uint32_t lastIdleYieldMs=0;
// Autosave only needs to check at 20ms intervals (quiet window >=1500ms),
// and the FabGL PCM mixer retains state between 2ms control updates.
// Disk II nibble read/write and C030 speaker edges still happen in the CPU
// MMIO path at their original guest-cycle timing.
static uint32_t lastAutoSavePollMs=0;
static uint32_t lastAudioPollMs=0;
#ifndef TK2000_PERF_TRACE
#define TK2000_PERF_TRACE 0
#endif
#if TK2000_PERF_TRACE
struct TKPerfTrace {
  uint64_t cpuUs=0, videoUs=0, sdUs=0, audioUs=0;
  uint32_t worstCPU=0, worstVideo=0, worstSD=0, beginMs=0;
  uint32_t cpuSlices=0, frames=0;
};
static TKPerfTrace perf;
#endif
#ifndef TK2000_PERIODIC_HEALTH_LOG
#define TK2000_PERIODIC_HEALTH_LOG 0
#endif
#if TK2000_PERIODIC_HEALTH_LOG
static uint32_t lastHealthReportMs=0;
#endif
static bool showFPS=true; // v0.38: display the validated FPS/EPS meter on boot.
static bool f3KeyLatched=false;
static bool f4KeyLatched=false;
static uint32_t joystickStatusUntilMs=0;
static bool joystickStatusVisible=false;
static bool f1KeyLatched=false;
static bool f2KeyLatched=false;
static bool f12KeyLatched=false;
// Both PC left/right modifiers feed one original TK line. A release on one
// side must not cancel the other side's still-held SHIFT or CONTROL.
static TKModifierSides modifierSides;
static bool joystickEnabled=false; // User-selectable; OFF on every ESP32 boot.
static bool keyboardBR=true; // BR/ABNT2 is the default; US remains selectable in F1.
static bool diskLEDsEnabled=true; // Display D1/D2 activity LEDs by default; does not alter drive I/O.
static constexpr bool artifactsColorEnabled=true; // HGR artifact colors always enabled (validated v0.35 palette).
static bool driveSoundEnabled=true; // Validated FabGL PCM Disk II sound; ON by default.
static uint32_t lastJoystickPollMs=0;
static TKJoystickInput joystickKeys;
static bool mainMenuOpen=false;
static int mainMenuSelected=0;
static bool mainMenuEnterLatched=false; // One toggle per physical Enter press.
static bool emulatorReady=false;
static bool vgaReady=false;
// D1/D2 LED colours are derived from the SAME 2ms Disk II events and
// envelope lengths as FabGL PCM; no independent 90ms LED hold timer.
// Events are still visible when DRIVE SOUND is OFF.
static uint8_t diskLedState[2]={0,0};
static void refreshDiskLEDs(uint32_t nowMs){
  diskLedState[0]=machine.driveLED(0,nowMs);
  diskLedState[1]=machine.driveLED(1,nowMs);
}
static const char *startupError="";


#if TK2000_PERIODIC_HEALTH_LOG
static uint32_t activeVideoBytes(){
  if (!machine.ram) return 0;
  const uint32_t base=machine.video.page == 1 ? 0x2000u : 0xA000u;
  uint32_t count=0;
  for (uint32_t i=0;i<0x2000u;++i) if (machine.ram[base+i]) ++count;
  return count;
}
#endif

// ESP32 restart is only available from F1; F12 remains a direct warm reset.
// Flush disk changes before restarting so the reset does not discard writes.
static void resetESP32() {
  Serial.println("MENU F1: full ESP32 reset requested");
  bool disksOk = machine.flushDisks();
  Serial.printf("MENU F1: disk flush %s\n", disksOk ? "OK" : "ERROR");
  if (!disksOk) {
    Serial.println("MENU F1 ABORTED: disk changes could not be saved; check SD card");
    return;
  }
  TKUnmountSD();
  // Reinicio do ESP32: a selecao otadata ja foi limpa em setup(), permitindo o factory loader.
  Serial.println("MENU F1: restarting ESP32 (factory loader no proximo boot)...");
  delay(150);
  ESP.restart();
  while (true) delay(1000);
}

static bool mountDiskFromSD(int drive) {
  char p1[40], p2[40];
  snprintf(p1,sizeof(p1),"/TK-2000/DISKS/DISK%d.NIB",drive+1);
  snprintf(p2,sizeof(p2),"/TK-2000/DISKS/DISK%d.DSK",drive+1);
  // Prefer NIB, but a corrupt/unreadable NIB must not hide a valid DSK.
  if(SD.exists(p1) && machine.mountDisk(drive,p1,true))return true;
  if(SD.exists(p2) && machine.mountDisk(drive,p2,true))return true;
  return false;
}

static void diskStatus() {
  Serial.printf("D1: %s\n", machine.disk.mounted(0)?machine.disk.path(0):"empty");
  Serial.printf("D2: %s\n", machine.disk.mounted(1)?machine.disk.path(1):"empty");
}

// F2 image browser: the complete, dynamically growing index lives in PSRAM.
// The VGA only sees a 15-line scrolling window. No fixed 48-file limit.
static TKDiskBrowser diskBrowser;
static int diskSelected=0;
static int diskScroll=0;
static int diskMenuDrive=0;
static bool diskMenuOpen=false;
static char diskMessage[96]="";

static void clearHostModifiers(){
  modifierSides.clear();
  machine.setShift(false);
  machine.setCtrl(false);
}
static void refreshDiskList(){
  diskSelected=0;
  diskScroll=0;
  const bool complete=diskBrowser.refresh();
  if(!diskBrowser.directoryFound())
    snprintf(diskMessage,sizeof(diskMessage),"PASTA /TK-2000/DISKS NAO ENCONTRADA");
  else if(!complete)
    snprintf(diskMessage,sizeof(diskMessage),"LISTA PARCIAL: PSRAM INSUFICIENTE (%d)",diskBrowser.count());
  else
    snprintf(diskMessage,sizeof(diskMessage),"%d IMAGENS - USE SETAS PARA ROLAR",diskBrowser.count());
  Serial.printf("[F2] %d disk images; PSRAM list=%u bytes; complete=%s\n",
                diskBrowser.count(),(unsigned)diskBrowser.bytesReserved(),complete?"YES":"NO");
}
static void drawDiskMenu(){
  if(!diskMenuOpen || !vgaReady)return;
  const char *names[TKDiskBrowser::kVisibleRows];
  for(int i=0;i<TKDiskBrowser::kVisibleRows;++i){
    const char* path=diskBrowser.pathAt(diskScroll+i);
    if(!path)names[i]="";
    else {
      const char* n=strrchr(path,'/');
      names[i]=n?n+1:path;
    }
  }
  vga.showDiskMenu(diskMenuDrive,machine.disk.path(0),machine.disk.path(1),
                   names,TKDiskBrowser::kVisibleRows,diskBrowser.count(),
                   diskSelected,diskScroll,diskMessage);
}
static void updateDiskMenuNavigation(int previousSelection,int previousScroll){
  if(!diskMenuOpen||!vgaReady||diskSelected==previousSelection)return;
  const char *names[TKDiskBrowser::kVisibleRows];
  for(int i=0;i<TKDiskBrowser::kVisibleRows;++i){
    const char* path=diskBrowser.pathAt(diskScroll+i);
    const char* base=path?strrchr(path,'/'):nullptr;
    names[i]=path?(base?base+1:path):"";
  }
  if(diskScroll==previousScroll)
    vga.updateDiskMenuSelection(previousSelection,diskSelected,diskScroll,names,
                                TKDiskBrowser::kVisibleRows,diskBrowser.count());
  else vga.updateDiskMenuScroll(names,TKDiskBrowser::kVisibleRows,
                                diskBrowser.count(),diskSelected,diskScroll);
}
static void closeDiskMenu(){
  diskMenuOpen=false;
  machine.keyboard.releaseInputs(); // release keys whose PS/2 KEYUP was consumed by F2
  clearHostModifiers();
  joystickKeys.clear(); machine.clearJoystick();
  vga.dismissDiskMenu();
  diskBrowser.clear(); // release every reserved PSRAM byte until next F2
  lastCpuMicros=micros();cpuCycleBudget=0;cpuCycleRemainder=0;
  fpsMeterActive=false; // Pause/menu time must not reduce measured EPS.
  framePacer.reset();
}
static void openDiskMenu(){
  if(!emulatorReady)return;
  machine.keyboard.releaseInputs(); // do not leave cursor/SPACE held in guest
  clearHostModifiers();
  joystickKeys.clear(); machine.clearJoystick();
  refreshDiskList();diskMenuOpen=true;diskMenuDrive=0;
  drawDiskMenu();
}
static void diskMenuKey(fabgl::VirtualKey vk){
  if(vk==fabgl::VK_ESCAPE){closeDiskMenu();return;}
  const bool navigation=(vk==fabgl::VK_UP || vk==fabgl::VK_DOWN);
  const int priorSelection=diskSelected,priorScroll=diskScroll;
  if(vk==fabgl::VK_LEFT || vk==fabgl::VK_1) diskMenuDrive=0;
  if(vk==fabgl::VK_RIGHT || vk==fabgl::VK_2) diskMenuDrive=1;
  if(vk==fabgl::VK_UP && diskSelected>0)--diskSelected;
  if(vk==fabgl::VK_DOWN && diskSelected+1<diskBrowser.count())++diskSelected;
  if(vk==fabgl::VK_e || vk==fabgl::VK_E){
    if(machine.disk.eject(diskMenuDrive,true))
      snprintf(diskMessage,sizeof(diskMessage),"D%d EJETADO",diskMenuDrive+1);
    else snprintf(diskMessage,sizeof(diskMessage),"ERRO SALVANDO D%d; MANTIDO",diskMenuDrive+1);
  }
  if((vk==fabgl::VK_RETURN || vk==fabgl::VK_KP_ENTER) && diskBrowser.count()>0){
    const char *path=diskBrowser.pathAt(diskSelected);
    if(path && machine.mountDisk(diskMenuDrive,path,true)) {
      snprintf(diskMessage,sizeof(diskMessage),"D%d MONTADO: %.65s",diskMenuDrive+1,path);
      Serial.printf("[F2] D%d mounted: %s\n",diskMenuDrive+1,path);
    } else {
      snprintf(diskMessage,sizeof(diskMessage),"ERRO MONTANDO D%d - VERIFIQUE SD",diskMenuDrive+1);
      Serial.printf("[F2] D%d mount FAILED: %s\n",diskMenuDrive+1,path?path:"(none)");
    }
  }
  // Continuous one-item navigation; scroll only as the selection crosses
  // the first/last visible row. No manual pages or fixed-image limit.
  diskScroll=TKDiskBrowser::scrollForSelection(
      diskSelected,diskScroll,diskBrowser.count());
  if(navigation)updateDiskMenuNavigation(priorSelection,priorScroll);
  else drawDiskMenu();
}

// F3 cassette selector is intentionally independent of F2's disk browser.
// Only ONE virtual tape is mounted, regardless of ROM motor A/B controls.
static TKTapeBrowser tapeBrowser;
static bool tapeMenuOpen=false;
static bool tapeMenuCreating=false;
static bool tapeOverwriteConfirm=false;
static bool tapeSavePending=false;
static bool tapeAppendRecording=false; // decision made only at S/A confirmation
static bool tapeNeedLoad=false;
static int tapeSelected=0,tapeScroll=0;
static char tapePath[128]={};
static char tapeProposedPath[128]={};
static char tapeNameInput[80]={};
static char tapeMessage[100]={};
static uint8_t translate(fabgl::VirtualKey vk);

static bool selectTapeFile(const char* path){
  if(!path || !*path)return false;
  File file=SD.open(path,FILE_READ);
  if(!file)return false;
  const size_t n=file.size();
  if(n<4 || n>8u*1024u*1024u){file.close();return false;}
  uint8_t* image=(uint8_t*)ps_malloc(n);
  if(!image){file.close();return false;}
  const bool readOK=file.read(image,n)==n;
  file.close();
  if(!readOK || !machine.tape.mount(image,n)){
    free(image);return false;
  }
  snprintf(tapePath,sizeof(tapePath),"%s",path);
  return true;
}
static bool createBlankTape(const char* path){
  if(!path || !*path || SD.exists(path))return false;
  File file=SD.open(path,FILE_WRITE);
  if(!file)return false;
  const uint8_t signature[4]={'C','T','K','2'};
  const bool ok=file.write(signature,sizeof(signature))==sizeof(signature);
  file.flush();file.close();
  if(!ok){SD.remove(path);return false;}
  return selectTapeFile(path);
}
static void refreshTapeList(){
  tapeSelected=tapeScroll=0;
  const bool ok=tapeBrowser.refresh();
  if(!tapeBrowser.directoryFound())
    snprintf(tapeMessage,sizeof(tapeMessage),"PASTA /TK-2000/CT2 NAO ENCONTRADA");
  else if(!ok)
    snprintf(tapeMessage,sizeof(tapeMessage),"LISTA CT2 PARCIAL: MEMORIA INSUFICIENTE");
  else
    snprintf(tapeMessage,sizeof(tapeMessage),"%d FITAS CT2 - USE SETAS PARA ROLAR",tapeBrowser.count());
}
static void drawTapeMenu(){
  if(!tapeMenuOpen || !vgaReady)return;
  const char* names[TKTapeBrowser::kVisibleRows];
  for(int i=0;i<TKTapeBrowser::kVisibleRows;++i){
    const char* path=tapeBrowser.pathAt(tapeScroll+i);
    const char* slash=path?strrchr(path,'/'):nullptr;
    names[i]=path?(slash?slash+1:path):"";
  }
  const char* transport=machine.tape.recording()?"GRAVANDO":
       machine.tape.manualPaused()?"PAUSA MANUAL":
       machine.tape.autoPaused()?"PAUSA AUTO":
       machine.tape.playing()?"PLAY":
       machine.tape.finished()?"FIM":"PRONTA";
  vga.showTapeMenu(tapePath,names,TKTapeBrowser::kVisibleRows,
                   tapeBrowser.count(),tapeSelected,tapeScroll,tapeMessage,
                   tapeNameInput,tapeMenuCreating,tapeOverwriteConfirm,tapeSavePending,
                   transport,machine.tape.blockNumber(),machine.tape.blockCount(),
                   machine.tape.progress());
}
static void closeTapeMenu(){
  if(!tapeMenuOpen)return;
  tapeMenuOpen=false;tapeMenuCreating=false;tapeOverwriteConfirm=false;
  tapeSavePending=false;tapeNeedLoad=false;
  machine.keyboard.releaseInputs();clearHostModifiers();
  joystickKeys.clear();machine.clearJoystick();
  if(vgaReady)vga.dismissDiskMenu();
  tapeBrowser.clear();
  lastCpuMicros=micros();cpuCycleBudget=0;cpuCycleRemainder=0;
  fpsMeterActive=false;framePacer.reset();
}
static void openTapeMenu(bool needsLoad=false,bool saving=false){
  if(!emulatorReady || diskMenuOpen)return;
  machine.keyboard.releaseInputs();clearHostModifiers();
  joystickKeys.clear();machine.clearJoystick();
  tapeMenuCreating=false;tapeOverwriteConfirm=false;
  tapeSavePending=saving;tapeNeedLoad=needsLoad;
  tapeAppendRecording=false;
  tapeNameInput[0]=tapeProposedPath[0]=0;
  refreshTapeList();tapeMenuOpen=true;
  if(saving)snprintf(tapeMessage,sizeof(tapeMessage),"SALVAR FITA: ESCOLHA ARQUIVO OU N: NOVO");
  if(needsLoad)snprintf(tapeMessage,sizeof(tapeMessage),"LOADT/LOADA: ESCOLHA FITA PARA LER");
  drawTapeMenu();
}
static void beginTapeWrite(const char* path,bool append=false){
  snprintf(tapePath,sizeof(tapePath),"%s",path);
  tapeAppendRecording=append;
  machine.tape.beginRecord();
  Serial.printf("[K7] recording %s %s\n",append?"APPEND to":"OVERWRITE",tapePath);
  closeTapeMenu();
}

// Locate the first DATA record without loading the prior CT2 into a second
// PSRAM buffer. A mixed SAVEA/SAVET stream cannot be played by our decoder
// (its pilot timing is uniform for a given mounted file), so reject it.
static int existingTapeFormat(const char* path){
  File in=SD.open(path,FILE_READ);
  if(!in)return -1;
  if(!in.seek(4)){in.close();return -1;}
  while((size_t)in.position()+4u<=in.size()){
    uint8_t head[4];
    if(in.read(head,4)!=4){in.close();return -1;}
    const uint32_t len=(uint32_t)head[2] | ((uint32_t)head[3]<<8);
    if(head[0]=='D' && head[1]=='A'){
      in.close();return len==4u?1:0;
    }
    if((uint64_t)in.position()+len>in.size() || !in.seek(in.position()+len)){
      in.close();return -1;
    }
  }
  in.close();return -1; // a blank CTK2 header can accept either mode
}
static int newTapeFormat(const uint8_t* bytes,size_t count){
  for(size_t p=4;p+4<=count;){
    const size_t n=(size_t)bytes[p+2] | ((size_t)bytes[p+3]<<8);
    if(bytes[p]=='D' && bytes[p+1]=='A')return n==4u?1:0;
    p+=4u+n;
  }
  return -1;
}
static bool saveRecordedTape(){
  if(!tapePath[0])return false;
  uint8_t *encoded=nullptr;size_t count=0;
  if(!machine.tape.encodeRecording(&encoded,&count)){
    Serial.println("[K7] ERROR: unable to decode captured CT2 waveform; old file untouched");
    machine.tape.stopRecord();return false;
  }
  // Do not build a journal around invalid CT2 data or lose the original
  // recording if a previous power failure left only a .bak file.
  if(!CassetteCT2::valid(encoded,count) ||
     !TKTapeJournal::safeToOverwrite(tapePath)){
    Serial.println("[K7] ERROR: invalid recording or incomplete old save journal");
    free(encoded);machine.tape.stopRecord();return false;
  }
  const bool exists=SD.exists(tapePath);
  size_t originalSize=0;
  if(tapeAppendRecording){
    if(!exists || !TKTapeJournal::valid(tapePath)){
      Serial.println("[K7] APPEND refused: prior CT2 is missing or invalid");
      free(encoded);machine.tape.stopRecord();return false;
    }
    File old=SD.open(tapePath,FILE_READ);
    originalSize=old?old.size():0;
    if(old)old.close();
    // First four CTK2 bytes are a FILE signature, not another tape track.
    // Append only CA/CB/DA records; the decoder reads the combined stream.
    const int oldFormat=existingTapeFormat(tapePath);
    const int capturedFormat=newTapeFormat(encoded,count);
    if(count<4u || originalSize<4u ||
       (oldFormat>=0 && oldFormat!=capturedFormat) ||
       originalSize+(count-4u)>8u*1024u*1024u){
      Serial.println("[K7] APPEND refused: incompatible format or 8 MiB limit");
      free(encoded);machine.tape.stopRecord();return false;
    }
  }
  char tmp[144],bak[144];
  snprintf(tmp,sizeof(tmp),"%s.tmp",tapePath);
  snprintf(bak,sizeof(bak),"%s.bak",tapePath);
  // FILE_WRITE can append to an existing FAT file; a stale tmp must be
  // removed successfully before writing any bytes.
  if(SD.exists(tmp) && !SD.remove(tmp)){
    free(encoded);machine.tape.stopRecord();return false;
  }
  File out=SD.open(tmp,FILE_WRITE);
  bool wrote=(bool)out;
  if(wrote && tapeAppendRecording){
    File old=SD.open(tapePath,FILE_READ);
    if(!old)wrote=false;
    else {
      uint8_t chunk[512];size_t remaining=originalSize;
      while(wrote && remaining){
        const size_t step=remaining<sizeof(chunk)?remaining:sizeof(chunk);
        if(old.read(chunk,step)!=step || out.write(chunk,step)!=step)wrote=false;
        remaining-=step;
      }
      old.close();
    }
  }
  if(wrote){
    const size_t skip=tapeAppendRecording?4u:0u;
    wrote=out.write(encoded+skip,count-skip)==count-skip;
  }
  if(out){out.flush();out.close();}
  free(encoded);
  // Validate the ENTIRE reconstructed CT2 before touching the original.
  if(!wrote || !TKTapeJournal::valid(tmp)){
    SD.remove(tmp);machine.tape.stopRecord();
    Serial.println("[K7] ERROR writing complete validated CT2; original preserved");
    return false;
  }
  // Transaction: valid old primary => .bak; valid new .tmp => primary.
  // On failure, try to restore the prior file. The journal recovery path
  // at next F3 can restore a surviving .bak after power loss.
  if(exists){
    if(SD.exists(bak) && !SD.remove(bak)){
      SD.remove(tmp);machine.tape.stopRecord();return false;
    }
    if(!SD.rename(tapePath,bak)){
      SD.remove(tmp);machine.tape.stopRecord();return false;
    }
  }
  if(!SD.rename(tmp,tapePath)){
    if(exists)SD.rename(bak,tapePath);
    SD.remove(tmp);machine.tape.stopRecord();return false;
  }
  if(exists)SD.remove(bak);
  machine.tape.stopRecord();
  Serial.printf("[K7] %s %u bytes: %s\n",
                tapeAppendRecording?"appended":"saved",(unsigned)(count-(tapeAppendRecording?4u:0u)),tapePath);
  tapeAppendRecording=false;
  return selectTapeFile(tapePath);
}
static void updateTapeMenuNavigation(int priorSelected,int priorScroll){
  if(!tapeMenuOpen || !vgaReady || tapeSelected==priorSelected)return;
  const char *names[TKTapeBrowser::kVisibleRows];
  for(int i=0;i<TKTapeBrowser::kVisibleRows;++i) {
    const char* path=tapeBrowser.pathAt(tapeScroll+i);
    const char* base=path?strrchr(path,'/'):nullptr;
    names[i]=path?(base?base+1:path):"";
  }
  if(tapeScroll==priorScroll)
    vga.updateTapeMenuSelection(priorSelected,tapeSelected,tapeScroll,names,
                                TKTapeBrowser::kVisibleRows,tapeBrowser.count());
  else vga.updateTapeMenuScroll(names,TKTapeBrowser::kVisibleRows,
                                tapeBrowser.count(),tapeSelected,tapeScroll);
}
static void tapeMenuKey(fabgl::VirtualKey vk){
  if(tapeOverwriteConfirm){
    if(vk==fabgl::VK_s || vk==fabgl::VK_S || vk==fabgl::VK_y || vk==fabgl::VK_Y ||
       vk==fabgl::VK_a || vk==fabgl::VK_A){
      const bool append=(vk==fabgl::VK_a || vk==fabgl::VK_A);
      tapeOverwriteConfirm=false;
      beginTapeWrite(tapeProposedPath,append);
      return;
    }
    if(vk==fabgl::VK_n || vk==fabgl::VK_N || vk==fabgl::VK_ESCAPE){
      tapeOverwriteConfirm=false;
      snprintf(tapeMessage,sizeof(tapeMessage),"SOBRESCRITA CANCELADA - ARQUIVO PRESERVADO");
    }
    drawTapeMenu();return;
  }
  if(tapeMenuCreating){
    if(vk==fabgl::VK_ESCAPE){tapeMenuCreating=false;drawTapeMenu();return;}
    if(vk==fabgl::VK_BACKSPACE){
      const size_t n=strlen(tapeNameInput);
      if(n)tapeNameInput[n-1]=0;
    } else if(vk==fabgl::VK_RETURN || vk==fabgl::VK_KP_ENTER){
      char path[128]={};
      if(!tapeBrowser.newPath(tapeNameInput,path,sizeof(path))){
        snprintf(tapeMessage,sizeof(tapeMessage),"NOME INVALIDO OU EXTENSAO DIFERENTE DE .CT2");
      } else {
        const int found=tapeBrowser.findPath(path);
        if(found>=0){
          snprintf(path,sizeof(path),"%s",tapeBrowser.pathAt(found));
          if(tapeSavePending){
            snprintf(tapeProposedPath,sizeof(tapeProposedPath),"%s",path);
            tapeOverwriteConfirm=true;tapeMenuCreating=false;
          } else if(selectTapeFile(path)){
            closeTapeMenu();return;
          }
        } else if(tapeSavePending){
          // A not-yet-existing file is created only after the ROM finishes
          // generating its complete cassette waveform.
          beginTapeWrite(path);return;
        } else if(createBlankTape(path)){
          closeTapeMenu();return;
        } else snprintf(tapeMessage,sizeof(tapeMessage),"ERRO AO CRIAR CT2 NO SD");
      }
    } else {
      const uint8_t c=translate(vk);
      const size_t n=strlen(tapeNameInput);
      if(n<sizeof(tapeNameInput)-2 &&
         ((c>='A'&&c<='Z')||(c>='0'&&c<='9')||c==' '||c=='.'||c=='-'||c=='_')){
        tapeNameInput[n]=(char)c;tapeNameInput[n+1]=0;
      }
    }
    drawTapeMenu();return;
  }
  if(vk==fabgl::VK_ESCAPE || vk==fabgl::VK_F3){closeTapeMenu();return;}
  const bool navigation=(vk==fabgl::VK_UP || vk==fabgl::VK_DOWN);
  const int priorSelection=tapeSelected,priorScroll=tapeScroll;
  if(vk==fabgl::VK_p || vk==fabgl::VK_P){
    if(machine.tape.recording())
      snprintf(tapeMessage,sizeof(tapeMessage),"GRAVACAO ATIVA: NAO PODE PAUSAR");
    else if(!machine.tape.mounted())
      snprintf(tapeMessage,sizeof(tapeMessage),"SELECIONE UMA FITA CT2");
    else {
      machine.tape.togglePause(machine.cpu.cycles);
      snprintf(tapeMessage,sizeof(tapeMessage),"TRANSPORTE: %s",
               machine.tape.paused()?"PAUSADO":
               machine.tape.playing()?"REPRODUZINDO":"PRONTO");
    }
    drawTapeMenu();return;
  }
  if(vk==fabgl::VK_r || vk==fabgl::VK_R){
    if(machine.tape.recording())
      snprintf(tapeMessage,sizeof(tapeMessage),"GRAVACAO ATIVA: NAO PODE REBOBINAR");
    else if(machine.tape.mounted()){
      machine.tape.rewind();
      snprintf(tapeMessage,sizeof(tapeMessage),"FITA REBOBINADA PARA O INICIO");
    }else snprintf(tapeMessage,sizeof(tapeMessage),"SELECIONE UMA FITA CT2");
    drawTapeMenu();return;
  }
  if(vk==fabgl::VK_UP && tapeSelected>0)--tapeSelected;
  if(vk==fabgl::VK_DOWN && tapeSelected+1<tapeBrowser.count())++tapeSelected;
  if(vk==fabgl::VK_n || vk==fabgl::VK_N){
    tapeMenuCreating=true;tapeNameInput[0]=0;
    snprintf(tapeMessage,sizeof(tapeMessage),"DIGITE O NOME; .CT2 SERA ADICIONADO");
  }
  if(vk==fabgl::VK_e || vk==fabgl::VK_E){
    machine.tape.eject();tapePath[0]=0;
    snprintf(tapeMessage,sizeof(tapeMessage),"FITA EJETADA");
  }
  if((vk==fabgl::VK_RETURN || vk==fabgl::VK_KP_ENTER)&&tapeBrowser.count()>0){
    const char* path=tapeBrowser.pathAt(tapeSelected);
    if(path){
      if(tapeSavePending){
        snprintf(tapeProposedPath,sizeof(tapeProposedPath),"%s",path);
        tapeOverwriteConfirm=SD.exists(path);
        if(!tapeOverwriteConfirm){beginTapeWrite(path);return;}
      } else if(selectTapeFile(path)){
        Serial.printf("[K7] selected: %s\n",path);
        closeTapeMenu();return;
      } else snprintf(tapeMessage,sizeof(tapeMessage),"CT2 CORROMPIDO OU MEMORIA INSUFICIENTE");
    }
  }
  tapeScroll=TKTapeBrowser::scrollForSelection(tapeSelected,tapeScroll,tapeBrowser.count());
  if(navigation)updateTapeMenuNavigation(priorSelection,priorScroll);
  else drawTapeMenu();
}
static void serviceTapeRequest(){
  if(tapeMenuOpen || diskMenuOpen || mainMenuOpen)return;
  const CassetteCT2::Request req=machine.tape.request();
  if(req==CassetteCT2::None)return;
  machine.tape.clearRequest();
  if(req==CassetteCT2::Load){
    if(!tapePath[0])openTapeMenu(true,false);
    // When selected, the CT2 player auto-starts at the next ROM input poll.
  } else if(req==CassetteCT2::Save){
    if(!tapePath[0])openTapeMenu(false,true);
    else {
      // Even a previously mounted tape requires explicit overwrite approval.
      openTapeMenu(false,true);
      snprintf(tapeProposedPath,sizeof(tapeProposedPath),"%s",tapePath);
      if(SD.exists(tapePath))tapeOverwriteConfirm=true;
      else beginTapeWrite(tapePath);
      drawTapeMenu();
    }
  }
}

static void resetEmulatedTK2000(const char* shortcut, bool cold=false);
// Compact F1 overlay: the virtual CPU pauses just as it does in the F2 browser.
// Never allocate a disk list while browsing this menu.
static const int kMainMenuRows=9;
static void drawMainMenu(){
  if(!mainMenuOpen || !vgaReady)return;
  vga.showMainMenu(mainMenuSelected,showFPS,joystickEnabled,keyboardBR,diskLEDsEnabled,driveSoundEnabled);
}
static void closeMainMenu(){
  if(!mainMenuOpen)return;
  mainMenuOpen=false;
  mainMenuEnterLatched=false;
  machine.keyboard.releaseInputs();
  clearHostModifiers();
  machine.clearJoystick();
  joystickKeys.clear();
  if(vgaReady)vga.dismissMainMenu();
  lastCpuMicros=micros();cpuCycleBudget=0;cpuCycleRemainder=0;
  fpsMeterActive=false;
  framePacer.reset();
}
static void openMainMenu(){
  if(!emulatorReady || !vgaReady || diskMenuOpen || tapeMenuOpen)return;
  machine.keyboard.releaseInputs();clearHostModifiers();machine.clearJoystick();
  joystickKeys.clear();
  vga.page=machine.video.page; vga.color=machine.video.color && artifactsColorEnabled;
  vga.render(showFPS,fpsValue,epsValue);
  mainMenuSelected=0;mainMenuEnterLatched=false;mainMenuOpen=true;
  machine.serviceAudio(millis(),true); // No mechanism noise while the CPU is paused.
  drawMainMenu();
}
static void toggleFPS(){
  showFPS=!showFPS;
  fpsMeterActive=false;fpsValue=0;epsValue=0;
  Serial.printf("EPS OSD: %s\n",showFPS?"ON":"OFF");
}
static void toggleJoystick(){
  joystickEnabled=!joystickEnabled;
  // Avoid held keys and joystick direction getting stuck after mode changes.
  machine.keyboard.releaseInputs();clearHostModifiers();machine.clearJoystick();
  joystickKeys.clear();
  machine.setJoystickEnabled(joystickEnabled);
  // The guest CPU/RAM/KBOUT are untouched; newly enabled input is live.
  Serial.printf("JOYSTICK (CURSORES+ESPACO): %s\n",joystickEnabled?"ON":"OFF");
}
static void toggleJoystickF4(){
  toggleJoystick(); // Same path as F1: clears stale input and switches live.
  joystickStatusVisible=true;
  joystickStatusUntilMs=millis()+1500u;
  if(vgaReady) vga.drawJoystickStatus(true,joystickEnabled);
}
static void toggleKeyboardLayout(){
  keyboardBR=!keyboardBR;
  // Clear the guest matrix and the joystick BEFORE changing the PS/2 VK
  // conversion table. A held key interpreted using the old layout must not
  // stay pressed forever after the switch.
  machine.keyboard.releaseInputs();
  clearHostModifiers();
  machine.clearJoystick();
  joystickKeys.clear();
  if(kbd)kbd->setLayout(keyboardBR ? &TK2000ABNT2Layout : &fabgl::USLayout);
  Serial.printf("LAYOUT TECLADO: %s\n", keyboardBR?"BR (ABNT2)":"US");
}
static void toggleDiskLEDs(){
  diskLEDsEnabled=!diskLEDsEnabled;
  // Apply immediately while F1 is visible. Only the overlay is affected;
  // drive counters and the disk controller continue operating unchanged.
  if(vgaReady)vga.drawDiskLEDs(diskLedState[0],diskLedState[1],diskLEDsEnabled);
  Serial.printf("LED DISCOS: %s\n",diskLEDsEnabled?"ON":"OFF");
}
static void toggleDriveSound(){
  driveSoundEnabled=!driveSoundEnabled;
  machine.setDriveSoundEnabled(driveSoundEnabled);
  machine.serviceAudio(millis(),true); // OFF silences a running effect immediately.
  Serial.printf("DRIVE SOUND: %s\n",driveSoundEnabled?"ON":"OFF");
}
static void mainMenuKey(fabgl::VirtualKey vk){
  if(vk==fabgl::VK_ESCAPE || vk==fabgl::VK_F1){closeMainMenu();return;}
  const int previousSelection=mainMenuSelected;
  if(vk==fabgl::VK_UP && mainMenuSelected>0)--mainMenuSelected;
  if(vk==fabgl::VK_DOWN && mainMenuSelected<kMainMenuRows-1)++mainMenuSelected;
  if(vk==fabgl::VK_RETURN || vk==fabgl::VK_KP_ENTER){
    switch(mainMenuSelected){
      // Navigation/actions close F1; ON/OFF toggles remain in the same row.
      case 0:closeMainMenu();openDiskMenu();return;
      case 1:closeMainMenu();openTapeMenu();return;
      case 2:toggleFPS();break;
      case 3:toggleJoystick();break;
      case 4:toggleKeyboardLayout();break;
      case 5:toggleDiskLEDs();break;
      case 6:toggleDriveSound();break;
      case 7:closeMainMenu();resetEmulatedTK2000("MENU COLD RESET",true);return;
      case 8:closeMainMenu();resetESP32();return;
      default:return;
    }
  }
  if(vk==fabgl::VK_UP || vk==fabgl::VK_DOWN){
    if(mainMenuSelected!=previousSelection)
      vga.updateMainMenuSelection(previousSelection,mainMenuSelected,showFPS,
                                  joystickEnabled,keyboardBR,diskLEDsEnabled,driveSoundEnabled);
  } else if(vk==fabgl::VK_RETURN || vk==fabgl::VK_KP_ENTER) drawMainMenu();
}

// SD must be initialized BEFORE machine.init(): both system and disk ROMs
// must be loaded before the first CPU reset.
static bool startEmulatorFromSD() {
  if (!TKMountSD()) {
    startupError="CARTAO SD NAO DETECTADO";
    Serial.println("ERROR: SD initialization failed (HSPI GPIO14/12/13, MISO 2 or 35)");
    return false;
  }
  char actualPath[192]={};
  TKROMStatus result=TKLoadSystemROMFromSD(machine,actualPath,sizeof(actualPath));
  if (result!=TKROMStatus::OK) {
    startupError=TKROMStatusText(result);
    Serial.printf("ERROR: %s. Expected SD path: /TK-2000/ROMS/TK-2000.ROM\n",startupError);
    return false;
  }
  Serial.printf("System ROM loaded: %s (16384 bytes, CRC32 %08lX)\n",
                actualPath,(unsigned long)machine.systemROMCRC32());
  char diskROMPath[192]={};
  TKROMStatus diskROMResult=TKLoadDiskROMFromSD(machine,diskROMPath,sizeof(diskROMPath));
  if (diskROMResult!=TKROMStatus::OK) {
    startupError=TKDiskROMStatusText(diskROMResult);
    Serial.printf("ERROR: %s. Expected SD path: /TK-2000/ROMS/DISK-II.ROM\n",startupError);
    return false;
  }
  Serial.printf("Disk ROM loaded: %s (256 bytes, CRC32 %08lX)\n",
                diskROMPath,(unsigned long)machine.diskROMCRC32());
  if (!machine.init()) {
    startupError="ERRO AO INICIALIZAR EMULADOR";
    Serial.println("ERROR: CPU initialization failed after ROM load");
    return false;
  }
  mountDiskFromSD(0); mountDiskFromSD(1);
  // Start the original ROM immediately. Without D1 it draws the TK2000
  // header and waits for F12 (emulated physical RESET) before BASIC.
  // With D1 inserted, the original ROM boots its Disk II automatically.
  startupError="";
  lastCpuMicros=micros();
  cpuCycleRemainder=0; cpuCycleBudget=0;
  lastAutoSavePollMs=lastAudioPollMs=millis();
  diskStatus();
  return true;
}

// F12 is the usual RESET, preserving guest RAM. F11 cold-starts the machine
// like the MC-1000 F10, without rebooting the ESP32 or ejecting its disks.
// On disk flush errors the reset is aborted and F2 remains open.
static void resetEmulatedTK2000(const char* shortcut, bool cold){
  if (!emulatorReady) {
    if (vgaReady) {
      Serial.printf("%s: retry loading system and disk ROMs from SD\n", shortcut);
      if (diskMenuOpen) closeDiskMenu();
      if (tapeMenuOpen) closeTapeMenu();
      TKUnmountSD();
      emulatorReady=startEmulatorFromSD();
      if (!emulatorReady) vga.showStartupError(startupError);
      else vga.render(showFPS,fpsValue,epsValue);
    } else Serial.printf("%s: VGA unavailable; check serial diagnostics and restart\n", shortcut);
    return;
  }
  Serial.printf("%s: %s reset TK-2000 (ESP32 remains running)\n",
                shortcut,cold?"COLD":"WARM");
  if (!(cold?machine.coldReset():machine.reset())) {
    Serial.printf("%s ABORTED: disk flush failed\n", shortcut);
    return;
  }
  if (diskMenuOpen) closeDiskMenu();
  if (tapeMenuOpen) closeTapeMenu();
  joystickKeys.clear(); machine.clearJoystick();
  if (cold && vgaReady) vga.invalidate();
  fpsMeterActive=false; fpsValue=0; epsValue=0;
  lastCpuMicros=micros();cpuCycleBudget=0;cpuCycleRemainder=0;
  framePacer.reset();
  lastAutoSavePollMs=lastAudioPollMs=millis();
}

static uint8_t translate(fabgl::VirtualKey vk){
  // FabGL emits VK_a..VK_z for ordinary letter presses and VK_A..VK_Z
  // when the shifted variant is active. Both identify the same physical TK key.
  if(vk>=fabgl::VK_a && vk<=fabgl::VK_z) return (uint8_t)('A'+(vk-fabgl::VK_a));
  if(vk>=fabgl::VK_A && vk<=fabgl::VK_Z) return (uint8_t)('A'+(vk-fabgl::VK_A));
  if(vk>=fabgl::VK_0 && vk<=fabgl::VK_9) return (uint8_t)('0'+(vk-fabgl::VK_0));
  if(vk>=fabgl::VK_KP_0 && vk<=fabgl::VK_KP_9) return (uint8_t)('0'+(vk-fabgl::VK_KP_0));
  switch(vk){
    case fabgl::VK_BACKSPACE: return 8;
    case fabgl::VK_RETURN:
    case fabgl::VK_KP_ENTER: return 13;
    case fabgl::VK_SPACE: return ' ';
    case fabgl::VK_ESCAPE: return 27;
    case fabgl::VK_UP:
    case fabgl::VK_KP_UP: return 0x0B;
    case fabgl::VK_DOWN:
    case fabgl::VK_KP_DOWN: return 0x0A;
    case fabgl::VK_RIGHT:
    case fabgl::VK_KP_RIGHT: return 0x15;
    case fabgl::VK_LEFT:
    case fabgl::VK_KP_LEFT: return 8;
    case fabgl::VK_QUESTION: return '?';
    case fabgl::VK_SLASH: return '/';
    case fabgl::VK_PERIOD: return '.';
    case fabgl::VK_GREATER: return '>';
    case fabgl::VK_COMMA: return ',';
    case fabgl::VK_LESS: return '<';
    case fabgl::VK_COLON: return ':';
    case fabgl::VK_SEMICOLON: return ';';
    case fabgl::VK_AT: return '@';
    case fabgl::VK_CARET: return '^';
    case fabgl::VK_PLUS: return '+';
    case fabgl::VK_EQUALS: return '=';
    case fabgl::VK_MINUS: return '-';
    case fabgl::VK_ASTERISK: return '*';
    case fabgl::VK_QUOTE: return '\'';
    case fabgl::VK_QUOTEDBL: return '"';
    case fabgl::VK_AMPERSAND: return '&';
    case fabgl::VK_EXCLAIM: return '!';
    case fabgl::VK_HASH: return '#';
    case fabgl::VK_DOLLAR: return '$';
    case fabgl::VK_PERCENT: return '%';
    // No dedicated cedilla in the original TK-2000 keyboard matrix.
    // Neither Ç nor Shift+Ç may inject an ordinary C.
    case fabgl::VK_CEDILLA_c:
    case fabgl::VK_CEDILLA_C: return 0;
    case fabgl::VK_LEFTPAREN: return '(';
    case fabgl::VK_RIGHTPAREN: return ')';
    default: return 0;
  }
}

// ESP-WORKS-compatible: the factory loader selects this application in ota_0.
// Clear only the boot selection data (NOT factory, NVS, partition table, nor app).
// Guard against unexpected partitions before erasing any flash.
static bool returnToFactoryOnNextBoot(){
  const esp_partition_t* running=esp_ota_get_running_partition();
  const esp_partition_t* info=esp_partition_find_first(
    ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, nullptr);
  if (!running || !info) {
    Serial.println("[BOOTLOADER] running partition or otadata not found; no flash write");
    return false;
  }
  if (running->type != ESP_PARTITION_TYPE_APP ||
      running->subtype != ESP_PARTITION_SUBTYPE_APP_OTA_0 ||
      running->address != 0x130000 || info->address != 0xE000 || info->size != 0x2000) {
    Serial.printf("[BOOTLOADER] unexpected layout (app=%08lX, otadata=%08lX/%08lX); no erase\n",
                  (unsigned long)running->address,(unsigned long)info->address,
                  (unsigned long)info->size);
    return false;
  }
  const esp_err_t err=esp_partition_erase_range(info,0,info->size);
  Serial.printf("[BOOTLOADER] clear otadata for factory on next power-up: %s (%d)\n",
                err == ESP_OK ? "OK" : "FAILED", (int)err);
  return err == ESP_OK;
}

void setup(){
 Serial.begin(115200); delay(100);
 Serial.printf("TK-2000 firmware %s (ESP-WORKS ESP32_Bootloader / ota_0)\n", TK2000_FW_VERSION);
 returnToFactoryOnNextBoot();
 Serial.printf("[BOOT] reset reason=%d freeHeap=%u largestInternal=%u PSRAM=%u\n",
               (int)esp_reset_reason(), (unsigned)ESP.getFreeHeap(),
               (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
               (unsigned)ESP.getPsramSize());
 // Emulated RAM AND the Saturn 128K banks are allocated in PSRAM first. The physical FabGL framebuffer
 // MUST be internal DMA memory; report the largest available block after
 // allocating the virtual machine RAM, not before.
 bool ramReady=machine.prepareRAM();
 const uint32_t largestBeforeVGA=(uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_DMA);
 Serial.printf("[BOOT] 64KiB RAM + 128KiB Saturn PSRAM %s; largest DMA=%u bytes, PSRAM total=%u\n",
               ramReady ? "OK" : "FAIL",
               (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_DMA),
               (unsigned)ESP.getPsramSize());
 // The VGA controller needs contiguous internal DMA-capable memory. Refuse to
 // initialize if clearly insufficient, before invoking FabGL allocation.
 if (!ramReady || largestBeforeVGA < 48u*1024u) {
   Serial.printf("[BOOT FATAL] VGA preflight: only %u internal contiguous bytes available\n",(unsigned)largestBeforeVGA);
   startupError="MEMORIA VGA INSUFICIENTE";
   emulatorReady=false;
 } else {
   vga.setDirtyTracker(&machine.videoDirty);
   vgaReady=vga.begin(machine.ram);
   Serial.printf("[BOOT] VGA 640x200 viewport %s: freeHeap=%u largestInternal=%u width=%d height=%d\n",
                 vgaReady?"OK":"FAIL", (unsigned)ESP.getFreeHeap(),
                 (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                 vga.ctrl.getViewPortWidth(), vga.ctrl.getViewPortHeight());
   if (vgaReady) {
     machine.beginAudio();
     // The ON default must reach the PCM mixer, not just the F1 label.
     machine.setDriveSoundEnabled(driveSoundEnabled);
     emulatorReady=startEmulatorFromSD();
   } else startupError="FALHA AO ALOCAR VGA 640x200";
 }
 Serial.printf("[BOOT] ROM + CPU initialization: %s, heap: %u bytes\n", emulatorReady ? "OK" : "FAIL", (unsigned)ESP.getFreeHeap());
 if (!emulatorReady && vgaReady) vga.showStartupError(startupError);
 ps2.begin(PS2Preset::KeyboardPort0_MousePort1);
 kbd=ps2.keyboard();
 if(kbd)kbd->setLayout(keyboardBR ? &TK2000ABNT2Layout : &fabgl::USLayout);

 Serial.println("TK2000 TTGO VGA - 115200");
 lastCpuMicros=micros();
 if (!emulatorReady) Serial.println("Boot blocked: F12 tenta reler ROM; reinicio ESP32 disponivel no F1 quando pronto.");
}
void loop(){
 while(kbd && kbd->virtualKeyAvailable()>0){
   bool keyDown=false;
#if TK2000_PS2_CTRL_TRACE
   // Use the metadata-bearing FabGL 1.0.9 overload ONLY in diagnostic
   // builds; the normal firmware keeps its validated lightweight VK path.
   fabgl::VirtualKeyItem ps2item;
   if(!kbd->getNextVirtualKey(&ps2item, 0)) break;
   const fabgl::VirtualKey vk=ps2item.vk;
   keyDown=ps2item.down;
   const bool ctrlVK=(vk==fabgl::VK_LCTRL || vk==fabgl::VK_RCTRL);
   const bool letterVK=((vk>=fabgl::VK_a && vk<=fabgl::VK_z) ||
                        (vk>=fabgl::VK_A && vk<=fabgl::VK_Z));
   if(ctrlVK || (modifierSides.ctrl() && letterVK)){
     Serial.printf("[PS2 CTRL] vk=%d %s scan=%02X/%02X/%02X hostCTRL=%d fabCTRL=%d SHIFT=%d\n",
                   (int)vk,keyDown?"DOWN":"UP",
                   ps2item.scancode[0],ps2item.scancode[1],ps2item.scancode[2],
                   (int)modifierSides.ctrl(),(int)ps2item.CTRL,(int)ps2item.SHIFT);
   }
#else
   fabgl::VirtualKey vk=kbd->getNextVirtualKey(&keyDown, 0);
#endif
   if(vk==fabgl::VK_NONE) break;

   // F8 now belongs to the original TK REPEAT key, never ESP32 reset.
   // ESP32 restart remains available ONLY from the F1 menu.
   if(vk==fabgl::VK_F6 || vk==fabgl::VK_F11) continue;
   if(vk==fabgl::VK_F8){
     if (emulatorReady && !diskMenuOpen && !mainMenuOpen && !tapeMenuOpen)
       machine.keyboard.setRepeat(keyDown);
     continue;
   }

   if(vk==fabgl::VK_F1){
     if(!keyDown)f1KeyLatched=false;
     else if(!f1KeyLatched){
       f1KeyLatched=true;
       if(mainMenuOpen)closeMainMenu();
       else if(!diskMenuOpen && !tapeMenuOpen)openMainMenu();
     }
     continue;
   }
   // Typematic PS/2 key repeats must not repeatedly close/reopen F2 or
   // reset the 6502 with F12. Release must be processed even inside a menu.
   if(vk==fabgl::VK_F2){
     if(!keyDown) f2KeyLatched=false;
     else if(!f2KeyLatched){
       f2KeyLatched=true;
       if(mainMenuOpen){closeMainMenu();openDiskMenu();}
       else if(diskMenuOpen)closeDiskMenu();
       else if(emulatorReady && !tapeMenuOpen)openDiskMenu();
     }
     continue;
   }
   // F3 now opens the independent CT2 browser; the FPS switch remains in F1.
   if(vk==fabgl::VK_F3){
     if(!keyDown)f3KeyLatched=false;
     else if(!f3KeyLatched){
       f3KeyLatched=true;
       if(mainMenuOpen){closeMainMenu();openTapeMenu();}
       else if(tapeMenuOpen)closeTapeMenu();
       else if(emulatorReady && !diskMenuOpen)openTapeMenu();
     }
     continue;
   }
   if(vk==fabgl::VK_F12){
     if(!keyDown) f12KeyLatched=false;
     else if(!f12KeyLatched){
       f12KeyLatched=true;
       if(!diskMenuOpen && !mainMenuOpen && !tapeMenuOpen)resetEmulatedTK2000("F12");
     }
     continue;
   }
   // F4 is a latched shortcut: key autorepeat must not oscillate ON/OFF.
   if(vk==fabgl::VK_F4){
     if(!keyDown) f4KeyLatched=false;
     else if(!f4KeyLatched){
       f4KeyLatched=true;
       if(emulatorReady && !mainMenuOpen && !diskMenuOpen && !tapeMenuOpen) toggleJoystickF4();
     }
     continue;
   }
   if(mainMenuOpen){
     // Holding Enter must not toggle ON/OFF repeatedly via PS/2 typematic.
     if(vk==fabgl::VK_RETURN || vk==fabgl::VK_KP_ENTER){
       if(!keyDown) mainMenuEnterLatched=false;
       else if(!mainMenuEnterLatched){
         mainMenuEnterLatched=true;
         mainMenuKey(vk);
       }
     } else if(keyDown) mainMenuKey(vk);
     continue;
   }
   // The disk selector owns keyboard input while open. Never leak navigation
   // keystrokes into the emulated machine or advance the CPU under the menu.
   if(diskMenuOpen){
     if(keyDown)diskMenuKey(vk);
     continue;
   }
   if(tapeMenuOpen){
     if(keyDown)tapeMenuKey(vk);
     continue;
   }

   // Physical PC modifiers: preserve BOTH sides independently (US and BR).
   if(vk==fabgl::VK_LSHIFT || vk==fabgl::VK_RSHIFT){
     modifierSides.setShift(vk==fabgl::VK_RSHIFT,keyDown);
     machine.setShift(modifierSides.shift());
     continue;
   }
   if(vk==fabgl::VK_LCTRL || vk==fabgl::VK_RCTRL){
     modifierSides.setCtrl(vk==fabgl::VK_RCTRL,keyDown);
     machine.setCtrl(modifierSides.ctrl());
     continue;
   }
   // TAB = physical FIRE on the original scanned TK matrix, regardless of
   // virtual joystick mode. The space bar remains joystick FIRE only when ON.
   if(vk==fabgl::VK_TAB){
     if(emulatorReady)machine.keyboard.setFire(keyDown);
     continue;
   }

   // With joystick ON, these physical keys are reserved for the joystick.
   // Do not simultaneously type cursors or spaces into the emulated BASIC.
   const int joyAction = TKJoystickAction(vk);
   if(emulatorReady && joystickEnabled && joyAction>=0){
     // Process the event immediately; periodic physical-state reconciliation
     // below repairs lost releases and numeric-keypad VK transitions.
     joystickKeys.event(vk,keyDown);
     machine.setJoystickMask(joystickKeys.mask());
     continue;
   }
   uint8_t c=translate(vk);
   if(c && emulatorReady) machine.key(c,keyDown);

   if(keyDown){
   }
 }
 if (diskMenuOpen || tapeMenuOpen || mainMenuOpen) {
   // A paused menu must not postpone a pending automatic disk commit.
   if(emulatorReady){
     machine.disk.serviceAutoSave(millis());
     machine.serviceAudio(millis(),true);
   }
   delay(10);return;
 }
 if (!emulatorReady) { delay(40); return; }
 // BASIC's cassette ROM raises these requests through $C010/$C020 only;
 // no tape-menu branch is added to the 6502 per-instruction hot path.
 serviceTapeRequest();
 if(tapeMenuOpen){delay(10);return;}
 if(machine.tape.recordFinished(machine.cpu.cycles)) {
   if(!saveRecordedTape())Serial.println("[K7] SAVE FAILED; previous CT2 not modified");
 }
 // FabGL keeps the current physical key state independently of its VK queue.
 // Reconcile periodically: a dropped PS/2 key-up must not leave the game
 // driving LEFT forever. Polling at ~12 ms avoids per-instruction overhead.
 if (joystickEnabled && kbd) {
   const uint32_t joyNow=millis();
   if ((uint32_t)(joyNow-lastJoystickPollMs)>=12u) {
     lastJoystickPollMs=joyNow;
     // Never turn ON a joystick direction just because FabGL reports a VK
     // as pressed: stale VK aliases used to inject phantom LEFT forever.
     joystickKeys.releaseMissing(
       [&](fabgl::VirtualKey vk) { return kbd->isVKDown(vk); });
     machine.setJoystickMask(joystickKeys.mask());
   }
 }
 // Check auto-save once every 20ms, rather than every tight CPU slice.
 // This does NOT postpone a motor-off flush or change the safe 1500ms
 // quiet time / atomic .tmp/.bak transaction for DSK and NIB.
 const uint32_t nowMs=millis();
 machine.keyboard.serviceRepeat(nowMs); // no heap, no ISR; inert until F8 held
 if ((uint32_t)(nowMs-lastAutoSavePollMs)>=20u) {
   lastAutoSavePollMs=nowMs;
#if TK2000_PERF_TRACE
   const uint32_t beforeSD=micros();
#endif
   machine.disk.serviceAutoSave(nowMs);
#if TK2000_PERF_TRACE
   const uint32_t duration=micros()-beforeSD;
   perf.sdUs+=duration;
   if(duration>perf.worstSD)perf.worstSD=duration;
#endif
 }
 // As in the MSX/MC-1000 fast pacing strategy, avoid a mandatory RTOS
 // tick every 20 ms; at 100-Hz ticks that steals up to half of CPU time.
 // Still block periodically to let IDLE0/IDLE1 feed their watchdogs.
 if (TKCPUPacing::idleBlockDue(nowMs,lastIdleYieldMs)) {
   delay(1);
   lastIdleYieldMs=millis();
 }
 // Run the 6502 from elapsed wall-clock time rather than assuming each loop
 // takes 16.67 ms. The old fixed 17,038-cycle batch slowed the emulated machine
 // whenever video rendering took longer, and accelerated it when the loop was fast.
 uint32_t cpuNow=micros();
 uint32_t elapsedUs=cpuNow-lastCpuMicros;
 lastCpuMicros=cpuNow;
 if(elapsedUs>50000) elapsedUs=50000; // avoid a long catch-up burst after stalls
 // Detect natural pauses between stages without advancing any pending
 // CT2 half-wave. This stops 8x turbo while the game runs or presents titles.
 machine.tape.service(machine.cpu.cycles);
 // 8x K7 target + verified-ROM native busy-wait acceleration inside
 // Machine::run(): keep original virtual C010/C020 pulse widths and RAM
 // semantics while reducing physical 6502 interpreter workload.
 // Gameplay and unrelated devices continue at standard 1x.
 const uint8_t cassetteRate=TKCassetteTurbo::rate(machine.tape.playing(),
                                                machine.tape.recording());
 // One log every five seconds of uninterrupted K7 operation. It reports
 // actual guest cycles per physical time (x1.00 corresponds to 1.023 MHz).
 // Between-stage pauses do not pollute the comparison with gameplay at 1x.
 if(cassetteRate>1u){
   if(!lastK7ReportMs){
     lastK7ReportMs=nowMs;lastK7ReportCycles=machine.cpu.cycles;
#if TK2000_K7_MOON_PROFILE
     if(machine.tape.gameInputProfile()==TKGameInputProfile::MoonPatrol){
       moonK7Profile.reset();
       moonK7Profile.readSamples=machine.tape.diagnosticReadSamples();
       moonK7Profile.decodedEdges=machine.tape.diagnosticDecodedEdges();
       (void)machine.takeK7MoonCPUStats();
     }
#endif
   }
   else if((uint32_t)(nowMs-lastK7ReportMs)>=
           (machine.tape.gameInputProfile()==TKGameInputProfile::MoonPatrol?2000u:5000u)){
     const uint32_t elapsed=nowMs-lastK7ReportMs;
     const uint64_t ticks=machine.cpu.cycles-lastK7ReportCycles;
     const uint32_t hundredths=(uint32_t)((ticks*100u)/(uint64_t)(elapsed*1023u));
     if(machine.tape.gameInputProfile()==TKGameInputProfile::MoonPatrol)
       measuredMoonTurboHundredths=(uint16_t)(hundredths>999u?999u:hundredths);
#if TK2000_K7_SERIAL_TRACE
     Serial.printf("[K7 TURBO] real=%lu.%02lux target=8x progress=%u%% pending=%lu cycles\n",
                   (unsigned long)(hundredths/100u), (unsigned long)(hundredths%100u),
                   (unsigned)machine.tape.progress(), (unsigned long)cpuCycleBudget);
#endif
#if TK2000_K7_MOON_PROFILE
     if(machine.tape.gameInputProfile()==TKGameInputProfile::MoonPatrol){
       const auto host=machine.takeK7MoonCPUStats();
       const uint32_t reads=machine.tape.diagnosticReadSamples();
       const uint32_t edges=machine.tape.diagnosticDecodedEdges();
       // Count percentages over the SAME measured wall-time window.
       const unsigned long cpuPct=(unsigned long)((moonK7Profile.cpuUs*100u)/((uint64_t)elapsed*1000u));
       const unsigned long vgaPct=(unsigned long)((moonK7Profile.vgaUs*100u)/((uint64_t)elapsed*1000u));
       Serial.printf("[K7 MOON PROFILE] PC=%04X cpu=%lu%% vga=%lu%% steps=%lu fast=%lu savedCycles=%llu romOff=%lu "
                     "C010=%lu edges=%lu saturated=%lu burstCap=%lu droppedGuestCycles=%llu\n",
                     (unsigned)machine.cpu.PC,cpuPct,vgaPct,
                     (unsigned long)host.executedInstructions,(unsigned long)host.nativeSkips,
                     (unsigned long long)host.nativeSkippedCycles,(unsigned long)host.romHiddenInstructions,
                     (unsigned long)(reads-moonK7Profile.readSamples),
                     (unsigned long)(edges-moonK7Profile.decodedEdges),
                     (unsigned long)moonK7Profile.queueLimitHits,
                     (unsigned long)moonK7Profile.burstLimitHits,
                     (unsigned long long)moonK7Profile.droppedCycles);
       moonK7Profile.readSamples=reads;
       moonK7Profile.decodedEdges=edges;
       moonK7Profile.reset();
     }
#endif
     lastK7ReportMs=nowMs;lastK7ReportCycles=machine.cpu.cycles;
   }
 }else{lastK7ReportMs=0;lastK7ReportCycles=0;measuredMoonTurboHundredths=0;}
 cpuCycleBudget=TKCassetteTurbo::afterTransportStops(cpuCycleBudget,
                                                     lastCassetteRate,cassetteRate);
 lastCassetteRate=cassetteRate;
 const uint32_t earnedCycles=TKCassetteTurbo::cyclesFromUs(
     elapsedUs,cassetteRate,cpuCycleRemainder);
 // Keep the proven v0.7 non-cassette scheduling 100% unchanged; only K7
 // turbo needs a capped pending queue, since the ESP32 may not sustain 8x.
 if(cassetteRate==1u)cpuCycleBudget+=earnedCycles;
 else {
#if TK2000_K7_MOON_PROFILE
   if(machine.tape.gameInputProfile()==TKGameInputProfile::MoonPatrol){
     const uint64_t backlog=(uint64_t)cpuCycleBudget+earnedCycles;
     if(backlog>TKCassetteTurbo::kMaxPendingCycles){
       ++moonK7Profile.queueLimitHits;
       moonK7Profile.droppedCycles+=backlog-TKCassetteTurbo::kMaxPendingCycles;
     }
   }
#endif
   cpuCycleBudget=TKCassetteTurbo::addBounded(cpuCycleBudget,earnedCycles);
 }
 // Batch at least 512 cycles so the instruction-level core does not overshoot
 // its wall-clock budget on every tiny loop iteration.
 if(cpuCycleBudget>=512){
   // Bounded batches avoid monopolizing one core after a delayed frame.
   const uint32_t maxSlice = cassetteRate>1u ? 8192u : 4096u;
   const uint32_t slice = cpuCycleBudget > maxSlice ? maxSlice : cpuCycleBudget;
#if TK2000_PERF_TRACE
   const uint32_t beforeCPU=micros();
#endif
   const uint32_t burstStarted=micros();
   machine.run(slice);
   cpuCycleBudget-=slice;
   // Most tapes keep v0.82's four 8Ki-cycle slices / 2.5ms cap. Moon
   // Patrol's second 130-block loader was falling behind the 8x budget;
   // grant up to eight slices / 3.5ms ONLY for the exact verified CT2.
   // Both limits are bounded to leave time for FabGL/PS2/audio and 60Hz
   // video, and the guest CT2 edge durations remain unchanged.
   if(cassetteRate>1u){
     const bool moonTape=(machine.tape.gameInputProfile()==TKGameInputProfile::MoonPatrol);
     const unsigned maxBursts=moonTape?8u:4u;
     const uint32_t maxBurstUs=TKMoonTurboPolicy::maxBurstMicroseconds(moonTape);
     for(unsigned extra=1;extra<maxBursts && cpuCycleBudget>=512u;++extra){
       if((uint32_t)(micros()-burstStarted)>=maxBurstUs)break;
       if(!(machine.tape.playing() || machine.tape.recording()))break;
       const uint32_t more=cpuCycleBudget>8192u?8192u:cpuCycleBudget;
       machine.run(more);
       cpuCycleBudget-=more;
     }
   }
#if TK2000_K7_MOON_PROFILE
   if(cassetteRate>1u && machine.tape.gameInputProfile()==TKGameInputProfile::MoonPatrol){
     const uint32_t burstCost=micros()-burstStarted;
     moonK7Profile.cpuUs+=burstCost;
     if(cpuCycleBudget>=512u && burstCost>=TKMoonTurboPolicy::maxBurstMicroseconds(true))++moonK7Profile.burstLimitHits;
   }
#endif
#if TK2000_PERF_TRACE
   const uint32_t duration=micros()-beforeCPU;
   perf.cpuUs+=duration;
   ++perf.cpuSlices;
   if(duration>perf.worstCPU)perf.worstCPU=duration;
#endif
 }
 // Avoid repeated register-frequency and 2-drive counter scans on every
 // short loop iteration. FabGL's DAC interrupt continues outputting samples
 // independently; a 2ms control update is sufficient for step envelopes.
 if ((uint32_t)(nowMs-lastAudioPollMs)>=2u) {
   lastAudioPollMs=nowMs;
#if TK2000_PERF_TRACE
   const uint32_t beforeAudio=micros();
#endif
   machine.serviceAudio(nowMs);
#if TK2000_PERF_TRACE
   perf.audioUs+=micros()-beforeAudio;
#endif
 }
#if TK2000_PERIODIC_HEALTH_LOG
 if ((uint32_t)(millis()-lastHealthReportMs)>=2000u) {
   lastHealthReportMs=millis();
   Serial.printf("[HEALTH] PC=%04X cycles=%llu page=%u color=%u vramNonZero=%lu "
                 "heap=%u largestDMA=%u FPS=%u EPS=%u\n",
                 machine.cpu.PC, (unsigned long long)machine.cpu.cycles,
                 machine.video.page, machine.video.color ? 1u : 0u,
                 (unsigned long)activeVideoBytes(),(unsigned)ESP.getFreeHeap(),
                 (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_DMA),
                 (unsigned)fpsValue,(unsigned)epsValue);
 }
#endif
 // 60 guest video services per second, paced with microsecond precision.
 // No made-up FPS: only completed VGA services count toward the meter.
 const uint32_t videoNow=micros();
 // Hardware VGA sync remains continuous. Only the bitmap render/update
 // (normally 60 Hz) is divided by four for the Moon CT2 transport, freeing
 // ESP32 CPU for real tape processing. At pause/end gameplay returns to 60Hz.
 const bool moonVideoThrottle=cassetteRate>1u &&
         machine.tape.gameInputProfile()==TKGameInputProfile::MoonPatrol;
 if (framePacer.due(videoNow) &&
     TKMoonTurboPolicy::videoService(moonVideoThrottle,++moonVideoFrameSequence)) {
   // MC-1000 v0.12 metering: count actual periodic VGA services and
   // guest 6502 cycles during the SAME exact microsecond sampling interval.
   // It measures emulation throughput, not how often bitmap pixels change.
   const uint64_t sampleTime=(uint64_t)esp_timer_get_time();
   const uint64_t guestCycles=machine.cpu.cycles;
   if (!showFPS) {
     fpsMeterActive=false;
   } else if (!fpsMeterActive) {
     fpsMeter.Reset(sampleTime,guestCycles);
     fpsMeterActive=true;
   } else {
     TKFPS_EPS_Meter::Result sample;
     if (fpsMeter.Sample(sampleTime,guestCycles,&sample)) {
       fpsValue=sample.fps;
       epsValue=sample.eps;
     }
   }
#if TK2000_PERF_TRACE
   const uint32_t beforeVideo=micros();
#endif
#if TK2000_K7_MOON_PROFILE
   const bool moonOnVga=cassetteRate>1u &&
            machine.tape.gameInputProfile()==TKGameInputProfile::MoonPatrol;
   const uint32_t beforeMoonVga=moonOnVga?micros():0u;
#endif
   vga.page=machine.video.page;
   vga.color=machine.video.color && artifactsColorEnabled;
   vga.render(showFPS,fpsValue,epsValue);
   refreshDiskLEDs(millis());
   vga.drawDiskLEDs(diskLedState[0],diskLedState[1],diskLEDsEnabled);
   if (joystickStatusVisible && (int32_t)(millis()-joystickStatusUntilMs)>=0)
     joystickStatusVisible=false;
   vga.drawJoystickStatus(joystickStatusVisible,joystickEnabled);
   vga.drawK7TurboRate(machine.tape.gameInputProfile()==TKGameInputProfile::MoonPatrol &&
                       cassetteRate>1u,measuredMoonTurboHundredths);
   vga.show();
#if TK2000_K7_MOON_PROFILE
   if(moonOnVga)moonK7Profile.vgaUs+=micros()-beforeMoonVga;
#endif
#if TK2000_PERF_TRACE
   const uint32_t duration=micros()-beforeVideo;
   perf.videoUs+=duration;
   ++perf.frames;
   if(duration>perf.worstVideo)perf.worstVideo=duration;
#endif
 }
#if TK2000_PERF_TRACE
 if(!perf.beginMs)perf.beginMs=nowMs;
 if((uint32_t)(nowMs-perf.beginMs)>=2000u){
   Serial.printf("[PERF 2s] CPU=%llu ms (%lu calls,max %lu us) VGA=%llu ms (%lu frames,max %lu us) SD-autosave=%llu ms (max %lu us) AUDIO=%llu ms FPS/EPS=%u/%u\n",
    (unsigned long long)(perf.cpuUs/1000u),(unsigned long)perf.cpuSlices,
    (unsigned long)perf.worstCPU,(unsigned long long)(perf.videoUs/1000u),
    (unsigned long)perf.frames,(unsigned long)perf.worstVideo,
    (unsigned long long)(perf.sdUs/1000u),(unsigned long)perf.worstSD,
    (unsigned long long)(perf.audioUs/1000u),(unsigned)fpsValue,(unsigned)epsValue);
   perf=TKPerfTrace();perf.beginMs=nowMs;
 }
#endif
}
