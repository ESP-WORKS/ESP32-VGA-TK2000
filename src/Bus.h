#pragma once
#include <stdint.h>
#include <stddef.h>
class TKMachine;
class TKBus {
public:
  TKMachine* m=nullptr;
  uint8_t read(uint16_t a); void write(uint16_t a,uint8_t v);
};
