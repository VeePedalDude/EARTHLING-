"""Full test pass for one sketch: build, memory, render matrix, sanitizers, report."""
import json
import os
import subprocess
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

import numpy as np

from . import analyze as A
from . import build as B
from . import controls as C
from . import hw as H
from . import signals as S
from .boards import BOARDS

M7_FACTOR = float(os.environ.get("DAISYRIG_M7_FACTOR", "30"))


def signal_dir(sr):
    d = B.ROOT / "build" / "signals" / str(int(sr))
    if not (d / "signals.json").exists():
        B.log(f"generating test signals at {int(sr)} Hz ...")
        S.make_set(d, sr)
    return d, json.loads((d / "signals.json").read_text())["signals"]


def sketch_config(binary):
    r = subprocess.run([binary, "--config-only"], capture_output=True, text=True, timeout=60)
    if r.returncode != 0:
        raise RuntimeError(f"setup() failed:\n{r.stderr}")
    return json.loads(r.stdout.strip().splitlines()[-1])


def run_one(binary, wav_in, wav_out, events=None, extra=(), logs=None, timeout=600):
    rep = str(wav_out) + ".report.json"
    cmd = [binary, "--in", str(wav_in), "--out", str(wav_out), "--report", rep, *extra]
    if events:
        cmd += ["--events", str(events)]
    if logs:
        cmd += ["--logs", str(logs)]
    t0 = time.time()
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
        rc, err = r.returncode, r.stderr
    except subprocess.TimeoutExpired:
        rc, err = -999, f"timeout after {timeout}s (loop() or the callback never returns?)"
    report = {}
    if Path(rep).exists():
        try:
            report = json.loads(Path(rep).read_text())
        except json.JSONDecodeError:
            pass
    return {"rc": rc, "stderr": err[-4000:], "report": report, "wall": time.time() - t0}


def sanitizer_errors(stderr):
    keys = ("ERROR: AddressSanitizer", "runtime error:", "ERROR: LeakSanitizer", "SUMMARY:")
    return [l.strip() for l in stderr.splitlines() if any(k in l for k in keys)][:8]


def default_ctls(board):
    ctls = ["noon", "min", "max", "sweep", "twiddle"]
    if "FS1" in BOARDS[board]["names"]:
        ctls.append("footswitch")
    if "SW1" in BOARDS[board]["names"] and board == "terrarium":
        ctls.append("toggles")
    return ctls


