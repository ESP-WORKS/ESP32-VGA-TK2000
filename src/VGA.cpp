#include <stdio.h>
#include <string.h>
#include "VGA.h"
#include "HGRColor.h"
#include "HGRFast.h"
static const uint8_t font5x7[][5]={
{0,0,0,0,0},{0,0,95,0,0},{0,7,0,7,0},{20,127,20,127,20},{36,42,127,42,18},{35,19,8,100,98},{54,73,85,34,80},{0,5,3,0,0},{0,28,34,65,0},{0,65,34,28,0},{20,8,62,8,20},{8,8,62,8,8},{0,80,48,0,0},{8,8,8,8,8},{0,96,96,0,0},{32,16,8,4,2},
{62,81,73,69,62},{0,66,127,64,0},{66,97,81,73,70},{33,65,69,75,49},{24,20,18,127,16},{39,69,69,69,57},{60,74,73,73,48},{1,113,9,5,3},{54,73,73,73,54},{6,73,73,41,30},
{0,54,54,0,0},{0,86,54,0,0},{8,20,34,65,0},{20,20,20,20,20},{65,34,20,8,0},{2,1,81,9,6},{50,73,121,65,62},
{126,17,17,17,126},{127,73,73,73,54},{62,65,65,65,34},{127,65,65,34,28},{127,73,73,73,65},{127,9,9,9,1},{62,65,73,73,122},{127,8,8,8,127},{0,65,127,65,0},{32,64,65,63,1},{127,8,20,34,65},{127,64,64,64,64},{127,2,12,2,127},{127,4,8,16,127},{62,65,65,65,62},{127,9,9,9,6},{62,65,97,33,94},{127,9,25,41,70},{38,73,73,73,50},{1,1,127,1,1},{63,64,64,64,63},{31,32,64,32,31},{63,64,56,64,63},{99,20,8,20,99},{3,4,120,4,3},{97,81,73,69,67},
{0,127,65,65,0},{2,4,8,16,32},{0,65,65,127,0},{4,2,1,2,4},{64,64,64,64,64},{0,1,2,4,0},{32,84,84,84,120},{127,72,68,68,56},{56,68,68,68,0},{56,68,68,72,127},{56,84,84,84,24},{8,126,9,1,2},{12,82,82,82,62},{127,8,4,4,120},{0,68,125,64,0},{32,64,68,61,0},{127,16,40,68,0},{0,65,127,64,0},{124,4,24,4,120},{124,8,4,4,120},{56,68,68,68,56},{124,20,20,20,8},{8,20,20,24,124},{124,8,4,4,8},{72,84,84,84,32},{4,63,68,64,32},{60,64,64,32,124},{28,32,64,32,28},{60,64,48,64,60},{68,40,16,40,68},{12,80,80,80,60},{68,100,84,76,68}};
bool TKVGA::begin(uint8_t*r){
  ram=r;
  ctrl.begin();
  ctrl.setResolution(VGA_640x200_70Hz);
  // FabGL can reduce the viewport height when DMA memory is scarce.
  // Never write pixels beyond the actually allocated framebuffer.
  if (ctrl.getViewPortWidth() != 640 || ctrl.getViewPortHeight() != 200) {
    ram=nullptr;
    return false;
  }
  // The direct DMA path writes every one of the 200 scanlines. Testing
  // only first/last could still dereference a missing intermediate row.
  for (int y=0; y<200; ++y) {
    if (ctrl.getScanline(y)==nullptr) {
      ram=nullptr;
      return false;
    }
  }
  // Prepare native raw VGA pixels once. Drawing through Canvas queues one
  // primitive per operation; direct raw pixels bypass that queue entirely.
  // TK-2000 reference colors from tk_video.cpp: green (20 C0 00),
  // blue (00 80 FF), cyan (20 B0 B0), red (F0 10 00). Quantize to RGB222.
  // Other palette indices are reserved for menu and FPS/EPS overlay.
  static const uint8_t rgb[8][3] = {
    {0,0,0}, {3,0,0}, {0,2,0}, {3,2,0},
    {0,2,3}, {2,0,3}, {0,2,2}, {3,3,3}
  };
  for (uint8_t c=0; c<8; ++c)
    rawColors[c] = ctrl.createRawPixel(fabgl::RGB222(rgb[c][0], rgb[c][1], rgb[c][2]));
  rect(0, 0, 640, 200, 0);
  cacheValid = false;
  return true;
}

