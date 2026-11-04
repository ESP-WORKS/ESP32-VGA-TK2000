#include "SDSupport.h"
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>

namespace {
constexpr int kSDClock = 14;
constexpr int kSDMosi = 12;
constexpr int kSDCs = 13;
constexpr int kTtgoMiso = 2;
constexpr int kAltMiso = 35;
constexpr uint32_t kSDFrequency = 4000000;
SPIClass sdSPI(HSPI);
bool mounted = false;

bool tryPins(int miso) {
  SD.end();
  sdSPI.end();
  delay(20);
  sdSPI.begin(kSDClock, miso, kSDMosi, kSDCs);
  if (SD.begin(kSDCs, sdSPI, kSDFrequency)) {
    Serial.printf("TK2000: SD OK: HSPI CLK=%d MISO=%d MOSI=%d CS=%d\n",
                  kSDClock, miso, kSDMosi, kSDCs);
    mounted = true;
    return true;
  }
  SD.end();
  sdSPI.end();
  return false;
}
} // namespace

bool TKMountSD() {
  if (mounted) return true;
  // TTGO VGA32 normalmente usa MISO GPIO2; GPIO35 e alternativa de placa.
  // Os demais pinos do SD seguem a montagem do hardware TTGO, nao o boot.
  if (tryPins(kTtgoMiso)) return true;
  if (tryPins(kAltMiso)) return true;
  Serial.println("TK2000: cannot mount SD on either supported MISO pin");
  return false;
}

void TKUnmountSD() {
  if (!mounted) return;
  SD.end();
  sdSPI.end();
  mounted = false;
}
