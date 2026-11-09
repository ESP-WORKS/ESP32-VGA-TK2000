#pragma once
#include <stdint.h>
#include <assert.h>
#define VGA_640x200_70Hz 0
namespace fabgl {
struct RGB222 { int r,g,b; RGB222(int R,int G,int B):r(R),g(G),b(B) {} };
class VGAController {
public:
  void begin() {}
  void setResolution(int) {}
  int getViewPortWidth() const {return 640;}
  int getViewPortHeight() const {return 200;}
  uint8_t createRawPixel(RGB222 rgb) const {return uint8_t((rgb.r<<4)|(rgb.g<<2)|rgb.b);}
  uint8_t* getScanline(int y) {assert(y>=0&&y<200);return scanlines[y];}
  void setRawPixel(int x,int y,uint8_t value) {assert(x>=0&&x<640&&y>=0&&y<200);scanlines[y][x]=value;}
private:
  uint8_t scanlines[200][640]={};
};
}
