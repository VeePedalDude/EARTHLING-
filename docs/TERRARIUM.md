# PedalPCB Terrarium: pinout for DaisyDuino (Arduino IDE)

The Terrarium is a pedal PCB for the Electro-Smith Daisy Seed. It has six
B10K pots, four SPDT ON/ON toggles, two SPST momentary footswitches, two LEDs,
a TL072 buffer and an L78L05 regulator, and it fits a 125B enclosure.

## Pin map

| Control | Daisy pin (Arduino name) | Seed header pin | `TerrariumDuino.h` | Reads as |
|---|---|---|---|---|
| Knob 1 | A1 (D16) | 23 | `Knob(KNOB_1)` | 0.0–1.0 |
| Knob 2 | A2 (D17) | 24 | `Knob(KNOB_2)` | 0.0–1.0 |
| Knob 3 | A3 (D18) | 25 | `Knob(KNOB_3)` | 0.0–1.0 |
| Knob 4 | A4 (D19) | 26 | `Knob(KNOB_4)` | 0.0–1.0 |
| Knob 5 | A5 (D20) | 27 | `Knob(KNOB_5)` | 0.0–1.0 |
| Knob 6 | A6 (D21) | 28 | `Knob(KNOB_6)` | 0.0–1.0 |
| Toggle 1 | D10 | 11 | `Switch(SWITCH_1)` | `true` = ON (closed to ground) |
| Toggle 2 | D9 | 10 | `Switch(SWITCH_2)` | 〃 |
| Toggle 3 | D8 | 9 | `Switch(SWITCH_3)` | 〃 |
| Toggle 4 | D7 | 8 | `Switch(SWITCH_4)` | 〃 |
| Footswitch 1 | D25 | 32 | `FootswitchTapped(FOOTSWITCH_1)` | momentary, active-low |
| Footswitch 2 | D26 | 33 | `FootswitchTapped(FOOTSWITCH_2)` | momentary, active-low |
| LED 1 | D22 (DAC 2) | 29 | `SetLed(LED_1, on)` | HIGH = lit |
| LED 2 | D23 (DAC 1) | 30 | `SetLed(LED_2, on)` | HIGH = lit |
| Audio in | Seed audio in 1 → `in[0]` | 16 | | **mono** |
| Audio out | `out[0]` → Seed audio out 1 | 18 | | **mono** |

Things to know:

- **Mono.** Only the left channel is wired. Process `in[0]` and write `out[0]`. Copying it to `out[1]` is harmless.
- **Switches are active-low** and use `INPUT_PULLUP`. With raw `digitalRead()`, ON and pressed read `LOW`. `TerrariumDuino.h` inverts this for you.
- **The LEDs are on the Seed's DAC pins** (D22 = DAC 2, D23 = DAC 1). PedalPCB-community test firmware dims them by driving the DAC. With DaisyDuino, `digitalWrite` on/off is the safe choice. Treat `analogWrite` dimming as untested.
- **Raw `analogRead()` of a knob returns 0–1023** (10-bit). DaisyDuino's `AnalogControl` assumes 10 bits too.
- **Petal programs don't drive the Terrarium's LEDs or footswitches.** The knobs reach the same ADC pins in a different order, and the Terrarium uses different pins for the LEDs and footswitches. Use this pin map, not `DAISY_PETAL`.

## Arduino IDE settings

Tools → Board → **Generic STM32H7 Series**, Board part number → **Daisy Seed**,
Upload method → **STM32CubeProgrammer (DFU)**. If you print over USB, set USB
support → **CDC (generic 'Serial' supersede U(S)ART)**. To upload, hold BOOT,
tap RESET, release BOOT, then click Upload.

## Sources

- PedalPCB `terrarium.h` (github.com/PedalPCB/Terrarium): switch and knob indices into the Daisy Petal tables, and LED pins 22/23
- schult/terrarium-test `util/Terrarium.cpp`: direct Seed pins (A1–A6; D10, D9, D8, D7; D25, D26; LEDs on DAC 2/1) and mono passthrough `out[0] = in[0]`
- Electro-Smith Daisy Seed pinout / Pod schematic: header pin numbers
- PedalPCB Terrarium build document: parts and controls
- Electro-Smith DaisyWiki "Getting Started (Arduino Edition)": IDE settings
