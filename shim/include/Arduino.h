// Host-side stand-in for the Arduino core (STM32duino / Daisy Seed variant).
// Only what DaisyDuino sketches commonly touch. Behaviour is driven by the
// simulator (sim_api.h): pin levels come from control scripts, time comes
// from the simulated clock, Serial goes to a log file.
#pragma once
#ifndef DAISYRIG
#define DAISYRIG 1
#endif

#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <string>
#include <algorithm>
#include <type_traits>
#include "binary.h"

typedef bool boolean;
typedef uint8_t byte;
typedef uint16_t word;

#define HIGH 0x1
#define LOW 0x0
#define INPUT 0x0
#define OUTPUT 0x1
#define INPUT_PULLUP 0x2
#define INPUT_PULLDOWN 0x3
#define INPUT_ANALOG 0x4
#define OUTPUT_OPEN_DRAIN 0x5

#define CHANGE 2
#define FALLING 3
#define RISING 4

#ifndef PI
#define PI 3.1415926535897932384626433832795
#endif
#define HALF_PI 1.5707963267948966192313216916398
#define TWO_PI 6.283185307179586476925286766559
#define DEG_TO_RAD 0.017453292519943295769236907684886
#define RAD_TO_DEG 57.295779513082320876798154814105
#define EULER 2.718281828459045235360287471352

#define DEC 10
#define HEX 16
#define OCT 8
#define BIN 2

// ---- Daisy Seed pin numbering (STM32duino DAISY_SEED variant) ----
// Digital pins D0..D32 are 0..32. Analog pins share the digital numbers,
// so analogRead(A1) and analogRead(16) read the same physical pin.
enum : uint8_t {
  D0 = 0, D1, D2, D3, D4, D5, D6, D7, D8, D9, D10, D11, D12, D13, D14, D15,
  D16, D17, D18, D19, D20, D21, D22, D23, D24, D25, D26, D27, D28, D29, D30,
  D31, D32
};
#define A0 15
#define A1 16
#define A2 17
#define A3 18
#define A4 19
#define A5 20
#define A6 21
#define A7 22
#define A8 23
#define A9 24
#define A10 25
#define A11 28
#define LED_BUILTIN 33   // virtual pin for the Seed's user LED (PC7)
#define PIN_SIM_MAX 64

#define digitalPinToInterrupt(p) (p)
#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#define bitSet(value, bit) ((value) |= (1UL << (bit)))
#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))
#define bitWrite(value, bit, bitvalue) ((bitvalue) ? bitSet(value, bit) : bitClear(value, bit))
#define bit(b) (1UL << (b))
#define lowByte(w) ((uint8_t)((w) & 0xff))
#define highByte(w) ((uint8_t)((w) >> 8))
#define sq(x) ((x) * (x))
#define radians(deg) ((deg) * DEG_TO_RAD)
#define degrees(rad) ((rad) * RAD_TO_DEG)
#define noInterrupts() ((void)0)
#define interrupts() ((void)0)
#define F(s) (s)
#define PROGMEM
#define pgm_read_byte(addr) (*(const uint8_t *)(addr))
#define pgm_read_word(addr) (*(const uint16_t *)(addr))
#define pgm_read_float(addr) (*(const float *)(addr))

