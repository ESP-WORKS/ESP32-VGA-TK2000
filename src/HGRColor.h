#pragma once
#include <stdint.h>
// Reference: tk2000-emulator-master/src/tk_video.cpp DrawColorVideoSource.
// Palette indices: BLACK=0, RED=1, GREEN=2, BLUE=4, CYAN=6, WHITE=7.
// The bit7 of each 7-pixel byte selects GREEN/BLUE or CYAN/RED.
// An OFF pixel between two ON pixels generates the opposite artifact phase,
// as in the original standard color-video lookup (not white-text filter mode).
static inline bool TKHGRBit(const uint8_t row[40],int x){
  return x>=0 && x<280 && (row[x/7] & (uint8_t)(1u << (x%7)));
}
static inline uint8_t TKHGRColor(const uint8_t row[40],int x,bool color){
  const bool current=TKHGRBit(row,x);
  const bool left=TKHGRBit(row,x-1);
  const bool right=TKHGRBit(row,x+1);
  if(!color)return current?7:0;
  if(current && (left||right))return 7; // adjacent ON bits -> white
  if(!current && !(left&&right))return 0;
  const bool phase=(row[x/7]&0x80u)!=0;
  const bool parity=((x&1)!=0) ^ !current; // holes invert parity
  static const uint8_t colors[4]={2,6,4,1}; // GREEN,CYAN,BLUE,RED
  return colors[((uint8_t)parity << 1) | (uint8_t)phase];
}
