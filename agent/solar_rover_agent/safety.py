"""Safety rules applied on top of whatever drives the car (policy or script)."""
from __future__ import annotations

import numpy as np


def side_guard(act, left_m: float, right_m: float, slow_m: float):
    """Something close on a front corner while driving forward: slow down in
    proportion to how close it is, and if the command isn't already turning
    away, turn away. The simulator runs the same rule
    (sim/solar_rover_sim/env.py side_guard; a test there checks they agree).

    act is (left, right) in -1..1; distances in metres (2.0 = nothing seen)."""
    a = np.asarray(act, dtype=float)
    fwd, turn = 0.5 * (a[0] + a[1]), 0.5 * (a[1] - a[0])   # turn > 0 = left
    near = min(left_m, right_m)
    if fwd <= 0 or near >= slow_m:
        return a
    away = -1.0 if left_m < right_m else 1.0
    fwd *= max(near, 0.0) / slow_m
    if turn * away <= 0:
        turn = 0.4 * away
    return np.array([fwd - turn, fwd + turn])


def side_m(tel) -> tuple[float, float]:
    """Corner sensor readings from telemetry in metres; no reading or not fitted = 2.0."""
    side = getattr(tel, "side", None) or [-1, -1]
    return tuple(2.0 if (v is None or v < 0) else v / 1000.0 for v in side[:2])
