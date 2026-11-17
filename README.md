# daisyrig: a host test rig for Arduino-IDE Daisy Seed sketches

`daisyrig` compiles a DaisyDuino `.ino` sketch for your computer instead of the
Daisy. It plays realistic guitar and test signals through the audio callback,
moves the knobs and footswitches from a script, and measures what comes out.
It reports NaNs, buffer overruns, clicks, clipping, runaway feedback, unwritten
outputs, heap use and Serial calls inside the callback, memory placement and
CPU headroom. It can also compile the same sketch with the real ARM toolchain
to get true flash and RAM numbers.

```
./daisyrig setup                       # fetch DaisyDuino, build runtimes, make signals (~30 s)
./daisyrig test path/to/MySketch --board terrarium
```

You get `REPORT.md` (verdict, findings, verified vs inferred numbers),
`results.json`, and a folder of rendered WAVs to listen to.

## Why

Flashing and listening is slow, and it misses whatever you didn't happen to
play. On the host you can rerun 100+ renders in seconds after every change. You
can bisect a click down to one line, and you can compare before and after with
numbers instead of memory.

## What is real and what is simulated

| Part | How it runs |
|---|---|
| DaisySP (every module) | **real** DaisySP source from DaisyDuino, compiled for the host |
| `Switch`, `AnalogControl`, `Encoder`, `Led`, `GateIn`, `Parameter` | **real** DaisyDuino classes, reading simulated pins |
| `DAISY.init/begin`, `DaisyHardware` (Seed, Pod, Petal, Patch) | re-implemented. Same API, and the callback is driven from WAV input |
| Arduino core (`analogRead`, `digitalRead`, `millis`, `delay`, `Serial`, `attachInterrupt`, `String` ...) | simulated with a clock that runs on audio time |
| SDRAM (`DSY_SDRAM_BSS`) | separate section, **filled with random bytes at boot like real SDRAM** (`--sdram-fill zero` to disable) |
| U8g2 / U8x8, Adafruit_SSD1306, LiquidCrystal_I2C | stubs that log the drawn text, frame by frame, to `*.display.txt` |
| MIDI library (`MIDI.h`) | stub. Incoming notes, CCs and pitch bends come from the control script |
| Wire / SPI | accept-everything stubs |

**Timing model.** `setup()` runs once and `loop()` runs forever. Audio blocks
fire like interrupts whenever simulated time passes the next block deadline:
during `delay()`, while `millis()`/`micros()` are polled, and between `loop()`
iterations. Each `loop()` pass costs 1/4 of a block period by default
(`--loops-per-block`). Code that polls the clock therefore behaves as it does
on hardware, and audio timing stays sample-exact.

## Commands

```
daisyrig setup [--hw]                  # --hw also installs arduino-cli + STM32 core for hwcheck
daisyrig test SKETCH [--board B] [--ctl file.ctl] [--full] [--hw]
                     [--baseline old/results.json] [--save-baseline f] [--strict]
daisyrig build SKETCH [-o bin] [--asan]
daisyrig run BIN in.wav out.wav [--ctl preset|file] [--board B] [--block N] [--adc-noise LSB] [--ftz]
daisyrig analyze out.wav [--ref in.wav]
daisyrig null a.wav b.wav              # sample-wise diff (bit-exact refactor check)
daisyrig compare before.json after.json
daisyrig hwcheck SKETCH                # real arm-gcc build: flash / RAM / ARM warnings
daisyrig signals --out dir [--sr 48000]
daisyrig lint FILE...                  # leftover rig hooks / host-only code before delivery
```

`SKETCH` is a sketch folder (Arduino layout, all `.ino` files merged and
`.cpp` files compiled) or a single `.ino`. Arduino-style function prototypes
are generated automatically.

Boards (`--board`) map names to Daisy pins for control scripts: `seed` (A0–A11,
D0–D32), `terrarium` (POT1–6, SW1–4, FS1–2, LED1–2), `pod`, `petal`. These maps
only name pins. Your sketch reads whatever pins it reads.

## What `test` does

1. **Host build** with `-Wall -Wextra -Wdouble-promotion -Wshadow`. Sketch warnings are listed; library warnings are suppressed.
2. **Memory**: static RAM vs SDRAM from the sketch's own objects, with the biggest buffers listed.
3. **Render matrix**: every signal at `noon`, and the quick set under `min`, `max`, `sweep`, `twiddle`, plus `footswitch`/`toggles` on boards that have them. Two more runs use jittery ADC (2 LSB noise).
4. **Per-render checks**: NaN/Inf (with the first time it happens), samples left unwritten by the callback, heap allocation / `delay()` / `Serial` inside the callback, clipping, DC, clicks (on sine, sweep and silence), idle noise, tails and self-oscillation, THD and aliasing on sines, latency from the impulse, gain, and spectral centroid.
5. **Determinism**: the same render twice must be bit-identical.
6. **AddressSanitizer + UBSan** build, run on the quick set.
7. **CPU**: host time per block, measured on a quiet sequential run.
8. Optional **`--hw`**: real Daisy compile through arduino-cli, which reports flash (128 KB limit) and RAM (512 KB).
9. Optional **`--baseline`**: before→after table of every key metric.

