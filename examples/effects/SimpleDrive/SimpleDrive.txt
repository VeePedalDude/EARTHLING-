// -----------------------------------------------------------------------------
// Created by VeePedalDude - veepedaldude@gmail.com
// Free and open source, shared with the DIY pedal community for the love of the
// craft. I make nothing from it. Questions, ideas, or just want to connect?
// Email me anytime.
// -----------------------------------------------------------------------------
// =============================================================================
// SimpleDrive - soft-clipping overdrive for the PedalPCB Terrarium (Daisy Seed)
// Arduino IDE + DaisyDuino. Single file: no extra headers needed.
//
// CONTROLS
//   KNOB_1 (A1)   Drive     KNOB_2 (A2)   Tone     KNOB_3 (A3)   Level
//   SWITCH_1 (D10)          ON = asymmetric clipping (warmer, even harmonics)
//   FOOTSWITCH_1 (D25)      bypass on/off     LED_1 (D22)  lit when effect is on
//
// SIGNAL FLOW
//   in[0] -> [1 Drive gain] -> [2 Pre-filter] -> [3 Clipper] -> [4 DC remover]
//         -> [5 Tone] -> [6 Level] -> [7 Bypass crossfade] -> out[0]
//
// HOW TO TWEAK
//   Change values in the TWEAK ZONE only. Each one says what it does, its unit
//   and a safe range. The modules below use them; you shouldn't need to edit them.
// =============================================================================
#include "DaisyDuino.h"

// =============================================================================
// TWEAK ZONE
// =============================================================================

// --- 1 Drive ----------------------------------------------------------------
const float DRIVE_GAIN_MIN   = 1.0f;     // x linear, knob at 0   (1.0 = clean)
const float DRIVE_GAIN_MAX   = 60.0f;    // x linear, knob at max (60 = +35.6 dB). Safe 2..200
const float DRIVE_CURVE      = 2.0f;     // knob taper: 1 = linear, 2 = more control at low drive

// --- 2 Pre-filter (before the clipper: tames fizz and aliasing) -------------
const float PRE_FILTER_HZ    = 7000.0f;  // Hz. Lower = smoother, darker. Safe 2000..12000

// --- 3 Clipper --------------------------------------------------------------
const float ASYM_BIAS        = 0.3f;     // SWITCH_1 ON: offset into the clipper. Safe 0.05..0.6

// --- 4 DC remover ------------------------------------------------------------
const float DC_TRACK_HZ      = 0.8f;     // Hz. Removes the offset asymmetric clipping leaves. Safe 0.3..5

// --- 5 Tone (lowpass after the clipper) ------------------------------------
const float TONE_MIN_HZ      = 600.0f;   // Hz, knob at 0
const float TONE_MAX_HZ      = 8000.0f;  // Hz, knob at max. Safe up to 16000
const float TONE_CURVE       = 2.0f;     // knob taper: 2 = finer control at the dark end

// --- 6 Level -----------------------------------------------------------------
const float LEVEL_MAX        = 0.9f;     // output at full knob (clipper peaks at 1.0). Keep <= 1.0

// --- Feel ----------------------------------------------------------------------
const float KNOB_GLIDE_MS    = 10.0f;    // ms. How fast the sound follows a knob. Safe 2..100
const float TONE_GLIDE_MS    = 20.0f;    // ms. How fast the tone follows its knob. Safe 5..200
const float BYPASS_FADE_MS   = 10.0f;    // ms crossfade on FOOTSWITCH_1. Safe 2..50

// =============================================================================
// PINS (PedalPCB Terrarium)
// =============================================================================
const uint8_t PIN_DRIVE = A1, PIN_TONE = A2, PIN_LEVEL = A3;
const uint8_t PIN_SWITCH_1 = D10, PIN_FOOTSWITCH_1 = D25, PIN_LED_1 = D22;

// =============================================================================
// MODULES (signal-flow order)
// =============================================================================
float sample_rate = 48000.0f;

// time in ms -> one-pole smoothing coefficient per sample (for fonepole)
float CoefFromMs(float ms, float rate) { return 1.0f / (ms * 0.001f * rate); }

// 1 Drive gain, smoothed so turning the knob makes no zipper noise
struct DriveGain {
  float gain = DRIVE_GAIN_MIN, coef = 0.002f;
  void Init() { coef = CoefFromMs(KNOB_GLIDE_MS, sample_rate); }
  float Process(float x, float target) { fonepole(gain, target, coef); return x * gain; }
};

// 2 Pre-filter
struct PreFilter {
  Tone lp;
  void Init() { lp.Init(sample_rate); float hz = PRE_FILTER_HZ; lp.SetFreq(hz); }
  float Process(float x) { return lp.Process(x); }
};

