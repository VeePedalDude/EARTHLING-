"""Objective measurements on a sketch's output (optionally against its input)."""
import numpy as np
from scipy import signal as sps

EPS = 1e-12


def db(x):
    return float(20 * np.log10(max(float(x), EPS)))


def rms(x):
    return float(np.sqrt(np.mean(np.square(x)))) if len(x) else 0.0


def _rolling_rms(x, win):
    if len(x) < win:
        return np.array([rms(x)])
    c = np.cumsum(np.concatenate([[0.0], x * x]))
    return np.sqrt(np.maximum(0, (c[win:] - c[:-win]) / win))


def click_events(x, sr, ratio=3.0, floor=2e-4, blk_ms=2.0, nb=8):
    """Discontinuities: a 2 ms block whose peak |second difference| is > `ratio`
    times the median block peak of its +-16 ms neighbourhood (and above `floor`).
    Steady tones, sweeps and smooth modulation keep |d2| locally uniform; jumps in
    a delay pointer, unsmoothed gain steps and buffer-wrap bugs do not.
    Returns event times (s). Heuristic - listen at the flagged times."""
    x = np.nan_to_num(np.asarray(x, dtype=np.float64))
    d2 = np.abs(np.diff(x, 2))
    B = max(4, int(sr * blk_ms / 1000))
    n = len(d2) // B
    if n < 6:
        return []
    bm = d2[: n * B].reshape(n, B).max(axis=1)
    ev = []
    for j in range(n):
        lo, hi = max(0, j - nb), min(n, j + nb + 1)
        neigh = np.concatenate([bm[lo:max(lo, j - 1)], bm[min(hi, j + 2):hi]])
        if len(neigh) < 4:
            continue
        if bm[j] > ratio * np.median(neigh) and bm[j] > floor:
            ev.append((j * B + int(np.argmax(d2[j * B:(j + 1) * B]))) / sr)
    out = []
    for t in ev:
        if not out or t - out[-1] > 0.005:
            out.append(float(t))
    return out


def distorted(inp, out):
    """True if out is far from a scaled copy of inp (crest factor collapsed by clipping)."""
    ci = np.max(np.abs(inp)) / (rms(inp) + EPS)
    co = np.max(np.abs(out)) / (rms(out) + EPS)
    return co < 0.9 * ci


def thd(x, sr, f0):
    """THD (%), aliasing (dB re fundamental) and SINAD-ish noise for a steady sine segment."""
    n = len(x)
    w = sps.windows.blackmanharris(n)
    X = np.abs(np.fft.rfft(x * w)) ** 2
    fr = np.fft.rfftfreq(n, 1 / sr)
    bw = 6 * sr / n  # bins either side

    def band(f):
        m = np.abs(fr - f) <= bw
        return float(X[m].sum())

    fund = band(f0)
    harm, alias = 0.0, 0.0
    for kk in range(2, 30):
        fk = kk * f0
        if fk < sr / 2:
            if kk <= 10:
                harm += band(fk)
        else:
            fa = abs(((fk + sr / 2) % sr) - sr / 2)
            if fa > 20 and abs(fa - f0) > 2 * bw:
                alias += band(fa)
    total = float(X.sum())
    noise = max(total - fund - harm - alias, EPS)
    return {
        "thd_pct": round(100 * np.sqrt(harm / max(fund, EPS)), 4),
        "alias_db": round(10 * np.log10(max(alias, EPS) / max(fund, EPS)), 1),
        "noise_db": round(10 * np.log10(noise / max(fund, EPS)), 1),
        "fund_dbfs": round(10 * np.log10(max(fund, EPS) / (np.sum(w) ** 2 / 4)) , 1),
    }