void TKVGA::rect(int x,int y,int w,int h,uint8_t c){
  if (w <= 0 || h <= 0) return;
  // Limit all software drawing to the actual FabGL viewport.
  if (x < 0) { w += x; x=0; }
  if (y < 0) { h += y; y=0; }
  const int viewW=ctrl.getViewPortWidth();
  const int viewH=ctrl.getViewPortHeight();
  if (x >= viewW || y >= viewH) return;
  if (w > viewW-x) w=viewW-x;
  if (h > viewH-y) h=viewH-y;
  if (w <= 0 || h <= 0) return;
  if (c > 7) c = 7;
  const uint8_t raw = rawColors[c];
  const int xEnd = x + w;
  const int yEnd = y + h;
  for (int py=y; py<yEnd; ++py)
    for (int px=x; px<xEnd; ++px)
      ctrl.setRawPixel(px, py, raw);
}

// TK-2000 / MPF-II has no native text-character generator. The monitor's
// characters are drawn into the video bitmap, so interpreting video RAM as
// ASCII creates the repeated '|'/garbage symptom seen in v0.6. Always render
// the bitmap like the MAME TK2000 driver (character pointer is null there).
static inline uint16_t hiresAddress(uint16_t base, int y, int byteCol){
  return (uint16_t)(base + ((y & 7) * 0x400) + ((y & 0x38) << 4) + ((y & 0xC0) >> 6) * 0x28 + byteCol);
}

// FabGL 1.0.9 VGAController raw scanlines store four-pixel blocks as
// byte offsets 2,3,0,1. getScanline() is provided by the controller, and
// direct writes remove >100,000 setRawPixel() calls for a full HGR refresh.
// Set TK2000_VGA_DIRECT_SCANLINE=0 to revert to the conservative API path.
#ifndef TK2000_VGA_DIRECT_SCANLINE
#define TK2000_VGA_DIRECT_SCANLINE 1
#endif

// Render a single 280-pixel TK-2000 row, scaled horizontally to 560 pixels.
// The 40 source bytes are copied from PSRAM only once. Neighbour checks use
// this local row instead of repeatedly accessing slow external RAM.
void TKVGA::hiresRow(int y, const uint8_t source[40]) {
  // Colors use the reference TK video lookup, including gap/fringe pixels.
  const int py = 4 + y;
#if defined(ESP32) && TK2000_VGA_DIRECT_SCANLINE
  // Build the whole row off-screen, then perform one fast DMA-row update.
  // Interleaving slow color calculations with visible scanline writes used
  // to leave partially updated rows on the VGA output ('RF-like snow').
  uint8_t packedRow[560];
  uint8_t * const scanline = ctrl.getScanline(py);
#endif
  for (int b=0; b<40; ++b) {
    uint8_t pixels[7];
    TKHGRFast7(source,b,color,pixels);
    for (int bit=0; bit<7; ++bit) {
      const uint8_t raw=rawColors[pixels[bit]];
      const int px=40+(b*7+bit)*2;
#if defined(ESP32) && TK2000_VGA_DIRECT_SCANLINE
      const int off=(px ^ 2)-40;
      packedRow[off]=raw;
      packedRow[off+1]=raw;
#else
      ctrl.setRawPixel(px,py,raw);
      ctrl.setRawPixel(px+1,py,raw);
#endif
    }
  }
#if defined(ESP32) && TK2000_VGA_DIRECT_SCANLINE
  // 40 is 32-bit aligned, so the packed RGB/sync byte order remains intact.
  memcpy(scanline+40,packedRow,sizeof(packedRow));
#endif
}

