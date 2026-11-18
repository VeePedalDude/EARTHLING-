// TerrariumTemplate - starting point for a PedalPCB Terrarium effect (Arduino IDE + DaisyDuino).
// Board: Generic STM32H7 Series > Daisy Seed. Upload: STM32CubeProgrammer (DFU).
//
// Controls
//   KNOB_1  Tone (lowpass cutoff)     KNOB_2  Resonance      KNOB_3  Level
//   SWITCH_1  Bandpass instead of lowpass
//   FOOTSWITCH_1  Bypass (LED_1 on when the effect is on)
#include "DaisyDuino.h"
#include "TerrariumDuino.h"

DaisyHardware hw;
Terrarium terrarium;
Svf filt;

void AudioCallback(float **in, float **out, size_t size) {
  terrarium.Process();
  filt.SetFreq(80.0f + terrarium.Knob(KNOB_1) * 7920.0f);
  filt.SetRes(terrarium.Knob(KNOB_2) * 0.9f);
  const float level = terrarium.Knob(KNOB_3);          // unity at full
  const bool band = terrarium.Switch(SWITCH_1);

  for (size_t i = 0; i < size; i++) {
    const float dry = in[0][i];
    filt.Process(dry);
    const float wet = (band ? filt.Band() : filt.Low()) * level;
    out[0][i] = terrarium.Output(dry, wet);
    out[1][i] = out[0][i];   // not wired on the Terrarium; keeps USB/other hosts sane
  }
}

void setup() {
  hw = DAISY.init(DAISY_SEED, AUDIO_SR_48K);
  filt.Init(DAISY.get_samplerate());
  terrarium.Init();
  DAISY.begin(AudioCallback);
}

void loop() {
  terrarium.Poll();
}
