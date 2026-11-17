// SPI stub: transfers return 0.
#pragma once
#include "Arduino.h"

#define SPI_MODE0 0
#define SPI_MODE1 1
#define SPI_MODE2 2
#define SPI_MODE3 3
struct SPISettings {
  SPISettings() {}
  SPISettings(uint32_t, uint8_t, uint8_t) {}
};
class SPIClass {
public:
  void begin() {}
  void end() {}
  void beginTransaction(SPISettings) {}
  void endTransaction() {}
  uint8_t transfer(uint8_t) { return 0; }
  uint16_t transfer16(uint16_t) { return 0; }
  void transfer(void *, size_t) {}
};
extern SPIClass SPI;
