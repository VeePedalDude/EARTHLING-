"""Test signal set: synthetic guitar (Karplus-Strong + pickup model) plus lab signals.

Every signal has 0.25 s of lead-in silence and >= 1.5 s of trailing silence so
the analyzer can measure tails, idle noise and self-oscillation.
"""
import json
from pathlib import Path

import numpy as np
from scipy import signal as sps

LEAD, TAIL = 0.25, 1.5
E_STD = [82.41, 110.0, 146.83, 196.0, 246.94, 329.63]  # standard tuning


def write_wav(path, x, sr):
    x = np.asarray(x, dtype=np.float32)
    if x.ndim == 1:
        x = x[:, None]
    n, ch = x.shape
    with open(path, "wb") as f:
        data = x.tobytes()
        f.write(b"RIFF" + (36 + len(data)).to_bytes(4, "little") + b"WAVE")
        f.write(b"fmt " + (16).to_bytes(4, "little") + (3).to_bytes(2, "little") + ch.to_bytes(2, "little"))
        f.write(int(sr).to_bytes(4, "little") + int(sr * ch * 4).to_bytes(4, "little"))
        f.write((ch * 4).to_bytes(2, "little") + (32).to_bytes(2, "little"))
        f.write(b"data" + len(data).to_bytes(4, "little") + data)


def read_wav(path):
    raw = Path(path).read_bytes()
    assert raw[:4] == b"RIFF" and raw[8:12] == b"WAVE", "not a WAV"
    i, fmt = 12, None
    while i < len(raw) - 8:
        cid, sz = raw[i:i + 4], int.from_bytes(raw[i + 4:i + 8], "little")
        body = raw[i + 8:i + 8 + sz]
        if cid == b"fmt ":
            tag, ch, sr = (int.from_bytes(body[0:2], "little"), int.from_bytes(body[2:4], "little"),
                           int.from_bytes(body[4:8], "little"))
            bits = int.from_bytes(body[14:16], "little")
            if tag == 0xFFFE:
                tag = int.from_bytes(body[24:26], "little")
            fmt = (tag, ch, sr, bits)
        elif cid == b"data":
            tag, ch, sr, bits = fmt
            if tag == 3 and bits == 32:
                x = np.frombuffer(body, dtype="<f4")
            elif tag == 1 and bits == 16:
                x = np.frombuffer(body, dtype="<i2") / 32768.0
            elif tag == 1 and bits == 24:
                b = np.frombuffer(body, dtype=np.uint8).reshape(-1, 3).astype(np.int32)
                x = ((b[:, 0] << 8 | b[:, 1] << 16 | b[:, 2] << 24) >> 8) / 8388608.0
            elif tag == 1 and bits == 32:
                x = np.frombuffer(body, dtype="<i4") / 2147483648.0
            else:
                raise ValueError(f"unsupported WAV {tag}/{bits}")
            return x.reshape(-1, ch).astype(np.float64), sr
        i += 8 + sz + (sz & 1)
    raise ValueError("no data chunk")


