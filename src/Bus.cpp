#include "Bus.h"
#include "Machine.h"
uint8_t TKBus::read(uint16_t a){return m->read(a);} void TKBus::write(uint16_t a,uint8_t v){m->write(a,v);}