// Arduino's min/max/constrain accept mixed types; std:: versions do not.
template <class A, class B>
inline auto min(A a, B b) -> typename std::common_type<A, B>::type {
  return (b < a) ? b : a;
}
template <class A, class B>
inline auto max(A a, B b) -> typename std::common_type<A, B>::type {
  return (a < b) ? b : a;
}
template <class T, class L, class H>
inline T constrain(T x, L lo, H hi) {
  return x < (T)lo ? (T)lo : (x > (T)hi ? (T)hi : x);
}
inline long map(long x, long in_min, long in_max, long out_min, long out_max) {
  if (in_max == in_min) return out_min;
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
using std::abs;
using std::round;

// ---- core API (implemented in arduino_sim.cpp) ----
void pinMode(uint32_t pin, uint32_t mode);
void digitalWrite(uint32_t pin, uint32_t val);
void digitalToggle(uint32_t pin);
int digitalRead(uint32_t pin);
int analogRead(uint32_t pin);
void analogWrite(uint32_t pin, int val);
void analogReadResolution(int bits);
void analogWriteResolution(int bits);
uint32_t millis();
uint32_t micros();
void delay(uint32_t ms);
void delayMicroseconds(uint32_t us);
void yield();
void attachInterrupt(uint32_t pin, void (*fn)(void), uint32_t mode);
void detachInterrupt(uint32_t pin);
long random(long howbig);
long random(long howsmall, long howbig);
void randomSeed(unsigned long seed);
void tone(uint8_t pin, unsigned int freq, unsigned long duration = 0);
void noTone(uint8_t pin);
unsigned long pulseIn(uint8_t pin, uint8_t state, unsigned long timeout = 1000000L);
void shiftOut(uint8_t dataPin, uint8_t clockPin, uint8_t bitOrder, uint8_t val);
#define LSBFIRST 0
#define MSBFIRST 1

// ---- minimal String ----
class String {
public:
  String() {}
  String(const char *s) : s_(s ? s : "") {}
  String(const std::string &s) : s_(s) {}
  String(char c) : s_(1, c) {}
  String(int v, int base = DEC) : s_(fmt_int((long)v, base)) {}
  String(unsigned v, int base = DEC) : s_(fmt_uint((unsigned long)v, base)) {}
  String(long v, int base = DEC) : s_(fmt_int(v, base)) {}
  String(unsigned long v, int base = DEC) : s_(fmt_uint(v, base)) {}
  String(float v, int dec = 2) : s_(fmt_f(v, dec)) {}
  String(double v, int dec = 2) : s_(fmt_f(v, dec)) {}
  const char *c_str() const { return s_.c_str(); }
  unsigned length() const { return (unsigned)s_.size(); }
  String &operator+=(const String &o) { s_ += o.s_; return *this; }
  String &operator+=(const char *o) { s_ += o; return *this; }
  String &operator+=(char c) { s_ += c; return *this; }
  friend String operator+(String a, const String &b) { a += b; return a; }
  friend String operator+(String a, const char *b) { a += b; return a; }
  friend String operator+(const char *a, const String &b) { return String(a) + b; }
  bool operator==(const String &o) const { return s_ == o.s_; }
  bool operator==(const char *o) const { return s_ == o; }
  bool operator!=(const String &o) const { return s_ != o.s_; }
  char operator[](unsigned i) const { return i < s_.size() ? s_[i] : 0; }
  char &operator[](unsigned i) { static char dummy; return i < s_.size() ? s_[i] : (dummy = 0); }
  char charAt(unsigned i) const { return (*this)[i]; }
  int indexOf(char c) const { auto p = s_.find(c); return p == std::string::npos ? -1 : (int)p; }
  int indexOf(const String &t) const { auto p = s_.find(t.s_); return p == std::string::npos ? -1 : (int)p; }
  String substring(unsigned a) const { return a < s_.size() ? String(s_.substr(a)) : String(); }
  String substring(unsigned a, unsigned b) const { return a < s_.size() && b > a ? String(s_.substr(a, b - a)) : String(); }
  long toInt() const { return std::strtol(s_.c_str(), nullptr, 10); }
  float toFloat() const { return std::strtof(s_.c_str(), nullptr); }
  void trim() {
    size_t a = s_.find_first_not_of(" \t\r\n");
    size_t b = s_.find_last_not_of(" \t\r\n");
    s_ = (a == std::string::npos) ? std::string() : s_.substr(a, b - a + 1);
  }
  bool startsWith(const String &p) const { return s_.rfind(p.s_, 0) == 0; }
  bool endsWith(const String &p) const {
    return s_.size() >= p.s_.size() && s_.compare(s_.size() - p.s_.size(), p.s_.size(), p.s_) == 0;
  }
  void toUpperCase() { for (auto &c : s_) c = (char)toupper(c); }
  void toLowerCase() { for (auto &c : s_) c = (char)tolower(c); }
  const std::string &str() const { return s_; }

private:
  static std::string fmt_int(long v, int base) {
    if (base == DEC) return std::to_string(v);
    return fmt_uint((unsigned long)v, base);
  }
  static std::string fmt_uint(unsigned long v, int base) {
    if (v == 0) return "0";
    std::string r;
    while (v) { int d = (int)(v % base); r.insert(r.begin(), (char)(d < 10 ? '0' + d : 'A' + d - 10)); v /= base; }
    return r;
  }
  static std::string fmt_f(double v, int dec) {
    char b[64];
    std::snprintf(b, sizeof b, "%.*f", dec, v);
    return b;
  }
  std::string s_;
};

// ---- Print / Serial ----
class Print {
public:
  virtual ~Print() {}
  virtual size_t write(uint8_t c) = 0;
  virtual size_t write(const uint8_t *buf, size_t n) {
    for (size_t i = 0; i < n; i++) write(buf[i]);
    return n;
  }
  size_t write(const char *s) { return write((const uint8_t *)s, std::strlen(s)); }
  size_t print(const char *s) { return write(s); }
  size_t print(const String &s) { return write(s.c_str()); }
  size_t print(char c) { return write((uint8_t)c); }
  size_t print(int v, int base = DEC) { return print(String(v, base)); }
  size_t print(unsigned v, int base = DEC) { return print(String(v, base)); }
  size_t print(long v, int base = DEC) { return print(String(v, base)); }
  size_t print(unsigned long v, int base = DEC) { return print(String(v, base)); }
  size_t print(double v, int dec = 2) { return print(String(v, dec)); }
  size_t println() { return write("\r\n"); }
  template <class T> size_t println(const T &v) { size_t n = print(v); return n + println(); }
  template <class T> size_t println(const T &v, int fmt) { size_t n = print(v, fmt); return n + println(); }
  int printf(const char *fmt, ...) __attribute__((format(printf, 2, 3)));
};

class HardwareSerial : public Print {
public:
  void begin(unsigned long) {}
  void begin(unsigned long, int) {}
  void end() {}
  int available() { return 0; }
  int read() { return -1; }
  int peek() { return -1; }
  void flush() {}
  explicit operator bool() const { return true; }
  size_t write(uint8_t c) override;
  using Print::write;
};
extern HardwareSerial Serial;
extern HardwareSerial Serial1;
typedef HardwareSerial USBSerial;

// Sketch entry points
void setup();
void loop();
