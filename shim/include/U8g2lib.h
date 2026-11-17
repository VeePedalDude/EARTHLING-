// U8g2 stub. Drawing calls are accepted; text is logged per frame to
// <logs>.display.txt so menus / readouts can be checked without hardware.
// Add a display class name to RIG_U8G2_CLASSES or a font to RIG_U8G2_FONTS
// if your sketch uses one that is missing.
#pragma once
#include "Arduino.h"
#include "sim_api.h"

#define U8G2_R0 0
#define U8G2_R1 1
#define U8G2_R2 2
#define U8G2_R3 3
#define U8X8_PIN_NONE 255

class U8G2 : public Print {
public:
  template <class... A> explicit U8G2(A...) {}
  bool begin() { return true; }
  void clearBuffer() { rig_display_clear("u8g2"); }
  void sendBuffer() { rig_display_frame("u8g2"); }
  void clearDisplay() { rig_display_clear("u8g2"); rig_display_frame("u8g2"); }
  void firstPage() { rig_display_clear("u8g2"); pages_ = 0; }
  uint8_t nextPage() { if (pages_++ == 0) { rig_display_frame("u8g2"); } return 0; }
  void setFont(const uint8_t *) {}
  void setFontMode(uint8_t) {}
  void setFontDirection(uint8_t) {}
  void setFontPosTop() {}
  void setFontPosBaseline() {}
  void setFontPosCenter() {}
  void setDrawColor(uint8_t) {}
  void setContrast(uint8_t) {}
  void setPowerSave(uint8_t) {}
  void setFlipMode(uint8_t) {}
  void setBitmapMode(uint8_t) {}
  uint16_t drawStr(int x, int y, const char *s) { rig_display_text("u8g2", x, y, s); return (uint16_t)(6 * std::strlen(s)); }
  uint16_t drawUTF8(int x, int y, const char *s) { return drawStr(x, y, s); }
  uint16_t drawGlyph(int, int, uint16_t) { return 6; }
  uint16_t getStrWidth(const char *s) { return (uint16_t)(6 * std::strlen(s)); }
  uint16_t getUTF8Width(const char *s) { return getStrWidth(s); }
  int8_t getMaxCharHeight() { return 8; }
  int8_t getAscent() { return 7; }
  int8_t getDescent() { return -1; }
  uint16_t getDisplayWidth() { return 128; }
  uint16_t getDisplayHeight() { return 64; }
  void drawPixel(int, int) {}
  void drawLine(int, int, int, int) {}
  void drawHLine(int, int, int) {}
  void drawVLine(int, int, int) {}
  void drawBox(int, int, int, int) {}
  void drawFrame(int, int, int, int) {}
  void drawRBox(int, int, int, int, int) {}
  void drawRFrame(int, int, int, int, int) {}
  void drawCircle(int, int, int, uint8_t = 0) {}
  void drawDisc(int, int, int, uint8_t = 0) {}
  void drawEllipse(int, int, int, int, uint8_t = 0) {}
  void drawTriangle(int, int, int, int, int, int) {}
  void drawXBM(int, int, int, int, const uint8_t *) {}
  void drawXBMP(int, int, int, int, const uint8_t *) {}
  void setCursor(int x, int y) { cx_ = x; cy_ = y; }
  size_t write(uint8_t c) override {
    char b[2] = {(char)c, 0};
    if (c != '\r' && c != '\n') rig_display_text("u8g2", cx_, cy_, b);
    cx_ += 6;
    return 1;
  }
  using Print::write;

private:
  int cx_ = 0, cy_ = 0, pages_ = 0;
};

#define RIG_U8G2_CLASSES(X)                                                                           \
  X(U8G2_SSD1306_128X64_NONAME_F_HW_I2C) X(U8G2_SSD1306_128X64_NONAME_1_HW_I2C)                       \
  X(U8G2_SSD1306_128X64_NONAME_F_SW_I2C) X(U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C)                    \
  X(U8G2_SSD1306_128X32_UNIVISION_1_HW_I2C) X(U8G2_SSD1306_128X64_NONAME_F_4W_HW_SPI)                 \
  X(U8G2_SSD1306_128X64_NONAME_F_4W_SW_SPI) X(U8G2_SH1106_128X64_NONAME_F_HW_I2C)                     \
  X(U8G2_SH1106_128X64_NONAME_1_HW_I2C) X(U8G2_SSD1309_128X64_NONAME2_F_4W_SW_SPI)                    \
  X(U8G2_SSD1309_128X64_NONAME2_1_4W_SW_SPI) X(U8G2_SSD1309_128X64_NONAME0_F_4W_HW_SPI)               \
  X(U8G2_SSD1309_128X64_NONAME2_F_HW_I2C) X(U8G2_SSD1327_MIDAS_128X128_F_HW_I2C)                      \
  X(U8G2_SSD1306_64X48_ER_F_HW_I2C) X(U8G2_ST7920_128X64_F_SW_SPI)
