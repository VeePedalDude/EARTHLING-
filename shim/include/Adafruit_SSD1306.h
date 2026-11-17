// Adafruit_SSD1306 / Adafruit_GFX stub: text logged per display() frame.
#pragma once
#include "Arduino.h"
#include "Wire.h"
#include "sim_api.h"

#define SSD1306_SWITCHCAPVCC 0x02
#define SSD1306_EXTERNALVCC 0x01
#define SSD1306_WHITE 1
#define SSD1306_BLACK 0
#define SSD1306_INVERSE 2
#define WHITE SSD1306_WHITE
#define BLACK SSD1306_BLACK

class Adafruit_GFX : public Print {
public:
  Adafruit_GFX(int16_t w, int16_t h) : w_(w), h_(h) {}
  void setCursor(int16_t x, int16_t y) { x_ = x; y_ = y; }
  void setTextSize(uint8_t s) { size_ = s; }
  void setTextColor(uint16_t) {}
  void setTextColor(uint16_t, uint16_t) {}
  void setTextWrap(bool) {}
  void setRotation(uint8_t) {}
  void setFont(const void * = nullptr) {}
  void drawPixel(int16_t, int16_t, uint16_t) {}
  void drawLine(int16_t, int16_t, int16_t, int16_t, uint16_t) {}
  void drawFastHLine(int16_t, int16_t, int16_t, uint16_t) {}
  void drawFastVLine(int16_t, int16_t, int16_t, uint16_t) {}
  void drawRect(int16_t, int16_t, int16_t, int16_t, uint16_t) {}
  void fillRect(int16_t, int16_t, int16_t, int16_t, uint16_t) {}
  void drawRoundRect(int16_t, int16_t, int16_t, int16_t, int16_t, uint16_t) {}
  void fillRoundRect(int16_t, int16_t, int16_t, int16_t, int16_t, uint16_t) {}
  void drawCircle(int16_t, int16_t, int16_t, uint16_t) {}
  void fillCircle(int16_t, int16_t, int16_t, uint16_t) {}
  void drawTriangle(int16_t, int16_t, int16_t, int16_t, int16_t, int16_t, uint16_t) {}
  void fillTriangle(int16_t, int16_t, int16_t, int16_t, int16_t, int16_t, uint16_t) {}
  void drawBitmap(int16_t, int16_t, const uint8_t *, int16_t, int16_t, uint16_t) {}
  void fillScreen(uint16_t) {}
  int16_t width() const { return w_; }
  int16_t height() const { return h_; }
  size_t write(uint8_t c) override {
    char b[2] = {(char)c, 0};
    if (c == '\n') { y_ += 8 * size_; x_ = 0; return 1; }
    if (c != '\r') rig_display_text("ssd1306", x_, y_, b);
    x_ += 6 * size_;
    return 1;
  }
  using Print::write;

protected:
  int16_t w_, h_, x_ = 0, y_ = 0;
  uint8_t size_ = 1;
};

class Adafruit_SSD1306 : public Adafruit_GFX {
public:
  template <class... A> Adafruit_SSD1306(int16_t w, int16_t h, A...) : Adafruit_GFX(w, h) {}
  bool begin(uint8_t = SSD1306_SWITCHCAPVCC, uint8_t = 0x3C, bool = true, bool = true) { return true; }
  void display() { rig_display_frame("ssd1306"); }
  void clearDisplay() { rig_display_clear("ssd1306"); }
  void invertDisplay(bool) {}
  void dim(bool) {}
  void ssd1306_command(uint8_t) {}
};