# ---------------- guitar model ----------------
class Guitar:
    def __init__(self, sr, seed=7):
        self.sr = sr
        self.rng = np.random.default_rng(seed)
        # humbucker into cable: resonant peak ~3.5 kHz, rolloff above
        w0 = 2 * np.pi * 3500 / sr
        q = 1.8
        alpha = np.sin(w0) / (2 * q)
        b = np.array([(1 - np.cos(w0)) / 2, 1 - np.cos(w0), (1 - np.cos(w0)) / 2])
        a = np.array([1 + alpha, -2 * np.cos(w0), 1 - alpha])
        self.pu_b, self.pu_a = b / a[0], a / a[0]

    def note(self, f0, dur, amp=0.4, t60=None, bright=0.6, pick_pos=0.18, mute=False):
        sr = self.sr
        n = int(dur * sr)
        P = sr / f0
        M = int(np.floor(P - 0.5))
        frac = P - 0.5 - M   # 2-tap average adds 0.5 sample of delay
        if t60 is None:
            t60 = 0.15 if mute else max(1.2, 4.0 * (110.0 / f0) ** 0.5)
        g = 10 ** (-3.0 / (t60 * f0))
        # loop: y[n] = x[n] + g*((1-frac)*avg(y[n-M]) + frac*avg(y[n-M-1]))
        c0, c1 = g * 0.5 * (1 - frac), g * 0.5
        c2 = g * 0.5 * frac
        a = np.zeros(M + 3)
        a[0] = 1.0
        a[M] -= c0
        a[M + 1] -= c1
        a[M + 2] -= c2
        L = max(2, int(P))
        exc = self.rng.uniform(-1, 1, L)
        cut = 1500 + bright * 6000 if not mute else 900
        bb, ab = sps.butter(2, min(0.99, cut / (sr / 2)))
        exc = sps.lfilter(bb, ab, exc)
        d = max(1, int(pick_pos * P))
        exc = exc - np.concatenate([np.zeros(d), exc[:-d]])
        x = np.zeros(n)
        x[:L] = exc
        y = sps.lfilter([1.0], a, x)
        if mute:
            y *= np.exp(-np.arange(n) / (0.08 * sr))
        # pickup position comb + resonance
        dp = max(1, int(0.12 * P))
        y = y - 0.6 * np.concatenate([np.zeros(dp), y[:-dp]])
        y = sps.lfilter(self.pu_b, self.pu_a, y)
        # release: fade over 15 ms at the end of the note
        fl = min(n, int(0.015 * sr))
        y[-fl:] *= np.linspace(1, 0, fl)
        pk = np.max(np.abs(y)) or 1.0
        return amp * y / pk


