"""daisyrig - host test rig for Arduino-IDE (DaisyDuino) Daisy Seed sketches."""
import argparse
import json
import re
import sys
from pathlib import Path

from . import analyze as A
from . import build as B
from . import controls as C
from . import hw as H
from . import signals as S
from . import suite as T
from .boards import BOARDS


def cmd_setup(a):
    if a.hw:
        print("arduino-cli:", H.setup_hw())
    B.ensure_deps()
    for p in ("release", "asan"):
        B.runtime_libs(p)
    T.signal_dir(48000)
    print("ready")


def cmd_build(a):
    r = B.build(a.sketch, a.output, "asan" if a.asan else "release", a.define or ())
    for w in r["warnings"]:
        print(w)
    mem = B.memcheck(r["objects"])
    print(json.dumps({"bin": r["bin"], "warnings": len(r["warnings"]), "memory": mem}, indent=2))


def cmd_signals(a):
    sigs = S.make_set(a.out, a.sr)
    for k, v in sigs.items():
        print(f"{k:15s} {v['peak_dbfs']:7.1f} dBFS  {v['desc']}")


def cmd_run(a):
    events = None
    if a.ctl:
        x, wsr = S.read_wav(a.input)
        dur = len(x) / wsr
        ev, _ = C.load(a.ctl, a.board, duration=dur)
        events = Path(a.output).with_suffix(".events.txt")
        C.write_events(ev, events)
    extra = []
    if a.block:
        extra += ["--block", str(a.block)]
    if a.adc_noise:
        extra += ["--adc-noise", str(a.adc_noise)]
    if a.ftz:
        extra += ["--ftz"]
    r = T.run_one(a.binary, a.input, a.output, events, extra, logs=str(Path(a.output).with_suffix("")))
    if r["stderr"]:
        print(r["stderr"], file=sys.stderr)
    out = {"rc": r["rc"], **r["report"]}
    if r["rc"] == 0 and Path(a.output).exists():
        o, sr = S.read_wav(a.output)
        i, _ = S.read_wav(a.input)
        meta = _meta_for(a.input)
        out["metrics"] = A.analyze(o, sr, i, meta)
    print(json.dumps(out, indent=2))


def _meta_for(wav):
    man = Path(wav).parent / "signals.json"
    if man.exists():
        sigs = json.loads(man.read_text())["signals"]
        return sigs.get(Path(wav).stem, {})
    return {}


def cmd_analyze(a):
    o, sr = S.read_wav(a.wav)
    i = S.read_wav(a.ref)[0] if a.ref else None
    print(json.dumps(A.analyze(o, sr, i, _meta_for(a.ref) if a.ref else {}), indent=2))


def cmd_null(a):
    x, _ = S.read_wav(a.a)
    y, _ = S.read_wav(a.b)
    print(json.dumps(A.wav_null(x, y), indent=2))


def cmd_test(a):
    out = Path(a.out or (B.ROOT / "build" / "reports" / Path(a.sketch).stem))
    res = T.run_suite(a.sketch, out, board=a.board, ctls=a.ctl_preset, full_signals=a.full,
                      asan=not a.no_asan, baseline=a.baseline, extra_flags=a.define or (),
                      ctl_files=a.ctl or (), hw=a.hw)
    if a.save_baseline:
        Path(a.save_baseline).write_text((out / "results.json").read_text())
    print((out / "REPORT.md").read_text())
    print(f"\nfull results: {out / 'results.json'}\nrenders:      {out / 'renders'}")
    fails = sum(f["severity"] == "FAIL" for f in res["findings"])
    sys.exit(1 if fails and a.strict else 0)


def cmd_hwcheck(a):
    r = H.hwcheck(a.sketch, a.target)
    for w in r.get("warnings", []):
        print(w)
    for e in r.get("errors", []):
        print(e)
    print(json.dumps({k: v for k, v in r.items() if k not in ("warnings", "errors")}, indent=2))
    sys.exit(0 if r.get("ok") else 1)


def cmd_compare(a):
    old = json.loads(Path(a.before).read_text())
    new = json.loads(Path(a.after).read_text())
    d = T.compare(old, new)
    print(json.dumps(d, indent=2))


DEBUG_PAT = re.compile(r"rig_probe|rig_event|DAISYRIG|std::cout|std::cerr|fprintf\s*\(\s*stderr|#include\s*<iostream>")


def cmd_scope(a):
    """Surgical-change check: how big is the diff, and how much is pure whitespace churn."""
    import difflib
    old = Path(a.old).read_text(errors="replace").splitlines()
    new = Path(a.new).read_text(errors="replace").splitlines()
    added = removed = hunks = 0
    for tag, i1, i2, j1, j2 in difflib.SequenceMatcher(None, old, new, autojunk=False).get_opcodes():
        if tag != "equal":
            hunks += 1
            removed += i2 - i1
            added += j2 - j1
    norm = lambda ls: [" ".join(l.split()) for l in ls]
    sm = difflib.SequenceMatcher(None, norm(old), norm(new), autojunk=False)
    real = sum(max(i2 - i1, j2 - j1) for t, i1, i2, j1, j2 in sm.get_opcodes() if t != "equal")
    churn = max(0, max(added, removed) - real)
    out = {"hunks": hunks, "lines_added": added, "lines_removed": removed,
           "net_lines": added - removed, "whitespace_only_lines": churn}
    print(json.dumps(out, indent=2))
    if churn:
        print(f"{churn} line(s) changed only in whitespace/formatting: revert them (surgical changes)")
    sys.exit(1 if churn and a.strict else 0)


