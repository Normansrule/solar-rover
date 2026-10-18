"""Inverse kinematics for the arm (logical angles as the firmware uses them).

Frame: origin on the turret axis at ground level, x forward, z up.
Shoulder axis height and link lengths come from hardware/scad/config.scad;
tools/fit_check.py checks them against the 3D model.
Angles: shoulder 0 = horizontal, + up. Elbow 0 = straight, + folds down.
Wrist 0 = in line with the forearm, + up. Yaw 0 = forward, + left.
"""
from __future__ import annotations

import math

SHOULDER_Z = 0.1476     # m above ground
SHOULDER_X = 0.0        # on the turret axis
L1, L2, L3 = 0.100, 0.090, 0.0735   # L3: wrist axis to the fingertip ends
HAND_OFF = 0.005        # finger midplane sits 5 mm off the hand axis, on the "up" side
LIMITS = {"yaw": (-90, 90), "shoulder": (-35, 120), "elbow": (0, 150), "wrist": (-90, 90)}


class Unreachable(ValueError):
    pass


def solve(x: float, y: float, z: float, pitch_deg: float = -90.0):
    """Joint angles (deg) that put the fingertip at (x, y, z) relative to the turret
    axis at ground level, with the gripper pointing at `pitch_deg` (-90 = straight down)."""
    yaw = math.degrees(math.atan2(y, x))
    r = math.hypot(x, y) - SHOULDER_X
    p = math.radians(pitch_deg)
    wx = r - L3 * math.cos(p) + HAND_OFF * math.sin(p)
    wz = z - SHOULDER_Z - L3 * math.sin(p) - HAND_OFF * math.cos(p)
    d = math.hypot(wx, wz)
    if d > L1 + L2 - 1e-6 or d < abs(L1 - L2) + 1e-6:
        raise Unreachable(f"target {d:.3f} m from the shoulder; reach is {abs(L1 - L2):.3f}..{L1 + L2:.3f} m")
    cos_int = (L1 ** 2 + L2 ** 2 - d ** 2) / (2 * L1 * L2)
    elbow = math.pi - math.acos(max(-1, min(1, cos_int)))           # + folds down
    cos_a = (L1 ** 2 + d ** 2 - L2 ** 2) / (2 * L1 * d)
    shoulder = math.atan2(wz, wx) + math.acos(max(-1, min(1, cos_a)))  # elbow-up solution
    fore_abs = shoulder - elbow
    wrist = p - fore_abs
    out = {"yaw": yaw, "shoulder": math.degrees(shoulder), "elbow": math.degrees(elbow), "wrist": math.degrees(wrist)}
    for k, (lo, hi) in LIMITS.items():
        if not lo - 0.5 <= out[k] <= hi + 0.5:
            raise Unreachable(f"{k} would be {out[k]:.0f} deg (limit {lo}..{hi})")
    return out


def forward(yaw, shoulder, elbow, wrist):
    """Fingertip position (x, y, z) for given joint angles in degrees."""
    s, e, w = map(math.radians, (shoulder, elbow, wrist))
    a1 = s; a2 = s - e; a3 = a2 + w
    r = SHOULDER_X + L1 * math.cos(a1) + L2 * math.cos(a2) + L3 * math.cos(a3) - HAND_OFF * math.sin(a3)
    z = SHOULDER_Z + L1 * math.sin(a1) + L2 * math.sin(a2) + L3 * math.sin(a3) + HAND_OFF * math.cos(a3)
    yr = math.radians(yaw)
    return r * math.cos(yr), r * math.sin(yr), z
