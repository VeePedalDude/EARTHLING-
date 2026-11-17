"""Control scripts (.ctl) -> time-stamped pin events for the simulator.

Syntax (one command per line, '#' comments, ';' separates commands):
    board terrarium              # seed | terrarium | pod | petal
    set POT1 0.5                 # at t=0
    at 1.5 set POT2 0.8          # absolute time in seconds
    ramp 1.0 3.0 POT3 0 1        # linear ramp, 1 ms steps
    at 2.0 tap FS1 50ms          # press + release (active-low aware)
    at 3.0 press FS2
    at 3.5 release FS2
    at 4.0 toggle SW1 on         # on/off for toggle switches
    at 5.0 turn ENC +3           # encoder detents (pod / petal)
    set ALLPOTS 0.5              # every pot on the board
    at 1.0 noteon 60 100         # MIDI into MIDI.h sketches (optional ch2 etc.)
    at 1.5 noteoff 60 ; at 2 cc 74 64 ; at 2.5 bend -4096
Levels are 0..1 (pots) or 0/1 (raw digital level). press/release/on/off
translate to the right raw level for active-low inputs.
"""
import math
import random

from .boards import BOARDS, resolve_pin

PRESETS = {
    "noon": "set ALLPOTS 0.5",
    "min": "set ALLPOTS 0.0",
    "max": "set ALLPOTS 1.0",
    # every pot sweeps 0 -> 1 over 10%..90% of the run: zipper noise / instability
    "sweep": "set ALLPOTS 0.0\nramp {d10} {d90} ALLPOTS 0 1",
    # fast back-and-forth twiddling of every pot (worst case for smoothing)
    "twiddle": "@twiddle",
    # bypass/engage footswitch twice: pops on switching
    "footswitch": "set ALLPOTS 0.5\nat {d25} tap FS1 60ms\nat {d60} tap FS1 60ms",
    "toggles": "set ALLPOTS 0.5\nat {d25} toggle SW1 on\nat {d40} toggle SW2 on\n"
               "at {d55} toggle SW3 on\nat {d70} toggle SW4 on",
}


def _level_for(pin, action, board):
    active_low = pin in BOARDS[board]["active_low"]
    pressed = action in ("press", "on")
    return 0.0 if (pressed == active_low) else 1.0


def _pins(name, board):
    if name.upper() == "ALLPOTS":
        return list(BOARDS[board]["pots"])
    return [resolve_pin(name, board)]


def _dur(tok):
    tok = tok.lower()
    if tok.endswith("ms"):
        return float(tok[:-2]) / 1000.0
    if tok.endswith("s"):
        return float(tok[:-1])
    return float(tok)


def compile_script(text, board="seed", duration=10.0, seed=1):
    """Return (events, board). events = list of (t, pin, level)."""
    fmt = {f"d{p}": f"{duration * p / 100.0:.4f}" for p in range(0, 101, 5)}
    if text.strip() == "@twiddle":
        return _twiddle(board, duration, seed), board
    text = text.format(**fmt) if "{" in text else text
    events = []
    for raw in text.splitlines():
        for cmd in raw.split("#", 1)[0].split(";"):
            tok = cmd.split()
            if not tok:
                continue
            t = 0.0
            if tok[0] == "board":
                board = tok[1].lower()
                if board not in BOARDS:
                    raise ValueError(f"unknown board {board}")
                continue
            if tok[0] == "at":
                t = _dur(tok[1])
                tok = tok[2:]
            op = tok[0]
            if op == "set":
                for p in _pins(tok[1], board):
                    events.append((t, p, float(tok[2])))
            elif op == "ramp":
                t0, t1 = _dur(tok[1]), _dur(tok[2])
                v0, v1 = float(tok[4]), float(tok[5])
                steps = max(1, int((t1 - t0) * 1000))
                for p in _pins(tok[3], board):
                    for i in range(steps + 1):
                        events.append((t0 + (t1 - t0) * i / steps, p, v0 + (v1 - v0) * i / steps))
            elif op in ("press", "release"):
                for p in _pins(tok[1], board):
                    events.append((t, p, _level_for(p, op, board)))
            elif op == "tap":
                hold = _dur(tok[2]) if len(tok) > 2 else 0.05
                for p in _pins(tok[1], board):
                    events.append((t, p, _level_for(p, "press", board)))
                    events.append((t + hold, p, _level_for(p, "release", board)))
            elif op == "toggle":
                state = tok[2].lower() if len(tok) > 2 else "on"
                for p in _pins(tok[1], board):
                    events.append((t, p, _level_for(p, "on" if state == "on" else "off", board)))
            elif op == "turn":
                enc = BOARDS[board]["encoder"]
                if not enc:
                    raise ValueError(f"board {board} has no encoder")
                a, b = enc
                n = int(tok[2])
                step = 0.004  # 4 ms per quadrature phase
                first, second = (b, a) if n > 0 else (a, b)  # DaisyDuino: B low first = +1
                tt = t
                for _ in range(abs(n)):
                    events += [(tt, first, 0.0), (tt + step, second, 0.0),
                               (tt + 2 * step, first, 1.0), (tt + 3 * step, second, 1.0)]
                    tt += 4 * step
            elif op in ("noteon", "noteoff", "cc", "program", "bend"):
                ch = int(tok[-1][2:]) if tok[-1].startswith("ch") else 1
                args = [a for a in tok[1:] if not a.startswith("ch")]
                st = {"noteon": 0x90, "noteoff": 0x80, "cc": 0xB0, "program": 0xC0, "bend": 0xE0}[op] | ((ch - 1) & 15)
                if op == "bend":
                    v = int(args[0]) + 8192
                    events.append((t, "M", st, v & 127, (v >> 7) & 127))
                elif op == "program":
                    events.append((t, "M", st, int(args[0]), 0))
                else:
                    d2 = int(args[1]) if len(args) > 1 else (0 if op == "noteoff" else 100)
                    events.append((t, "M", st, int(args[0]), d2))
            else:
                raise ValueError(f"unknown control command: {cmd.strip()}")
    events.sort(key=lambda e: e[0])
    return events, board


def _twiddle(board, duration, seed):
    rng = random.Random(seed)
    events = []
    pots = BOARDS[board]["pots"]
    for i, p in enumerate(pots):
        phase = rng.random() * 2 * math.pi
        rate = 3.0 + i * 0.7  # Hz
        t = 0.0
        while t < duration:
            v = 0.5 + 0.5 * math.sin(2 * math.pi * rate * t + phase)
            events.append((t, p, v))
            t += 0.001
    events.sort(key=lambda e: e[0])
    return events


def load(spec, board="seed", duration=10.0):
    """spec = preset name or path to a .ctl file."""
    if spec in PRESETS:
        return compile_script(PRESETS[spec], board, duration)
    with open(spec) as f:
        return compile_script(f.read(), board, duration)


def write_events(events, path):
    with open(path, "w") as f:
        for e in events:
            if e[1] == "M":
                f.write(f"{e[0]:.6f} M {e[2]} {e[3]} {e[4]}\n")
            else:
                f.write(f"{e[0]:.6f} {e[1]} {e[2]:.6f}\n")