// 3 Clipper: tanh soft clip; asymmetric mode biases it and rescales back to +-1
struct Clipper {
  float offset = 0.0f, scale = 1.0f;
  void Init() { offset = tanhf(ASYM_BIAS); scale = 1.0f / (1.0f + offset); }
  float Process(float x, bool asym) {
    if (!asym) return tanhf(x);
    return (tanhf(x + ASYM_BIAS) - offset) * scale;
  }
};

// 4 DC remover: very slow tracker, no tilt even on sub-bass
struct DcRemover {
  float dc = 0.0f, coef = 0.0001f;
  void Init() { coef = 2.0f * PI_F * DC_TRACK_HZ / sample_rate; }
  float Process(float x) { fonepole(dc, x, coef); return x - dc; }
};

// 5 Tone: lowpass whose cutoff glides to the knob target once per block
struct ToneControl {
  Tone lp;
  float hz = 3000.0f, coef = 0.05f;
  void Init(float block_rate) { lp.Init(sample_rate); coef = CoefFromMs(TONE_GLIDE_MS, block_rate); }
  void SetTarget(float target_hz) { fonepole(hz, target_hz, coef); lp.SetFreq(hz); }
  float Process(float x) { return lp.Process(x); }
};

// 6 Level, smoothed
struct Level {
  float level = 0.5f, coef = 0.002f;
  void Init() { coef = CoefFromMs(KNOB_GLIDE_MS, sample_rate); }
  float Process(float x, float target) { fonepole(level, target, coef); return x * level; }
};

// 7 Bypass crossfade between dry and effect
struct Bypass {
  float mix = 1.0f, coef = 0.002f;
  void Init() { coef = CoefFromMs(BYPASS_FADE_MS, sample_rate); }
  float Process(float dry, float wet, bool on) {
    fonepole(mix, on ? 1.0f : 0.0f, coef);
    return dry + mix * (wet - dry);
  }
};

DriveGain drive;
PreFilter pre_filter;
Clipper clipper;
DcRemover dc_remover;
ToneControl tone_ctl;
Level level;
Bypass bypass;

// =============================================================================
// CONTROLS: knob/switch -> parameter targets
// =============================================================================
DaisyHardware hw;
Switch footswitch_1, switch_1;
volatile float knob_drive = 0.5f, knob_tone = 0.5f, knob_level = 0.5f;   // written in loop()
volatile bool effect_on = true;

float DriveTarget(float k) { return DRIVE_GAIN_MIN + (DRIVE_GAIN_MAX - DRIVE_GAIN_MIN) * powf(k, DRIVE_CURVE); }
float ToneTarget(float k)  { return TONE_MIN_HZ + (TONE_MAX_HZ - TONE_MIN_HZ) * powf(k, TONE_CURVE); }
float LevelTarget(float k) { return LEVEL_MAX * k; }

// =============================================================================
// AUDIO (runs every block: 48 samples = 1 ms)
// =============================================================================
void AudioCallback(float **in, float **out, size_t size) {
  footswitch_1.Debounce();
  switch_1.Debounce();
  if (footswitch_1.RisingEdge()) effect_on = !effect_on;
  const bool asym = switch_1.Pressed();
  const float drive_target = DriveTarget(knob_drive);
  const float level_target = LevelTarget(knob_level);
  tone_ctl.SetTarget(ToneTarget(knob_tone));

  for (size_t i = 0; i < size; i++) {
    const float dry = in[0][i];
    float x = drive.Process(dry, drive_target);   // 1
    x = pre_filter.Process(x);                    // 2
    x = clipper.Process(x, asym);                 // 3
    x = dc_remover.Process(x);                    // 4
    x = tone_ctl.Process(x);                       // 5
    x = level.Process(x, level_target);           // 6
    out[0][i] = bypass.Process(dry, x, effect_on);// 7
    out[1][i] = out[0][i];                        // Terrarium is mono; harmless copy
  }
}

// =============================================================================
// SETUP / LOOP
// =============================================================================
void setup() {
  hw = DAISY.init(DAISY_SEED, AUDIO_SR_48K);
  sample_rate = DAISY.get_samplerate();
  const float block_rate = sample_rate / (float)DAISY.AudioBlockSize();

  footswitch_1.Init(block_rate, true, PIN_FOOTSWITCH_1, INPUT_PULLUP);
  switch_1.Init(block_rate, true, PIN_SWITCH_1, INPUT_PULLUP);
  pinMode(PIN_LED_1, OUTPUT);

  drive.Init();
  pre_filter.Init();
  clipper.Init();
  dc_remover.Init();
  tone_ctl.Init(block_rate);
  level.Init();
  bypass.Init();

  DAISY.begin(AudioCallback);
}

void loop() {
  knob_drive = analogRead(PIN_DRIVE) / 1023.0f;
  knob_tone = analogRead(PIN_TONE) / 1023.0f;
  knob_level = analogRead(PIN_LEVEL) / 1023.0f;
  digitalWrite(PIN_LED_1, effect_on ? HIGH : LOW);
  delay(1);
}