def cmd_lint(a):
    """Delivery check: no rig hooks / host-only code left in the file."""
    bad = 0
    for p in a.files:
        for n, line in enumerate(Path(p).read_text(errors="replace").splitlines(), 1):
            if DEBUG_PAT.search(line):
                print(f"{p}:{n}: {line.strip()}")
                bad += 1
    print(f"{bad} debug/host-only line(s)")
    sys.exit(1 if bad else 0)


def main(argv=None):
    p = argparse.ArgumentParser(prog="daisyrig", description=__doc__)
    sub = p.add_subparsers(dest="cmd", required=True)

    s = sub.add_parser("setup", help="fetch DaisyDuino, prebuild runtimes and signals")
    s.add_argument("--hw", action="store_true", help="also install arduino-cli + STM32 core for hwcheck")
    s.set_defaults(fn=cmd_setup)

    s = sub.add_parser("hwcheck", help="compile for the real Daisy (arm-gcc via arduino-cli): flash/RAM")
    s.add_argument("sketch")
    s.add_argument("--target", default="seed", choices=list(H.FQBN))
    s.set_defaults(fn=cmd_hwcheck)

    s = sub.add_parser("build", help="compile a sketch for the host")
    s.add_argument("sketch")
    s.add_argument("-o", "--output")
    s.add_argument("--asan", action="store_true")
    s.add_argument("-D", "--define", action="append", help="extra compiler flag, e.g. -D -DMYFLAG=1")
    s.set_defaults(fn=cmd_build)

    s = sub.add_parser("signals", help="write the test signal set")
    s.add_argument("--out", default="signals")
    s.add_argument("--sr", type=int, default=48000)
    s.set_defaults(fn=cmd_signals)

    s = sub.add_parser("run", help="render one input through a built sketch")
    s.add_argument("binary")
    s.add_argument("input")
    s.add_argument("output")
    s.add_argument("--ctl", help="preset (" + ", ".join(C.PRESETS) + ") or .ctl file")
    s.add_argument("--board", default="seed", choices=list(BOARDS))
    s.add_argument("--block", type=int)
    s.add_argument("--adc-noise", type=float)
    s.add_argument("--ftz", action="store_true", help="flush denormals to zero")
    s.set_defaults(fn=cmd_run)

    s = sub.add_parser("analyze", help="measure a WAV (optionally against its input)")
    s.add_argument("wav")
    s.add_argument("--ref")
    s.set_defaults(fn=cmd_analyze)

    s = sub.add_parser("null", help="sample-wise diff of two renders")
    s.add_argument("a")
    s.add_argument("b")
    s.set_defaults(fn=cmd_null)

    s = sub.add_parser("test", help="full test pass with report")
    s.add_argument("sketch")
    s.add_argument("--board", default="seed", choices=list(BOARDS))
    s.add_argument("--out")
    s.add_argument("--ctl", action="append", help="extra .ctl file (repeatable)")
    s.add_argument("--ctl-preset", action="append", help="limit to these presets (repeatable)")
    s.add_argument("--full", action="store_true", help="every signal for every control preset")
    s.add_argument("--no-asan", action="store_true")
    s.add_argument("--baseline", help="results.json from an earlier run to diff against")
    s.add_argument("--save-baseline", help="copy results.json here after the run")
    s.add_argument("-D", "--define", action="append")
    s.add_argument("--strict", action="store_true", help="exit 1 on any FAIL")
    s.add_argument("--hw", action="store_true", help="also compile for the real Daisy (needs setup --hw)")
    s.set_defaults(fn=cmd_test)

    s = sub.add_parser("compare", help="diff two results.json files")
    s.add_argument("before")
    s.add_argument("after")
    s.set_defaults(fn=cmd_compare)

    s = sub.add_parser("scope", help="diff size + whitespace-only churn between two versions")
    s.add_argument("old")
    s.add_argument("new")
    s.add_argument("--strict", action="store_true", help="exit 1 if any whitespace-only churn")
    s.set_defaults(fn=cmd_scope)

    s = sub.add_parser("lint", help="check a file for leftover rig hooks before delivery")
    s.add_argument("files", nargs="+")
    s.set_defaults(fn=cmd_lint)

    a = p.parse_args(argv)
    try:
        a.fn(a)
    except RuntimeError as e:
        print(str(e), file=sys.stderr)
        sys.exit(2)


if __name__ == "__main__":
    main()
