#pragma once
#include <stddef.h>

// Index of SD disk images. The *entire* growing list is stored in ESP32 PSRAM;
// only 15 pointers are passed to the VGA menu at a time. No fixed file limit.
class TKDiskBrowser {
public:
  static const int kVisibleRows = 15;
  static const size_t kPathBytes = 128;
  TKDiskBrowser() = default;
  ~TKDiskBrowser();
  TKDiskBrowser(const TKDiskBrowser&) = delete;
  TKDiskBrowser& operator=(const TKDiskBrowser&) = delete;

  void clear();
  bool refresh(); // returns true when scan finishes without allocation failure
  int count() const { return count_; }
  const char* pathAt(int index) const;
  static int scrollForSelection(int selected, int scroll, int total);
  bool directoryFound() const { return directoryFound_; }
  bool partialList() const { return partialList_; }
  size_t bytesReserved() const { return static_cast<size_t>(capacity_) * kPathBytes; }
private:
  bool reserveOne();
  static int compareNames(const void* a, const void* b);
  char (*entries_)[kPathBytes] = nullptr;
  int count_ = 0;
  int capacity_ = 0;
  bool directoryFound_ = false;
  bool partialList_ = false;
};