### Test signals (`build/signals/<sr>/`)

The guitar signals are physically modelled: Karplus-Strong strings with pick
and pickup-position combs and a humbucker resonance. Pitch is accurate to
under 1 cent. Each signal has 0.25 s of silence before it and 1.5 s after.

`pluck_low_e`, `pluck_high`, `chord_strum`, `palm_mute`, `legato_run` (fast 16ths, a hard case for trackers), `bend`, `hot_humbucker` (−0.4 dBFS), `quiet_pick` (−40 dBFS), `sine_1k`, `sine_3k5` (aliasing), `sine_100`, `sweep` (20 Hz–20 kHz), `impulse`, `silence`, `pink_noise`.

### Control scripts (`.ctl`)

```
board terrarium
set ALLPOTS 0.5
ramp 1.0 3.0 POT2 0 1        # 1 ms steps
at 2.0 tap FS1 60ms          # active-low aware
at 3.0 toggle SW1 on
at 0.5 turn ENC +3           # pod / petal encoder detents
at 1.0 noteon 60 100         # MIDI (MIDI.h sketches)
at 1.5 noteoff 60 ; at 2 cc 74 64 ; at 2.5 bend -4096
```

Presets: `noon`, `min`, `max`, `sweep` (all pots 0→1), `twiddle` (all pots
oscillating at 3–7 Hz across their full range, an extreme case), `footswitch`,
`toggles`. More examples are in `examples/controls/`.

### Probing internals

```cpp
#ifdef DAISYRIG
  rig_probe("pitch_hz", tracker.freq);   // -> out.probes.csv, time-stamped
#endif
```

`DAISYRIG` is only defined on the host. Run `daisyrig lint` before shipping to
make sure no probes are left in.

## Verified vs inferred

The report keeps these apart, and you should too.

- **Verified on the host:** anything about numerical behaviour. That covers NaN/Inf, overruns, unwritten samples, gain, THD/aliasing, clicks at specific times, determinism, and what happens when you turn the knobs.
- **Verified on target (with `--hw`):** it compiles for the Daisy, plus flash and RAM use.
- **Inferred:** the Daisy CPU %. This is host p99 time × `DAISYRIG_M7_FACTOR` (default 30), which is uncalibrated. To calibrate it, measure one sketch with `CpuLoadMeter` on hardware and set the factor. Click, tail and idle flags are heuristics, so listen at the times they flag.

Not simulated: codec noise and analog front-end behaviour, real ADC timing,
interrupt priority and DMA races, QSPI flash storage (`PersistentStorage`),
Daisy Field hardware, and the Patch SM pin map.

## Demo: what it catches

`examples/sketches/TremDelayBuggy` has typical bugs planted in it.
`TremDelayFixed` is the repaired version.

```
./daisyrig test examples/sketches/TremDelayBuggy --board terrarium --save-baseline /tmp/before.json
./daisyrig test examples/sketches/TremDelayFixed --board terrarium --baseline /tmp/before.json
```

The buggy version comes back FAIL. UBSan flags index 48000 out of bounds (an
off-by-one wrap). Turning the level knob to 0 produces NaN/Inf. In bypass,
`out[1]` is never written. Serial is called inside the callback. A click
appears at exactly t = 1.000 s, where the buffer wraps. The 188 KB buffer sits
in SRAM. The fixed version has zero FAIL findings, the buffer is in SDRAM, the
sanitizers are clean, and sweeping every pot produces zero clicks.

If you delete the SDRAM-clearing loop from the fixed version, the rig reports
7294 NaN samples on silent input. Random SDRAM at boot is a classic first-flash
bug.

## Self test

```
python3 tests/selftest.py     # ~30 s, also run by CI
```

## Requirements

Linux or macOS with g++ (C++17), Python 3.9+, numpy and scipy, and git.
`--hw` needs network access to GitHub. It downloads arduino-cli and the
STMicroelectronics STM32 core into the normal Arduino data folder, which it
shares with the Arduino IDE.

## Using it with Claude

`skills/daisy-host-test/SKILL.md` is a skill that teaches Claude (or any agent)
the whole workflow: build, test, bisect, fix, re-test, compare, and deliver a
clean file. Add it to your skills, or point the agent at it.

## Licence

MIT for this repo. DaisyDuino and DaisySP (MIT, Electrosmith) are fetched at a
pinned commit into `third_party/` and are not redistributed here.
