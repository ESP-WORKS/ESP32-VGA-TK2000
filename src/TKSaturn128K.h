#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#if defined(ESP32)
#include <Arduino.h>  // ps_malloc(), used before FabGL allocates DMA memory.
#endif

// Saturn 128K for the TK-2000 as decoded by rubemf's FPGA port:
// source/ramcard/ramcard.v, mapped on slot 3 at $C0B0-$C0BF.
// Eight 16-KiB banks; each bank has two independent $D000-$DFFF
// 4-KiB pages (A/B) and one shared $E000-$FFFF 8-KiB page.
// The card is always installed. Its read/write overlays start disabled;
// selecting a bank does NOT remove the stock TK BASIC ROM.
class TKSaturn128K {
public:
  static constexpr size_t kBytes = 8u * 0x4000u;

  ~TKSaturn128K() { free(_bytes); }
  TKSaturn128K(const TKSaturn128K&) = delete;
  TKSaturn128K& operator=(const TKSaturn128K&) = delete;
  TKSaturn128K() = default;

  bool allocate() {
    if (_bytes) return true;
#if defined(ESP32)
    // WROVER has PSRAM; never steal FabGL's internal DMA heap.
    _bytes = (uint8_t*)ps_malloc(kBytes);
#else
    _bytes = (uint8_t*)malloc(kBytes);
#endif
    if (!_bytes) return false;
    memset(_bytes, 0, kBytes);
    return true;
  }

  // Hardware RESET clears switches, not the contents of the expansion.
  void resetSwitches() {
    _bankB = false;
    _read = false;
    _write = false;
    _preWrite = false;
  }

  // Read AND write accesses to the same soft switches have side effects.
  // This decodes exactly the FPGA's addr[2]/addr[3]/addr[1:0] selection.
  void touch(uint16_t addr) {
    if ((addr & 0xFFF0u) != 0xC0B0u) return;
    if ((addr & 0x04u) != 0) {
      _bank = (uint8_t)(((addr & 0x08u) >> 1) | (addr & 0x03u));
    } else {
      _bankB = (addr & 0x08u) != 0;
      const bool odd = (addr & 0x01u) != 0;
      // An odd access arms writing, but only a SECOND consecutive odd
      // soft-switch access can enable it (Apple/Saturn LC safety latch).
      _write = odd && _preWrite;
      _preWrite = odd;
      _read = ((addr & 0x01u) != 0) == ((addr & 0x02u) != 0);
    }
  }

  bool readEnabled() const { return _read; }
  bool writeEnabled() const { return _write; }
  uint8_t bank() const { return _bank; }
  bool bankB() const { return _bankB; }
  bool allocated() const { return _bytes != nullptr; }

  uint8_t read(uint16_t addr) const { return _bytes[index(addr)]; }
  void write(uint16_t addr, uint8_t value) { _bytes[index(addr)] = value; }

private:
  size_t index(uint16_t addr) const {
    // FPGA: {bank16k[2],~bank16k[2],bank16k[1:0],addr[13],
    //        addr[12]&~(bankB&Dxxx),addr[11:0]}.
    // Its top bits encode the same eight logical bank indices but with a
    // fixed, nonmonotonic physical permutation not visible to the guest.
    const uint16_t offset = (addr < 0xE000u && _bankB)
                                ? (uint16_t)(addr & 0x0FFFu)
                                : (uint16_t)(addr & 0x3FFFu);
    return size_t(_bank) * 0x4000u + offset;
  }
  uint8_t* _bytes = nullptr;
  uint8_t _bank = 0;
  bool _bankB = false, _read = false, _write = false, _preWrite = false;
};
