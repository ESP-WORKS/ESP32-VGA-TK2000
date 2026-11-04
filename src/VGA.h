#pragma once
#include <stdint.h>
#include "fabgl.h"
#include "TKVideoDirty.h"
class TKVGA {
public:
 TKVGA() {}
 fabgl::VGAController ctrl; uint8_t* ram=nullptr; bool color=true; uint8_t page=1; int frame=0;
 void setDirtyTracker(TKVideoDirty* tracker) { dirtyTracker=tracker; }
 bool begin(uint8_t*r); void render(bool showFPS=false, uint16_t fps=0, uint16_t eps=0); void show();
 void showStartupError(const char *reason);
 void showDiskMenu(int drive, const char *disk1, const char *disk2,
                   const char *const *names, int rows, int total, int selected,
                   int scroll, const char *message);
 void showTapeMenu(const char *selectedTape, const char *const* names, int rows,
                   int total, int selected, int scroll, const char *message,
                   const char *entry, bool creating, bool overwrite, bool saving,
                   const char *transport=nullptr, uint16_t block=0,
                   uint16_t blocks=0, uint8_t progress=0);
 void showMainMenu(int selected,bool showFPS,bool joystickEnabled,bool keyboardBR,bool diskLEDsEnabled,bool driveSoundEnabled);
 // Direct-framebuffer row changes avoid blacking out the entire menu on
 // each PS/2 typematic arrow event (the source of visible flashing).
 void updateMainMenuSelection(int oldSelected,int selected,bool showFPS,bool joystickEnabled,bool keyboardBR,bool diskLEDsEnabled,bool driveSoundEnabled);
 void updateDiskMenuSelection(int oldSelected,int selected,int scroll,const char* const* names,int rows,int total);
 void updateTapeMenuSelection(int oldSelected,int selected,int scroll,const char* const* names,int rows,int total);
 // Redraw only scrolling rows + scrollbar, keeping headers and footers.
 void updateDiskMenuScroll(const char* const* names,int rows,int total,int selected,int scroll);
 void updateTapeMenuScroll(const char* const* names,int rows,int total,int selected,int scroll);

 void dismissMainMenu() { invalidate(); }
 void dismissDiskMenu() { needsFullClear=true; cacheValid=false; }
 // Disk LEDs are in the left 40-pixel border: off/green read/red write.
 void drawDiskLEDs(uint8_t d1, uint8_t d2, bool visible=true);
 // F4 joystick confirmation, using the same small OSD palette as the F1/FPS panels.
 void drawJoystickStatus(bool visible, bool enabled);
 // Effective guest K7 speed without a serial monitor (right border only).
 void drawK7TurboRate(bool visible,uint16_t hundredths);

 void invalidate() { cacheValid=false; ledStates[0]=ledStates[1]=255; joystickStatus=255; lastK7Rate=0xFFFFu; lastK7Visible=false; needsFullClear=true; }

private:
 TKVideoDirty* dirtyTracker=nullptr;
 void rect(int x,int y,int w,int h,uint8_t c);
 void hires();
 void drawText5x7(int x, int y, const char *text, uint8_t fg);
 uint8_t lastRows[192][40] = {};
 void hiresRow(int y, const uint8_t row[40]);
 bool cacheValid = false;
 bool needsFullClear = false;
 uint8_t lastPage = 0;
 bool lastColor = false;
 bool lastShowFPS = false;
 uint16_t lastEPS = 0xFFFF;
 uint8_t rawColors[8] = {};
 uint8_t ledStates[2] = {255,255};
 uint8_t joystickStatus=255;
 uint16_t lastK7Rate=0xFFFFu;
 bool lastK7Visible=false;
};
