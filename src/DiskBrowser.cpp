#include "DiskBrowser.h"
#include <Arduino.h>
#include <SD.h>
#include <esp_heap_caps.h>
#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {
constexpr const char* kDiskDirectory = "/TK-2000/DISKS";

bool supportedImage(const char* name, size_t length) {
  if (!name || length < 4) return false;
  const char* e = name + length - 4;
  if (e[0] != '.') return false;
  const int a = toupper(static_cast<unsigned char>(e[1]));
  const int b = toupper(static_cast<unsigned char>(e[2]));
  const int c = toupper(static_cast<unsigned char>(e[3]));
  return (a == 'D' && b == 'S' && c == 'K') ||
         (a == 'N' && b == 'I' && c == 'B');
}

int caseInsensitiveCompare(const char* a, const char* b) {
  while (*a && *b) {
    const int ca = toupper(static_cast<unsigned char>(*a));
    const int cb = toupper(static_cast<unsigned char>(*b));
    if (ca != cb) return ca < cb ? -1 : 1;
    ++a; ++b;
  }
  if (*a == *b) return 0;
  return *a ? 1 : -1;
}
} // namespace

TKDiskBrowser::~TKDiskBrowser() { clear(); }

void TKDiskBrowser::clear() {
  if (entries_) heap_caps_free(entries_);
  entries_ = nullptr;
  count_ = 0;
  capacity_ = 0;
  directoryFound_ = false;
  partialList_ = false;
}

bool TKDiskBrowser::reserveOne() {
  if (count_ < capacity_) return true;
  // Exponential growth keeps directory scanning linear, instead of
  // repeatedly copying the list for each new SD directory entry.
  if (capacity_ > (INT_MAX / 2) ||
      static_cast<size_t>(capacity_ > 0 ? capacity_ * 2 : 64) > SIZE_MAX / kPathBytes)
    return false;
  const int newCapacity = capacity_ ? capacity_ * 2 : 64;
  const size_t bytes = static_cast<size_t>(newCapacity) * kPathBytes;
  // Explicitly request PSRAM; never fall back to internal DMA-capable RAM.
  char (*next)[kPathBytes] = static_cast<char (*)[kPathBytes]>(
      heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!next) return false;
  if (entries_ && count_) memcpy(next, entries_, static_cast<size_t>(count_) * kPathBytes);
  if (entries_) heap_caps_free(entries_);
  entries_ = next;
  capacity_ = newCapacity;
  return true;
}

int TKDiskBrowser::compareNames(const void* a, const void* b) {
  const char* nameA = static_cast<const char*>(a);
  const char* nameB = static_cast<const char*>(b);
  return caseInsensitiveCompare(nameA, nameB);
}

int TKDiskBrowser::scrollForSelection(int selected, int scroll, int total) {
  if (total <= kVisibleRows) return 0;
  const int maxScroll = total - kVisibleRows;
  if (selected < 0) selected = 0;
  if (selected >= total) selected = total - 1;
  if (scroll < 0) scroll = 0;
  if (scroll > maxScroll) scroll = maxScroll;
  if (selected < scroll) scroll = selected;
  else if (selected >= scroll + kVisibleRows)
    scroll = selected - kVisibleRows + 1;
  return scroll > maxScroll ? maxScroll : scroll;
}

const char* TKDiskBrowser::pathAt(int index) const {
  return entries_ && index >= 0 && index < count_ ? entries_[index] : nullptr;
}

bool TKDiskBrowser::refresh() {
  // Reuse the PSRAM allocation while F1 is open, freeing it on menu exit.
  count_ = 0;
  directoryFound_ = false;
  partialList_ = false;
  File dir = SD.open(kDiskDirectory, FILE_READ);
  if (!dir || !dir.isDirectory()) {
    if (dir) dir.close();
    return false;
  }
  directoryFound_ = true;
  File file = dir.openNextFile();
  unsigned scanned = 0;
  while (file) {
    if (!file.isDirectory()) {
      const char* returnedName = file.name();
      // Arduino SD implementations may return a relative basename or a full
      // pathname; always rebuild under the single approved directory.
      const char* slash = returnedName ? strrchr(returnedName, '/') : nullptr;
      const char* basename = slash ? slash + 1 : returnedName;
      if (basename && *basename && supportedImage(basename, strlen(basename)) &&
          (file.size() == 143360u || file.size() == 232960u)) {
        char path[kPathBytes];
        const int length = snprintf(path, sizeof(path), "%s/%s", kDiskDirectory, basename);
        if (length > 0 && static_cast<size_t>(length) < sizeof(path) && SD.exists(path)) {
          if (!reserveOne()) {
            partialList_ = true;
            file.close();
            break;
          }
          memcpy(entries_[count_++], path, static_cast<size_t>(length) + 1u);
        }
      }
    }
    file.close();
    if ((++scanned & 31u) == 0u) delay(1); // FreeRTOS task/idle opportunity
    file = dir.openNextFile();
  }
  if (file) file.close();
  dir.close();
  // Sorting makes the index stable for the same SD contents, independent of
  // FAT directory enumeration order and without loading images themselves.
  if (count_ > 1) qsort(entries_, static_cast<size_t>(count_), kPathBytes, compareNames);
  return !partialList_;
}
