# daisyrig report: `TremDelayBuggy`

Board map **terrarium** · 48000 Hz · block 48 · 2 ch

## Verdict: FAIL  (3 fail, 6 warn)

- **FAIL** AddressSanitizer/UBSan reported problems in 18 run(s): examples/sketches/TremDelayBuggy/TremDelayBuggy.ino:41:24: runtime error: index 48000 out of bounds for type 'float [48000]'
- **FAIL** min/pluck_low_e: 167802 NaN / 288198 Inf from t=0.000s (+26 more)
- **FAIL** footswitch/pluck_low_e: callback left 79776 output samples unwritten (+8 more)
- **WARN** noon/hot_humbucker: 230 samples at/over full scale (peak 3.65 dBFS) (+24 more)
- **WARN** noon/sine_1k: 6 suspected click(s) at [1.0, 1.5005, 2.0, 2.5005] s (+13 more)
- **WARN** sweep/chord_strum: DC offset +0.0101 (+8 more)
- **WARN** footswitch/pluck_low_e: 12 heap allocations inside the audio callback (+8 more)
- **WARN** footswitch/pluck_low_e: Serial output inside the audio callback (+8 more)
- **WARN** Serial output inside the audio callback (slow/blocking on hardware) (+8 more)
- **INFO** noon/pluck_low_e: analogRead() called 23750x inside the callback (+70 more)
- **INFO** noon/palm_mute: still -34.2 dBFS 1.5 s after input stopped (long tail, feedback or self-oscillation) (+35 more)

## Verified (measured on the host build)

- Static RAM 188.3 KB (36.8% of 512 KB), SDRAM 0.0 KB
  - 187.5 KB `.bss` delay_buf
  - 0.7 KB `.bss` hw
  - 0.0 KB `.bss` lfo
  - 0.0 KB `.bss` fs1
  - 0.0 KB `.bss` write_pos
- Daisy build (arm-gcc, staged (daisyrig prototypes, no ctags)): flash 62.7 KB (48% of 128 KB), RAM 192.3 KB (37%)
- ASAN/UBSan: 18 run(s) with errors
- Determinism: bit-identical across runs
- Compiler warnings in sketch: 0

## Inferred (not measured on hardware)

- CPU: host p99 0.71% of block budget → Daisy estimate ~21.2% using factor 30.0 (uncalibrated)
- Click detection, tail and idle-noise flags are heuristics: listen at the flagged times.
- Memory sizes come from x86-64 objects; real ARM sizes differ slightly.

## Renders

