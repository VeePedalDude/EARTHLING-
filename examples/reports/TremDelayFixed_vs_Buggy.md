# daisyrig report: `TremDelayFixed`

Board map **terrarium** · 48000 Hz · block 48 · 2 ch

## Verdict: WARN  (0 fail, 1 warn)

- **WARN** twiddle/sine_1k: 16 suspected click(s) at [0.6555, 1.1776, 2.9208, 3.2323] s (+1 more)
- **INFO** noon/palm_mute: still -45.0 dBFS 1.5 s after input stopped (long tail, feedback or self-oscillation) (+26 more)

## Verified (measured on the host build)

- Static RAM 0.9 KB (0.2% of 512 KB), SDRAM 187.5 KB
  - 187.5 KB `sdram_bss` delay_buf
  - 0.7 KB `.bss` hw
  - 0.0 KB `.bss` lfo
  - 0.0 KB `.bss` fs1
  - 0.0 KB `.bss` write_pos
- Daisy build (arm-gcc, staged (daisyrig prototypes, no ctags)): flash 55.9 KB (43% of 128 KB), RAM 4.3 KB (0%)
- ASAN/UBSan: clean
- Determinism: bit-identical across runs
- Compiler warnings in sketch: 0

## Inferred (not measured on hardware)

- CPU: host p99 0.17% of block budget → Daisy estimate ~5.1% using factor 30.0 (uncalibrated)
- Click detection, tail and idle-noise flags are heuristics: listen at the flagged times.
- Memory sizes come from x86-64 objects; real ARM sizes differ slightly.

## Renders

