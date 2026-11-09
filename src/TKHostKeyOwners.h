#pragma once
#include <stdint.h>
#include "fabgl.h"

// Physical PS/2 VKs are independent owners of one guest matrix key.
// Thus main-row '1' and keypad '1' cannot release each other. Only transitions
// 0 -> 1 and 1 -> 0 are sent to KeyboardTK; host typematic is ignored.
// Storage is fixed, with no heap/ISR use. Used only on host keyboard events.
class TKHostKeyOwners {
public:
  static constexpr unsigned kCapacity=64;
  void clear() {
    for (unsigned i=0;i<kCapacity;++i) _slots[i].code=0;
  }
  template<class Emit>
  void event(fabgl::VirtualKey vk,bool down,uint8_t code,const Emit &emit) {
    if (vk==fabgl::VK_NONE) return;
    int found=findVK(vk);
    if (down) {
      if (found>=0 || !code) return;
      int freeSlot=-1;
      for(unsigned i=0;i<kCapacity;++i) if (!_slots[i].code) {freeSlot=(int)i;break;}
      if (freeSlot<0) return; // Never press a key which cannot be tracked/released.
      const bool first=!hasCode(code);
      _slots[freeSlot].vk=vk;
      _slots[freeSlot].code=code;
      _slots[freeSlot].missing=0;
      if (first) emit(code,true);
    } else {
      // FabGL may change a symbol VK when SHIFT changes before key-up.
      // An unmatched release can fall back to the same translated contact,
      // but an exact VK match ALWAYS wins (e.g. main '1' vs keypad '1').
      if(found<0 && code) {
        for(unsigned i=0;i<kCapacity;++i)
          if (_slots[i].code==code) { found=(int)i;break; }
      }
      if(found>=0) releaseAt((unsigned)found,emit);
    }
  }
  template<class IsDown,class Emit>
  void releaseMissing(const IsDown &isDown,const Emit &emit) {
    for(unsigned i=0;i<kCapacity;++i) {
      if (!_slots[i].code) continue;
      if (isDown(_slots[i].vk)) { _slots[i].missing=0; continue; }
      // Prevent a transient stale PS/2 snapshot from dropping a held
      // key. A normal KEYUP is still delivered immediately by event().
      if (++_slots[i].missing>=3) releaseAt(i,emit);
    }
  }
  unsigned active() const {
    unsigned n=0;for(unsigned i=0;i<kCapacity;++i) if(_slots[i].code)++n;
    return n;
  }
private:
  struct Slot {fabgl::VirtualKey vk=fabgl::VK_NONE;uint8_t code=0;uint8_t missing=0;};
  Slot _slots[kCapacity] = {};
  int findVK(fabgl::VirtualKey vk) const {
    for(unsigned i=0;i<kCapacity;++i)
      if (_slots[i].code && _slots[i].vk==vk) return (int)i;
    return -1;
  }
  bool hasCode(uint8_t code) const {
    for(unsigned i=0;i<kCapacity;++i) if(_slots[i].code==code)return true;
    return false;
  }
  template<class Emit> void releaseAt(unsigned index,const Emit &emit) {
    const uint8_t code=_slots[index].code;
    _slots[index].code=0;
    _slots[index].missing=0;
    if(code && !hasCode(code)) emit(code,false);
  }
};
