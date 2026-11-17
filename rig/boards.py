"""Pin maps for common Daisy Seed hosts. Pin numbers are Daisy Seed D-numbers."""

SEED_ANALOG = {"A0": 15, "A1": 16, "A2": 17, "A3": 18, "A4": 19, "A5": 20,
               "A6": 21, "A7": 22, "A8": 23, "A9": 24, "A10": 25, "A11": 28}

BOARDS = {
    # Bare Seed: only A*/D* names. ALLPOTS = every ADC pin.
    "seed": {
        "names": {},
        "pots": list(SEED_ANALOG.values()),
        "active_low": [],
        "encoder": None,
    },
    # PedalPCB Terrarium (Arduino pin numbers, from the PedalPCB Arduino template
    # and terrarium.h's mapping onto the Petal switch pins).
    "terrarium": {
        "names": {"POT1": 16, "POT2": 17, "POT3": 18, "POT4": 19, "POT5": 20, "POT6": 21,
                  "SW1": 10, "SW2": 9, "SW3": 8, "SW4": 7,
                  "FS1": 25, "FS2": 26, "LED1": 22, "LED2": 23},
        "pots": [16, 17, 18, 19, 20, 21],
        "active_low": [10, 9, 8, 7, 25, 26],   # wired to ground with INPUT_PULLUP
        "encoder": None,
    },
    "pod": {
        "names": {"KNOB1": 21, "KNOB2": 15, "SW1": 27, "SW2": 28,
                  "ENC_A": 26, "ENC_B": 25, "ENC_CLICK": 13},
        "pots": [21, 15],
        "active_low": [27, 28, 13],
        "encoder": (26, 25),
    },
    "petal": {
        "names": {"POT1": 16, "POT2": 19, "POT3": 17, "POT4": 20, "POT5": 18, "POT6": 21,
                  "EXPR": 15, "SW1": 8, "SW2": 9, "SW3": 10, "SW4": 13, "SW5": 25,
                  "SW6": 26, "SW7": 7, "ENC_A": 28, "ENC_B": 27, "ENC_CLICK": 14},
        "pots": [16, 19, 17, 20, 18, 21],
        "active_low": [8, 9, 10, 13, 25, 26, 7, 14],
        "encoder": (28, 27),
    },
}


def resolve_pin(name, board):
    n = name.upper()
    b = BOARDS[board]
    if n in b["names"]:
        return b["names"][n]
    if n in SEED_ANALOG:
        return SEED_ANALOG[n]
    if n.startswith("D") and n[1:].isdigit():
        return int(n[1:])
    if n.isdigit():
        return int(n)
    raise ValueError(f"unknown pin '{name}' for board '{board}' "
                     f"(known: {', '.join(sorted(b['names'])) or 'A0-A11, D0-D32'})")