void TKVGA::hires(){
  const uint16_t base = page == 1 ? 0x2000 : 0xA000;
  const bool force = !cacheValid || page != lastPage || color != lastColor;
  uint8_t source[40];
  for (int y=0;y<192;++y) {
    // The regular emulator path tracks guest HGR writes, so unchanged rows
    // need neither PSRAM reads nor palette calculations. Standalone VGA
    // clients without the tracker keep the conservative memcmp fallback.
    if (dirtyTracker) {
      const bool rowDirty=dirtyTracker->take(page,y);
      if (!force && !rowDirty) continue;
    }
    // One contiguous 40-byte PSRAM transfer instead of 40 indexed accesses.
    // The TK-2000 HGR addressing interleaves rows, but columns within a row
    // are contiguous; this preserves the exact byte order and artifact phase.
    memcpy(source, ram + hiresAddress(base,y,0), sizeof(source));
    if (!force) {
      if (color) {
        if (memcmp(source,lastRows[y],40)==0) continue;
      } else {
        // In monochrome, bit 7 only selects artifact-color phase and has
        // no visible effect. Avoid a needless 560-pixel redraw when a game
        // changes only HGR phase bits. Preserve the byte snapshot so a
        // future color-mode switch still triggers the correct full redraw.
        bool visibleChanged=false;
        for(int b=0;b<40;++b)
          if(((source[b]^lastRows[y][b])&0x7Fu)!=0){visibleChanged=true;break;}
        if(!visibleChanged){memcpy(lastRows[y],source,40);continue;}
      }
    }
    hiresRow(y,source);
    memcpy(lastRows[y],source,40);
  }
}

void TKVGA::render(bool showFPS, uint16_t fps, uint16_t eps){
  if (!ram) return;
  if (needsFullClear) {
    rect(0,0,640,200,0);
    needsFullClear=false;
    cacheValid=false;
    ledStates[0]=ledStates[1]=255;
    joystickStatus=255;
    lastK7Rate=0xFFFFu;lastK7Visible=false;
  }
  // Fast row-dirty rendering: hires() compares 40 logical bytes per line.
  // It redraws only changed lines, while page/color changes force a full redraw.
  // The performance OSD now shows only the EPS number, at the former
  // "FPS" heading position. Keep the fps argument for callers/serial
  // diagnostics, but a change in FPS alone must not redraw the overlay.
  (void)fps;
  const bool osdChanged = showFPS != lastShowFPS ||
                           (showFPS && eps != lastEPS);
  hires();
  if (osdChanged || !cacheValid) {
    // Stay in the right border x=601..639, outside active HGR x=40..599.
    // Clear the old full indicator area so no FPS/EPS labels or stale values
    // survive a menu dismissal or transition from the previous layout.
    rect(601, 4, 39, 59, 0);
    if (showFPS) {
      char value[12];
      snprintf(value,sizeof(value),"%u",(unsigned)eps);
      drawText5x7(605, 10, value, 3);
    }
  }
  lastPage=page; lastColor=color; lastShowFPS=showFPS; lastEPS=eps; cacheValid=true;
}

void TKVGA::drawDiskLEDs(uint8_t d1,uint8_t d2,bool visible) {
  if(!visible){
    // Clear both labels and lamps exactly once when LEDs are turned off.
    // Keep this in the left margin only, never touching the HGR picture.
    if(ledStates[0]!=254 || ledStates[1]!=254){
      rect(2,8,34,27,0);
      ledStates[0]=ledStates[1]=254; // hidden sentinel; force redraw on re-enable
    }
    return;
  }
  const uint8_t states[2]={d1,d2};
  for (int d=0;d<2;++d) {
    const uint8_t state=states[d] > 2 ? 0 : states[d];
    if (state==ledStates[d]) continue;
    const int y=8+d*15;
    // The RGB border is outside the logical 280x192 HGR window.
    rect(2,y,34,12,0);
    drawText5x7(3,y+2,d==0?"1":"2",7);
    rect(14,y+2,9,8,7);         // bezel
    rect(15,y+3,7,6,state==1?2:(state==2?1:0));
    ledStates[d]=state;
  }
}