def run_suite(sketch, out, board="seed", ctls=None, full_signals=False, asan=True,
              baseline=None, extra_flags=(), jobs=None, ctl_files=(), hw=False):
    out = Path(out)
    (out / "renders").mkdir(parents=True, exist_ok=True)
    res = {"sketch": str(Path(sketch).resolve()), "board": board, "findings": []}
    F = res["findings"]

    def find(sev, msg):
        F.append({"severity": sev, "msg": msg})

    # 1. build
    b = B.build(sketch, profile="release", extra_flags=extra_flags, workdir=out / "build")
    res["build"] = {"warnings": b["warnings"], "bin": b["bin"]}
    if b["warnings"]:
        find("WARN", f"{len(b['warnings'])} compiler warning(s) in sketch code (-Wall -Wextra -Wdouble-promotion -Wshadow)")
    res["memory"] = B.memcheck(b["objects"])
    res["memory"]["largest"] = [{"bytes": s, "section": sec, "symbol": sym}
                                for s, sec, sym in B.largest_symbols(b["objects"])]
    mem = res["memory"]
    if mem["sram_bytes"] > mem["sram_limit"]:
        find("FAIL", f"static RAM {mem['sram_bytes'] / 1024:.0f} KB > 512 KB: move big buffers to SDRAM (DSY_SDRAM_BSS)")
    elif mem["sram_pct"] > 80:
        find("WARN", f"static RAM at {mem['sram_pct']}% of 512 KB")

    if hw:
        h = H.hwcheck(sketch, "seed", workdir=out / "hwstage")
        res["hw"] = h
        if h.get("skipped"):
            find("INFO", "hardware compile skipped: " + h["reason"])
        elif not h["ok"]:
            find("FAIL", "does not compile for the Daisy (arm-gcc): " + (h["errors"][0][-200:] if h["errors"] else "?"))
        else:
            if h.get("flash_pct", 0) > 100 or h.get("ram_pct", 0) > 100:
                find("FAIL", f"over target limits: flash {h.get('flash_pct')}%, RAM {h.get('ram_pct')}%")
            elif h.get("flash_pct", 0) > 90:
                find("WARN", f"flash at {h['flash_pct']}% of 128 KB")
            if h["warnings"]:
                find("WARN", f"{len(h['warnings'])} arm-gcc warning(s) in sketch code")

    cfg = sketch_config(b["bin"])
    res["config"] = cfg
    if not cfg["audio_started"]:
        find("WARN", "setup() never started audio (DAISY.begin / StartAudio not called)")
    sdir, sigs = signal_dir(cfg["samplerate"])
    names = list(sigs) if full_signals else S.QUICK
    ctls = list(ctls or default_ctls(board)) + list(ctl_files)

    # 2. render matrix
    jobs_list = []
    evdir = out / "events"
    evdir.mkdir(exist_ok=True)
    for ctl in ctls:
        cname = Path(ctl).stem if ctl not in C.PRESETS else ctl
        sig_names = list(sigs) if (ctl == "noon" or full_signals) else names
        for s in sig_names:
            dur = sigs[s]["active_end"] + S.TAIL
            ev, _ = C.load(ctl, board, duration=dur)
            evf = evdir / f"{cname}__{s}.txt"
            C.write_events(ev, evf)
            rd = out / "renders" / cname
            rd.mkdir(exist_ok=True)
            jobs_list.append((cname, s, evf, rd / f"{s}.wav", ()))
    # ADC noise variant: jittery pots
    for s in ("pluck_low_e", "silence"):
        evf = evdir / f"noon__{s}.txt"
        rd = out / "renders" / "adc_noise"
        rd.mkdir(exist_ok=True)
        jobs_list.append(("adc_noise", s, evf, rd / f"{s}.wav", ("--adc-noise", "2")))

    mono = BOARDS[board].get("mono", False)
    base_extra = ("--mono",) if mono else ()

    def do(job):
        cname, s, evf, wav, extra = job
        r = run_one(b["bin"], sdir / sigs[s]["file"], wav, evf, base_extra + tuple(extra), logs=str(wav)[:-4])
        if Path(wav).exists() and r["rc"] == 0:
            o, sr = S.read_wav(wav)
            i, _ = S.read_wav(sdir / sigs[s]["file"])
            r["metrics"] = A.analyze(o[:, :1] if mono else o, sr, i, sigs[s])
        return {"ctl": cname, "signal": s, **r}

    with ThreadPoolExecutor(max_workers=jobs or os.cpu_count() or 4) as ex:
        runs = list(ex.map(do, jobs_list))
    res["runs"] = runs

    # 3. timing (sequential, quiet machine)
    timing = []
    for s in ("chord_strum", "pink_noise"):
        evf = evdir / f"noon__{s}.txt"
        # --ftz: x86 denormals are ~40x slower and would swamp the estimate
        r = run_one(b["bin"], sdir / sigs[s]["file"], out / f"timing_{s}.wav", evf, base_extra + ("--ftz",))
        timing.append(r["report"].get("host_load_p99_pct", 0.0))
    host_p99 = max(timing) if timing else 0.0
    res["cpu"] = {"host_load_p99_pct": host_p99, "m7_factor": M7_FACTOR,
                  "m7_estimate_pct": round(host_p99 * M7_FACTOR, 1),
                  "note": "M7 figure = host p99 (denormals flushed) x DAISYRIG_M7_FACTOR "
                          "(uncalibrated; calibrate against CpuLoadMeter on hardware)"}
    if host_p99 * M7_FACTOR > 70:
        find("WARN", f"estimated Daisy CPU ~{host_p99 * M7_FACTOR:.0f}% (inferred, see cpu.note)")

    # 4. determinism
    det = [r for r in runs if r["ctl"] == "noon" and r["signal"] == "pluck_low_e"]
    if det:
        w2 = out / "determinism.wav"
        run_one(b["bin"], sdir / sigs["pluck_low_e"]["file"], w2, evdir / "noon__pluck_low_e.txt", base_extra)
        a, _ = S.read_wav(out / "renders" / "noon" / "pluck_low_e.wav")
        c, _ = S.read_wav(w2)
        nul = A.wav_null(a, c)
        res["determinism"] = nul
        if not nul["identical"]:
            find("WARN", f"two identical runs differ (max diff {nul['max_abs_diff']:.2e}): uninitialised state or random seeding")

    # 5. sanitizers
    if asan:
        try:
            ba = B.build(sketch, profile="asan", extra_flags=extra_flags, workdir=out / "build")
            errs = []
            for s in S.QUICK:
                for ctl in ("noon", "sweep"):
                    evf = evdir / f"{ctl}__{s}.txt"
                    if not evf.exists():
                        continue
                    r = run_one(ba["bin"], sdir / sigs[s]["file"], out / "asan.wav", evf, base_extra)
                    e = sanitizer_errors(r["stderr"])
                    if e or r["rc"] not in (0,):
                        errs.append({"signal": s, "ctl": ctl, "rc": r["rc"], "errors": e or [r["stderr"][-500:]]})
            res["sanitizers"] = errs
            if errs:
                find("FAIL", f"AddressSanitizer/UBSan reported problems in {len(errs)} run(s): {errs[0]['errors'][0][:160]}")
        except RuntimeError as e:
            res["sanitizers"] = [{"errors": [str(e)[:500]]}]
            find("WARN", "ASAN build failed (see results.json)")

    # 6. per-run findings
    _run_findings(runs, sigs, find)
    if baseline:
        res["baseline_diff"] = compare(json.loads(Path(baseline).read_text()), res)
    (out / "results.json").write_text(json.dumps(res, indent=2, default=float))
    (out / "REPORT.md").write_text(report_md(res))
    return res


