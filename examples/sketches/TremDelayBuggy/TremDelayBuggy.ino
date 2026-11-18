// -----------------------------------------------------------------------------
// Created by VeePedalDude - veepedaldude@gmail.com
// Free and open source, shared with the DIY pedal community for the love of the
// craft. I make nothing from it. Questions, ideas, or just want to connect?
// Email me anytime.
// -----------------------------------------------------------------------------
// TremDelayBuggy - Terrarium tremolo + delay with deliberate, typical bugs.
// Used to show what `daisyrig test` catches. See TremDelayFixed for the repair.
//
// Terrarium: POT1 delay time, POT2 feedback, POT3 trem rate, POT4 trem depth,
// POT5 level, FS1 bypass, LED1 effect on.
#include "DaisyDuino.h"

#define MAX_DELAY 48000

DaisyHardware hw;
float delay_buf[MAX_DELAY];   // BUG(mem): 188 KB in internal SRAM instead of SDRAM
size_t write_pos = 0;
Oscillator lfo;
bool bypass = false;
Switch fs1;

void AudioCallback(float **in, float **out, size_t size) {
  // BUG(zipper): raw ADC read straight into parameters, no smoothing
  float time_knob = analogRead(A1) / 1023.0f;
  float feedback = analogRead(A2) / 1023.0f * 1.1f;      // BUG(runaway): >1 at max
  lfo.SetFreq(0.5f + analogRead(A3) / 1023.0f * 12.0f);
  float depth = analogRead(A4) / 1023.0f;
  float level = 1.0f / (analogRead(A5) / 1023.0f);       // BUG(div0): Inf at min

  size_t delay_samps = 100 + (size_t)(time_knob * (MAX_DELAY - 200));  // BUG(click): jumps

  fs1.Debounce();
  if (fs1.RisingEdge()) {
    bypass = !bypass;                                    // BUG(pop): hard switch
    Serial.println(bypass ? "bypass" : "on");            // BUG: Serial in callback
  }

  for (size_t i = 0; i < size; i++) {
    float dry = in[0][i];
    if (bypass) {
      out[0][i] = dry;                                   // BUG on stereo hosts: out[1] never written
      continue;
    }
    size_t rp = (write_pos + MAX_DELAY - delay_samps) % MAX_DELAY;
    float wet = delay_buf[rp];
    delay_buf[write_pos] = dry + wet * feedback;
    write_pos++;
    if (write_pos > MAX_DELAY) write_pos = 0;            // BUG(overflow): off by one
    float trem = 1.0f - depth * (0.5f + 0.5f * lfo.Process());
    float y = shape(dry + wet) * trem * level;
    out[0][i] = y;
    out[1][i] = y;
  }
}

void setup() {
  hw = DAISY.init(DAISY_SEED, AUDIO_SR_48K);
  lfo.Init(DAISY.get_samplerate());
  lfo.SetWaveform(Oscillator::WAVE_SIN);
  fs1.Init(DAISY.get_samplerate() / 48.0f, true, D25, INPUT_PULLUP);
  pinMode(D22, OUTPUT);
  Serial.begin(115200);
  DAISY.begin(AudioCallback);
}

void loop() {
  digitalWrite(D22, !bypass);
  delay(5);
}

// defined after use: needs an Arduino-style auto prototype
float shape(float x) { return x; }