void TKVGA::drawJoystickStatus(bool visible, bool enabled) {
  if (!ram) return;
  const uint8_t status = visible ? (enabled ? 1u : 2u) : 0u;
  if (status == joystickStatus) return;
  // Dedicated right margin x=601..639, below the FPS/EPS indicator.
  // Never cover active HGR x=40..599 or the two disk LEDs.
  rect(601, 75, 39, 36, 0); // Borderless JOY OSD, inside right overscan only.
  if (visible) {
    // No blue frame; preserve original JOY/ON/OFF colors and position.
    drawText5x7(610, 81, "JOY", 6);
    drawText5x7(enabled ? 614 : 610, 95, enabled ? "ON" : "OFF", 3);
  }
  joystickStatus=status;
}

void TKVGA::drawK7TurboRate(bool visible,uint16_t hundredths){
  if(!ram)return;
  if(lastK7Visible==visible && (!visible || lastK7Rate==hundredths))return;
  // Right-hand overscan: never write into the 560-pixel game display.
  rect(601,123,39,31,0);
  if(visible){
    drawText5x7(611,125,"K7",6);
    char value[8];
    // 39 pixels allow 5 glyphs: e.g. 8.0X (the actual measured speed).
    snprintf(value,sizeof(value),"%u.%uX",(unsigned)(hundredths/100u),
             (unsigned)((hundredths%100u)/10u));
    drawText5x7(607,139,value,3);
  }
  lastK7Visible=visible;lastK7Rate=hundredths;
}

void TKVGA::drawText5x7(int x, int y, const char *text, uint8_t fg) {
  for (int n=0; text && text[n] && x+5<=639; ++n, x+=6) {
    const unsigned char c=(unsigned char)text[n];
    unsigned char ascii=c;
    // Only the two Portuguese uppercase glyphs needed by the F1 joystick
    // label. Decode both UTF-8 bytes as one visible character (one cell),
    // retaining the proven ASCII font and spacing everywhere else.
    bool tildeA=false, cedillaC=false;
    if(c==0xC3 && text[n+1]){
      const unsigned char next=(unsigned char)text[n+1];
      if(next==0x83){ascii='A';tildeA=true;++n;} // Ã
      else if(next==0x87){ascii='C';cedillaC=true;++n;} // Ç
    }
    const uint8_t *glyph=font5x7[(ascii>=32 && ascii<=126)?ascii-32:0];
    for (int gx=0;gx<5;++gx)
      for (int gy=0;gy<7;++gy)
        if (glyph[gx]&(1u<<gy)) rect(x+gx,y+gy,1,1,fg);
    if(tildeA){ // tilde two pixels above the uppercase A
      rect(x+1,y-2,1,1,fg);
      rect(x+2,y-3,1,1,fg);
      rect(x+3,y-2,1,1,fg);
    }
    if(cedillaC){ // cedilla below the uppercase C
      rect(x+2,y+7,1,1,fg);
      rect(x+1,y+8,2,1,fg);
    }
  }
}

void TKVGA::show(){
  // Video pixels are written directly to the VGA controller's framebuffer;
  // there is no Canvas primitive queue to drain.
}


// The ROM is no longer embedded, so show an actionable VGA diagnostic when
// the SD card or file is missing. F12 reattempts mounting/loading the ROM.
void TKVGA::showStartupError(const char *reason){
  rect(0,0,640,200,0);
  const char *lines[]={"TK-2000 - ERRO NA INICIALIZACAO",reason,
                       "/TK-2000/ROMS/TK-2000.ROM",
                       "/TK-2000/ROMS/DISK-II.ROM",
                       "CORRIJA O SD E PRESSIONE F12"};
  for (int i=0; i<5; ++i) {
    const char *s=lines[i];
    if (!s) continue;
    int x=20,y=10+i*37;
    for (int n=0; s[n] && n<46; ++n) {
      unsigned char ch=(unsigned char)s[n];
      if (ch<32 || ch>126) ch=' ';
      for (int gx=0; gx<5; ++gx)
        for (int gy=0; gy<7; ++gy)
          if (font5x7[ch-32][gx] & (1<<gy)) rect(x+gx*2,y+gy*2,2,2,7);
      x+=12;
    }
  }
  cacheValid=false;
  ledStates[0]=ledStates[1]=255;
  needsFullClear=true;
}

