// I2C stub: transactions succeed and are counted; reads return nothing.
#pragma once
#include "Arduino.h"

class TwoWire : public Print {
public:
  void begin() {}
  void begin(uint8_t) {}
  void setClock(uint32_t) {}
  void setSDA(uint32_t) {}
  void setSCL(uint32_t) {}
  void beginTransmission(uint8_t addr) { addr_ = addr; }
  uint8_t endTransmission(bool = true) { tx_count++; return 0; }
  uint8_t requestFrom(uint8_t, uint8_t, bool = true) { return 0; }
  int available() { return 0; }
  int read() { return -1; }
  size_t write(uint8_t) override { bytes_written++; return 1; }
  using Print::write;
  unsigned long tx_count = 0, bytes_written = 0;

private:
  uint8_t addr_ = 0;
};
extern TwoWire Wire;
