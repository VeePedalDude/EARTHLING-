# Prompting a Terrarium effect, then testing it

You don't need to know C++. The loop is: describe the effect to an AI, test
what it writes, paste the report back, and repeat until it passes. Then upload
it from the Arduino IDE.

## 1. Start a sketch

```
./daisyrig new effects/MyFuzz
```

This creates `effects/MyFuzz/MyFuzz.ino` (a working filter template) and
`TerrariumDuino.h` next to it. Keep both files in that folder, because the
Arduino IDE needs them together.

## 2. Prompt

Paste this to Claude, ChatGPT or another assistant. Fill in the brackets, and
attach `MyFuzz.ino` and `TerrariumDuino.h`:

```text
Write a guitar effect for the PedalPCB Terrarium (Daisy Seed), Arduino IDE + DaisyDuino.
Effect: [describe the sound, e.g. "a gated octave fuzz with a tone control"]
Controls: [what each of KNOB_1..6, SWITCH_1..4 and FOOTSWITCH_2 should do]

Rules:
- Edit the attached MyFuzz.ino. Keep the TerrariumDuino.h API exactly as it is:
  terrarium.Init() in setup(), terrarium.Poll() in loop(),
  terrarium.Process() at the top of the callback,
  out[0][i] = terrarium.Output(dry, wet) for each sample (FOOTSWITCH_1 = bypass).
- Mono: read in[0], write out[0], copy to out[1].
- Only #include "DaisyDuino.h" and "TerrariumDuino.h". Never #include "daisysp.h".
  Use only the DaisySP built into DaisyDuino. OnePole, LadderFilter, Wavefolder,
  GranularPlayer and Soap are NOT available. Use fonepole, Tone/ATone, MoogLadder or Fold.
- In the audio callback: no delay(), Serial, analogRead, new/malloc.
- Buffers over 64 KB: DSY_SDRAM_BSS, and clear them in setup().
- Feedback below 1 or soft-clipped. No NaN at any knob position (watch divisions).
- Float literals with f (0.5f). Smooth anything that sets a delay time.
- Keep it short. Use DaisySP modules instead of writing your own.
Give me the complete .ino.
```

## 3. Test

```
./daisyrig test effects/MyFuzz --board terrarium
```

Read the **Verdict** line in `REPORT.md`. If it says FAIL or WARN, paste the
whole report back to the AI with:

```text
Here is the daisyrig test report for that sketch. Fix every FAIL first, then
the WARNs, without changing what the effect does. Change only the lines that
need it. Give me the complete .ino.
```

Test again. Repeat until there are no FAILs. Listen to the WAVs in
`build/reports/MyFuzz/renders/` while you go; your ears are the final test.

When it passes, check that it fits on the Daisy (needs `./daisyrig setup --hw`
once):

```
./daisyrig test effects/MyFuzz --board terrarium --hw
```

## 4. Upload

Open `MyFuzz.ino` in the Arduino IDE, using the settings in
`docs/TERRARIUM.md`. Put the Seed in boot mode: hold BOOT, tap RESET, release
BOOT. Then click Upload.

## What the rig can't tell you

The test can't tell you whether the effect sounds good. It also can't hear
real analog noise, and its CPU figure is an estimate. If a pedal that passed
crackles or freezes on hardware, CPU is the most likely reason. Lower the
voice or tap count and test again.
