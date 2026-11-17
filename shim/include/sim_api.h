// Simulator hooks. Sketches may use rig_probe() inside #ifdef DAISYRIG to
// log internal values; the harness uses the rest.
#pragma once
#include <cstdint>
#include <cstddef>

// Log a named value (one row per call, time-stamped) to <out>.probes.csv.
// Wrap in #ifdef DAISYRIG so hardware builds stay clean.
void rig_probe(const char *name, double value);

// Text drawn on a display stub (OLED / LCD); frames go to <out>.display.txt
void rig_display_text(const char *device, int x, int y, const char *text);
void rig_display_frame(const char *device);  // "sendBuffer"/"display()": flush a frame
void rig_display_clear(const char *device);

// MIDI input injected by control scripts. Returns false when none is due.
bool rig_midi_pop(uint8_t &status, uint8_t &d1, uint8_t &d2);
void rig_midi_out(uint8_t status, uint8_t d1, uint8_t d2);

// Record a non-pin hardware event (Petal ring LEDs, etc.)
void rig_event(const char *name, int index, double value);

namespace rigsim {
struct PinState {
  int mode = 0;          // Arduino pinMode
  bool driven = false;   // value set by the control script
  float level = 0.5f;    // 0..1 analog level from the script
  int out = -1;          // last digitalWrite/analogWrite value
};
PinState &pin(int p);
double now_seconds();        // simulated time
void advance(double seconds); // advance clock; runs due audio blocks
void set_pin_level(int p, float level);  // script -> pin (fires interrupts)
void warn_once(const char *key, const char *msg);
}  // namespace rigsim
