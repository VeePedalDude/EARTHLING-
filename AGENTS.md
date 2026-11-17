# Rules for agents (and humans) changing Daisy firmware or this rig

These adapt two public rule sets to embedded audio work: the Karpathy-inspired
guidelines (think first, simplicity, surgical changes, goal-driven execution)
and Ponytail ("the best code is the code you never wrote"). This file is short
on purpose. Every rule is one that `daisyrig` can check or that a diff review
can see.

## 1. Think before building
- State the assumptions you are making before you edit: target board, sample rate, block size, which pins mean what, and whether the input is mono guitar or stereo.
- If a request has two readings ("make the delay smoother": less zipper noise, or a darker repeat tone?), name both and ask. Don't pick one silently.
- If you're confused about what the code is meant to do, run it first (`daisyrig run` on a few signals) and listen or measure before you form an opinion.
- If a simpler fix exists than the one asked for, say so.

## 2. Define "done" as a number before you start
- Write down the success criterion first, for example: "0 FAIL findings", "clicks on `sweep/sine_1k` go from 35 to 0", "no NaN at any knob position", "flash < 100 KB".
- Take a baseline first: `daisyrig test SKETCH --save-baseline work/before.json`.
- You're done only when `daisyrig test ... --baseline work/before.json` shows the criterion met and nothing else got worse. Loop until it does. A fix that has only been compiled hasn't been tested.

## 3. Lazy senior dev: write less
- Use what already exists before you write anything, which means the DaisySP copy that comes inside DaisyDuino. Examples: `Svf`, `Tone`/`ATone`, `Biquad`, `MoogLadder`, `fonepole`, `DelayLine`, `Oscillator`, `ReverbSc`, `Overdrive`, `Fold` and `Compressor`. Don't hand-roll one of these without a measured reason (CPU, sound, or memory).
- **DaisySP under DaisyDuino:**
  - `#include "DaisyDuino.h"` is the only include. It already opens `namespace daisysp`.
  - Never add `#include "daisysp.h"`, and never install the standalone DaisySP library. That header doesn't exist under DaisyDuino.
  - The bundled copy is DaisyDuino 1.7.0's (March 2025). It is missing `OnePole`, `LadderFilter`, `Wavefolder`, `GranularPlayer` and `Soap`.
  - Only use a module after `daisyrig build` (or `hwcheck`) compiles it. If you need a missing module, copy its `.h`/`.cpp` into the sketch folder and keep the MIT header.
  - libDaisy code (`DaisySeed hw;`, `hw.Init()`, Makefile projects) is a different framework. Port it; don't paste it in.
- Use the simplest structure that works: no class or template for something used once, no config knobs nobody asked for, and no error handling for things that can't happen in a fixed-rate callback.
- If 200 lines could be 50, make it 50. Deleting code is a valid fix.
- If you take a deliberate shortcut, mark it with `// SHORTCUT: <why> - revisit if <condition>` so it can be found later.

## 4. Surgical changes
- Touch only the lines that the fix needs. Don't reformat, rename or "tidy" neighbouring code. Don't delete someone else's dead code unless you were asked to.
- Clean up only your own mess: probes, temporary prints and bisect copies.
- Check the size of the diff before you deliver: `daisyrig scope old.ino new.ino`. It reports changed lines and whitespace-only churn. Whitespace churn should be 0.
- One concern per change. If you find a second bug, report it and fix it as a separate, measured step.

## 5. Firmware hard rules (checked by `daisyrig test`)
- In the audio callback: no `delay()`, no `Serial`, no `new`/`malloc`, and no blocking I/O. Read controls in `loop()` or smooth them.
- Write every output channel on every code path.
- Put buffers over ~64 KB in `DSY_SDRAM_BSS`, and clear them in `setup()`, because SDRAM boots full of garbage.
- Use float literals with `f`. The sketch's code must build clean under `-Wall -Wextra -Wdouble-promotion -Wshadow`, and under arm-gcc with `--hw`.
- No NaN/Inf at any knob position (the `min`/`max` presets). Feedback stays below 1, or is soft-clipped.

## 6. Report honestly
- Keep three groups separate: **verified** (measured, with units), **inferred** (CPU estimate, analog behaviour), and **not fixed** (still audible, with times and settings).
- Bisect causes, don't guess them: freeze one thing, re-run, halve.
- Before delivery, `daisyrig lint FILE` must report 0 lines (no rig hooks left).
