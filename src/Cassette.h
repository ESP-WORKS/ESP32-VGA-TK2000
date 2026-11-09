#pragma once
#include <stddef.h>
#include <stdint.h>
#include "TKTapeGameControls.h"
#include "BuildConfig.h"

// CT2 is a stream of CTK2/CA/CB/DA records, NOT a dump of guest memory.
// Decode cassette transitions lazily: a 25 KiB CT2 can contain hundreds of
// thousands of edges, so expanding the waveform into std::vector<uint16_t>
// consumes excessive heap and can starve FabGL DMA.
class CassetteCT2 {
public:
  enum Request : uint8_t { None, Load, Save };
  CassetteCT2() = default;
  ~CassetteCT2();
  CassetteCT2(const CassetteCT2&) = delete;
  CassetteCT2& operator=(const CassetteCT2&) = delete;

  // Takes ownership of bytes allocated with malloc/ps_malloc; on failure,
  // ownership remains with caller. CT2 content is validated before install.
  bool mount(uint8_t* bytes, size_t size);
  void eject();
  bool mounted() const { return image_ != nullptr; }
  // A mounted CT2 is only a medium. The disk slot is withdrawn only from
  // the first real cassette read/write until the entire CT2 reaches 100%.
  // Auto/manual pauses between loader stages remain part of one operation.
  bool operationActive() const {
    return recording_ || request_!=None ||
           (everStarted_ && !finished_);
  }

#if TK2000_K7_MOON_PROFILE
  uint32_t diagnosticReadSamples() const { return diagnosticReadSamples_; }
  uint32_t diagnosticDecodedEdges() const { return diagnosticDecodedEdges_; }
#endif
  TKGameInputProfile gameInputProfile() const { return gameInputProfile_; }
  static bool valid(const uint8_t* bytes, size_t size);
  void resetTransport(); // Reset/F12 rewinds tape, but retains selected CT2.
  void beginPlay();
  uint8_t read(uint64_t cycles); // input line: bit 7 for $C010 ROM reader
  bool playing() const { return playing_; }
  uint16_t audioHalfWaveCycles() const { return edgeDuration_; }

  // Next actual half-wave transition in guest cycles; 0 before the first
  // sample or while stopped. Used solely by the ROM busy-wait fast path.
  uint64_t nextEdgeDeadline() const { return playing_ ? nextEdgeCycle_ : 0; }
  bool autoPaused() const { return autoPaused_; }
  bool manualPaused() const { return manualPaused_; }
  bool paused() const { return autoPaused_ || manualPaused_; }
  // Keep a cassette's transport position when a multi-stage game runs code
  // between reads. All times are *emulated* CPU cycles, not wall clock.
  void service(uint64_t cycles);
  void togglePause(uint64_t cycles);
  void rewind();
  // The TK monitor ROM is a known cassette reader. Other loader loops must
  // show sustained, repeated C010 polling without keyboard-row changes.
  bool qualifiesInput(uint16_t pc, uint64_t cycles);
  void keyboardRowChanged() { probeCount_=0; customReaderPC_=0; }
  uint16_t blockNumber() const { return currentBlock_; }
  uint16_t blockCount() const { return totalBlocks_; }
  uint8_t progress() const;
  bool finished() const { return finished_; }
  // A selected CT2 can be loaded again without reopening F3. A new ROM
  // cassette read following a long non-cassette BASIC interval rewinds it.
  bool mayRestartRead(uint64_t cycles) const {
    return finished_ && lastInputCycle_ && cycles>lastInputCycle_ &&
           cycles-lastInputCycle_>=250000u;
  }

  void requestLoad() { if (request_ == None) request_ = Load; }
  void requestSave() { if (request_ == None) request_ = Save; }
  Request request() const { return request_; }
  void clearRequest() { request_ = None; }
  void beginRecord();
  void outputToggle(uint64_t cycles); // read OR write of $C020-$C02F
  bool recording() const { return recording_; }
  bool recordFinished(uint64_t cycles) const;
  void stopRecord() { recording_ = false; }
  size_t capturedEdges() const { return edgesCount_; }
  uint16_t capturedDuration(size_t i) const { return i<edgesCount_?edges_[i]:0; }
  uint64_t lastOutputCycle() const { return lastOutputCycle_; }
  // Returns malloc buffer containing a complete CTK2 file; caller frees it.
  // Failed decodes cannot corrupt any existing SD file.
  bool encodeRecording(uint8_t** result, size_t* size) const;

private:
  uint8_t* image_ = nullptr;
  size_t imageSize_ = 0, recordAt_ = 4, chunkAt_ = 4;
  uint16_t chunkSize_ = 0;
  enum ChunkKind : uint8_t { ChunkNone, ChunkLeader, ChunkSync, ChunkData };
  ChunkKind chunkKind_=ChunkNone;
  size_t cachedByteIndex_=(size_t)-1;
  uint8_t cachedDataByte_=0;
  uint32_t edgeInChunk_ = 0;
  uint32_t edgesThisChunk_ = 0;
  uint16_t edgeDuration_ = 0;
  uint64_t nextEdgeCycle_ = 0, lastInputCycle_ = 0;
#if TK2000_K7_MOON_PROFILE
  // Monotonic counters, read only once per 5s; no micros() in the CT2 hot path.
  uint32_t diagnosticReadSamples_ = 0;
  uint32_t diagnosticDecodedEdges_ = 0;
#endif
  uint8_t level_ = 0x80;
  bool playing_ = false, finished_ = false;
  bool autoPaused_ = false, manualPaused_ = false, everStarted_ = false;
  // Snapshot the pending half-wave; do not drop its remainder across pauses.
  uint64_t pendingEdgeCycles_ = 0;
  static constexpr uint64_t kIdleCycles = 262144u;
  uint16_t totalBlocks_ = 0, currentBlock_ = 0;
  uint64_t totalPulseCycles_=0, advancedPulseCycles_=0;
  uint16_t probePC_ = 0, customReaderPC_ = 0;
  uint8_t probeCount_ = 0;
  uint64_t probeLastCycle_ = 0;
  void suspend(bool manual);
  void resume(uint64_t cycles);
  bool appleFormat_ = false;
  TKGameInputProfile gameInputProfile_ = TKGameInputProfile::Standard;
  Request request_ = None;
  uint16_t *edges_ = nullptr;
  size_t edgesCount_ = 0, edgesCapacity_ = 0;
  uint64_t lastOutputCycle_ = 0;
  bool recording_ = false;
  bool nextChunk();
  uint16_t nextDuration();
  bool reserveEdge();
};