// Compact F1 overlay. Only the central 390x170 area is painted, retaining
// the emulated bitmap and both disk LEDs in the surrounding margins.
void TKVGA::showMainMenu(int selected, bool showFPS, bool joystickEnabled, bool keyboardBR, bool diskLEDsEnabled, bool driveSoundEnabled){
  const int x=125, y=15, w=390, h=170;
  rect(x,y,w,h,4);              // blue outline
  rect(x+2,y+2,w-4,h-4,0);      // opaque black only inside the dialog
  rect(x+5,y+5,w-10,16,4);
  drawText5x7(x+13,y+10,"TK-2000 - MENU PRINCIPAL (F1)",7);
  const char *options[]={
    "SELECIONAR DISCO (F2)",
    "SELECIONAR FITA (F3)",
    "EXIBIR FPS",
    "EMULAÇÃO JOYSTICK (F4)",
    "LAYOUT TECLADO",
    "LED DISCOS",
    "SOM DOS DRIVES",
    "COLD RESET",
    "REINICIAR ESP32"
  };
  for(int i=0;i<9;++i){
    const int rowY=y+25+i*13;
    if(i==selected)rect(x+8,rowY-2,w-16,12,4);
    drawText5x7(x+17,rowY,options[i],7);
    if(i==2)drawText5x7(x+310,rowY,showFPS?"ON":"OFF",i==selected?7:3);
    if(i==3)drawText5x7(x+310,rowY,joystickEnabled?"ON":"OFF",i==selected?7:3);
    if(i==4)drawText5x7(x+310,rowY,keyboardBR?"BR":"US",i==selected?7:3);
    if(i==5)drawText5x7(x+310,rowY,diskLEDsEnabled?"ON":"OFF",i==selected?7:3);
    if(i==6)drawText5x7(x+310,rowY,driveSoundEnabled?"ON":"OFF",i==selected?7:3);
  }
  rect(x+8,y+147,w-16,1,7);
  drawText5x7(x+18,y+154,"SETAS: ESCOLHER  ENTER: EXECUTAR  ESC: SAIR",7);
}

// Update at most two F1 rows, never clear the 390x170 dialog underneath.
void TKVGA::updateMainMenuSelection(int oldSelected,int selected,bool showFPS,bool joystickEnabled,bool keyboardBR,bool diskLEDsEnabled,bool driveSoundEnabled) {
  const char* options[]={"SELECIONAR DISCO (F2)","SELECIONAR FITA (F3)","EXIBIR FPS",
                         "EMULAÇÃO JOYSTICK (F4)","LAYOUT TECLADO","LED DISCOS",
                         "SOM DOS DRIVES","COLD RESET","REINICIAR ESP32"};
  for(int pass=0;pass<2;++pass){
    const int i=pass?selected:oldSelected;
    if(i<0||i>=9||(pass && selected==oldSelected))continue;
    const int x=125,y=15,w=390,rowY=y+25+i*13;
    rect(x+8,rowY-2,w-16,12,i==selected?4:0);
    drawText5x7(x+17,rowY,options[i],7);
    const char *setting=nullptr;
    if(i==2)setting=showFPS?"ON":"OFF";
    if(i==3)setting=joystickEnabled?"ON":"OFF";
    if(i==4)setting=keyboardBR?"BR":"US";
    if(i==5)setting=diskLEDsEnabled?"ON":"OFF";
    if(i==6)setting=driveSoundEnabled?"ON":"OFF";
    if(setting)drawText5x7(x+310,rowY,setting,i==selected?7:3);
  }
}

