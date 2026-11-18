// daisyrig simulator runtime: Arduino core, DaisyDuino AudioClass/DaisyHardware,
// scheduler, and the host main() that drives a sketch from WAV input.
//
// Execution model (mirrors the Daisy Seed):
//   setup() runs once. DAISY.begin(cb) arms audio. Then loop() is called
//   forever. Audio blocks are "interrupts": they fire whenever simulated time
//   crosses the next block deadline - during delay(), during millis()/micros()
//   polling, and between loop() iterations (each loop() iteration costs
//   block_period / loops_per_block of simulated time).
#include "Arduino.h"
#include "DaisyDuino.h"
#include "SPI.h"
#include "Wire.h"
#include "sim_api.h"
#include "wav.h"

#include <chrono>
#include <cfloat>
#include <cstdarg>
#include <map>
#include <new>
#include <random>
#include <set>
#include <stdexcept>
#include <vector>
#if defined(__x86_64__) || defined(__i386__)
#include <xmmintrin.h>
#include <pmmintrin.h>
#endif

HardwareSerial Serial, Serial1;
TwoWire Wire;
SPIClass SPI;
AudioClass DAISY;

namespace {
struct EndOfInput {};

// ---------------- simulator state ----------------
struct Sim {
  rigsim::PinState pins[PIN_SIM_MAX];
  double clock = 0.0;
  int adc_bits = 10, pwm_bits = 8;
  double adc_noise_lsb = 0.0;
  std::mt19937 rng{12345};

  // audio
  float sr = 48000.f;
  size_t block = 48;
  int forced_block = 0;
  AudioHandle::AudioCallback cb = nullptr;
  AudioHandle::InterleavingAudioCallback icb = nullptr;
  bool audio_on = false;
  double next_block_t = 0.0;
  int nch = 2;
  bool mono = false;   // Terrarium-style: only in[0] is wired, only out[0] reaches the jack
  WavData in, out;
  size_t frame = 0, total_frames = 0;
  bool have_input = false;
  bool in_callback = false;

  // stats
  std::vector<double> block_ns;
  long nan_count = 0, inf_count = 0, unwritten = 0, overs = 0;
  double first_nan_t = -1, first_unwritten_t = -1;
  long loop_calls = 0, allocs_in_cb = 0, serial_bytes = 0, serial_in_cb = 0;
  long analogread_in_cb = 0, delay_in_cb = 0;
  long blocks = 0;
  double loop_cost = 0.0;
  int loops_per_block = 4;

  // control events
  struct Ev { double t; int pin; float level; };
  std::vector<Ev> events;
  size_t next_ev = 0;

  // MIDI input queue (from the events file) and display frames
  struct Midi { double t; uint8_t st, d1, d2; };
  std::vector<Midi> midi_in;
  size_t next_midi = 0;
  std::map<std::string, std::vector<std::string>> disp_items;
  std::map<std::string, std::string> disp_last;
  FILE *disp_f = nullptr;
  long midi_out_count = 0, display_frames = 0;

  // interrupts
  struct Isr { void (*fn)(); uint32_t mode; };
  std::map<int, Isr> isrs;

