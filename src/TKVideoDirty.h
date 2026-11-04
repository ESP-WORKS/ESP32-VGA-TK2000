#pragma once
#include <stdint.h>
#include <string.h>

// Tracks guest writes to the two Apple-II-style 8 KiB HGR pages. The bytes
// outside the 40 visible columns in each scanline are NOT rendered. No
// allocation or PSRAM read is required for a write notification.
class TKVideoDirty {
public:
  void reset() { memset(pending_, 1, sizeof(pending_)); }
  void mark(uint16_t address) {
    const uint8_t page = (address >= 0xA000u && address < 0xC000u) ? 1u : 0u;
    if (page == 0 && (address < 0x2000u || address >= 0x4000u)) return;
    if (page == 1 && (address < 0xA000u || address >= 0xC000u)) return;
    const uint16_t off = uint16_t(address & 0x1FFFu);
    const uint8_t low = uint8_t(off & 0x7Fu);
    // Three visible 40-byte rows per 128-byte group; the 8 trailing bytes
    // are blanking and must not dirty a visible scanline.
    if (low >= 120u) return;
    // Avoid division by 40 on every HGR guest write.  The three
    // 40-byte ranges are the three scanline bands in each 128-byte block.
    const uint8_t band = low < 40u ? 0u : (low < 80u ? 1u : 2u);
    const uint8_t y = uint8_t(band * 64u + ((off >> 7) & 7u) * 8u + ((off >> 10) & 7u));
    pending_[page][y] = 1;
  }
  bool take(uint8_t page, int y) {
    if (page < 1 || page > 2 || y < 0 || y >= 192) return true;
    const uint8_t p = uint8_t(page-1);
    const bool was = pending_[p][y] != 0;
    pending_[p][y] = 0;
    return was;
  }
  bool pending(uint8_t page, int y) const {
    return page >= 1 && page <= 2 && y >= 0 && y < 192 && pending_[page-1][y];
  }
private:
  uint8_t pending_[2][192] = {};
};
