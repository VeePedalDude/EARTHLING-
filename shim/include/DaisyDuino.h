// Host-side replacement for DaisyDuino.h.
// - DaisySP is the real library (compiled from third_party/DaisyDuino).
// - Switch / AnalogControl / Encoder / Led / GateIn / Parameter are the real
//   DaisyDuino classes; they read pins through the simulated Arduino core.
// - AudioClass / DaisyHardware are re-implemented here so the harness can
//   drive the audio callback from WAV input.
#pragma once
#ifndef DSY_DSYDUINO_H
#define DSY_DSYDUINO_H

#include "Arduino.h"
#include <Wire.h>
#include <stdio.h>
#include "sim_api.h"

// Memory-section macros. SDRAM uses a NOBITS section so huge buffers do not
// bloat the host binary, memcheck can measure it separately, and the runtime
// can fill it with garbage at boot like real SDRAM (see --sdram-fill).
#define DSY_SDRAM_BSS __attribute__((section("sdram_bss,\"aw\",@nobits#")))
#define DSY_SDRAM_DATA __attribute__((section("sdram_data")))
#define DMA_BUFFER_MEM_SECTION __attribute__((section(".sram1_bss,\"aw\",@nobits#")))
#define DSY_QSPI_BSS __attribute__((section(".qspiflash_bss,\"aw\",@nobits#")))
#define DTCM_MEM_SECTION __attribute__((section(".dtcmram_bss,\"aw\",@nobits#")))

#include "utility/DaisySP/daisysp.h"
using namespace daisysp;

#include "utility/ctrl.h"
#include "utility/encoder.h"
#include "utility/gatein.h"
#include "utility/led.h"
#include "utility/parameter.h"
#include "utility/switch.h"

#include "daisy_pod.h"
#include "daisy_patch.h"

#define OUT_L out[0]
#define OUT_R out[1]
#define IN_L in[0]
#define IN_R in[1]

// Petal pins (copied from daisy_petal.h, which drags in the PCA9685 driver)
#define PIN_PETAL_SWITCH_1 8
#define PIN_PETAL_SWITCH_2 9
#define PIN_PETAL_SWITCH_3 10
#define PIN_PETAL_SWITCH_4 13
#define PIN_PETAL_SWITCH_5 25
#define PIN_PETAL_SWITCH_6 26
#define PIN_PETAL_SWITCH_7 7
#define PIN_PETAL_ENC_A 28
#define PIN_PETAL_ENC_B 27
#define PIN_PETAL_ENC_CLICK 14
#define PIN_PETAL_EXPRESSION A0
#define PIN_PETAL_POT_1 A1
#define PIN_PETAL_POT_2 A4
#define PIN_PETAL_POT_3 A2
#define PIN_PETAL_POT_4 A5
#define PIN_PETAL_POT_5 A3
#define PIN_PETAL_POT_6 A6

namespace daisy {

class AudioHandle {
public:
  typedef void (*AudioCallback)(float **in, float **out, size_t size);
  typedef void (*InterleavingAudioCallback)(float *in, float *out, size_t size);
};

class SaiHandle {
public:
  struct Config {
    enum class SampleRate { SAI_8KHZ, SAI_16KHZ, SAI_32KHZ, SAI_48KHZ, SAI_96KHZ };
  };
};

class System {
public:
  static uint32_t GetNow() { return millis(); }
  static uint32_t GetUs() { return micros(); }
  static uint32_t GetTick();       // host steady clock, 200 MHz ticks
  static uint32_t GetTickFreq() { return 200000000u; }
  static void Delay(uint32_t ms) { delay(ms); }
  static void DelayUs(uint32_t us) { delayMicroseconds(us); }
};

// Same API as DaisyDuino's CpuLoadMeter; measures HOST time.
class CpuLoadMeter {
public:
  void Init(float sr, int bs, float = 1.0f) { budget_s_ = bs / sr; Reset(); }
  void OnBlockStart();
  void OnBlockEnd();
  float GetAvgCpuLoad() const { return avg_; }
  float GetMinCpuLoad() const { return min_; }
  float GetMaxCpuLoad() const { return max_; }
  void Reset() { avg_ = 0; min_ = 1e9f; max_ = 0; first_ = true; }

private:
  double budget_s_ = 0.001, t0_ = 0;
  float avg_ = 0, min_ = 0, max_ = 0;
  bool first_ = true;
};

}  // namespace daisy
using namespace daisy;