def _run_findings(runs, sigs, find):
    agg = {}

    def note(key, sev, msg):
        agg.setdefault((key, sev), []).append(msg)

    for r in runs:
        tag = f"{r['ctl']}/{r['signal']}"
        rep, m = r.get("report", {}), r.get("metrics", {})
        if r["rc"] != 0:
            note("crash", "FAIL", f"{tag}: exit {r['rc']} {r['stderr'][-200:].strip()}")
            continue
        if rep.get("nan_samples", 0) or rep.get("inf_samples", 0):
            note("nan", "FAIL", f"{tag}: {rep['nan_samples']} NaN / {rep['inf_samples']} Inf from t={rep['first_nonfinite_s']:.3f}s")
        if rep.get("unwritten_output_samples", 0):
            note("unwritten", "FAIL", f"{tag}: callback left {rep['unwritten_output_samples']} output samples unwritten")
        if rep.get("allocations_in_callback", 0):
            note("alloc", "WARN", f"{tag}: {rep['allocations_in_callback']} heap allocations inside the audio callback")
        if rep.get("delay_in_callback", 0):
            note("delaycb", "FAIL", f"{tag}: delay() inside the audio callback")
        if rep.get("serial_bytes_in_callback", 0):
            note("serialcb", "WARN", f"{tag}: Serial output inside the audio callback")
        if m.get("clipped_samples", 0):
            note("clip", "WARN", f"{tag}: {m['clipped_samples']} samples at/over full scale (peak {m['peak_dbfs']} dBFS)")
        if m.get("click_events", 0):
            note("click", "WARN", f"{tag}: {m['click_events']} suspected click(s) at {m['click_times'][:4]} s")
        if abs(m.get("dc_offset", 0)) > 0.005:
            note("dc", "WARN", f"{tag}: DC offset {m['dc_offset']:+.4f}")
        if r["signal"] == "silence" and m.get("idle_rms_dbfs", -999) > -80:
            note("idle", "WARN", f"{tag}: idle output {m['idle_rms_dbfs']} dBFS RMS on silent input "
                                 f"(dominant {m.get('end_dominant_hz')} Hz)")
        if r["signal"] != "silence" and m.get("end_rms_dbfs", -999) > -50:
            note("tail", "INFO", f"{tag}: still {m['end_rms_dbfs']} dBFS 1.5 s after input stopped "
                                 f"(long tail, feedback or self-oscillation)")
        if rep.get("analogread_in_callback", 0):
            note("adccb", "INFO", f"{tag}: analogRead() called {rep['analogread_in_callback']}x inside the callback")
        for w in rep.get("warnings", []):
            note("w:" + w, "WARN", w)
    for (key, sev), msgs in agg.items():
        extra = f" (+{len(msgs) - 1} more)" if len(msgs) > 1 else ""
        find(sev, msgs[0] + extra)


KEY_METRICS = ["peak_dbfs", "rms_dbfs", "gain_db", "click_events", "clipped_samples", "end_rms_dbfs",
               "thd_pct", "alias_db", "latency_samples", "centroid_hz"]


def compare(old, new):
    """Per (ctl, signal) deltas of key metrics between two results.json dicts."""
    idx = {(r["ctl"], r["signal"]): r.get("metrics", {}) for r in old.get("runs", [])}
    rows = []
    for r in new.get("runs", []):
        o = idx.get((r["ctl"], r["signal"]))
        if o is None:
            continue
        m = r.get("metrics", {})
        d = {}
        for k in KEY_METRICS:
            a, b = o.get(k), m.get(k)
            if isinstance(a, (int, float)) and isinstance(b, (int, float)) and a != b:
                d[k] = [a, b]
        if d:
            rows.append({"ctl": r["ctl"], "signal": r["signal"], "changes": d})
    cpu = [old.get("cpu", {}).get("host_load_p99_pct"), new.get("cpu", {}).get("host_load_p99_pct")]
    return {"changed_runs": rows, "cpu_host_p99": cpu,
            "findings_before": len(old.get("findings", [])), "findings_after": len(new.get("findings", []))}


