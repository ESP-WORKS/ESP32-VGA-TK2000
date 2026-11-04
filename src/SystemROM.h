#pragma once
#include <stddef.h>
#include <stdint.h>
class TKMachine;

// Required SD path: /TK-2000/ROMS/TK-2000.ROM.
// All three components are searched without ASCII case sensitivity.
enum class TKROMStatus : uint8_t {
  OK, FOLDER_MISSING, FILE_MISSING, OPEN_FAILED, INVALID_SIZE,
  OUT_OF_MEMORY, READ_FAILED, INVALID_VECTOR, INVALID_CONTENT
};

TKROMStatus TKLoadSystemROMFromSD(TKMachine &machine,
                                   char *resolvedPath=nullptr,
                                   size_t resolvedCapacity=0);
const char *TKROMStatusText(TKROMStatus status);

// External TK-2000 disk-interface ROM; same path search and case handling.
// Required path: /TK-2000/ROMS/DISK-II.ROM (exactly 256 bytes).
TKROMStatus TKLoadDiskROMFromSD(TKMachine &machine,
                                 char *resolvedPath=nullptr,
                                 size_t resolvedCapacity=0);
const char *TKDiskROMStatusText(TKROMStatus status);