def analyze(out, sr, inp=None, meta=None):
    """out/inp: (n, ch) arrays. meta: signal manifest entry."""
    meta = meta or {}
    out = np.atleast_2d(out.T).T if out.ndim == 1 else out
    n, nch = out.shape
    m = {"channels": nch, "seconds": round(n / sr, 3)}
    finite = np.isfinite(out)
    m["nonfinite_samples"] = int((~finite).sum())
    o = np.where(finite, out, 0.0)
    L = o[:, 0]
    m["peak_dbfs"] = round(db(np.max(np.abs(o))), 2)
    m["rms_dbfs"] = round(db(rms(o.ravel())), 2)
    m["dc_offset"] = round(float(np.mean(L)), 6)
    m["clipped_samples"] = int((np.abs(o) >= 0.999).sum())
    if nch > 1:
        R = o[:, 1]
        denom = np.sqrt(np.sum(L * L) * np.sum(R * R))
        m["lr_correlation"] = round(float(np.sum(L * R) / denom), 4) if denom > 0 else None
    a0 = int(meta.get("active_start", 0) * sr)
    a1 = int(meta.get("active_end", n / sr) * sr)
    a1 = max(a1, a0 + 1)
    mono = o.mean(axis=1)

    # Clicks are only judged on stationary inputs (sine, sweep, silence): there
    # any short broadband spike is a defect. Guitar attacks and their echoes
    # are legitimately transient, so guitar renders get no click verdict.
    judge_clicks = meta.get("kind") in (None, "sine", "sweep", "silence")
    if inp is not None:
        i = inp[:, 0] if inp.ndim > 1 else inp
        i = i[:n]
        r_in = rms(i[a0:a1])
        r_out = rms(mono[a0:a1])
        if r_in > 1e-7:
            m["gain_db"] = round(db(r_out) - db(r_in), 2)
        # null vs input (how much the effect changes the signal)
        if r_in > 1e-7:
            m["diff_vs_input_db"] = round(db(rms(mono[a0:a1] - i[a0:a1])) - db(r_in), 2)
    if not judge_clicks:
        m["click_events"] = None
        m["click_times"] = []
    elif inp is not None:
        ic = click_events(i, sr)
        oc = click_events(mono, sr)
        new = [t for t in oc if not any(abs(t - u) < 0.01 for u in ic)]
        # the input's own fade-in/out: a distorted onset is meant to sharpen
        edges = (a0 / sr, a1 / sr)
        new = [t for t in new if all(abs(t - e) > 0.04 for e in edges)]
        # below ~150 Hz a clipped sweep is a square wave; its edges look like clicks
        if meta.get("kind") == "sweep":
            k = np.log(meta["f2"] / meta["f1"]) / meta["seconds"]
            t150 = meta["active_start"] + np.log(150.0 / meta["f1"]) / k
            w = int(0.05 * sr)

            def edge_of_clipped_wave(t):
                c = int(t * sr)
                return t < t150 and distorted(i[max(0, c - w):c + w], mono[max(0, c - w):c + w])
            new = [t for t in new if not edge_of_clipped_wave(t)]
        m["click_events"] = len(new)
        m["click_times"] = [round(t, 4) for t in new[:12]]
    else:
        oc = click_events(mono, sr)
        m["click_events"] = len(oc)
        m["click_times"] = [round(t, 4) for t in oc[:12]]

    # tail / idle behaviour after the input goes silent
    env = _rolling_rms(mono, int(0.01 * sr))
    after = env[a1:] if a1 < len(env) else np.array([0.0])
    below = np.nonzero(after < 10 ** (-60 / 20))[0]
    m["tail_to_-60db_s"] = round(float(below[0] / sr), 3) if len(below) else None
    last = mono[-int(0.5 * sr):]
    m["end_rms_dbfs"] = round(db(rms(last)), 1)
    m["end_peak_dbfs"] = round(db(np.max(np.abs(last))), 1)
    if len(last) > 1024:
        spec = np.abs(np.fft.rfft(last * np.hanning(len(last))))
        f = np.fft.rfftfreq(len(last), 1 / sr)
        k = int(np.argmax(spec[1:]) + 1)
        m["end_dominant_hz"] = round(float(f[k]), 1)

    kind = meta.get("kind")
    if kind == "sine":
        seg = mono[a0 + int(0.3 * sr): a1 - int(0.1 * sr)]
        if len(seg) > 4096:
            m.update(thd(seg, sr, meta["f0"]))
    if kind == "impulse":
        pk = np.max(np.abs(mono)) + EPS
        hit = np.nonzero(np.abs(mono) > pk * 0.1)[0]
        if len(hit):
            m["latency_samples"] = int(hit[0] - a0)
    if kind == "silence":
        m["idle_rms_dbfs"] = m["rms_dbfs"]
    # spectral balance on the active region
    seg = mono[a0:a1]
    if len(seg) > 2048:
        f, P = sps.welch(seg, sr, nperseg=2048)
        tot = P.sum() + EPS
        m["centroid_hz"] = round(float((f * P).sum() / tot), 1)
        m["hf_above_10k_db"] = round(10 * np.log10(P[f > 10000].sum() / tot + EPS), 1)
    return m


def wav_null(a, b):
    """Sample-wise comparison of two renders (bit-exact refactor checks)."""
    n = min(len(a), len(b))
    d = a[:n] - b[:n]
    ref = rms(a[:n].ravel())
    return {"max_abs_diff": float(np.max(np.abs(d))) if n else 0.0,
            "diff_rms_db_re_a": round(db(rms(d.ravel())) - db(ref), 1) if ref > 0 else None,
            "identical": bool(n and np.array_equal(a[:n], b[:n]))}