#define RIG_U8G2_DECL(name) class name : public U8G2 { public: using U8G2::U8G2; };
RIG_U8G2_CLASSES(RIG_U8G2_DECL)

namespace rig_fonts { static const uint8_t dummy_font[1] = {0}; }
#define RIG_U8G2_FONTS(X)                                                                          \
  X(u8g2_font_6x10_tf) X(u8g2_font_6x10_mf) X(u8g2_font_6x12_tf) X(u8g2_font_5x7_tf)               \
  X(u8g2_font_5x8_tf) X(u8g2_font_4x6_tf) X(u8g2_font_7x13_tf) X(u8g2_font_8x13_tf)                \
  X(u8g2_font_9x15_tf) X(u8g2_font_10x20_tf) X(u8g2_font_ncenB08_tr) X(u8g2_font_ncenB10_tr)       \
  X(u8g2_font_ncenB12_tr) X(u8g2_font_ncenB14_tr) X(u8g2_font_helvB08_tr) X(u8g2_font_helvB10_tr)  \
  X(u8g2_font_helvB12_tr) X(u8g2_font_helvR08_tr) X(u8g2_font_helvR10_tr) X(u8g2_font_t0_12_mf)    \
  X(u8g2_font_t0_11_tf) X(u8g2_font_inb16_mf) X(u8g2_font_logisoso16_tr) X(u8g2_font_logisoso24_tr) \
  X(u8g2_font_profont12_tf) X(u8g2_font_profont15_tf) X(u8g2_font_unifont_t_symbols)              \
  X(u8g2_font_open_iconic_all_1x_t) X(u8g2_font_crox1h_tr) X(u8g2_font_micro_tr) X(u8g2_font_tom_thumb_4x6_tf)
#define RIG_U8G2_FONT_DECL(name) static const uint8_t *const name = rig_fonts::dummy_font;
RIG_U8G2_FONTS(RIG_U8G2_FONT_DECL)

// ---- U8x8 (text-only) ----
class U8X8 : public Print {
public:
  template <class... A> explicit U8X8(A...) {}
  bool begin() { return true; }
  void setFont(const uint8_t *) {}
  void clear() { rig_display_clear("u8x8"); rig_display_frame("u8x8"); }
  void clearDisplay() { clear(); }
  void clearLine(uint8_t) {}
  void setPowerSave(uint8_t) {}
  void setFlipMode(uint8_t) {}
  void setInverseFont(uint8_t) {}
  void setContrast(uint8_t) {}
  void refreshDisplay() { rig_display_frame("u8x8"); }
  void drawString(uint8_t col, uint8_t row, const char *s) {
    rig_display_text("u8x8", col, row, s);
    rig_display_frame("u8x8");
  }
  void drawUTF8(uint8_t col, uint8_t row, const char *s) { drawString(col, row, s); }
  void draw2x2String(uint8_t col, uint8_t row, const char *s) { drawString(col, row, s); }
  void drawGlyph(uint8_t, uint8_t, uint8_t) {}
  void drawTile(uint8_t, uint8_t, uint8_t, uint8_t *) {}
  void setCursor(uint8_t c, uint8_t r) { c_ = c; r_ = r; }
  uint8_t getCols() { return 16; }
  uint8_t getRows() { return 8; }
  size_t write(uint8_t ch) override {
    char b[2] = {(char)ch, 0};
    if (ch != '\r' && ch != '\n') rig_display_text("u8x8", c_++, r_, b);
    else rig_display_frame("u8x8");
    return 1;
  }
  using Print::write;

private:
  uint8_t c_ = 0, r_ = 0;
};

#define RIG_U8X8_CLASSES(X)                                                                  \
  X(U8X8_SSD1309_128X64_NONAME0_4W_SW_SPI) X(U8X8_SSD1309_128X64_NONAME2_4W_SW_SPI)          \
  X(U8X8_SSD1306_128X64_NONAME_HW_I2C) X(U8X8_SSD1306_128X64_NONAME_SW_I2C)                  \
  X(U8X8_SSD1306_128X32_UNIVISION_HW_I2C) X(U8X8_SH1106_128X64_NONAME_HW_I2C)                \
  X(U8X8_SSD1306_128X64_NONAME_4W_HW_SPI)
#define RIG_U8X8_DECL(name) class name : public U8X8 { public: using U8X8::U8X8; };
RIG_U8X8_CLASSES(RIG_U8X8_DECL)

#define RIG_U8X8_FONTS(X)                                                                   \
  X(u8x8_font_chroma48medium8_r) X(u8x8_font_amstrad_cpc_extended_r) X(u8x8_font_5x7_r)     \
  X(u8x8_font_8x13_1x2_r) X(u8x8_font_px437wyse700b_2x2_r) X(u8x8_font_pressstart2p_r)      \
  X(u8x8_font_victoriamedium8_r) X(u8x8_font_artossans8_r)
RIG_U8X8_FONTS(RIG_U8G2_FONT_DECL)
