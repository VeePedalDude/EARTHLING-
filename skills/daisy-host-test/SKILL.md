---
name: daisy-host-test
description: Test Arduino-IDE (DaisyDuino) Daisy Seed audio firmware by building and running it on the host with daisyrig, never by reading alone. Use when asked to test, debug, fix, review or improve a Daisy Seed / Terrarium / Pod / Petal sketch.
---

# Testing Daisy Seed sketches with daisyrig

Never judge DSP firmware by reading it. Build it, run it, measure it, and
report numbers.

**Rules** (full text in the repo's `AGENTS.md`):
- **Think first.** State your assumptions (board, sample rate, pins). If a request has two readings, ask which one is meant.
- **Define done as a number** before you edit, for example "clicks on sweep/sine_1k: 35 → 0".
- **Write less.** Use the DaisySP bundled with DaisyDuino (no `daisysp.h` include, no standalone DaisySP; `OnePole`, `LadderFilter`, `Wavefolder`, `GranularPlayer` and `Soap` aren't in it) before writing your own DSP. No one-use abstractions. Deleting code counts as a fix.
- **Tweakable layout.** For a new effect, follow AGENTS.md §5c: a TWEAK ZONE of named constants (units and safe ranges), one struct per stage, and a callback with one line per stage.
- **Surgical changes.** Touch only what the fix needs. `daisyrig scope old new` must show 0 whitespace-only lines.

## 0. Get the rig

```bash
git clone <this repo> daisy-host-rig && cd daisy-host-rig   # or use an existing checkout
pip install -r requirements.txt --break-system-packages 2>/dev/null || true
./daisyrig setup          # once; ~30 s
./daisyrig setup --hw     # optional: real ARM compile (arduino-cli + STM32 core, large download)
```

Put the user's sketch in its own folder, named like the `.ino`
(`MyPedal/MyPedal.ino`). Work on a copy, never the original upload. For a
new Terrarium effect, start with `./daisyrig new MyPedal`, which creates the
template plus `TerrariumDuino.h`. Pins are in `docs/TERRARIUM.md`. The
Terrarium is mono (`in[0]` → `out[0]`).

## 1. Baseline before touching anything

```bash
./daisyrig test MyPedal --board terrarium --save-baseline work/before.json [--hw]
```

- Choose `--board` from the pins the sketch uses. Terrarium means A1–A6 knobs, D25/D26 footswitches and D22/D23 LEDs. Pod means `DAISY_POD`. Petal means `DAISY_PETAL`. Anything else is `seed`.
- If it fails to compile on the host, decide whether the fault is in the sketch or in the shim. If the shim is missing an Arduino or library API, add a stub under `shim/include/` and keep going. If the sketch is wrong, that is a finding in itself.
- Write control scripts (`.ctl`) for how the pedal is actually used: its footswitch modes, toggle combinations, encoder menus and MIDI. Pass them with `--ctl file.ctl` (repeatable).

## 2. Read the report like an engineer

`REPORT.md` lists FAIL, then WARN, then INFO, and keeps verified numbers apart
from inferred ones. Treat these as real bugs:

- **NaN/Inf**: note the first time it happens and which knob settings cause it.
- **Sanitizer errors**: the file:line is in `results.json` under `sanitizers`.
- **Unwritten output samples**: some path through the callback skips `out[ch][i]`.
- **delay(), Serial, or heap allocation inside the callback.**
- **Garbage on silent input**: usually an SDRAM buffer that was never cleared. The rig fills SDRAM with random bytes at boot, like the real chip.
- **Static RAM over 512 KB**, or flash over 128 KB with `--hw`.

Heuristic flags (clicks, tails, idle noise) need confirming. Open the render
in `renders/<ctl>/<signal>.wav` at the flagged time. Measure with
`./daisyrig analyze`, or with numpy around that sample index: |second
difference| against its neighbourhood, or the spectrum.

## 3. Find causes by bisecting, not guessing

To find out which part of the code causes an artifact:

1. Copy the sketch and freeze or disable one suspect, for example a knob
   (`t_time = 0.5f;`), a stage (bypass it) or a modulation (set its depth to 0).
2. Rebuild and run that single case:
   `./daisyrig build Variant -o v && ./daisyrig run v build/signals/48000/sine_1k.wav v.wav --ctl twiddle --board terrarium`
3. If the artifact goes away, the cause is inside what you froze. Halve again.
4. To watch an internal value, use `rig_probe("name", value)` inside `#ifdef DAISYRIG`. The values land in `*.probes.csv`.

## 4. Fix, then quantify before vs after

After every change:

```bash
./daisyrig test MyPedal --board terrarium --baseline work/before.json
```

- Make one concern per change. Run `./daisyrig scope before.ino after.ino` and keep the diff minimal, with 0 whitespace-only lines.
- Re-run the whole suite, not only the case you fixed. Earlier tests must still pass.
- Report each fix with numbers: the metric, its value before, its value after, and the units (clicks/run, dBFS, THD %, NaN count, KB, host CPU %).
- For refactors that should not change the sound, check that `./daisyrig null old.wav new.wav` shows `identical: true`. If it doesn't, explain the difference in dB.
- Typical fixes (DaisyDuino's bundled DaisySP only): smooth control reads (`fonepole`), read controls in `loop()` and pass them through volatiles, interpolate delay reads, cap feedback below 1 with a soft clip in the loop, crossfade bypass over about 10 ms, write every output channel on every path, put large buffers in `DSY_SDRAM_BSS` and clear them in `setup()`, and use `f`-suffixed float literals (avoid `-Wdouble-promotion`).

## 5. Deliver a clean file

- Remove debug taps, then run `./daisyrig lint MyPedal/MyPedal.ino`. It must report 0 lines.
- Run a final `./daisyrig scope original.ino MyPedal.ino` and include its numbers in the hand-off.
- The final file must compile with no sketch warnings: the report's "Compiler warnings in sketch: 0" (host `-Wall -Wextra`). With `--hw`, the arm-gcc warnings must also be 0.
- Deliver two files: `Name.ino` and `Name.txt`, where the `.txt` is an exact copy of the `.ino`. Give the before→after table, reports and renders only if asked.

## 6. What to tell the user

Keep these three groups separate:

- **Verified**: measured on the host or target build, with numbers.
- **Inferred**: Daisy CPU % (host time × uncalibrated factor), anything about
  the analog front end, and real ADC or codec behaviour.
- **Not fixed / still audible**: artifacts you found but did not or could not
  remove, with the time, settings and size of each.

Don't claim it "sounds better". Say what changed, in measured units.