def report_md(res):
    L = [f"# daisyrig report: `{Path(res['sketch']).name}`", ""]
    cfg = res.get("config", {})
    mono = BOARDS.get(res["board"], {}).get("mono")
    L.append(f"Board map **{res['board']}** · {cfg.get('samplerate', 0):.0f} Hz · block {cfg.get('blocksize')} · "
             + ("mono (in[0] → out[0])" if mono else f"{cfg.get('channels')} ch"))
    L.append("")
    order = {"FAIL": 0, "WARN": 1, "INFO": 2}
    fs = sorted(res["findings"], key=lambda f: order.get(f["severity"], 3))
    nfail = sum(f["severity"] == "FAIL" for f in fs)
    nwarn = sum(f["severity"] == "WARN" for f in fs)
    L.append(f"## Verdict: {'FAIL' if nfail else ('WARN' if nwarn else 'PASS')}  ({nfail} fail, {nwarn} warn)")
    L.append("")
    for f in fs:
        L.append(f"- **{f['severity']}** {f['msg']}")
    if not fs:
        L.append("- no findings")
    L += ["", "## Verified (measured on the host build)", ""]
    mem = res.get("memory", {})
    L.append(f"- Static RAM {mem.get('sram_bytes', 0) / 1024:.1f} KB ({mem.get('sram_pct')}% of 512 KB), "
             f"SDRAM {mem.get('sdram_bytes', 0) / 1024:.1f} KB")
    for s in mem.get("largest", [])[:5]:
        L.append(f"  - {s['bytes'] / 1024:.1f} KB `{s['section']}` {s['symbol']}")
    h = res.get("hw")
    if h and h.get("ok"):
        L.append(f"- Daisy build (arm-gcc, {h['mode']}): flash {h.get('flash_bytes', 0) / 1024:.1f} KB "
                 f"({h.get('flash_pct')}% of {h.get('flash_max', 0) // 1024} KB), RAM {h.get('ram_bytes', 0) / 1024:.1f} KB "
                 f"({h.get('ram_pct')}%)")
    elif h and not h.get("skipped"):
        L.append("- Daisy build (arm-gcc): **failed**")
        for e in h.get("errors", [])[:5]:
            L.append(f"  - `{e.split('/')[-1][:200]}`")
    san = res.get("sanitizers")
    if san is not None:
        L.append(f"- ASAN/UBSan: {'clean' if not san else f'{len(san)} run(s) with errors'}")
    det = res.get("determinism")
    if det:
        L.append(f"- Determinism: {'bit-identical across runs' if det['identical'] else 'runs differ'}")
    L.append(f"- Compiler warnings in sketch: {len(res['build']['warnings'])}")
    for w in res["build"]["warnings"][:10]:
        L.append(f"  - `{w.split('/')[-1][:180]}`")
    cpu = res.get("cpu", {})
    L += ["", "## Inferred (not measured on hardware)", "",
          f"- CPU: host p99 {cpu.get('host_load_p99_pct', 0):.2f}% of block budget → Daisy estimate "
          f"~{cpu.get('m7_estimate_pct')}% using factor {cpu.get('m7_factor')} (uncalibrated)",
          "- Click detection, tail and idle-noise flags are heuristics: listen at the flagged times.",
          "- Memory sizes come from x86-64 objects; real ARM sizes differ slightly.", ""]
    L += ["## Renders", "",
          "| ctl | signal | peak dBFS | gain dB | clicks | clipped | tail→-60 s | end dBFS | THD % | alias dB | host p99 % |",
          "|---|---|---|---|---|---|---|---|---|---|---|"]
    for r in res.get("runs", []):
        m, rep = r.get("metrics", {}), r.get("report", {})
        L.append("| {} | {} | {} | {} | {} | {} | {} | {} | {} | {} | {} |".format(
            r["ctl"], r["signal"], m.get("peak_dbfs", "-"), m.get("gain_db", "-"), m.get("click_events", "-"),
            m.get("clipped_samples", "-"), m.get("tail_to_-60db_s", "-"), m.get("end_rms_dbfs", "-"),
            m.get("thd_pct", "-"), m.get("alias_db", "-"), round(rep.get("host_load_p99_pct", 0), 3)))
    bd = res.get("baseline_diff")
    if bd:
        L += ["", "## Before → after", "",
              f"Findings {bd['findings_before']} → {bd['findings_after']} · host CPU p99 "
              f"{bd['cpu_host_p99'][0]} → {bd['cpu_host_p99'][1]} %", ""]
        if not bd["changed_runs"]:
            L.append("No key metric changed.")
        else:
            L += ["| ctl | signal | metric | before | after |", "|---|---|---|---|---|"]
            for row in bd["changed_runs"]:
                for k, (a, b) in row["changes"].items():
                    L.append(f"| {row['ctl']} | {row['signal']} | {k} | {a} | {b} |")
    L.append("")
    return "\n".join(L)