  // logs
  FILE *serial_f = nullptr, *pins_f = nullptr, *probe_f = nullptr;
  std::map<std::string, std::string> warnings;
} S;

const float kSentinel = 1.2345e30f;

void apply_events_until(double t) {
  while (S.next_ev < S.events.size() && S.events[S.next_ev].t <= t) {
    auto &e = S.events[S.next_ev++];
    rigsim::set_pin_level(e.pin, e.level);
  }
}

void run_block() {
  size_t bs = S.block;
  int nch = S.nch;
  static std::vector<std::vector<float>> ib, ob;
  static std::vector<float *> ip, op;
  static std::vector<float> ii, oi;
  ib.resize(nch); ob.resize(nch); ip.resize(nch); op.resize(nch);
  for (int c = 0; c < nch; c++) {
    ib[c].assign(bs, 0.f);
    ob[c].assign(bs, kSentinel);
    for (size_t i = 0; i < bs; i++) {
      size_t fi = S.frame + i;
      if (S.have_input && fi < S.total_frames) ib[c][i] = S.in.ch[c][fi];
    }
    ip[c] = ib[c].data();
    op[c] = ob[c].data();
  }
  S.in_callback = true;
  auto t0 = std::chrono::steady_clock::now();
  if (S.cb) {
    S.cb(ip.data(), op.data(), bs);
  } else if (S.icb) {
    ii.assign(bs * 2, 0.f); oi.assign(bs * 2, kSentinel);
    for (size_t i = 0; i < bs; i++) { ii[2 * i] = ib[0][i]; ii[2 * i + 1] = ib[1 % nch][i]; }
    S.icb(ii.data(), oi.data(), bs * 2);
    for (size_t i = 0; i < bs; i++) { ob[0][i] = oi[2 * i]; if (nch > 1) ob[1][i] = oi[2 * i + 1]; }
  }
  auto t1 = std::chrono::steady_clock::now();
  S.in_callback = false;
  S.block_ns.push_back((double)std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
  S.blocks++;
  double tblock = (double)S.frame / S.sr;
  for (int c = 0; c < nch; c++)
    for (size_t i = 0; i < bs; i++) {
      size_t fi = S.frame + i;
      float v = ob[c][i];
      if (v == kSentinel && S.mono && c > 0) {
        v = 0.f;  // unconnected channel: not writing it is fine
      } else if (v == kSentinel) {
        S.unwritten++;
        if (S.first_unwritten_t < 0) S.first_unwritten_t = tblock;
        v = 0.f;
      } else if (std::isnan(v)) {
        S.nan_count++;
        if (S.first_nan_t < 0) S.first_nan_t = tblock + i / S.sr;
      } else if (std::isinf(v)) {
        S.inf_count++;
        if (S.first_nan_t < 0) S.first_nan_t = tblock + i / S.sr;
      } else if (std::fabs(v) > 1.0f) {
        S.overs++;
      }
      if (fi < S.out.frames()) S.out.ch[c][fi] = v;
    }
  S.frame += bs;
}

void run_due_blocks(double target) {
  if (S.in_callback) return;  // no nested "interrupts"
  while (S.audio_on && S.next_block_t <= target) {
    if (S.have_input && S.frame >= S.total_frames) throw EndOfInput{};
    S.clock = S.next_block_t;
    apply_events_until(S.clock);
    run_block();
    S.next_block_t += (double)S.block / S.sr;
  }
}

void warn(const std::string &key, const std::string &msg) {
  bool cb = S.in_callback;
  S.in_callback = false;  // the rig's own bookkeeping must not count as sketch allocations
  if (!S.warnings.count(key)) S.warnings[key] = msg;
  S.in_callback = cb;
}

std::string jesc(const std::string &s) {
  std::string r;
  for (char c : s) {
    if (c == '"' || c == '\\') { r += '\\'; r += c; }
    else if (c == '\n') r += "\\n";
    else if ((unsigned char)c < 0x20) r += ' ';
    else r += c;
  }
  return r;
}
}  // namespace

// ---------------- allocation tracking ----------------
// (disabled under ASAN, which owns the allocator)
#if defined(__SANITIZE_ADDRESS__)
#define RIG_NO_NEW_HOOK
#endif
#ifndef RIG_NO_NEW_HOOK
void *operator new(size_t n) {
  if (S.in_callback) S.allocs_in_cb++;
  void *p = std::malloc(n ? n : 1);
  if (!p) throw std::bad_alloc();
  return p;
}
void *operator new[](size_t n) { return operator new(n); }
void operator delete(void *p) noexcept { std::free(p); }
void operator delete[](void *p) noexcept { std::free(p); }
void operator delete(void *p, size_t) noexcept { std::free(p); }
void operator delete[](void *p, size_t) noexcept { std::free(p); }
#endif

// ---------------- rigsim API ----------------
namespace rigsim {
PinState &pin(int p) {
  static PinState dummy;
  if (p < 0 || p >= PIN_SIM_MAX) return dummy;
  return S.pins[p];
}
double now_seconds() { return S.clock; }
void advance(double dt) {
  double target = S.clock + dt;
  run_due_blocks(target);
  S.clock = std::max(S.clock, target);
  apply_events_until(S.clock);
}
static bool digital_level(int p) {
  auto &ps = pin(p);
  if (ps.driven) return ps.level >= 0.5f;
  if (ps.mode == INPUT_PULLUP) return true;
  if (ps.mode == OUTPUT) return ps.out > 0;
  return false;
}
void set_pin_level(int p, float level) {
  if (p < 0 || p >= PIN_SIM_MAX) return;
  bool before = digital_level(p);
  S.pins[p].driven = true;
  S.pins[p].level = level;
  bool after = digital_level(p);
  auto it = S.isrs.find(p);
  if (it != S.isrs.end() && before != after) {
    uint32_t m = it->second.mode;
    if (m == CHANGE || (m == RISING && after) || (m == FALLING && !after)) it->second.fn();
  }
}
void warn_once(const char *key, const char *msg) { warn(key, msg); }
}  // namespace rigsim

void rig_probe(const char *name, double value) {
  if (S.probe_f) std::fprintf(S.probe_f, "%.6f,%s,%.9g\n", S.clock, name, value);
}
void rig_event(const char *name, int index, double value) {
  if (S.pins_f) std::fprintf(S.pins_f, "%.6f,%s[%d],event,%.6g\n", S.clock, name, index, value);
}

void rig_display_clear(const char *dev) { S.disp_items[dev].clear(); }
void rig_display_text(const char *dev, int x, int y, const char *text) {
  char b[64];
  std::snprintf(b, sizeof b, "%04d,%04d ", y, x);
  S.disp_items[dev].push_back(std::string(b) + text);
}
void rig_display_frame(const char *dev) {
  auto items = S.disp_items[dev];
  std::sort(items.begin(), items.end());
  std::string frame;
  for (auto &it : items) frame += "  (" + it.substr(5, 4) + "," + it.substr(0, 4) + ") " + it.substr(10) + "\n";
  if (frame == S.disp_last[dev]) return;
  S.disp_last[dev] = frame;
  S.display_frames++;
  if (S.disp_f) std::fprintf(S.disp_f, "t=%.4f %s\n%s", S.clock, dev, frame.c_str());
}
bool rig_midi_pop(uint8_t &st, uint8_t &d1, uint8_t &d2) {
  if (S.next_midi >= S.midi_in.size() || S.midi_in[S.next_midi].t > S.clock) return false;
  auto &m = S.midi_in[S.next_midi++];
  st = m.st; d1 = m.d1; d2 = m.d2;
  return true;
}
void rig_midi_out(uint8_t st, uint8_t d1, uint8_t d2) {
  S.midi_out_count++;
  if (S.pins_f) std::fprintf(S.pins_f, "%.6f,midi_out,%02X,%d %d\n", S.clock, st, d1, d2);
}

// ---------------- Arduino core ----------------
void pinMode(uint32_t p, uint32_t mode) { rigsim::pin((int)p).mode = (int)mode; }
static void log_pin_out(uint32_t p, const char *kind, int v) {
  auto &ps = rigsim::pin((int)p);
  if (ps.out == v) return;
  ps.out = v;
  if (S.pins_f) std::fprintf(S.pins_f, "%.6f,%u,%s,%d\n", S.clock, p, kind, v);
}
void digitalWrite(uint32_t p, uint32_t v) { log_pin_out(p, "digital", v ? 1 : 0); }
void digitalToggle(uint32_t p) { digitalWrite(p, rigsim::pin((int)p).out > 0 ? 0 : 1); }
int digitalRead(uint32_t p) { return rigsim::digital_level((int)p) ? HIGH : LOW; }
int analogRead(uint32_t p) {
  if (S.in_callback) S.analogread_in_cb++;
  auto &ps = rigsim::pin((int)p);
  double full = (double)((1 << S.adc_bits) - 1);
  double v = (ps.driven ? ps.level : 0.5) * full;
  if (S.adc_noise_lsb > 0) {
    std::normal_distribution<double> nd(0.0, S.adc_noise_lsb);
    v += nd(S.rng);
  }
  long r = std::lround(v);
  return (int)std::max(0L, std::min((long)full, r));
}
void analogWrite(uint32_t p, int v) { log_pin_out(p, "pwm", v); }
void analogReadResolution(int bits) { S.adc_bits = std::max(1, std::min(16, bits)); }
void analogWriteResolution(int bits) { S.pwm_bits = bits; }
uint32_t millis() {
  if (!S.in_callback) rigsim::advance(1e-6);  // polling costs time; prevents hangs
  return (uint32_t)(S.clock * 1000.0);
}
uint32_t micros() {
  if (!S.in_callback) rigsim::advance(1e-6);
  return (uint32_t)(S.clock * 1e6);
}
void delay(uint32_t ms) {
  if (S.in_callback) {
    S.delay_in_cb++;
    warn("delay_cb", "delay() called inside the audio callback (blocks audio on hardware)");
    return;
  }
  rigsim::advance(ms / 1000.0);
}
void delayMicroseconds(uint32_t us) {
  if (S.in_callback) { S.delay_in_cb++; return; }
  rigsim::advance(us / 1e6);
}
void yield() { rigsim::advance(1e-6); }
void attachInterrupt(uint32_t p, void (*fn)(void), uint32_t mode) { S.isrs[(int)p] = {fn, mode}; }
void detachInterrupt(uint32_t p) { S.isrs.erase((int)p); }
static std::mt19937 g_user_rng(1);
long random(long howbig) { return howbig <= 0 ? 0 : (long)(g_user_rng() % (unsigned long)howbig); }
long random(long lo, long hi) { return hi <= lo ? lo : lo + random(hi - lo); }
void randomSeed(unsigned long s) { g_user_rng.seed((unsigned)s); }
void tone(uint8_t p, unsigned int f, unsigned long) { log_pin_out(p, "tone", (int)f); }
void noTone(uint8_t p) { log_pin_out(p, "tone", 0); }
unsigned long pulseIn(uint8_t, uint8_t, unsigned long) { return 0; }
void shiftOut(uint8_t, uint8_t, uint8_t, uint8_t) {}

size_t HardwareSerial::write(uint8_t c) {
  if (S.in_callback) {
    S.serial_in_cb++;
    warn("serial_cb", "Serial output inside the audio callback (slow/blocking on hardware)");
  }
  S.serial_bytes++;
  if (S.serial_f) std::fputc(c, S.serial_f);
  return 1;
}
int Print::printf(const char *fmt, ...) {
  char buf[512];
  va_list ap;
  va_start(ap, fmt);
  int n = std::vsnprintf(buf, sizeof buf, fmt, ap);
  va_end(ap);
  if (n > 0) write((const uint8_t *)buf, std::min((size_t)n, sizeof buf - 1));
  return n;
}

// ---------------- daisy::System / CpuLoadMeter ----------------
uint32_t daisy::System::GetTick() {
  using namespace std::chrono;
  return (uint32_t)(duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count() / 5);
}
void daisy::CpuLoadMeter::OnBlockStart() {
  using namespace std::chrono;
  t0_ = duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count() * 1e-9;
}
void daisy::CpuLoadMeter::OnBlockEnd() {
  using namespace std::chrono;
  double t = duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count() * 1e-9;
  float load = (float)((t - t0_) / budget_s_);
  if (first_) { avg_ = load; first_ = false; }
  else avg_ += 0.01f * (load - avg_);
  min_ = std::min(min_, load);
  max_ = std::max(max_, load);
}

// ---------------- DaisyHardware ----------------
void DaisyHardware::Init(float rate, DaisyDuinoDevice device) {
  device_ = device;
  num_channels = 2;
  numControls = numGates = numSwitches = numLeds = 0;
  switch (device) {
    case DAISY_SEED: break;
    case DAISY_POD:
      buttons[0].Init(rate, true, PIN_POD_SWITCH_1, INPUT_PULLUP);
      buttons[1].Init(rate, true, PIN_POD_SWITCH_2, INPUT_PULLUP);
      encoder.Init(rate, PIN_POD_ENC_A, PIN_POD_ENC_B, PIN_POD_ENC_CLICK, INPUT_PULLUP, INPUT_PULLUP, INPUT_PULLUP);
      leds[0].Init(PIN_POD_LED_1_RED, PIN_POD_LED_1_GREEN, PIN_POD_LED_1_BLUE, true);
      leds[1].Init(PIN_POD_LED_2_RED, PIN_POD_LED_2_GREEN, PIN_POD_LED_2_BLUE, true);
      controls[0].Init(PIN_POD_POT_1, rate);
      controls[1].Init(PIN_POD_POT_2, rate);
      numSwitches = numLeds = numControls = 2;
      break;
    case DAISY_PETAL: {
      numSwitches = 7; numLeds = 12; numControls = 6;
      encoder.Init(rate, PIN_PETAL_ENC_A, PIN_PETAL_ENC_B, PIN_PETAL_ENC_CLICK, INPUT_PULLUP, INPUT_PULLUP, INPUT_PULLUP);
      const uint8_t pots[6] = {PIN_PETAL_POT_1, PIN_PETAL_POT_2, PIN_PETAL_POT_3, PIN_PETAL_POT_4, PIN_PETAL_POT_5, PIN_PETAL_POT_6};
      for (int i = 0; i < 6; i++) controls[i].Init(pots[i], rate);
      expression.Init(PIN_PETAL_EXPRESSION, rate);
      const uint8_t sw[7] = {PIN_PETAL_SWITCH_1, PIN_PETAL_SWITCH_2, PIN_PETAL_SWITCH_3, PIN_PETAL_SWITCH_4,
                             PIN_PETAL_SWITCH_5, PIN_PETAL_SWITCH_6, PIN_PETAL_SWITCH_7};
      for (int i = 0; i < 7; i++) buttons[i].Init(rate, true, sw[i], INPUT_PULLUP);
      break;
    }
    case DAISY_PATCH:
      encoder.Init(rate, PIN_PATCH_ENC_A, PIN_PATCH_ENC_B, PIN_PATCH_ENC_CLICK, INPUT_PULLUP, INPUT_PULLUP, INPUT_PULLUP);
      gateIns[0].Init(20, INPUT, true);
      gateIns[1].Init(19, INPUT, true);
      numGates = 2; num_channels = 4; numControls = 4;
      controls[0].Init(PIN_PATCH_CTRL_1, rate, true);
      controls[1].Init(PIN_PATCH_CTRL_2, rate, true);
      controls[2].Init(PIN_PATCH_CTRL_3, rate, true);
      controls[3].Init(PIN_PATCH_CTRL_4, rate, true);
      break;
    default:
      warn("device", "Field / Patch SM controls are not simulated; audio runs as a Seed");
      break;
  }
}
void DaisyHardware::ProcessAnalogControls() {
  for (int i = 0; i < numControls; i++) controls[i].Process();
  if (device_ == DAISY_PETAL) expression.Process();
}
void DaisyHardware::ProcessDigitalControls() {
  if (device_ == DAISY_PATCH || device_ == DAISY_POD || device_ == DAISY_PETAL) encoder.Debounce();
  for (int i = 0; i < numSwitches; i++) buttons[i].Debounce();
  for (int i = 0; i < numGates; i++) gateIns[i].Debounce();
}
void DaisyHardware::SetRingLed(uint8_t idx, float r, float g, float b) {
  rig_event("ring_r", idx, r); rig_event("ring_g", idx, g); rig_event("ring_b", idx, b);
}
void DaisyHardware::SetFootswitchLed(uint8_t idx, float bright) { rig_event("fs_led", idx, bright); }
void DaisyHardware::SetKeyboardLed(uint8_t row, uint8_t idx, float v) { rig_event("kb_led", row * 8 + idx, v); }
void DaisyHardware::SetKnobLed(uint8_t idx, float v) { rig_event("knob_led", idx, v); }
void DaisyHardware::ClearLeds() {}
float DaisyHardware::GetKnobValue(uint8_t idx) { return idx < 12 ? controls[idx].Process() : 0.f; }

// ---------------- AudioClass ----------------
DaisyHardware AudioClass::init(DaisyDuinoDevice device, DaisyDuinoSampleRate sr) {
  const float rates[] = {8000.f, 16000.f, 32000.f, 48000.f, 96000.f};
  S.sr = (sr < AUDIO_SR_LAST) ? rates[sr] : 48000.f;
  if (!S.forced_block) S.block = 48;
  DaisyHardware hw;
  hw.Init(S.sr / (float)S.block, device);  // DaisyDuino uses the callback rate
  S.nch = hw.num_channels;
  return hw;
}
static void arm_audio() {
  S.audio_on = true;
  S.next_block_t = S.clock;
}
void AudioClass::StartAudio(AudioHandle::AudioCallback cb) { S.cb = cb; S.icb = nullptr; arm_audio(); }
void AudioClass::StartAudio(AudioHandle::InterleavingAudioCallback cb) { S.icb = cb; S.cb = nullptr; arm_audio(); }
void AudioClass::StopAudio() { S.audio_on = false; }
void AudioClass::SetAudioSampleRate(SaiHandle::Config::SampleRate r) {
  const float rates[] = {8000.f, 16000.f, 32000.f, 48000.f, 96000.f};
  S.sr = rates[(int)r];
}
float AudioClass::AudioSampleRate() { return S.sr; }
void AudioClass::SetAudioBlockSize(size_t bs) { if (!S.forced_block) S.block = bs; }
size_t AudioClass::AudioBlockSize() { return S.block; }

// ---------------- SDRAM boot contents ----------------
extern "C" {
extern char __start_sdram_bss[] __attribute__((weak));
extern char __stop_sdram_bss[] __attribute__((weak));
}
static size_t fill_sdram(const std::string &mode) {
  if (!__start_sdram_bss || !__stop_sdram_bss) return 0;
  size_t n = (size_t)(__stop_sdram_bss - __start_sdram_bss);
  if (mode == "zero") { std::memset(__start_sdram_bss, 0, n); return n; }
  std::mt19937 g(0xDA15);
  for (size_t i = 0; i < n; i++) __start_sdram_bss[i] = (char)(g() & 0xff);
  return n;
}

// ---------------- host main ----------------
static void usage() {
  std::fprintf(stderr,
               "usage: <sketch-bin> [--in in.wav] [--out out.wav] [--events ev.txt] [--seconds S]\n"
               "       [--block N] [--loops-per-block N] [--adc-noise LSB] [--ftz] [--report r.json]\n"
               "       [--logs prefix] [--sdram-fill random|zero] [--mono] [--config-only]\n"
               "--mono: only in[0]/out[0] are wired (PedalPCB Terrarium); in[1] is silent, out[1] ignored.\n"
               "SDRAM (DSY_SDRAM_BSS) starts as random bytes, like hardware, unless --sdram-fill zero.\n");
}

static void write_report(FILE *f, double wall_s) {
  std::vector<double> ns = S.block_ns;
  std::sort(ns.begin(), ns.end());
  double mean = 0;
  for (double v : ns) mean += v;
  if (!ns.empty()) mean /= ns.size();
  auto pct = [&](double p) { return ns.empty() ? 0.0 : ns[std::min(ns.size() - 1, (size_t)(p * ns.size()))]; };
  double budget_ns = 1e9 * S.block / S.sr;
  std::fprintf(f, "{\n");
  std::fprintf(f, "  \"samplerate\": %.0f,\n  \"blocksize\": %zu,\n  \"channels\": %d,\n  \"mono\": %s,\n", S.sr,
               S.block, S.nch, S.mono ? "true" : "false");
  std::fprintf(f, "  \"audio_started\": %s,\n  \"callback_type\": \"%s\",\n", (S.cb || S.icb) ? "true" : "false",
               S.cb ? "non-interleaved" : (S.icb ? "interleaved" : "none"));
  std::fprintf(f, "  \"blocks\": %ld,\n  \"sim_seconds\": %.4f,\n  \"wall_seconds\": %.3f,\n", S.blocks, S.clock, wall_s);
  std::fprintf(f, "  \"loop_calls\": %ld,\n", S.loop_calls);
  std::fprintf(f, "  \"host_block_ns\": {\"mean\": %.0f, \"p50\": %.0f, \"p99\": %.0f, \"max\": %.0f, \"budget\": %.0f},\n",
               mean, pct(0.5), pct(0.99), ns.empty() ? 0.0 : ns.back(), budget_ns);
  std::fprintf(f, "  \"host_load_mean_pct\": %.3f,\n  \"host_load_p99_pct\": %.3f,\n", 100 * mean / budget_ns,
               100 * pct(0.99) / budget_ns);
  std::fprintf(f, "  \"nan_samples\": %ld,\n  \"inf_samples\": %ld,\n  \"first_nonfinite_s\": %.6f,\n", S.nan_count,
               S.inf_count, S.first_nan_t);
  std::fprintf(f, "  \"over_full_scale_samples\": %ld,\n", S.overs);
  std::fprintf(f, "  \"unwritten_output_samples\": %ld,\n  \"first_unwritten_s\": %.6f,\n", S.unwritten,
               S.first_unwritten_t);
  std::fprintf(f, "  \"allocations_in_callback\": %ld,\n  \"analogread_in_callback\": %ld,\n", S.allocs_in_cb,
               S.analogread_in_cb);
  std::fprintf(f, "  \"delay_in_callback\": %ld,\n  \"serial_bytes_in_callback\": %ld,\n  \"serial_bytes\": %ld,\n",
               S.delay_in_cb, S.serial_in_cb, S.serial_bytes);
  std::fprintf(f, "  \"display_frames\": %ld,\n  \"midi_in_events\": %zu,\n  \"midi_out_messages\": %ld,\n",
               S.display_frames, S.midi_in.size(), S.midi_out_count);
  std::fprintf(f, "  \"warnings\": [");
  bool first = true;
  for (auto &w : S.warnings) {
    std::fprintf(f, "%s\"%s\"", first ? "" : ", ", jesc(w.second).c_str());
    first = false;
  }
  std::fprintf(f, "]\n}\n");
}

int main(int argc, char **argv) {
  std::string in_path, out_path, ev_path, report_path, logs, sdram_mode = "random";
  double seconds = -1;
  bool config_only = false, ftz = false;
  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    auto next = [&]() -> std::string {
      if (i + 1 >= argc) { usage(); std::exit(2); }
      return argv[++i];
    };
    if (a == "--in") in_path = next();
    else if (a == "--out") out_path = next();
    else if (a == "--events") ev_path = next();
    else if (a == "--seconds") seconds = std::stod(next());
    else if (a == "--block") S.forced_block = std::stoi(next());
    else if (a == "--loops-per-block") S.loops_per_block = std::max(1, std::stoi(next()));
    else if (a == "--adc-noise") S.adc_noise_lsb = std::stod(next());
    else if (a == "--report") report_path = next();
    else if (a == "--logs") logs = next();
    else if (a == "--ftz") ftz = true;
    else if (a == "--mono") S.mono = true;
    else if (a == "--sdram-fill") sdram_mode = next();
    else if (a == "--config-only") config_only = true;
    else { usage(); return 2; }
  }
#if defined(__x86_64__) || defined(__i386__)
  if (ftz) { _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON); _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON); }