void TKVGA::updateDiskMenuScroll(const char* const* names,int rows,int total,int selected,int scroll) {
  const int visible=rows<15?rows:15;
  rect(18,48,606,122,0);
  char line[104];
  for(int i=0;i<visible;++i){
    const int index=scroll+i;if(index>=total)break;
    const int y=49+i*8;
    if(index==selected)rect(18,y-1,594,8,4);
    snprintf(line,sizeof(line),"%d  %.88s",index+1,names[i]?names[i]:"");
    drawText5x7(25,y,line,7);
  }
  if(total>visible && visible>0) {
    const int barY=48,barH=122,barX=617;
    rect(barX,barY,7,barH,7);rect(barX+1,barY+1,5,barH-2,0);
    int thumbH=(barH-2)*visible/total;
    if(thumbH<8)thumbH=8;
    if(thumbH>barH-2)thumbH=barH-2;
    const int maxScroll=total-visible;
    const int clamped=scroll<0?0:(scroll>maxScroll?maxScroll:scroll);
    rect(barX+1,barY+1+(barH-2-thumbH)*clamped/maxScroll,5,thumbH,3);
  }
}

void TKVGA::updateTapeMenuScroll(const char* const* names,int rows,int total,int selected,int scroll) {
  updateDiskMenuScroll(names,rows,total,selected,scroll);
}

void TKVGA::updateDiskMenuSelection(int oldSelected,int selected,int scroll,const char* const* names,int rows,int total) {
  const int visible=rows<15?rows:15;
  for(int pass=0;pass<2;++pass) {
    const int index=pass?selected:oldSelected;
    if(index<scroll || index>=scroll+visible || index>=total ||
       (pass && selected==oldSelected))continue;
    const int i=index-scroll,y=49+i*8;
    rect(18,y-1,594,8,index==selected?4:0);
    char line[104];
    snprintf(line,sizeof(line),"%d  %.88s",index+1,names[i]?names[i]:"");
    drawText5x7(25,y,line,7);
  }
}

void TKVGA::updateTapeMenuSelection(int oldSelected,int selected,int scroll,const char* const* names,int rows,int total) {
  updateDiskMenuSelection(oldSelected,selected,scroll,names,rows,total);
}

// Full-screen modal overlay. The normal bitmap renderer is suspended until
// dismissal and then repaints every pixel, including borders and FPS.
void TKVGA::showDiskMenu(int drive, const char *disk1, const char *disk2,
                         const char *const *names, int rows, int total,
                         int selected, int scroll, const char *message){
  // 640x200: rows at y=50..169, 8 px each; 15 entries fit without clipping
  // the title, both drive labels or the bottom two-line help legend.
  rect(0,0,640,200,0);
  rect(12,5,616,16,4);
  drawText5x7(24,9,"TK-2000 - SELECIONAR DISCO (F2)",7);
  char line[104];
  const char *d1=strrchr(disk1?disk1:"",'/');
  const char *d2=strrchr(disk2?disk2:"",'/');
  snprintf(line,sizeof(line),"D1: %.35s", disk1 && *disk1 ? (d1?d1+1:disk1):"VAZIO");
  drawText5x7(22,25,line,drive==0?3:7);
  snprintf(line,sizeof(line),"D2: %.35s", disk2 && *disk2 ? (d2?d2+1:disk2):"VAZIO");
  drawText5x7(324,25,line,drive==1?3:7);
  rect(12,36,616,1,7);
  snprintf(line,sizeof(line),"%.93s",message?message:"");
  drawText5x7(22,40,line,2);
  const int visible = rows<15?rows:15;
  const int firstY=49;
  for(int i=0;i<visible;++i){
    const int index=scroll+i;
    if(index>=total)break;
    const int y=firstY+i*8;
    if(index==selected)rect(18,y-1,594,8,4);
    snprintf(line,sizeof(line),"%d  %.88s",index+1,names[i]?names[i]:"");
    drawText5x7(25,y,line,7);
  }
  // Proportional scroll thumb: shows both position and window size even when
  // there are hundreds/thousands of images in the PSRAM-backed directory.
  if(total>visible && visible>0){
    const int barY=48, barH=122, barX=617;
    rect(barX,barY,7,barH,7);
    rect(barX+1,barY+1,5,barH-2,0);
    int thumbH=(barH-2)*visible/total;
    if(thumbH<8)thumbH=8;
    if(thumbH>barH-2)thumbH=barH-2;
    const int maxScroll=total-visible;
    const int clampedScroll=scroll<0?0:(scroll>maxScroll?maxScroll:scroll);
    const int top=barY+1+(barH-2-thumbH)*clampedScroll/maxScroll;
    rect(barX+1,top,5,thumbH,3);
  }
  rect(12,172,616,1,7);
  drawText5x7(22,177,"SETAS: SCROLL   1/2: DRIVE   ENTER: MONTAR",7);
  drawText5x7(22,187,"E: EJETAR   ESC/F2: SAIR",7);
}

