// Host-only replacement for the SD-backed DiskImage (no media in tests).
#include "DiskImage.h"
DiskImage::~DiskImage(){}
bool DiskImage::mount(const char*,bool){return false;}
bool DiskImage::eject(bool){_type=NONE;return true;}
uint8_t* DiskImage::track(uint8_t){return nullptr;}
bool DiskImage::flush(){return true;}
bool DiskImage::dirty() const{return false;}
bool DiskImage::loadTrack(uint8_t){return false;}
bool DiskImage::saveTrack(uint8_t){return true;}
uint32_t millis(){return 0;} uint32_t micros(){return 0;}
