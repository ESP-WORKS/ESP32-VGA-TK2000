#pragma once
#include <Arduino.h>
#include <FS.h>

class DiskImage {
public:
  ~DiskImage();
  DiskImage()=default;
  DiskImage(const DiskImage&)=delete;
  DiskImage& operator=(const DiskImage&)=delete;
  enum Type { NONE, DSK, NIB };
  bool mount(const char *path, bool writable=true);
  bool eject(bool flush=true);
  bool mounted() const { return _type != NONE; }
  bool writable() const { return _writable; }
  Type type() const { return _type; }
  const char *path() const { return _path.c_str(); }
  uint8_t *track(uint8_t t);
  // Legacy TK disk nibblizer produces 6384 bytes for a DOS-order .DSK
  // track; raw .NIB images contain 6656 physical bytes per track.
  size_t trackSize() const { return _type==DSK ? 6384u : 6656u; }
  bool flush();
  bool dirty() const;
  void markDirty() { if (_writable && _cachedTrack>=0) _trackDirty[_cachedTrack]=true; }
  bool loadTrack(uint8_t t);
  bool saveTrack(uint8_t t);
private:
  Type _type=NONE;
  bool _writable=false, _dirty=false;
  String _path;
  uint8_t *_image=nullptr;
  size_t _size=0;
  int _cachedTrack=-1;
  // Independent nibble buffers in PSRAM: formatting several tracks never
  // overwrites an unfinished one. Allocated lazily (up to 35 x 6656 bytes).
  uint8_t *_tracks[35]={};
  bool _trackDirty[35]={};
  void freeTrackBuffers();
  bool preserveOriginalBackup();
};