// Independent F3 tape selector. Shares the full-screen F2 visual vocabulary,
// but has its own list, selection, create-file editor and confirmation state.
void TKVGA::showTapeMenu(const char* selectedTape,const char* const* names,
                         int rows,int total,int selected,int scroll,
                         const char* message,const char* entry,bool creating,
                         bool overwrite,bool saving,const char* transport,
                         uint16_t block,uint16_t blocks,uint8_t progress) {
  rect(0,0,640,200,0);
  rect(12,5,616,16,4);
  drawText5x7(24,9,"TK-2000 - SELECIONAR FITA (F3)",7);
  char line[106];
  const char* basename=selectedTape?strrchr(selectedTape,'/'):nullptr;
  snprintf(line,sizeof(line),"FITA: %.53s",selectedTape && *selectedTape?
           (basename?basename+1:selectedTape):"NENHUMA");
  drawText5x7(22,25,line,3);
  // Transport state and CA block count use the same F2 menu colors; no
  // extra full-screen overlays or host-side timing depend on this display.
  if(transport && *transport){
    snprintf(line,sizeof(line),"%.18s",transport);
    drawText5x7(505,25,line,3);
  }
  rect(12,36,616,1,7);
  snprintf(line,sizeof(line),"%.69s",message?message:"");
  drawText5x7(22,40,line,2);
  if(blocks){
    snprintf(line,sizeof(line),"%u/%u %u%%",(unsigned)block,
             (unsigned)blocks,(unsigned)progress);
    drawText5x7(529,40,line,3);
  }
  if(overwrite){
    rect(40,74,560,60,4);rect(42,76,556,56,0);
    drawText5x7(64,83,"ARQUIVO CT2 JA EXISTE",7);
    drawText5x7(64,100,"S: SUBSTITUIR   A: ADICIONAR FAIXA",3);
    drawText5x7(64,116,"N/ESC: CANCELAR (MANTER ORIGINAL)",7);
  } else if(creating) {
    rect(35,69,570,70,4);rect(37,71,566,66,0);
    drawText5x7(57,82,"CRIAR NOVO ARQUIVO CT2",7);
    snprintf(line,sizeof(line),"> %.76s_",entry?entry:"");
    drawText5x7(57,101,line,3);
    drawText5x7(57,121,"ENTER: CRIAR     ESC: CANCELAR",7);
  } else {
    const int visible=rows<15?rows:15;
    for(int i=0;i<visible;++i){
      const int index=scroll+i;
      if(index>=total)break;
      const int y=49+i*8;
      if(index==selected)rect(18,y-1,594,8,4);
      snprintf(line,sizeof(line),"%d  %.88s",index+1,names[i]?names[i]:"");
      drawText5x7(25,y,line,7);
    }
    if(total>visible && visible>0){
      const int barY=48,barH=122,barX=617;
      rect(barX,barY,7,barH,7);rect(barX+1,barY+1,5,barH-2,0);
      int thumbH=(barH-2)*visible/total;
      if(thumbH<8)thumbH=8;
      if(thumbH>barH-2)thumbH=barH-2;
      int maxScroll=total-visible;
      int current=scroll<0?0:(scroll>maxScroll?maxScroll:scroll);
      rect(barX+1,barY+1+(barH-2-thumbH)*current/maxScroll,5,thumbH,3);
    }
  }
  rect(12,172,616,1,7);
  drawText5x7(22,177,saving?"SETAS: SCROLL  ENTER: SALVAR  N: NOVA FITA":"SETAS: SCROLL  ENTER: SELECIONAR  N: NOVA",7);
  drawText5x7(22,187,"P: PLAY/PAUSE  R: REBOBINAR  E: EJETAR  ESC/F3: SAIR",7);
}
