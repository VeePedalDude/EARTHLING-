"""Turn an Arduino-IDE DaisyDuino sketch into a host executable."""
import hashlib
import os
import re
import shutil
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SHIM_INC = ROOT / "shim" / "include"
SHIM_SRC = ROOT / "shim" / "src"
TP = ROOT / "third_party" / "DaisyDuino"
DD_SRC = TP / "src"
DAISYDUINO_URL = "https://github.com/electro-smith/DaisyDuino.git"
DAISYDUINO_COMMIT = "45c2687d123bceda9a4c549b03c67a0c3cec25ce"
CACHE = ROOT / "build" / "cache"
CXX = os.environ.get("CXX", "g++")

PROFILES = {
    "release": ["-O2", "-g"],
    "asan": ["-O1", "-g", "-fno-omit-frame-pointer", "-fsanitize=address,undefined",
             "-fno-sanitize-recover=undefined"],
}
BASE_FLAGS = ["-std=gnu++17", "-DDAISYRIG=1", "-DARDUINO=10819", "-DARDUINO_ARCH_STM32",
              "-fno-strict-aliasing"]
SKETCH_WARN = ["-Wall", "-Wextra", "-Wdouble-promotion", "-Wshadow", "-Wno-unused-parameter"]


def log(msg):
    print(f"[daisyrig] {msg}", file=sys.stderr)


def ensure_deps():
    if (DD_SRC / "utility" / "DaisySP" / "daisysp.h").exists():
        return
    log(f"fetching DaisyDuino @ {DAISYDUINO_COMMIT[:10]} ...")
    TP.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(["git", "clone", "--quiet", DAISYDUINO_URL, str(TP)], check=True)
    subprocess.run(["git", "-C", str(TP), "checkout", "--quiet", DAISYDUINO_COMMIT], check=True)


def _inc():
    return [f"-I{SHIM_INC}", "-isystem", str(DD_SRC)]


def _compile(src, obj, flags):
    cmd = [CXX, *BASE_FLAGS, *flags, *_inc(), "-c", str(src), "-o", str(obj)]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError(f"compile failed: {src}\n{r.stderr}")
    return obj


def runtime_libs(profile="release"):
    """Build (once) libdaisysp.a and librig.a for a profile; return their paths."""
    ensure_deps()
    flags = PROFILES[profile]
    srcs = sorted((DD_SRC / "utility" / "DaisySP" / "modules").glob("*.cpp"))
    rig_srcs = [DD_SRC / "utility" / f"{n}.cpp" for n in ("switch", "ctrl", "encoder", "led", "parameter")]
    rig_srcs.append(SHIM_SRC / "sim_runtime.cpp")
    h = hashlib.sha1()
    h.update(" ".join([CXX, *BASE_FLAGS, *flags]).encode())
    for p in [*srcs, *rig_srcs, *sorted(SHIM_INC.glob("*.h")), SHIM_SRC / "wav.h"]:
        h.update(p.read_bytes())
    d = CACHE / f"{profile}-{h.hexdigest()[:12]}"
    dsp, rig = d / "libdaisysp.a", d / "librig.a"
    if dsp.exists() and rig.exists():
        return dsp, rig
    log(f"building runtime ({profile}) ...")
    d.mkdir(parents=True, exist_ok=True)
    extra = ["-w"]  # third-party + runtime: quiet
    with ThreadPoolExecutor(max_workers=os.cpu_count() or 4) as ex:
        dsp_objs = list(ex.map(lambda s: _compile(s, d / f"dsp_{s.stem}.o", flags + extra), srcs))
        rig_objs = list(ex.map(lambda s: _compile(s, d / f"rig_{s.stem}.o", flags + extra), rig_srcs))
    subprocess.run(["ar", "rcs", str(dsp), *map(str, dsp_objs)], check=True)
    subprocess.run(["ar", "rcs", str(rig), *map(str, rig_objs)], check=True)
    return dsp, rig


# ---------------- .ino preprocessing ----------------
_KEYWORDS = {"if", "for", "while", "switch", "catch", "return", "else", "do", "sizeof"}


def _strip(code):
    """Blank out comments, strings and char literals, keeping offsets."""
    out = list(code)
    i, n = 0, len(code)
    while i < n:
        c = code[i]
        if code.startswith("//", i):
            j = code.find("\n", i)
            j = n if j < 0 else j
            for k in range(i, j):
                out[k] = " "
            i = j
        elif code.startswith("/*", i):
            j = code.find("*/", i + 2)
            j = n if j < 0 else j + 2
            for k in range(i, j):
                if out[k] != "\n":
                    out[k] = " "
            i = j
        elif c in "\"'":
            j = i + 1
            while j < n and code[j] != c:
                j += 2 if code[j] == "\\" else 1
            for k in range(i + 1, min(j, n)):
                if out[k] != "\n":
                    out[k] = " "
            i = j + 1
        else:
            i += 1
    return "".join(out)


