#!/usr/bin/env python3
"""End-to-end self test: the rig must catch the planted bugs in TremDelayBuggy,
pass TremDelayFixed, drive MIDI and encoders, and keep detectors honest."""
import json
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))

import numpy as np  # noqa: E402

from rig import analyze as A  # noqa: E402
from rig import build as B  # noqa: E402
from rig import controls as C  # noqa: E402
from rig import signals as S  # noqa: E402
from rig import suite as T  # noqa: E402

failures = []


def check(cond, msg):
    print(("  ok   " if cond else "  FAIL ") + msg)
    if not cond:
        failures.append(msg)


def main():
    tmp = Path(tempfile.mkdtemp(prefix="daisyrig-selftest-"))
    print("prototype generator")
    protos, _ = B.find_prototypes("int a;\nvoid f(int x) {}\nstatic inline float g(float y) { return y; }\n"
                                  "struct S { void m() {} };\nvoid h(int z = 3) {}\n")
    check(protos == ["void f(int x);", "static inline float g(float y);"], f"protos {protos}")

    print("control scripts")
    ev, _ = C.compile_script("board terrarium\nat 1 tap FS1 50ms\nramp 0 1 POT1 0 1", "seed", 2.0)
    fs = [e for e in ev if e[1] == 25]
    check(fs == [(1.0, 25, 0.0), (1.05, 25, 1.0)], "tap on active-low footswitch")
    check(len([e for e in ev if e[1] == 16]) == 1001, "1 ms ramp steps")

    print("click detector on clean and broken sines")
    sr = 48000
    t = np.arange(sr) / sr
    x = 0.25 * np.sin(2 * np.pi * 1000 * t)
    check(len(A.click_events(x, sr)) == 0, "clean sine: no clicks")
    y = x.copy()
    y[24000:] = 0.25 * np.sin(2 * np.pi * 1000 * t[24000:] + 1.0)
    check(len(A.click_events(y, sr)) == 1, "phase jump: one click")

    print("TremDelayBuggy (must FAIL with the planted bugs)")
    rb = T.run_suite(ROOT / "examples/sketches/TremDelayBuggy", tmp / "buggy", board="terrarium",
                     ctls=["noon", "min", "footswitch"])
    msgs = " | ".join(f["msg"] for f in rb["findings"])
    check(any("out of bounds" in m for e in rb["sanitizers"] for m in e["errors"]), "UBSan catches index 48000")
    check("NaN" in msgs, "NaN/Inf at POT5 = 0")
    check("unwritten" in msgs, "out[1] left unwritten in bypass")
    check("Serial output inside the audio callback" in msgs, "Serial in callback")
    noon_sine = next(r for r in rb["runs"] if r["ctl"] == "noon" and r["signal"] == "sine_1k")
    check(1.0 in noon_sine["metrics"]["click_times"], "click at buffer wrap (t = 1.000 s)")
    check(rb["memory"]["sram_bytes"] > 180_000, "188 KB buffer counted as internal SRAM")

    print("TremDelayFixed (must have no FAIL)")
    rf = T.run_suite(ROOT / "examples/sketches/TremDelayFixed", tmp / "fixed", board="terrarium",
                     ctls=["noon", "min", "sweep", "footswitch"], baseline=tmp / "buggy" / "results.json")
    fails = [f for f in rf["findings"] if f["severity"] == "FAIL"]
    check(not fails, f"no FAIL findings {fails}")
    check(rf["memory"]["sdram_bytes"] > 180_000 and rf["memory"]["sram_bytes"] < 10_000, "buffer moved to SDRAM")
    check(not rf["sanitizers"], "sanitizers clean")
    sweep_sine = next(r for r in rf["runs"] if r["ctl"] == "sweep" and r["signal"] == "sine_1k")
    check(sweep_sine["metrics"]["click_events"] == 0, "no zipper clicks while sweeping every pot")

    print("MIDI injection (DaisyDuino Pod/Midi example)")
    mid = ROOT / "third_party/DaisyDuino/examples/Pod/Midi"
    if mid.exists():
        b = B.build(mid, workdir=tmp / "midi")
        sdir, sigs = T.signal_dir(48000)
        ev, _ = C.compile_script("at 0.2 noteon 69 100\nat 0.8 noteoff 69", "pod", 2.0)
        evf = tmp / "midi.txt"
        C.write_events(ev, evf)
        r = T.run_one(b["bin"], sdir / "silence.wav", tmp / "midi.wav", evf)
        o, _ = S.read_wav(tmp / "midi.wav")
        seg = o[int(0.35 * sr):int(0.65 * sr), 0]
        spec = np.abs(np.fft.rfft(seg * np.hanning(len(seg)), 8 * len(seg)))
        f0 = np.argmax(spec[1:]) * sr / (8 * len(seg))
        check(r["rc"] == 0 and abs(f0 - 440) < 3, f"note 69 plays at {f0:.1f} Hz (expect 440)")
    print()
    print(f"{'PASS' if not failures else 'FAIL'}: {len(failures)} failure(s); artifacts in {tmp}")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
