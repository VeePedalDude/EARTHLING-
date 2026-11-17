// LiquidCrystal_I2C stub: printed text is logged as display frames.
#pragma once
#include "Arduino.h"
#include "sim_api.h"

class LiquidCrystal_I2C : public Print {
public:
  LiquidCrystal_I2C(uint8_t, uint8_t cols, uint8_t rows) : cols_(cols), rows_(rows) {}
  void init() {}
  void begin() {}
  void begin(uint8_t, uint8_t) {}
  void backlight() {}
  void noBacklight() {}
  void clear() { rig_display_clear("lcd"); rig_display_frame("lcd"); c_ = r_ = 0; }
  void home() { c_ = r_ = 0; }
  void setCursor(uint8_t c, uint8_t r) { rig_display_frame("lcd"); c_ = c; r_ = r; }
  void noCursor() {}
  void cursor() {}
  void noBlink() {}
  void blink() {}
  void display() {}
  void noDisplay() {}
  void createChar(uint8_t, uint8_t[]) {}
  size_t write(uint8_t ch) override {
    char b[2] = {(char)ch, 0};
    if (c_ < cols_ && r_ < rows_) rig_display_text("lcd", c_, r_, b);
    c_++;
    return 1;
  }
  using Print::write;

private:
  uint8_t cols_, rows_, c_ = 0, r_ = 0;
};
