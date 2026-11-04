#include "Video.h"
// MAME tk2000.cpp maps C050-C05F as address-decoded soft switches.
// C050=color, C051=monochrome; C054=page 1, C055=page 2.
void VideoTK::writeSwitch(uint16_t a){
  switch(a){
    case 0xC050: color=true; break;
    case 0xC051: color=false; break;
    case 0xC054: page=1; break;
    case 0xC055: page=2; break;
    default: break;
  }
}