def _fade(n, sr, ms=20.0):
    """Raised-cosine fade in/out: no slope kinks for the click detector to trip on."""
    k = min(n // 2, int(sr * ms / 1000))
    w = np.ones(n)
    ramp = 0.5 - 0.5 * np.cos(np.pi * np.arange(k) / k)
    w[:k] = ramp
    w[n - k:] = ramp[::-1]
    return w


def _place(total, parts, sr):
    out = np.zeros(int(total * sr))
    for t, x in parts:
        i = int(t * sr)
        j = min(len(out), i + len(x))
        out[i:j] += x[: j - i]
    return out


def make_set(outdir, sr=48000):
    outdir = Path(outdir)
    outdir.mkdir(parents=True, exist_ok=True)
    g = Guitar(sr)
    sigs = {}

    def add(name, x, kind, desc, **meta):
        x = np.concatenate([np.zeros(int(LEAD * sr)), x, np.zeros(int(TAIL * sr))])
        write_wav(outdir / f"{name}.wav", x, sr)
        active = np.nonzero(np.abs(x) > 1e-5)[0]
        sigs[name] = {"file": f"{name}.wav", "kind": kind, "desc": desc,
                      "active_start": float(active[0] / sr) if len(active) else LEAD,
                      "active_end": float(active[-1] / sr) if len(active) else LEAD,
                      "peak_dbfs": float(20 * np.log10(np.max(np.abs(x)) + 1e-12)), **meta}

    add("pluck_low_e", g.note(E_STD[0], 3.0, amp=0.45), "guitar", "open low E, normal pick, 3 s sustain")
    add("pluck_high", g.note(659.26, 2.0, amp=0.4, bright=0.8), "guitar", "high E 12th fret, bright pick")
    strum = [(i * 0.012, g.note(f, 3.0, amp=0.22)) for i, f in enumerate([82.41, 123.47, 164.81, 207.65, 246.94, 329.63])]
    add("chord_strum", _place(3.2, strum, sr), "guitar", "E major down-strum, 12 ms between strings")
    bpm8 = 60 / 140 / 2
    chugs = [(i * bpm8, g.note(82.41, bpm8 * 0.9, amp=0.5, mute=True)) for i in range(16)]
    add("palm_mute", _place(16 * bpm8 + 0.2, chugs, sr), "guitar", "palm-muted low E 8ths at 140 bpm")
    scale = [196.0, 220.0, 246.94, 261.63, 293.66, 329.63, 369.99, 392.0]
    s16 = 60 / 160 / 4
    run = [(i * s16, g.note(f, s16 * 1.6, amp=0.35, t60=0.8)) for i, f in enumerate(scale + scale[::-1])]
    add("legato_run", _place(16 * s16 + 0.3, run, sr), "guitar", "fast 16th-note scale up/down (tracking hard case)")
    # bend: time-varying resample of a sustained A note, +2 semitones over 0.3 s
    base = g.note(220.0, 3.5, amp=0.4, t60=4.0)
    t = np.arange(len(base)) / sr
    ratio = np.where(t < 0.6, 1.0, np.where(t < 0.9, 2 ** (2 / 12 * (t - 0.6) / 0.3), 2 ** (2 / 12)))
    pos = np.cumsum(ratio)
    pos = pos[pos < len(base) - 1]
    add("bend", np.interp(pos, np.arange(len(base)), base), "guitar", "A3 bent up a whole step after 0.6 s")
    hot = [(i * 0.008, g.note(f, 2.5, amp=0.30)) for i, f in enumerate(E_STD)]
    hotx = _place(2.6, hot, sr)
    add("hot_humbucker", 0.95 * hotx / np.max(np.abs(hotx)), "guitar", "hard strum peaking at -0.45 dBFS")
    add("quiet_pick", g.note(146.83, 2.5, amp=0.01), "guitar", "very soft D3 at -40 dBFS (gates, trackers)")

    n = int(2.0 * sr)
    tt = np.arange(n) / sr
    fade = _fade(n, sr)
    add("sine_1k", 0.25 * np.sin(2 * np.pi * 1000 * tt) * fade, "sine", "1 kHz at -12 dBFS", f0=1000.0)
    add("sine_3k5", 0.5 * np.sin(2 * np.pi * 3500 * tt) * fade, "sine", "3.5 kHz at -6 dBFS (aliasing)", f0=3500.0)
    add("sine_100", 0.5 * np.sin(2 * np.pi * 100 * tt) * fade, "sine", "100 Hz at -6 dBFS (LF / DC handling)", f0=100.0)
    ns = int(5.0 * sr)
    ts = np.arange(ns) / sr
    f1, f2 = 20.0, 20000.0
    k = np.log(f2 / f1) / 5.0
    sweep = 0.25 * np.sin(2 * np.pi * f1 * (np.exp(k * ts) - 1) / k)
    sweep *= _fade(ns, sr)
    add("sweep", sweep, "sweep", "20 Hz-20 kHz log sweep, 5 s, -12 dBFS", f1=f1, f2=f2, seconds=5.0)
    imp = np.zeros(int(0.01 * sr))
    imp[0] = 0.5
    add("impulse", imp, "impulse", "single 0.5 sample (latency, tails, stability)")
    add("silence", np.zeros(int(2.0 * sr)), "silence", "digital silence (idle noise, DC, self-oscillation)")
    rng = np.random.default_rng(3)
    wn = rng.standard_normal(int(2.0 * sr))
    pink = sps.lfilter([0.049922035, -0.095993537, 0.050612699, -0.004408786],
                       [1, -2.494956002, 2.017265875, -0.522189400], wn)
    pink *= 0.1 / np.sqrt(np.mean(pink ** 2))
    add("pink_noise", pink * fade, "noise", "pink noise, -20 dBFS RMS")
    (outdir / "signals.json").write_text(json.dumps({"samplerate": sr, "signals": sigs}, indent=2))
    return sigs


QUICK = ["pluck_low_e", "chord_strum", "palm_mute", "hot_humbucker", "quiet_pick", "sine_1k", "sweep", "impulse", "silence"]