#endif
  if (S.forced_block) S.block = (size_t)S.forced_block;
  if (!logs.empty()) {
    S.serial_f = std::fopen((logs + ".serial.txt").c_str(), "w");
    S.pins_f = std::fopen((logs + ".pins.csv").c_str(), "w");
    S.probe_f = std::fopen((logs + ".probes.csv").c_str(), "w");
    S.disp_f = std::fopen((logs + ".display.txt").c_str(), "w");
    if (S.pins_f) std::fprintf(S.pins_f, "t,pin,kind,value\n");
    if (S.probe_f) std::fprintf(S.probe_f, "t,name,value\n");
  }
  if (!ev_path.empty()) {
    FILE *f = std::fopen(ev_path.c_str(), "r");
    if (!f) { std::fprintf(stderr, "cannot open events %s\n", ev_path.c_str()); return 2; }
    char line[256];
    while (std::fgets(line, sizeof line, f)) {
      double t; int p; float lv; unsigned st, d1, d2;
      if (std::sscanf(line, "%lf M %u %u %u", &t, &st, &d1, &d2) == 4)
        S.midi_in.push_back({t, (uint8_t)st, (uint8_t)d1, (uint8_t)d2});
      else if (std::sscanf(line, "%lf %d %f", &t, &p, &lv) == 3)
        S.events.push_back({t, p, lv});
    }
    std::fclose(f);
    std::stable_sort(S.midi_in.begin(), S.midi_in.end(), [](const Sim::Midi &a, const Sim::Midi &b) { return a.t < b.t; });
    std::stable_sort(S.events.begin(), S.events.end(), [](const Sim::Ev &a, const Sim::Ev &b) { return a.t < b.t; });
  }
  apply_events_until(0.0);
  size_t sdram_bytes = fill_sdram(sdram_mode);
  (void)sdram_bytes;

  auto w0 = std::chrono::steady_clock::now();
  int rc = 0;
  try {
    setup();
    if (config_only) {
      std::printf("{\"samplerate\": %.0f, \"blocksize\": %zu, \"channels\": %d, \"audio_started\": %s}\n", S.sr,
                  S.block, S.nch, (S.cb || S.icb) ? "true" : "false");
      return 0;
    }
    if (!in_path.empty()) {
      WavData w;
      std::string err;
      if (!wav_read(in_path, w, err)) { std::fprintf(stderr, "input: %s\n", err.c_str()); return 2; }
      if (std::fabs(w.sr - S.sr) > 0.5f) {
        std::fprintf(stderr, "input WAV is %d Hz but the sketch runs at %.0f Hz; regenerate signals with --sr %.0f\n",
                     w.sr, S.sr, S.sr);
        return 3;
      }
      S.in.sr = w.sr;
      S.in.ch.assign(S.nch, {});
      for (int c = 0; c < S.nch; c++)
        S.in.ch[c] = (S.mono && c > 0) ? std::vector<float>(w.frames(), 0.f) : w.ch[std::min(c, w.channels - 1)];
      S.have_input = true;
      S.total_frames = w.frames();
    } else {
      double secs = seconds > 0 ? seconds : 2.0;
      S.total_frames = (size_t)(secs * S.sr);
      S.in.ch.assign(S.nch, std::vector<float>(S.total_frames, 0.f));
      S.have_input = true;
    }
    if (seconds > 0) S.total_frames = std::min(S.total_frames, (size_t)(seconds * S.sr));
    S.out.sr = (int)S.sr;
    S.out.ch.assign(S.nch, std::vector<float>(S.total_frames, 0.f));
    S.loop_cost = (double)S.block / S.sr / S.loops_per_block;
    double end_t = (double)S.total_frames / S.sr;
    if (!S.audio_on) S.next_block_t = 1e300;
    // Audio started during setup(): the stream starts at t = begin() time.
    while (true) {
      if (!S.audio_on && S.clock >= end_t) break;
      loop();
      S.loop_calls++;
      rigsim::advance(S.loop_cost);
    }
  } catch (EndOfInput &) {
  } catch (std::exception &e) {
    std::fprintf(stderr, "sketch threw: %s\n", e.what());
    rc = 4;
  }
  double wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - w0).count();
  if (!out_path.empty() && !wav_write_f32(out_path, S.out)) { std::fprintf(stderr, "cannot write %s\n", out_path.c_str()); rc = 2; }
  FILE *rf = report_path.empty() ? stdout : std::fopen(report_path.c_str(), "w");
  if (rf) { write_report(rf, wall); if (rf != stdout) std::fclose(rf); }
  for (FILE *f : {S.serial_f, S.pins_f, S.probe_f, S.disp_f}) if (f) std::fclose(f);
  return rc;
}