| ctl | signal | peak dBFS | gain dB | clicks | clipped | tail→-60 s | end dBFS | THD % | alias dB | host p99 % |
|---|---|---|---|---|---|---|---|---|---|---|
| noon | pluck_low_e | -2.25 | 7.43 | None | 0 | None | -51.5 | - | - | 0.196 |
| noon | pluck_high | -3.45 | 7.56 | None | 0 | 0.35 | -51.1 | - | - | 0.209 |
| noon | chord_strum | -2.28 | 7.62 | None | 0 | 1.36 | -55.4 | - | - | 0.108 |
| noon | palm_mute | -0.31 | 6.81 | None | 0 | 1.024 | -34.2 | - | - | 0.178 |
| noon | legato_run | -0.53 | 6.18 | None | 0 | None | -29.6 | - | - | 0.281 |
| noon | bend | -3.32 | 7.12 | None | 0 | 0.728 | -60.3 | - | - | 0.267 |
| noon | hot_humbucker | 3.65 | 7.28 | None | 230 | None | -44.1 | - | - | 0.256 |
| noon | quiet_pick | -35.34 | 7.85 | None | 0 | 0.0 | -82.3 | - | - | 0.265 |
| noon | sine_1k | -7.19 | -0.94 | 6 | 0 | None | -26.5 | 0.0348 | -84.2 | 0.263 |
| noon | sine_3k5 | 1.84 | 3.24 | 5 | 5428 | None | -18.4 | 0.015 | -64.4 | 0.219 |
| noon | sine_100 | 7.54 | 10.18 | 6 | 109988 | None | -10.3 | 0.0102 | -210.7 | 0.194 |
| noon | sweep | 2.38 | 7.07 | 5 | 3562 | None | -20.3 | - | - | 1.114 |
| noon | impulse | -1.53 | 4.49 | None | 0 | 0.0 | -57.6 | - | - | 0.254 |
| noon | silence | -240.0 | - | 0 | 0 | 0.0 | -240.0 | - | - | 0.213 |
| noon | pink_noise | -0.03 | 6.33 | None | 0 | None | -25.6 | - | - | 0.184 |
| min | pluck_low_e | -240.0 | -210.8 | None | 0 | 0.0 | -240.0 | - | - | 0.229 |
| min | chord_strum | -240.0 | -211.02 | None | 0 | 0.0 | -240.0 | - | - | 0.169 |
| min | palm_mute | -240.0 | -214.48 | None | 0 | 0.0 | -240.0 | - | - | 0.075 |
| min | hot_humbucker | -240.0 | -217.28 | None | 0 | 0.0 | -240.0 | - | - | 0.226 |
| min | quiet_pick | -240.0 | -175.69 | None | 0 | 0.0 | -240.0 | - | - | 0.179 |
| min | sine_1k | -240.0 | -224.89 | 0 | 0 | 0.0 | -240.0 | 0.0 | 0.0 | 0.181 |
| min | sweep | -240.0 | -224.93 | 0 | 0 | 0.0 | -240.0 | - | - | 0.098 |
| min | impulse | -240.0 | -233.98 | None | 0 | 0.0 | -240.0 | - | - | 0.239 |
| min | silence | -240.0 | - | 0 | 0 | 0.0 | -240.0 | - | - | 0.193 |
| max | pluck_low_e | -8.19 | -0.45 | None | 0 | None | -25.2 | - | - | 0.259 |
| max | chord_strum | -5.61 | -0.06 | None | 0 | None | -30.0 | - | - | 0.229 |
| max | palm_mute | -5.95 | -1.83 | None | 0 | None | -23.2 | - | - | 0.222 |
| max | hot_humbucker | -1.05 | -0.53 | None | 0 | None | -21.5 | - | - | 0.391 |
| max | quiet_pick | -41.28 | -0.58 | None | 0 | 0.0 | -72.1 | - | - | 0.362 |
| max | sine_1k | -7.45 | -1.63 | 2 | 0 | None | -13.4 | 0.0001 | -134.5 | 0.371 |
| max | sweep | 1.35 | -0.12 | 1 | 100 | None | -11.1 | - | - | 0.178 |
| max | impulse | -9.83 | -9.81 | None | 0 | 0.0 | -240.0 | - | - | 0.34 |
| max | silence | -240.0 | - | 0 | 0 | 0.0 | -240.0 | - | - | 0.371 |
| sweep | pluck_low_e | 46.7 | 42.22 | None | 17246 | 0.434 | -56.9 | - | - | 0.19 |
| sweep | chord_strum | 50.55 | 43.29 | None | 15484 | 0.025 | -60.8 | - | - | 0.235 |
| sweep | palm_mute | 31.08 | 24.79 | None | 9588 | 0.372 | -32.0 | - | - | 0.206 |
| sweep | hot_humbucker | 56.9 | 44.03 | None | 24030 | None | -49.4 | - | - | 0.234 |
| sweep | quiet_pick | 8.73 | 40.01 | None | 230 | 0.0 | -93.7 | - | - | 0.255 |
| sweep | sine_1k | 54.1 | 39.91 | 58 | 51404 | None | -21.4 | 0.0518 | -82.6 | 0.239 |
| sweep | sweep | 53.64 | 39.08 | 339 | 91046 | None | -17.5 | - | - | 0.117 |
| sweep | impulse | 19.04 | 25.06 | None | 8 | 0.0 | -240.0 | - | - | 0.309 |
| sweep | silence | -240.0 | - | 0 | 0 | 0.0 | -240.0 | - | - | 0.271 |
| twiddle | pluck_low_e | 44.11 | 40.23 | None | 20188 | 0.267 | -16.7 | - | - | 0.229 |
| twiddle | chord_strum | 44.92 | 40.93 | None | 18598 | 0.094 | -22.4 | - | - | 0.261 |
| twiddle | palm_mute | 51.17 | 43.53 | None | 25688 | 0.266 | -12.0 | - | - | 0.149 |
| twiddle | hot_humbucker | 49.51 | 39.22 | None | 27866 | 0.418 | 1.1 | - | - | 0.271 |
| twiddle | quiet_pick | 6.5 | 38.69 | None | 108 | 0.0 | -42.5 | - | - | 0.248 |
| twiddle | sine_1k | 54.6 | 41.56 | 95 | 68060 | 0.95 | 19.9 | 1.5521 | -54.9 | 0.257 |
| twiddle | sweep | 60.05 | 43.86 | 147 | 135832 | 0.623 | 20.8 | - | - | 0.108 |
| twiddle | impulse | 1.58 | 7.6 | None | 2 | 0.0 | -240.0 | - | - | 0.282 |
| twiddle | silence | -240.0 | - | 0 | 0 | 0.0 | -240.0 | - | - | 0.247 |
| footswitch | pluck_low_e | -2.25 | 6.97 | None | 0 | None | -37.2 | - | - | 0.185 |
| footswitch | chord_strum | -2.28 | 7.24 | None | 0 | None | -37.7 | - | - | 0.176 |
| footswitch | palm_mute | -1.21 | 2.79 | None | 0 | 0.0 | -33.6 | - | - | 0.197 |
| footswitch | hot_humbucker | 3.65 | 6.35 | None | 230 | None | -29.4 | - | - | 0.186 |
| footswitch | quiet_pick | -35.34 | 6.71 | None | 0 | 0.156 | -68.8 | - | - | 0.267 |
| footswitch | sine_1k | -7.19 | -1.25 | 6 | 0 | None | -23.2 | 0.0124 | -98.3 | 0.207 |
| footswitch | sweep | 1.66 | 4.1 | 4 | 1132 | None | -20.4 | - | - | 0.109 |
| footswitch | impulse | -1.53 | 4.49 | None | 0 | 0.0 | -45.4 | - | - | 1.286 |
| footswitch | silence | -240.0 | - | 0 | 0 | 0.0 | -240.0 | - | - | 0.214 |
| toggles | pluck_low_e | -2.25 | 7.43 | None | 0 | None | -51.5 | - | - | 0.183 |
| toggles | chord_strum | -2.28 | 7.62 | None | 0 | 1.36 | -55.4 | - | - | 0.193 |
| toggles | palm_mute | -0.31 | 6.81 | None | 0 | 1.024 | -34.2 | - | - | 0.104 |
| toggles | hot_humbucker | 3.65 | 7.28 | None | 230 | None | -44.1 | - | - | 0.203 |
| toggles | quiet_pick | -35.34 | 7.85 | None | 0 | 0.0 | -82.3 | - | - | 0.196 |
| toggles | sine_1k | -7.19 | -0.94 | 6 | 0 | None | -26.5 | 0.0348 | -84.2 | 0.219 |
| toggles | sweep | 2.38 | 7.07 | 5 | 3562 | None | -20.3 | - | - | 0.098 |
| toggles | impulse | -1.53 | 4.49 | None | 0 | 0.0 | -57.6 | - | - | 1.163 |
| toggles | silence | -240.0 | - | 0 | 0 | 0.0 | -240.0 | - | - | 0.181 |
| adc_noise | pluck_low_e | -2.25 | 7.63 | None | 0 | 1.394 | -52.6 | - | - | 0.191 |
| adc_noise | silence | -240.0 | - | 0 | 0 | 0.0 | -240.0 | - | - | 0.421 |