| ctl | signal | peak dBFS | gain dB | clicks | clipped | tail→-60 s | end dBFS | THD % | alias dB | host p99 % |
|---|---|---|---|---|---|---|---|---|---|---|
| noon | pluck_low_e | -10.78 | 1.11 | None | 0 | 0.758 | -64.6 | - | - | 0.11 |
| noon | pluck_high | -11.59 | 1.21 | None | 0 | 0.208 | -63.9 | - | - | 0.129 |
| noon | chord_strum | -11.45 | 0.89 | None | 0 | 0.419 | -69.8 | - | - | 0.134 |
| noon | palm_mute | -9.9 | -0.48 | None | 0 | 0.095 | -45.0 | - | - | 0.133 |
| noon | legato_run | -10.95 | -0.36 | None | 0 | None | -39.8 | - | - | 0.113 |
| noon | bend | -11.47 | 1.22 | None | 0 | 0.032 | -74.3 | - | - | 0.122 |
| noon | hot_humbucker | -7.73 | -0.73 | None | 0 | 1.377 | -56.8 | - | - | 0.07 |
| noon | quiet_pick | -39.53 | 3.27 | None | 0 | 0.0 | -95.3 | - | - | 0.11 |
| noon | sine_1k | -14.02 | -5.38 | 0 | 0 | None | -33.5 | 7.0073 | -79.2 | 0.076 |
| noon | sine_3k5 | -10.1 | -6.0 | 0 | 0 | None | -29.5 | 9.1122 | -42.9 | 1.288 |
| noon | sine_100 | -7.55 | -2.33 | 0 | 0 | None | -26.3 | 16.1382 | -197.8 | 0.285 |
| noon | sweep | -9.97 | -1.61 | 0 | 0 | None | -39.6 | - | - | 0.106 |
| noon | impulse | -10.46 | -4.44 | None | 0 | 0.0 | -74.0 | - | - | 1.197 |
| noon | silence | -240.0 | - | 0 | 0 | 0.0 | -240.0 | - | - | 1.244 |
| noon | pink_noise | -10.49 | -0.75 | None | 0 | None | -36.1 | - | - | 0.11 |
| min | pluck_low_e | -240.0 | -210.8 | None | 0 | 0.0 | -240.0 | - | - | 3.018 |
| min | chord_strum | -240.0 | -211.02 | None | 0 | 0.0 | -240.0 | - | - | 3.497 |
| min | palm_mute | -240.0 | -214.48 | None | 0 | 0.0 | -240.0 | - | - | 3.597 |
| min | hot_humbucker | -240.0 | -217.28 | None | 0 | 0.0 | -240.0 | - | - | 2.827 |
| min | quiet_pick | -240.0 | -175.69 | None | 0 | 0.0 | -240.0 | - | - | 1.997 |
| min | sine_1k | -240.0 | -224.89 | 0 | 0 | 0.0 | -240.0 | 0.0 | 0.0 | 2.157 |
| min | sweep | -240.0 | -224.93 | 0 | 0 | 0.0 | -240.0 | - | - | 2.165 |
| min | impulse | -240.0 | -233.98 | None | 0 | 0.0 | -240.0 | - | - | 1.178 |
| min | silence | -240.0 | - | 0 | 0 | 0.0 | -240.0 | - | - | 1.739 |
| max | pluck_low_e | -6.89 | 4.7 | None | 0 | None | -25.5 | - | - | 0.126 |
| max | chord_strum | -6.59 | 4.64 | None | 0 | None | -28.8 | - | - | 0.076 |
| max | palm_mute | -4.93 | 2.43 | None | 0 | None | -23.6 | - | - | 0.165 |
| max | hot_humbucker | -2.68 | 2.84 | None | 0 | None | -25.6 | - | - | 0.1 |
| max | quiet_pick | -35.17 | 6.89 | None | 0 | 0.0 | -67.2 | - | - | 0.076 |
| max | sine_1k | -8.37 | 0.56 | 0 | 0 | None | -17.0 | 4.0208 | -75.6 | 0.083 |
| max | sweep | -4.57 | 1.46 | 0 | 0 | None | -17.3 | - | - | 0.089 |
| max | impulse | -10.58 | -6.71 | None | 0 | 0.0 | -240.0 | - | - | 0.125 |
| max | silence | -240.0 | - | 0 | 0 | 0.0 | -240.0 | - | - | 0.128 |
| sweep | pluck_low_e | -29.62 | -14.46 | None | 0 | 0.448 | -52.6 | - | - | 0.1 |
| sweep | chord_strum | -31.84 | -17.35 | None | 0 | 0.01 | -57.2 | - | - | 0.186 |
| sweep | palm_mute | -8.69 | -2.72 | None | 0 | 0.006 | -28.4 | - | - | 0.142 |
| sweep | hot_humbucker | -25.41 | -15.11 | None | 0 | None | -44.8 | - | - | 0.085 |
| sweep | quiet_pick | -64.33 | -15.69 | None | 0 | 0.0 | -89.5 | - | - | 0.109 |
| sweep | sine_1k | -8.91 | -4.46 | 0 | 0 | None | -19.8 | 1.9811 | -91.9 | 0.106 |
| sweep | sweep | -6.49 | -2.76 | 0 | 0 | None | -18.8 | - | - | 0.111 |
| sweep | impulse | -26.52 | -24.18 | None | 0 | 0.0 | -240.0 | - | - | 1.155 |
| sweep | silence | -240.0 | - | 0 | 0 | 0.0 | -240.0 | - | - | 0.113 |
| twiddle | pluck_low_e | -4.81 | 3.51 | None | 0 | 0.035 | -65.6 | - | - | 0.082 |
| twiddle | chord_strum | -3.02 | 3.6 | None | 0 | 0.0 | -62.8 | - | - | 0.104 |
| twiddle | palm_mute | -3.87 | 1.54 | None | 0 | 0.016 | -43.5 | - | - | 0.116 |
| twiddle | hot_humbucker | -0.99 | 2.12 | None | 0 | 0.018 | -46.5 | - | - | 0.087 |
| twiddle | quiet_pick | -34.71 | 6.06 | None | 0 | 0.0 | -86.9 | - | - | 0.104 |
| twiddle | sine_1k | -2.96 | 0.3 | 16 | 0 | 0.988 | -32.3 | 10.8503 | -40.1 | 0.085 |
| twiddle | sweep | -3.23 | 0.2 | 50 | 0 | 0.652 | -30.0 | - | - | 0.075 |
| twiddle | impulse | -7.65 | -15.34 | None | 0 | 0.0 | -52.4 | - | - | 0.132 |
| twiddle | silence | -240.0 | - | 0 | 0 | 0.0 | -240.0 | - | - | 0.082 |
| footswitch | pluck_low_e | -10.78 | 0.22 | None | 0 | 0.758 | -64.6 | - | - | 1.701 |
| footswitch | chord_strum | -11.45 | 0.09 | None | 0 | 0.419 | -69.8 | - | - | 0.628 |
| footswitch | palm_mute | -6.02 | -0.24 | None | 0 | 0.095 | -45.0 | - | - | 0.744 |
| footswitch | hot_humbucker | -7.73 | -1.61 | None | 0 | 1.377 | -56.8 | - | - | 0.592 |
| footswitch | quiet_pick | -39.53 | 2.41 | None | 0 | 0.0 | -95.3 | - | - | 0.7 |
| footswitch | sine_1k | -12.04 | -0.93 | 0 | 0 | None | -33.5 | 0.1706 | -110.7 | 0.552 |
| footswitch | sweep | -10.09 | -0.83 | 0 | 0 | None | -39.6 | - | - | 1.313 |
| footswitch | impulse | -10.46 | -4.44 | None | 0 | 0.0 | -74.0 | - | - | 0.212 |
| footswitch | silence | -240.0 | - | 0 | 0 | 0.0 | -240.0 | - | - | 0.123 |
| toggles | pluck_low_e | -10.78 | 1.11 | None | 0 | 0.758 | -64.6 | - | - | 0.171 |
| toggles | chord_strum | -11.45 | 0.89 | None | 0 | 0.419 | -69.8 | - | - | 0.116 |
| toggles | palm_mute | -9.9 | -0.48 | None | 0 | 0.095 | -45.0 | - | - | 0.14 |
| toggles | hot_humbucker | -7.73 | -0.73 | None | 0 | 1.377 | -56.8 | - | - | 0.129 |
| toggles | quiet_pick | -39.53 | 3.27 | None | 0 | 0.0 | -95.3 | - | - | 0.11 |
| toggles | sine_1k | -14.02 | -5.38 | 0 | 0 | None | -33.5 | 7.0073 | -79.2 | 0.13 |
| toggles | sweep | -9.97 | -1.61 | 0 | 0 | None | -39.6 | - | - | 0.112 |
| toggles | impulse | -10.46 | -4.44 | None | 0 | 0.0 | -74.0 | - | - | 1.335 |
| toggles | silence | -240.0 | - | 0 | 0 | 0.0 | -240.0 | - | - | 0.11 |
| adc_noise | pluck_low_e | -10.81 | 0.99 | None | 0 | 0.465 | -66.9 | - | - | 0.931 |
| adc_noise | silence | -240.0 | - | 0 | 0 | 0.0 | -240.0 | - | - | 0.13 |