def find_prototypes(code):
    """Arduino-style: top-level free function definitions -> (prototypes, first_def_offset)."""
    s = _strip(code)
    # blank preprocessor lines
    s = re.sub(r"(?m)^[ \t]*#.*$", lambda m: " " * len(m.group(0)), s)
    protos, first = [], None
    depth, stmt_start, i = 0, 0, 0
    while i < len(s):
        c = s[i]
        if c == "{":
            if depth == 0:
                head = s[stmt_start:i].strip()
                m = re.match(r"^(?P<ret>[\w:<>,\*&\s]+?)\s*\b(?P<name>[A-Za-z_]\w*)\s*\((?P<args>[^()]*)\)\s*(?:const\s*)?$",
                             head, re.S)
                if m and m.group("name") not in _KEYWORDS:
                    ret = " ".join(m.group("ret").split())
                    bad = (not ret or "=" in head or "::" in m.group("name")
                           or re.search(r"\b(class|struct|union|enum|namespace|template|typedef|operator)\b", head)
                           or "=" in m.group("args"))
                    if not bad:
                        protos.append(f"{ret} {m.group('name')}({' '.join(m.group('args').split())});")
                        if first is None:
                            first = stmt_start + (len(s[stmt_start:i]) - len(s[stmt_start:i].lstrip()))
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                stmt_start = i + 1
        elif c == ";" and depth == 0:
            stmt_start = i + 1
        elif c == "\n" and depth == 0 and s[stmt_start:i].strip() == "":
            stmt_start = i + 1
        i += 1
    return protos, first


def ino_sources(sketch):
    p = Path(sketch).resolve()
    if p.is_dir():
        main = p / f"{p.name}.ino"
        inos = sorted(x for x in p.glob("*.ino") if x != main)
        inos = ([main] if main.exists() else []) + inos
        if not inos:
            raise FileNotFoundError(f"no .ino in {p}")
        return p, inos, sorted(p.glob("*.cpp"))
    return p.parent, [p], []


def preprocess(sketch, out_cpp, prototypes=True):
    sk_dir, inos, _ = ino_sources(sketch)
    code_parts = []
    for ino in inos:
        code_parts.append((ino, ino.read_text(errors="replace")))
    merged = "".join(f"#line 1 \"{f}\"\n{c}\n" for f, c in code_parts)
    out = ['#include <Arduino.h>\n']
    if prototypes:
        protos, first = find_prototypes(merged)
        protos = [p for p in protos if not re.match(r"^void (setup|loop)\(\);$", p)]
        if protos and first is not None:
            line = merged.count("\n", 0, first)
            # line number in the *original* file for the #line directive
            before = merged[:first]
            last_line_dir = before.rfind("#line 1 \"")
            fname = before[last_line_dir + 9: before.find('"', last_line_dir + 9)]
            orig_line = before.count("\n", last_line_dir) if last_line_dir >= 0 else line
            merged = (merged[:first] + "\n".join(protos) + f"\n#line {orig_line} \"{fname}\"\n" + merged[first:])
    out.append(merged)
    Path(out_cpp).write_text("".join(out))
    return sk_dir


# DaisySP classes that exist upstream but not in the copy bundled with DaisyDuino 1.7.0
NOT_IN_DAISYDUINO = {"OnePole": "use fonepole() or Tone/ATone", "LadderFilter": "use MoogLadder",
                     "Wavefolder": "use Fold", "GranularPlayer": "port granularplayer.h/.cpp into the sketch folder",
                     "Soap": "port soap.h/.cpp into the sketch folder"}


def explain(err):
    """Turn common DaisyDuino/DaisySP compile errors into a plain hint (same failures happen on hardware)."""
    hints = []
    if re.search(r"daisysp\.h: No such file", err):
        hints.append('DaisyDuino has no "daisysp.h": DaisySP comes with #include "DaisyDuino.h" '
                     "(namespace already open). Remove the daisysp.h include. Don't install standalone DaisySP.")
    for cls, alt in NOT_IN_DAISYDUINO.items():
        if re.search(rf"['‘]{cls}['’] (does not name a type|was not declared)", err):
            hints.append(f"{cls} is in current DaisySP but not in DaisyDuino's bundled copy: {alt}.")
    if re.search(r"(daisy_seed|daisy_petal|daisy_pod)\.h: No such file", err) or "DaisySeed" in err:
        hints.append("That is libDaisy (Makefile/C++) code, not DaisyDuino: use DaisyHardware hw = DAISY.init(...).")
    return ("\n\nhint: " + "\nhint: ".join(hints)) if hints else ""


