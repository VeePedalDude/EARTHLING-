// TerrariumDuino.h - PedalPCB Terrarium controls for DaisyDuino (Arduino IDE).
// Copy this file into your sketch folder, next to YourEffect.ino.
//
// Pins (Daisy Seed), from PedalPCB's terrarium.h and its Petal pin tables:
//   KNOB_1..6     A1  A2  A3  A4  A5  A6   (D16-D21, header pins 23-28)
//   SWITCH_1..4   D10 D9  D8  D7          (header pins 11, 10, 9, 8)  ON = closed to ground
//   FOOTSWITCH_1  D25 (header pin 32)     FOOTSWITCH_2  D26 (pin 33)  momentary, to ground
//   LED_1         D22 (pin 29, DAC 2)     LED_2         D23 (pin 30, DAC 1)
//   Audio: mono. Input jack -> in[0] (pin 16), out[0] (pin 18) -> output jack.
//
// Use:
//   setup():  hw = DAISY.init(DAISY_SEED, AUDIO_SR_48K); terrarium.Init(); ... DAISY.begin(cb);
//   loop():   terrarium.Poll();           // reads knobs, drives LEDs (slow work lives here)
//   callback: terrarium.Process();        // once per block: debounces switches, smooths knobs
//             out[0][i] = terrarium.Output(dry, wet);   // once per sample: FS1 bypass crossfade
#pragma once
#include "DaisyDuino.h"

enum TerrariumControl {
  KNOB_1 = 0, KNOB_2, KNOB_3, KNOB_4, KNOB_5, KNOB_6,
  SWITCH_1 = 0, SWITCH_2, SWITCH_3, SWITCH_4,
  FOOTSWITCH_1 = 0, FOOTSWITCH_2,
  LED_1 = 0, LED_2
};

class Terrarium {
public:
  static constexpr uint8_t kKnobPin[6] = {A1, A2, A3, A4, A5, A6};
  static constexpr uint8_t kSwitchPin[4] = {D10, D9, D8, D7};
  static constexpr uint8_t kFootswitchPin[2] = {D25, D26};
  static constexpr uint8_t kLedPin[2] = {D22, D23};

  // Call in setup(), after DAISY.init() and before DAISY.begin().
  void Init() {
    const float sr = DAISY.get_samplerate();
    const float block_rate = sr / (float)DAISY.AudioBlockSize();
    for (int i = 0; i < 4; i++) switch_[i].Init(block_rate, true, kSwitchPin[i], INPUT_PULLUP);
    for (int i = 0; i < 2; i++) foot_[i].Init(block_rate, true, kFootswitchPin[i], INPUT_PULLUP);
    for (int i = 0; i < 2; i++) { pinMode(kLedPin[i], OUTPUT); digitalWrite(kLedPin[i], LOW); }
    for (int i = 0; i < 6; i++) raw_[i] = smooth_[i] = analogRead(kKnobPin[i]) / 1023.0f;
    xfade_step_ = 1.0f / (0.010f * sr);   // 10 ms bypass crossfade
  }

  // Call every pass of loop(). ADC reads and LED writes stay out of the audio callback.
  void Poll() {
    for (int i = 0; i < 6; i++) raw_[i] = analogRead(kKnobPin[i]) / 1023.0f;
    digitalWrite(kLedPin[LED_1], (effect_on_ || led_[LED_1]) ? HIGH : LOW);
    digitalWrite(kLedPin[LED_2], led_[LED_2] ? HIGH : LOW);
  }

  // Call once at the top of the audio callback.
  void Process() {
    for (int i = 0; i < 4; i++) switch_[i].Debounce();
    for (int i = 0; i < 2; i++) foot_[i].Debounce();
    for (int i = 0; i < 6; i++) smooth_[i] += 0.1f * (raw_[i] - smooth_[i]);  // ~10 ms
    if (use_bypass_ && foot_[FOOTSWITCH_1].RisingEdge()) effect_on_ = !effect_on_;
  }

  // Once per sample: crossfades dry <-> wet when FOOTSWITCH_1 toggles the effect.
  float Output(float dry, float wet) {
    const float target = effect_on_ ? 1.0f : 0.0f;
    if (mix_ < target) mix_ = fminf(target, mix_ + xfade_step_);
    else if (mix_ > target) mix_ = fmaxf(target, mix_ - xfade_step_);
    return dry + mix_ * (wet - dry);
  }

  float Knob(int k) const { return smooth_[k]; }                 // 0.0 .. 1.0, smoothed
  bool Switch(int s) { return switch_[s].Pressed(); }            // toggle ON?
  bool FootswitchPressed(int f) { return foot_[f].Pressed(); }    // held down now
  bool FootswitchTapped(int f) { return foot_[f].RisingEdge(); }  // pressed this block
  bool EffectOn() const { return effect_on_; }
  void SetEffectOn(bool on) { effect_on_ = on; }
  void SetLed(int l, bool on) { led_[l] = on; }                   // LED_1 also lights when effect is on
  void UseFootswitch1ForBypass(bool use) { use_bypass_ = use; }   // false: FS1 is yours

private:
  daisy::Switch switch_[4], foot_[2];
  volatile float raw_[6] = {0};
  float smooth_[6] = {0};
  volatile bool led_[2] = {false, false};
  volatile bool effect_on_ = true;
  bool use_bypass_ = true;
  float mix_ = 1.0f, xfade_step_ = 0.002f;
};
