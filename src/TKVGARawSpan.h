#pragma once
#include <stdint.h>
#include <string.h>
#include "BuildConfig.h"

// FabGL VGA raw pixels are packed [2,3,0,1] within each group of 4.
// Uses one short contiguous memset for the aligned interior instead of
// long per-pixel writes during VGA DMA scanout. Handles arbitrary edges.
static inline void TKVGAFillPackedSpan(uint8_t *scanline, int x, int endX,
                                      uint8_t rawColor) {
  if (!scanline || endX<=x) return;
  while (x<endX && (x&3)) {
    scanline[x^2]=rawColor;
    ++x;
  }
  const int alignedEnd=endX&~3;
  if (alignedEnd>x) {
#if TK2000_VGA_WORD_STORES
    // 4 visible pixels = one naturally aligned 32-bit FabGL scan word.
    // Aligned words cannot be partially changed by another CPU load;
    // this minimizes 4-pixel streaks if DMA fetches this line mid-draw.
    if ((((uintptr_t)(scanline+x)) & 3u)==0u) {
      volatile uint32_t *dst=(volatile uint32_t *)(void *)(scanline+x);
      const uint32_t word=(uint32_t)rawColor*0x01010101u;
      for (int i=0;i<(alignedEnd-x)/4;++i) dst[i]=word;
    } else
#endif
      memset(scanline+x,rawColor,(size_t)(alignedEnd-x));
    x=alignedEnd;
  }
  while (x<endX) {
    scanline[x^2]=rawColor;
    ++x;
  }
}

// Copy a pre-rendered HGR row without exposing partially updated 4-pixel
// groups to the VGA DMA. The 560-pixel active region starts at x=40 and
// is exactly divisible by four. Fallback preserves arbitrary alignment.
static inline void TKVGACopyPackedRow(uint8_t *dst,const uint8_t *src,size_t n) {
#if TK2000_VGA_WORD_STORES
  if ((((uintptr_t)dst)&3u)==0 && (n&3u)==0) {
    volatile uint32_t *words=(volatile uint32_t *)(void *)dst;
    for(size_t i=0;i<n/4u;++i) {
      uint32_t word;
      memcpy(&word,src+i*4u,4u);
      words[i]=word;
    }
    return;
  }
#endif
  memcpy(dst,src,n);
}