enum DaisyDuinoDevice : short {
  DAISY_SEED,
  DAISY_POD,
  DAISY_PETAL,
  DAISY_FIELD,
  DAISY_PATCH,
  DAISY_PATCH_SM,
  DAISY_LAST,
};

enum DaisyDuinoSampleRate {
  AUDIO_SR_8K,
  AUDIO_SR_16K,
  AUDIO_SR_32K,
  AUDIO_SR_48K,
  AUDIO_SR_96K,
  AUDIO_SR_LAST,
};

class DaisyHardware {
public:
  DaisyHardware() {}
  Switch buttons[7];
  AnalogControl controls[12];
  AnalogControl cv[4];
  Encoder encoder;
  Led leds[2];
  GateIn gateIns[2];
  AnalogControl expression;

  int num_channels = 2;
  int numSwitches = 0, numLeds = 0, numGates = 0, numControls = 0;

  void Init(float control_update_rate, DaisyDuinoDevice device);
  float AnalogReadToVolts(int input) {
    float ret = (512.f - (float)input) * .009765625f;
    return constrain(ret, 0, 1024);
  }
  int VoltsToAnalogWrite(float volts) { return constrain(volts * 51.2f, 0, 255); }
  void WriteCvOut(uint8_t pin, float voltage) { analogWrite(pin, VoltsToAnalogWrite(voltage)); }
  float GetAdcValue(size_t idx) { return (int)idx < numControls ? controls[idx].Value() : 0.f; }
  void SetRingLed(uint8_t idx, float r, float g, float b);
  void SetFootswitchLed(uint8_t idx, float bright);
  void SetKeyboardLed(uint8_t row, uint8_t idx, float value);
  void SetKnobLed(uint8_t idx, float bright);
  void ClearLeds();
  void UpdateLeds() {}
  float GetKnobValue(uint8_t idx);
  bool KeyboardState(size_t) { return false; }
  bool KeyboardRisingEdge(size_t) { return false; }
  bool KeyboardFallingEdge(size_t) { return false; }
  void ProcessAnalogControls();
  void ProcessDigitalControls();
  void DebounceControls() { ProcessDigitalControls(); }
  void ProcessAllControls() { ProcessAnalogControls(); ProcessDigitalControls(); }
  void ResetToBootloader() { rigsim::warn_once("boot", "ResetToBootloader() called"); }

private:
  DaisyDuinoDevice device_ = DAISY_SEED;
};

class AudioClass {
public:
  DaisyHardware init(DaisyDuinoDevice device, DaisyDuinoSampleRate sr = AUDIO_SR_48K);
  void begin(AudioHandle::AudioCallback cb) { StartAudio(cb); }
  void begin(AudioHandle::InterleavingAudioCallback cb) { StartAudio(cb); }
  void end() { StopAudio(); }
  float get_samplerate() { return AudioSampleRate(); }
  float get_callbackrate() { return AudioCallbackRate(); }
  float get_blocksize() { return (float)AudioBlockSize(); }
  void StartAudio(AudioHandle::AudioCallback cb);
  void StartAudio(AudioHandle::InterleavingAudioCallback cb);
  void ChangeAudioCallback(AudioHandle::AudioCallback cb) { StartAudio(cb); }
  void ChangeAudioCallback(AudioHandle::InterleavingAudioCallback cb) { StartAudio(cb); }
  void StopAudio();
  void SetAudioSampleRate(SaiHandle::Config::SampleRate samplerate);
  float AudioSampleRate();
  void SetAudioBlockSize(size_t blocksize);
  size_t AudioBlockSize();
  float AudioCallbackRate() { return AudioSampleRate() / (float)AudioBlockSize(); }
};
extern AudioClass DAISY;

#endif
