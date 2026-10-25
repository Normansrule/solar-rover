"""Record what the car was told and what it measured, one JSON line per step.

The simulator's fitter (sim/solar_rover_sim/fit.py) reads these files to tune
the simulator's motor numbers to the real car. Each line is

    {"t": <seconds>, "cmd": [left, right], "tel": {<telemetry as the car sent it>}}

with cmd in -1..1 (the D command divided by 1000), or null while nothing was sent.
"""
from __future__ import annotations

import dataclasses
import json
import pathlib
import time


class RunLog:
    def __init__(self, path, clock=time.monotonic):
        self.clock = clock
        self.path = pathlib.Path(path)
        self.path.parent.mkdir(parents=True, exist_ok=True)
        self.f = open(self.path, "a", buffering=1)
        self.t0 = clock()

    def write(self, tel, cmd):
        d = dataclasses.asdict(tel)
        d.pop("received_at", None)
        row = {"t": round(self.clock() - self.t0, 3),
               "cmd": None if cmd is None else [round(float(cmd[0]), 3), round(float(cmd[1]), 3)],
               "tel": d}
        self.f.write(json.dumps(row) + "\n")

    def close(self):
        self.f.close()


def read(path):
    """Rows of a run log as dicts (skips blank or broken lines)."""
    rows = []
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                rows.append(json.loads(line))
            except ValueError:
                continue
    return rows