## Before → after

Findings 11 → 2 · host CPU p99 0.707 → 0.171 %

| ctl | signal | metric | before | after |
|---|---|---|---|---|
| noon | pluck_low_e | peak_dbfs | -2.25 | -10.78 |
| noon | pluck_low_e | rms_dbfs | -23.75 | -30.08 |
| noon | pluck_low_e | gain_db | 7.43 | 1.11 |
| noon | pluck_low_e | end_rms_dbfs | -51.5 | -64.6 |
| noon | pluck_low_e | centroid_hz | 2186.9 | 2155.0 |
| noon | pluck_high | peak_dbfs | -3.45 | -11.59 |
| noon | pluck_high | rms_dbfs | -29.01 | -35.41 |
| noon | pluck_high | gain_db | 7.56 | 1.21 |
| noon | pluck_high | end_rms_dbfs | -51.1 | -63.9 |
| noon | pluck_high | centroid_hz | 2120.1 | 2069.6 |
| noon | chord_strum | peak_dbfs | -2.28 | -11.45 |
| noon | chord_strum | rms_dbfs | -23.47 | -30.21 |
| noon | chord_strum | gain_db | 7.62 | 0.89 |
| noon | chord_strum | end_rms_dbfs | -55.4 | -69.8 |
| noon | chord_strum | centroid_hz | 1812.7 | 1717.9 |
| noon | palm_mute | peak_dbfs | -0.31 | -9.9 |
| noon | palm_mute | rms_dbfs | -20.19 | -27.62 |
| noon | palm_mute | gain_db | 6.81 | -0.48 |
| noon | palm_mute | end_rms_dbfs | -34.2 | -45.0 |
| noon | palm_mute | centroid_hz | 736.4 | 732.6 |
| noon | legato_run | peak_dbfs | -0.53 | -10.95 |
| noon | legato_run | rms_dbfs | -18.2 | -25.03 |
| noon | legato_run | gain_db | 6.18 | -0.36 |
| noon | legato_run | end_rms_dbfs | -29.6 | -39.8 |
| noon | legato_run | centroid_hz | 2209.5 | 2208.3 |
| noon | bend | peak_dbfs | -3.32 | -11.47 |
| noon | bend | rms_dbfs | -27.29 | -33.19 |
| noon | bend | gain_db | 7.12 | 1.22 |
| noon | bend | end_rms_dbfs | -60.3 | -74.3 |
| noon | bend | centroid_hz | 1517.8 | 1392.7 |
| noon | hot_humbucker | peak_dbfs | 3.65 | -7.73 |
| noon | hot_humbucker | rms_dbfs | -17.77 | -25.79 |
| noon | hot_humbucker | gain_db | 7.28 | -0.73 |
| noon | hot_humbucker | clipped_samples | 230 | 0 |
| noon | hot_humbucker | end_rms_dbfs | -44.1 | -56.8 |
| noon | hot_humbucker | centroid_hz | 1956.0 | 1843.9 |
| noon | quiet_pick | peak_dbfs | -35.34 | -39.53 |
| noon | quiet_pick | rms_dbfs | -58.76 | -63.36 |
| noon | quiet_pick | gain_db | 7.85 | 3.27 |
| noon | quiet_pick | end_rms_dbfs | -82.3 | -95.3 |
| noon | quiet_pick | centroid_hz | 1877.7 | 1916.4 |
| noon | sine_1k | peak_dbfs | -7.19 | -14.02 |
| noon | sine_1k | rms_dbfs | -17.47 | -22.08 |
| noon | sine_1k | gain_db | -0.94 | -5.38 |
| noon | sine_1k | click_events | 6 | 0 |
| noon | sine_1k | end_rms_dbfs | -26.5 | -33.5 |
| noon | sine_1k | thd_pct | 0.0348 | 7.0073 |
| noon | sine_1k | alias_db | -84.2 | -79.2 |
| noon | sine_1k | centroid_hz | 999.9 | 1006.9 |
| noon | sine_3k5 | peak_dbfs | 1.84 | -10.1 |
| noon | sine_3k5 | rms_dbfs | -7.73 | -16.95 |
| noon | sine_3k5 | gain_db | 3.24 | -6.0 |
| noon | sine_3k5 | click_events | 5 | 0 |
| noon | sine_3k5 | clipped_samples | 5428 | 0 |
| noon | sine_3k5 | end_rms_dbfs | -18.4 | -29.5 |
| noon | sine_3k5 | thd_pct | 0.015 | 9.1122 |
| noon | sine_3k5 | alias_db | -64.4 | -42.9 |
| noon | sine_3k5 | centroid_hz | 3500.1 | 3560.1 |
| noon | sine_100 | peak_dbfs | 7.54 | -7.55 |
| noon | sine_100 | rms_dbfs | -0.54 | -13.45 |
| noon | sine_100 | gain_db | 10.18 | -2.33 |
| noon | sine_100 | click_events | 6 | 0 |
| noon | sine_100 | clipped_samples | 109988 | 0 |
| noon | sine_100 | end_rms_dbfs | -10.3 | -26.3 |
| noon | sine_100 | thd_pct | 0.0102 | 16.1382 |
| noon | sine_100 | alias_db | -210.7 | -197.8 |
| noon | sine_100 | centroid_hz | 99.7 | 105.0 |
| noon | sweep | peak_dbfs | 2.38 | -9.97 |
| noon | sweep | rms_dbfs | -8.95 | -17.86 |
| noon | sweep | gain_db | 7.07 | -1.61 |
| noon | sweep | click_events | 5 | 0 |
| noon | sweep | clipped_samples | 3562 | 0 |
| noon | sweep | end_rms_dbfs | -20.3 | -39.6 |
| noon | sweep | centroid_hz | 2016.8 | 2288.7 |
| noon | impulse | peak_dbfs | -1.53 | -10.46 |
| noon | impulse | rms_dbfs | -47.32 | -57.99 |
| noon | impulse | gain_db | 4.49 | -4.44 |
| noon | impulse | end_rms_dbfs | -57.6 | -74.0 |
| noon | pink_noise | peak_dbfs | -0.03 | -10.49 |
| noon | pink_noise | rms_dbfs | -15.45 | -22.79 |
| noon | pink_noise | gain_db | 6.33 | -0.75 |
| noon | pink_noise | end_rms_dbfs | -25.6 | -36.1 |
| noon | pink_noise | centroid_hz | 3223.3 | 2755.6 |
| max | pluck_low_e | peak_dbfs | -8.19 | -6.89 |
| max | pluck_low_e | rms_dbfs | -28.75 | -25.22 |
| max | pluck_low_e | gain_db | -0.45 | 4.7 |
| max | pluck_low_e | end_rms_dbfs | -25.2 | -25.5 |
| max | pluck_low_e | centroid_hz | 2219.7 | 2166.7 |
| max | chord_strum | peak_dbfs | -5.61 | -6.59 |
| max | chord_strum | rms_dbfs | -28.43 | -25.26 |
| max | chord_strum | gain_db | -0.06 | 4.64 |
| max | chord_strum | end_rms_dbfs | -30.0 | -28.8 |
| max | chord_strum | centroid_hz | 1900.2 | 1806.5 |
| max | palm_mute | peak_dbfs | -5.95 | -4.93 |
| max | palm_mute | rms_dbfs | -26.19 | -23.41 |
| max | palm_mute | gain_db | -1.83 | 2.43 |
| max | palm_mute | end_rms_dbfs | -23.2 | -23.6 |
| max | palm_mute | centroid_hz | 694.6 | 704.5 |
| max | hot_humbucker | peak_dbfs | -1.05 | -2.68 |
| max | hot_humbucker | rms_dbfs | -23.17 | -21.28 |
| max | hot_humbucker | gain_db | -0.53 | 2.84 |
| max | hot_humbucker | end_rms_dbfs | -21.5 | -25.6 |
| max | hot_humbucker | centroid_hz | 2011.4 | 1897.5 |
| max | quiet_pick | peak_dbfs | -41.28 | -35.17 |
| max | quiet_pick | rms_dbfs | -65.24 | -58.47 |
| max | quiet_pick | gain_db | -0.58 | 6.89 |
| max | quiet_pick | end_rms_dbfs | -72.1 | -67.2 |
| max | quiet_pick | centroid_hz | 2000.7 | 1970.3 |
| max | sine_1k | peak_dbfs | -7.45 | -8.37 |
| max | sine_1k | rms_dbfs | -15.67 | -15.45 |
| max | sine_1k | gain_db | -1.63 | 0.56 |
| max | sine_1k | click_events | 2 | 0 |
| max | sine_1k | end_rms_dbfs | -13.4 | -17.0 |
| max | sine_1k | thd_pct | 0.0001 | 4.0208 |
| max | sine_1k | alias_db | -134.5 | -75.6 |
| max | sine_1k | centroid_hz | 1000.0 | 1004.6 |
| max | sweep | peak_dbfs | 1.35 | -4.57 |
| max | sweep | rms_dbfs | -14.18 | -14.22 |
| max | sweep | gain_db | -0.12 | 1.46 |
| max | sweep | click_events | 1 | 0 |
| max | sweep | clipped_samples | 100 | 0 |
| max | sweep | end_rms_dbfs | -11.1 | -17.3 |
| max | sweep | centroid_hz | 1083.4 | 1808.3 |
| max | impulse | peak_dbfs | -9.83 | -10.58 |
| max | impulse | rms_dbfs | -58.12 | -57.41 |
| max | impulse | gain_db | -9.81 | -6.71 |
| sweep | pluck_low_e | peak_dbfs | 46.7 | -29.62 |
| sweep | pluck_low_e | rms_dbfs | 11.02 | -45.33 |
| sweep | pluck_low_e | gain_db | 42.22 | -14.46 |
| sweep | pluck_low_e | clipped_samples | 17246 | 0 |
| sweep | pluck_low_e | end_rms_dbfs | -56.9 | -52.6 |
| sweep | pluck_low_e | centroid_hz | 1996.0 | 962.2 |
| sweep | chord_strum | peak_dbfs | 50.55 | -31.84 |
| sweep | chord_strum | rms_dbfs | 12.19 | -48.21 |
| sweep | chord_strum | gain_db | 43.29 | -17.35 |
| sweep | chord_strum | clipped_samples | 15484 | 0 |
| sweep | chord_strum | end_rms_dbfs | -60.8 | -57.2 |
| sweep | chord_strum | centroid_hz | 1518.6 | 743.2 |
| sweep | palm_mute | peak_dbfs | 31.08 | -8.69 |
| sweep | palm_mute | rms_dbfs | -2.72 | -27.8 |
| sweep | palm_mute | gain_db | 24.79 | -2.72 |
| sweep | palm_mute | clipped_samples | 9588 | 0 |
| sweep | palm_mute | end_rms_dbfs | -32.0 | -28.4 |
| sweep | palm_mute | centroid_hz | 529.3 | 669.7 |
| sweep | hot_humbucker | peak_dbfs | 56.9 | -25.41 |
| sweep | hot_humbucker | rms_dbfs | 18.96 | -39.55 |
| sweep | hot_humbucker | gain_db | 44.03 | -15.11 |
| sweep | hot_humbucker | clipped_samples | 24030 | 0 |
| sweep | hot_humbucker | end_rms_dbfs | -49.4 | -44.8 |
| sweep | hot_humbucker | centroid_hz | 1399.3 | 713.4 |
| sweep | quiet_pick | peak_dbfs | 8.73 | -64.33 |
| sweep | quiet_pick | rms_dbfs | -26.63 | -81.99 |
| sweep | quiet_pick | gain_db | 40.01 | -15.69 |
| sweep | quiet_pick | clipped_samples | 230 | 0 |
| sweep | quiet_pick | end_rms_dbfs | -93.7 | -89.5 |
| sweep | quiet_pick | centroid_hz | 1668.5 | 503.1 |
| sweep | sine_1k | peak_dbfs | 54.1 | -8.91 |
| sweep | sine_1k | rms_dbfs | 22.07 | -19.08 |
| sweep | sine_1k | gain_db | 39.91 | -4.46 |
| sweep | sine_1k | click_events | 58 | 0 |
| sweep | sine_1k | clipped_samples | 51404 | 0 |
| sweep | sine_1k | end_rms_dbfs | -21.4 | -19.8 |
| sweep | sine_1k | thd_pct | 0.0518 | 1.9811 |
| sweep | sine_1k | alias_db | -82.6 | -91.9 |
| sweep | sine_1k | centroid_hz | 991.6 | 875.7 |
| sweep | sweep | peak_dbfs | 53.64 | -6.49 |
| sweep | sweep | rms_dbfs | 22.7 | -17.91 |
| sweep | sweep | gain_db | 39.08 | -2.76 |
| sweep | sweep | click_events | 339 | 0 |
| sweep | sweep | clipped_samples | 91046 | 0 |
| sweep | sweep | end_rms_dbfs | -17.5 | -18.8 |
| sweep | sweep | centroid_hz | 97.0 | 4060.6 |
| sweep | impulse | peak_dbfs | 19.04 | -26.52 |
| sweep | impulse | rms_dbfs | -29.37 | -72.46 |
| sweep | impulse | gain_db | 25.06 | -24.18 |
| sweep | impulse | clipped_samples | 8 | 0 |
| twiddle | pluck_low_e | peak_dbfs | 44.11 | -4.81 |
| twiddle | pluck_low_e | rms_dbfs | 9.15 | -27.68 |
| twiddle | pluck_low_e | gain_db | 40.23 | 3.51 |
| twiddle | pluck_low_e | clipped_samples | 20188 | 0 |
| twiddle | pluck_low_e | end_rms_dbfs | -16.7 | -65.6 |
| twiddle | pluck_low_e | centroid_hz | 2193.1 | 5117.1 |
| twiddle | chord_strum | peak_dbfs | 44.92 | -3.02 |
| twiddle | chord_strum | rms_dbfs | 9.87 | -27.5 |
| twiddle | chord_strum | gain_db | 40.93 | 3.6 |
| twiddle | chord_strum | clipped_samples | 18598 | 0 |
| twiddle | chord_strum | end_rms_dbfs | -22.4 | -62.8 |
| twiddle | chord_strum | centroid_hz | 1705.4 | 4286.2 |
| twiddle | palm_mute | peak_dbfs | 51.17 | -3.87 |
| twiddle | palm_mute | rms_dbfs | 16.59 | -25.58 |
| twiddle | palm_mute | gain_db | 43.53 | 1.54 |
| twiddle | palm_mute | clipped_samples | 25688 | 0 |
| twiddle | palm_mute | end_rms_dbfs | -12.0 | -43.5 |
| twiddle | palm_mute | centroid_hz | 925.8 | 1691.7 |
| twiddle | hot_humbucker | peak_dbfs | 49.51 | -0.99 |
| twiddle | hot_humbucker | rms_dbfs | 14.23 | -22.95 |
| twiddle | hot_humbucker | gain_db | 39.22 | 2.12 |
| twiddle | hot_humbucker | clipped_samples | 27866 | 0 |
| twiddle | hot_humbucker | end_rms_dbfs | 1.1 | -46.5 |
| twiddle | hot_humbucker | centroid_hz | 1823.8 | 4197.5 |
| twiddle | quiet_pick | peak_dbfs | 6.5 | -34.71 |
| twiddle | quiet_pick | rms_dbfs | -27.87 | -60.56 |
| twiddle | quiet_pick | gain_db | 38.69 | 6.06 |
| twiddle | quiet_pick | clipped_samples | 108 | 0 |
| twiddle | quiet_pick | end_rms_dbfs | -42.5 | -86.9 |
| twiddle | quiet_pick | centroid_hz | 1646.7 | 5174.9 |
| twiddle | sine_1k | peak_dbfs | 54.6 | -2.96 |
| twiddle | sine_1k | rms_dbfs | 25.54 | -16.94 |
| twiddle | sine_1k | gain_db | 41.56 | 0.3 |
| twiddle | sine_1k | click_events | 95 | 16 |
| twiddle | sine_1k | clipped_samples | 68060 | 0 |
| twiddle | sine_1k | end_rms_dbfs | 19.9 | -32.3 |
| twiddle | sine_1k | thd_pct | 1.5521 | 10.8503 |
| twiddle | sine_1k | alias_db | -54.9 | -40.1 |
| twiddle | sine_1k | centroid_hz | 1107.1 | 2228.7 |
| twiddle | sweep | peak_dbfs | 60.05 | -3.23 |
| twiddle | sweep | rms_dbfs | 28.14 | -16.0 |
| twiddle | sweep | gain_db | 43.86 | 0.2 |
| twiddle | sweep | click_events | 147 | 50 |
| twiddle | sweep | clipped_samples | 135832 | 0 |
| twiddle | sweep | end_rms_dbfs | 20.8 | -30.0 |
| twiddle | sweep | centroid_hz | 3628.1 | 2919.3 |
| twiddle | impulse | peak_dbfs | 1.58 | -7.65 |
| twiddle | impulse | rms_dbfs | -47.68 | -53.79 |
| twiddle | impulse | gain_db | 7.6 | -15.34 |
| twiddle | impulse | clipped_samples | 2 | 0 |
| twiddle | impulse | end_rms_dbfs | -240.0 | -52.4 |
| footswitch | pluck_low_e | peak_dbfs | -2.25 | -10.78 |
| footswitch | pluck_low_e | rms_dbfs | -23.87 | -30.97 |
| footswitch | pluck_low_e | gain_db | 6.97 | 0.22 |
| footswitch | pluck_low_e | end_rms_dbfs | -37.2 | -64.6 |
| footswitch | pluck_low_e | centroid_hz | 2252.2 | 2230.0 |
| footswitch | chord_strum | peak_dbfs | -2.28 | -11.45 |
| footswitch | chord_strum | rms_dbfs | -23.52 | -31.01 |
| footswitch | chord_strum | gain_db | 7.24 | 0.09 |
| footswitch | chord_strum | end_rms_dbfs | -37.7 | -69.8 |
| footswitch | chord_strum | centroid_hz | 1838.2 | 1781.8 |
| footswitch | palm_mute | peak_dbfs | -1.21 | -6.02 |
| footswitch | palm_mute | rms_dbfs | -23.31 | -27.4 |
| footswitch | palm_mute | gain_db | 2.79 | -0.24 |
| footswitch | palm_mute | end_rms_dbfs | -33.6 | -45.0 |
| footswitch | palm_mute | centroid_hz | 678.7 | 762.2 |
| footswitch | hot_humbucker | peak_dbfs | 3.65 | -7.73 |
| footswitch | hot_humbucker | rms_dbfs | -17.82 | -26.67 |
| footswitch | hot_humbucker | gain_db | 6.35 | -1.61 |
| footswitch | hot_humbucker | clipped_samples | 230 | 0 |
| footswitch | hot_humbucker | end_rms_dbfs | -29.4 | -56.8 |
| footswitch | hot_humbucker | centroid_hz | 1978.5 | 1913.4 |
| footswitch | quiet_pick | peak_dbfs | -35.34 | -39.53 |
| footswitch | quiet_pick | rms_dbfs | -58.99 | -64.22 |
| footswitch | quiet_pick | gain_db | 6.71 | 2.41 |
| footswitch | quiet_pick | end_rms_dbfs | -68.8 | -95.3 |
| footswitch | quiet_pick | centroid_hz | 1986.6 | 2014.4 |
| footswitch | sine_1k | peak_dbfs | -7.19 | -12.04 |
| footswitch | sine_1k | rms_dbfs | -16.14 | -18.34 |
| footswitch | sine_1k | gain_db | -1.25 | -0.93 |
| footswitch | sine_1k | click_events | 6 | 0 |
| footswitch | sine_1k | end_rms_dbfs | -23.2 | -33.5 |
| footswitch | sine_1k | thd_pct | 0.0124 | 0.1706 |
| footswitch | sine_1k | alias_db | -98.3 | -110.7 |
| footswitch | sine_1k | centroid_hz | 999.9 | 1001.1 |
| footswitch | sweep | peak_dbfs | 1.66 | -10.09 |
| footswitch | sweep | rms_dbfs | -11.4 | -17.09 |
| footswitch | sweep | gain_db | 4.1 | -0.83 |
| footswitch | sweep | click_events | 4 | 0 |
| footswitch | sweep | clipped_samples | 1132 | 0 |
| footswitch | sweep | end_rms_dbfs | -20.4 | -39.6 |
| footswitch | sweep | centroid_hz | 2857.8 | 2198.9 |
| footswitch | impulse | peak_dbfs | -1.53 | -10.46 |
| footswitch | impulse | rms_dbfs | -47.82 | -59.47 |
| footswitch | impulse | gain_db | 4.49 | -4.44 |
| footswitch | impulse | end_rms_dbfs | -45.4 | -74.0 |
| toggles | pluck_low_e | peak_dbfs | -2.25 | -10.78 |
| toggles | pluck_low_e | rms_dbfs | -23.75 | -30.08 |
| toggles | pluck_low_e | gain_db | 7.43 | 1.11 |
| toggles | pluck_low_e | end_rms_dbfs | -51.5 | -64.6 |
| toggles | pluck_low_e | centroid_hz | 2186.9 | 2155.0 |
| toggles | chord_strum | peak_dbfs | -2.28 | -11.45 |
| toggles | chord_strum | rms_dbfs | -23.47 | -30.21 |
| toggles | chord_strum | gain_db | 7.62 | 0.89 |
| toggles | chord_strum | end_rms_dbfs | -55.4 | -69.8 |
| toggles | chord_strum | centroid_hz | 1812.7 | 1717.9 |
| toggles | palm_mute | peak_dbfs | -0.31 | -9.9 |
| toggles | palm_mute | rms_dbfs | -20.19 | -27.62 |
| toggles | palm_mute | gain_db | 6.81 | -0.48 |
| toggles | palm_mute | end_rms_dbfs | -34.2 | -45.0 |
| toggles | palm_mute | centroid_hz | 736.4 | 732.6 |
| toggles | hot_humbucker | peak_dbfs | 3.65 | -7.73 |
| toggles | hot_humbucker | rms_dbfs | -17.77 | -25.79 |
| toggles | hot_humbucker | gain_db | 7.28 | -0.73 |
| toggles | hot_humbucker | clipped_samples | 230 | 0 |
| toggles | hot_humbucker | end_rms_dbfs | -44.1 | -56.8 |
| toggles | hot_humbucker | centroid_hz | 1956.0 | 1843.9 |
| toggles | quiet_pick | peak_dbfs | -35.34 | -39.53 |
| toggles | quiet_pick | rms_dbfs | -58.76 | -63.36 |
| toggles | quiet_pick | gain_db | 7.85 | 3.27 |
| toggles | quiet_pick | end_rms_dbfs | -82.3 | -95.3 |
| toggles | quiet_pick | centroid_hz | 1877.7 | 1916.4 |
| toggles | sine_1k | peak_dbfs | -7.19 | -14.02 |
| toggles | sine_1k | rms_dbfs | -17.47 | -22.08 |
| toggles | sine_1k | gain_db | -0.94 | -5.38 |
| toggles | sine_1k | click_events | 6 | 0 |
| toggles | sine_1k | end_rms_dbfs | -26.5 | -33.5 |
| toggles | sine_1k | thd_pct | 0.0348 | 7.0073 |
| toggles | sine_1k | alias_db | -84.2 | -79.2 |
| toggles | sine_1k | centroid_hz | 999.9 | 1006.9 |
| toggles | sweep | peak_dbfs | 2.38 | -9.97 |
| toggles | sweep | rms_dbfs | -8.95 | -17.86 |
| toggles | sweep | gain_db | 7.07 | -1.61 |
| toggles | sweep | click_events | 5 | 0 |
| toggles | sweep | clipped_samples | 3562 | 0 |
| toggles | sweep | end_rms_dbfs | -20.3 | -39.6 |
| toggles | sweep | centroid_hz | 2016.8 | 2288.7 |
| toggles | impulse | peak_dbfs | -1.53 | -10.46 |
| toggles | impulse | rms_dbfs | -47.32 | -57.99 |
| toggles | impulse | gain_db | 4.49 | -4.44 |
| toggles | impulse | end_rms_dbfs | -57.6 | -74.0 |
| adc_noise | pluck_low_e | peak_dbfs | -2.25 | -10.81 |
| adc_noise | pluck_low_e | rms_dbfs | -23.56 | -30.21 |
| adc_noise | pluck_low_e | gain_db | 7.63 | 0.99 |
| adc_noise | pluck_low_e | end_rms_dbfs | -52.6 | -66.9 |
| adc_noise | pluck_low_e | centroid_hz | 2362.2 | 2190.9 |