def build(sketch, out_bin=None, profile="release", extra_flags=(), workdir=None):
    """Compile a sketch. Returns dict(bin, warnings[list], objects[list], log)."""
    dsp, rig = runtime_libs(profile)
    sk_dir, inos, cpps = ino_sources(sketch)
    name = sk_dir.name if Path(sketch).is_dir() else Path(sketch).stem
    wd = Path(workdir or (ROOT / "build" / "sketches" / name)) / profile
    wd.mkdir(parents=True, exist_ok=True)
    out_bin = Path(out_bin) if out_bin else wd / name
    flags = [*PROFILES[profile], *SKETCH_WARN, f"-I{sk_dir}", *extra_flags]
    objs, logs = [], []
    attempt_err = None
    for protos in (True, False):
        cpp = wd / f"{name}.ino.cpp"
        preprocess(sketch, cpp, prototypes=protos)
        cmd = [CXX, *BASE_FLAGS, *flags, *_inc(), "-c", str(cpp), "-o", str(wd / f"{name}.ino.o")]
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode == 0:
            logs.append(r.stderr)
            objs = [wd / f"{name}.ino.o"]
            break
        attempt_err = attempt_err or r.stderr
    else:
        raise RuntimeError("sketch failed to compile:\n" + attempt_err + explain(attempt_err))
    for c in cpps:
        o = wd / f"{c.stem}.o"
        r = subprocess.run([CXX, *BASE_FLAGS, *flags, *_inc(), "-c", str(c), "-o", str(o)],
                           capture_output=True, text=True)
        if r.returncode:
            raise RuntimeError(f"{c} failed to compile:\n{r.stderr}" + explain(r.stderr))
        logs.append(r.stderr)
        objs.append(o)
    link = [CXX, *PROFILES[profile], *map(str, objs), str(rig), str(dsp), "-lm", "-o", str(out_bin)]
    r = subprocess.run(link, capture_output=True, text=True)
    if r.returncode:
        raise RuntimeError("link failed:\n" + r.stderr)
    log_text = "\n".join(logs)
    warnings = [l for l in log_text.splitlines() if ": warning:" in l and "third_party" not in l]
    return {"bin": str(out_bin), "warnings": warnings, "objects": [str(o) for o in objs], "log": log_text}


# ---------------- memory estimate ----------------
SEED_LIMITS = {"sram": 512 * 1024, "sdram": 64 * 1024 * 1024}


def memcheck(objects):
    """Sum section sizes of the sketch's own objects (host build => inferred)."""
    tot = {}
    for o in objects:
        r = subprocess.run(["size", "-A", o], capture_output=True, text=True)
        for line in r.stdout.splitlines()[2:]:
            parts = line.split()
            if len(parts) >= 2 and parts[1].isdigit():
                tot[parts[0]] = tot.get(parts[0], 0) + int(parts[1])
    sram = tot.get(".bss", 0) + tot.get(".data", 0) + sum(v for k, v in tot.items() if k.startswith((".bss.", ".data.")))
    sdram = tot.get("sdram_bss", 0) + tot.get("sdram_data", 0)
    return {
        "sram_bytes": sram, "sram_limit": SEED_LIMITS["sram"],
        "sdram_bytes": sdram, "sdram_limit": SEED_LIMITS["sdram"],
        "sram_pct": round(100.0 * sram / SEED_LIMITS["sram"], 1),
        "sdram_pct": round(100.0 * sdram / SEED_LIMITS["sdram"], 2),
        "note": "host object sizes; pointer-heavy structs are larger on x86-64 than on the Cortex-M7",
    }


def largest_symbols(objects, n=8):
    """(size, section, symbol) for the biggest data objects in the sketch."""
    rows = []
    for o in objects:
        r = subprocess.run(["objdump", "-t", "-C", o], capture_output=True, text=True)
        for line in r.stdout.splitlines():
            m = re.match(r"^[0-9a-f]+\s+\S*\s+O\s+(\S+)\s+([0-9a-f]+)\s+(.*)$", line)
            if m and not m.group(1).startswith((".rodata", "*")):
                rows.append((int(m.group(2), 16), m.group(1), m.group(3).strip()))
    rows.sort(reverse=True)
    return rows[:n]
