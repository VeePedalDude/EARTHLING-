"""Real target compile (arm-none-eabi via arduino-cli + STM32duino core + DaisyDuino).

Gives true flash / RAM numbers and catches code that builds on the host shim
but not on the Daisy (missing headers, ARM-only issues, the 128 KB flash limit).
"""
import os
import platform
import re
import shutil
import subprocess
import tarfile
import urllib.request
from pathlib import Path

from . import build as B

TOOLS = B.ROOT / "build" / "tools"
STM32_INDEX = "https://github.com/stm32duino/BoardManagerFiles/raw/main/package_stmicroelectronics_index.json"
CLI_VERSION = "1.1.1"
FQBN = {
    "seed": "STMicroelectronics:stm32:GenH7:pnum=DAISY_SEED",
    "patch_sm": "STMicroelectronics:stm32:GenH7:pnum=DAISY_PATCH_SM",
}


def cli_path():
    p = shutil.which("arduino-cli")
    if p:
        return p
    local = TOOLS / "arduino-cli"
    return str(local) if local.exists() else None


def _run(args, timeout=900):
    return subprocess.run(args, capture_output=True, text=True, timeout=timeout)


def setup_hw():
    """Install arduino-cli (if missing) and the STM32 core into the normal
    Arduino data dir (~/.arduino15), which the Arduino IDE shares."""
    cli = cli_path()
    if not cli:
        TOOLS.mkdir(parents=True, exist_ok=True)
        arch = {"x86_64": "64bit", "aarch64": "ARM64", "arm64": "ARM64"}.get(platform.machine(), "64bit")
        osname = {"Linux": "Linux", "Darwin": "macOS"}.get(platform.system(), "Linux")
        url = (f"https://github.com/arduino/arduino-cli/releases/download/v{CLI_VERSION}/"
               f"arduino-cli_{CLI_VERSION}_{osname}_{arch}.tar.gz")
        B.log(f"downloading {url}")
        tgz = TOOLS / "arduino-cli.tgz"
        urllib.request.urlretrieve(url, tgz)
        with tarfile.open(tgz) as t:
            t.extract("arduino-cli", TOOLS)
        cli = str(TOOLS / "arduino-cli")
    if _run([cli, "config", "dump"]).returncode != 0:
        _run([cli, "config", "init"])
    _run([cli, "config", "add", "board_manager.additional_urls", STM32_INDEX])
    r = _run([cli, "core", "update-index"])
    B.log("core index: " + ("ok" if r.returncode == 0 else "partial (arduino.cc unreachable is fine)"))
    data = Path.home() / ".arduino15"
    for f, body in (("library_index.json", '{"libraries":[]}'), ("package_index.json", '{"packages":[]}')):
        if not (data / f).exists():  # offline stand-ins so the CLI can start
            data.mkdir(exist_ok=True)
            (data / f).write_text(body)
    B.log("installing STMicroelectronics:stm32 (large download, several minutes) ...")
    r = _run([cli, "core", "install", "STMicroelectronics:stm32"], timeout=3600)
    if r.returncode != 0:
        raise RuntimeError(r.stderr[-2000:])
    return cli


def _fake_ctags():
    d = TOOLS / "noctags"
    d.mkdir(parents=True, exist_ok=True)
    f = d / "ctags"
    if not f.exists():
        f.write_text("#!/bin/sh\nexit 0\n")
        f.chmod(0o755)
    return d


def _parse(out):
    res = {}
    m = re.search(r"Sketch uses (\d+) bytes \((\d+)%\) of program storage space\. Maximum is (\d+)", out)
    if m:
        res.update(flash_bytes=int(m[1]), flash_pct=int(m[2]), flash_max=int(m[3]))
    m = re.search(r"Global variables use (\d+) bytes \((\d+)%\) of dynamic memory.*?Maximum is (\d+)", out, re.S)
    if m:
        res.update(ram_bytes=int(m[1]), ram_pct=int(m[2]), ram_max=int(m[3]))
    return res


def hwcheck(sketch, board="seed", workdir=None):
    cli = cli_path()
    if not cli:
        return {"ok": False, "skipped": True, "reason": "arduino-cli not found (run `daisyrig setup --hw`)"}
    fqbn = FQBN.get(board, FQBN["seed"])
    libs = []
    user_dd = list((Path.home() / "Arduino" / "libraries").glob("DaisyDuino*"))
    if not user_dd:
        stage_libs = B.ROOT / "build" / "hwlibs"
        stage_libs.mkdir(parents=True, exist_ok=True)
        link = stage_libs / "DaisyDuino"
        if not link.exists():
            link.symlink_to(B.TP)
        libs = ["--libraries", str(stage_libs)]
    sk_dir, inos, cpps = B.ino_sources(sketch)
    base = [cli, "compile", "-b", fqbn, "--warnings", "all", *libs]
    r = _run([*base, str(sk_dir)])
    mode = "arduino (native ctags)"
    out = r.stdout + r.stderr
    if r.returncode != 0 and "ctags" in out:
        # No Arduino ctags available: stage the sketch with daisyrig's own prototypes.
        name = sk_dir.name if Path(sketch).is_dir() else Path(sketch).stem
        stage = Path(workdir or (B.ROOT / "build" / "hwstage")) / name
        if stage.exists():
            shutil.rmtree(stage)
        stage.mkdir(parents=True)
        B.preprocess(sketch, stage / f"{name}.ino", prototypes=True)
        for f in [*cpps, *sk_dir.glob("*.h"), *sk_dir.glob("*.hpp")]:
            shutil.copy(f, stage / f.name)
        r = _run([*base, "--build-property", f"runtime.tools.ctags.path={_fake_ctags()}", str(stage)])
        mode = "staged (daisyrig prototypes, no ctags)"
        out = r.stdout + r.stderr
    res = {"ok": r.returncode == 0, "fqbn": fqbn, "mode": mode, **_parse(out)}
    lines = [l for l in out.splitlines() if "discovery" not in l and "Error initializing" not in l]
    res["errors"] = [l for l in lines if ": error:" in l][:20]
    res["warnings"] = [l for l in lines if ": warning:" in l and "/libraries/" not in l
                       and "/packages/" not in l and "DaisyDuino/src" not in l][:40]
    if not res["ok"] and not res["errors"]:
        res["errors"] = lines[-15:]
    return res
