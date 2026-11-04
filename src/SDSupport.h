#pragma once
// Mount the TTGO VGA32 microSD using explicit HSPI wiring.
// NO flash/NVS operations are performed by this module.
bool TKMountSD();
void TKUnmountSD();
