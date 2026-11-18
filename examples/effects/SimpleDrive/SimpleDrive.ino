// SimpleDrive - soft-clipping overdrive for the PedalPCB Terrarium (Daisy Seed)
// Arduino IDE + DaisyDuino. Single file: no extra headers needed.
//
// KNOB_1 (A1)  Drive      KNOB_2 (A2)  Tone      KNOB_3 (A3)  Level
// SWITCH_1 (D10)  ON = asymmetric clipping (warmer, more even harmonics)
// FOOTSWITCH_1 (D25)  bypass, LED_1 (D22) lit when the effect is on
#include "DaisyDuino.h"

const uint8_t PIN_DRIVE = A1, PIN_TONE = A2, PIN_LEVEL = A3;
const uint8_t PIN_SW1 = D10, PIN_FS1 = D25, PIN_LED1 = D22;

DaisyHardware hw;
Switch fs1, sw1;
Tone tone_lp;     // post-clip lowpass
Tone pre_lp;      // pre-clip lowpass: tames fizz and aliasing

// written in loop(), read in the callback
volatile float k_drive = 0.5f, k_tone = 0.5f, k_level = 0.5f;
volatile bool effect_on = true;

float gain = 1.0f, level = 0.5f, tone_hz = 3000.0f, mix = 1.0f;
float dc_est = 0.0f;   // slow DC tracker (asymmetric mode leaves DC)

void AudioCallback(float **in, float **out, size_t size) {
  fs1.Debounce();
  sw1.Debounce();
  if (fs1.RisingEdge()) effect_on = !effect_on;
  const bool asym = sw1.Pressed();

  const float target_gain = 1.0f + 59.0f * k_drive * k_drive;   // 0 .. +35.6 dB
  const float target_level = 0.9f * k_level;    // clipper peaks at 1.0
  fonepole(tone_hz, 600.0f + 7400.0f * k_tone * k_tone, 0.05f);
  tone_lp.SetFreq(tone_hz);
  const float mix_target = effect_on ? 1.0f : 0.0f;

  for (size_t i = 0; i < size; i++) {
    fonepole(gain, target_gain, 0.002f);     // ~10 ms, no zipper noise
    fonepole(level, target_level, 0.002f);
    fonepole(mix, mix_target, 0.002f);       // ~10 ms bypass crossfade

    float dry = in[0][i];
    float x = dry * gain;
    x = pre_lp.Process(x);
    if (asym) x += 0.3f;                     // bias -> asymmetric clipping
    float y = tanhf(x);
    if (asym) y = (y - 0.2913f) * 0.7744f;   // remove offset (tanh 0.3), rescale to +-1
    fonepole(dc_est, y, 0.0001f);            // ~0.8 Hz: no tilt even on sub-bass
    y -= dc_est;
    y = tone_lp.Process(y) * level;

    out[0][i] = dry + mix * (y - dry);
    out[1][i] = out[0][i];                   // Terrarium is mono; harmless copy
  }
}

void setup() {
  hw = DAISY.init(DAISY_SEED, AUDIO_SR_48K);
  const float sr = DAISY.get_samplerate();
  const float block_rate = sr / (float)DAISY.AudioBlockSize();
  fs1.Init(block_rate, true, PIN_FS1, INPUT_PULLUP);
  sw1.Init(block_rate, true, PIN_SW1, INPUT_PULLUP);
  pinMode(PIN_LED1, OUTPUT);
  tone_lp.Init(sr);
  pre_lp.Init(sr);
  float pre_hz = 7000.0f;
  pre_lp.SetFreq(pre_hz);
  DAISY.begin(AudioCallback);
}

void loop() {
  k_drive = analogRead(PIN_DRIVE) / 1023.0f;
  k_tone = analogRead(PIN_TONE) / 1023.0f;
  k_level = analogRead(PIN_LEVEL) / 1023.0f;
  digitalWrite(PIN_LED1, effect_on ? HIGH : LOW);
  delay(1);
}
