// -----------------------------------------------------------------------------
// Created by VeePedalDude - veepedaldude@gmail.com
// Free and open source, shared with the DIY pedal community for the love of the
// craft. I make nothing from it. Questions, ideas, or just want to connect?
// Email me anytime.
// -----------------------------------------------------------------------------
// TremDelayFixed - the repaired version of TremDelayBuggy.
//
// Terrarium: POT1 delay time, POT2 feedback, POT3 trem rate, POT4 trem depth,
// POT5 level, FS1 bypass, LED1 effect on.
#include "DaisyDuino.h"

#define MAX_DELAY 48000

DaisyHardware hw;
float DSY_SDRAM_BSS delay_buf[MAX_DELAY];   // big buffers live in SDRAM
size_t write_pos = 0;
Oscillator lfo;
Switch fs1;

// control targets: written by loop(), read by the callback
volatile float t_time = 0.5f, t_fb = 0.3f, t_rate = 0.3f, t_depth = 0.5f, t_level = 0.8f;
// smoothed values: callback only
float s_time = 0.5f * MAX_DELAY, s_fb = 0.3f, s_depth = 0.5f, s_level = 0.8f;
bool bypass = false;
float wet_mix = 1.0f;   // 1 = effect, 0 = bypass (crossfaded)
volatile bool led_on = true;

static inline float soft_clip(float x) { return x / (1.0f + fabsf(x)); }

void AudioCallback(float **in, float **out, size_t size) {
  fs1.Debounce();
  if (fs1.RisingEdge()) {
    bypass = !bypass;
    led_on = !bypass;
  }
  const float target_time = 100.0f + t_time * (MAX_DELAY - 200.0f);
  const float mix_target = bypass ? 0.0f : 1.0f;
  lfo.SetFreq(0.5f + t_rate * 12.0f);

  for (size_t i = 0; i < size; i++) {
    fonepole(s_time, target_time, 0.0002f);   // ~100 ms glide, no jumps
    fonepole(s_fb, t_fb * 0.95f, 0.002f);     // capped below 1
    fonepole(s_depth, t_depth, 0.002f);
    fonepole(s_level, t_level, 0.002f);
    fonepole(wet_mix, mix_target, 0.002f);    // ~10 ms bypass crossfade

    const float dry = in[0][i];
    // fractional read with linear interpolation
    float rp = (float)write_pos - s_time;
    if (rp < 0.0f) rp += MAX_DELAY;
    size_t i0 = (size_t)rp;
    float frac = rp - (float)i0;
    size_t i1 = (i0 + 1) % MAX_DELAY;
    const float wet = delay_buf[i0] + frac * (delay_buf[i1] - delay_buf[i0]);

    delay_buf[write_pos] = soft_clip(dry + wet * s_fb);
    if (++write_pos >= MAX_DELAY) write_pos = 0;

    const float trem = 1.0f - s_depth * (0.5f + 0.5f * lfo.Process());
    const float fx = shape(dry + wet) * trem * s_level;
    const float y = dry + wet_mix * (fx - dry);
    out[0][i] = y;
    out[1][i] = y;
  }
}

void setup() {
  hw = DAISY.init(DAISY_SEED, AUDIO_SR_48K);
  for (size_t i = 0; i < MAX_DELAY; i++) delay_buf[i] = 0.0f;  // SDRAM is not zeroed
  lfo.Init(DAISY.get_samplerate());
  lfo.SetWaveform(Oscillator::WAVE_SIN);
  fs1.Init(DAISY.get_samplerate() / 48.0f, true, D25, INPUT_PULLUP);
  pinMode(D22, OUTPUT);
  DAISY.begin(AudioCallback);
}

void loop() {
  t_time = analogRead(A1) / 1023.0f;
  t_fb = analogRead(A2) / 1023.0f;
  t_rate = analogRead(A3) / 1023.0f;
  t_depth = analogRead(A4) / 1023.0f;
  t_level = analogRead(A5) / 1023.0f;
  digitalWrite(D22, led_on);
  delay(2);
}

float shape(float x) { return soft_clip(1.5f * x) / soft_clip(1.5f); }
