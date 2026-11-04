#pragma once
#include <stdint.h>
class VideoTK { public: uint8_t page=1; bool color=true; bool inverse=false; void writeSwitch(uint16_t a); void render(uint8_t*ram); };
