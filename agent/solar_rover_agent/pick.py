"""Scripted pick-up: turn toward a coloured object, creep up until the ToF
reads ~15 cm, then reach down with IK, close the gripper and lift. This is a
starting point for a learned grasping policy, not one itself."""
from __future__ import annotations

import time

from . import arm_ik

GRAB_DISTANCE_M = 0.07     # ToF distance (from the nose) where the object is in reach
# (with the gripper pointing down the arm reaches the ground up to ~18 cm from
# the turret axis, i.e. ~11 cm ahead of the nose)
NOSE_FROM_TURRET_M = 0.074 # nose front face ahead of the turret axis (182 - 108 mm)


class PickRoutine:
    def __init__(self, link, finder):
        self.link, self.finder = link, finder
        self.state = "search"
        self.t0 = time.monotonic()

    def step(self, tel):
        """Returns wheel commands (left, right), or None when the arm is doing the work."""
        s = self.state
        if s == "search":
            blob = self.finder.find()
            if not blob:
                return -0.25, 0.25                      # spin slowly to look around
            self.state = "approach"
            return 0.0, 0.0
        if s == "approach":
            blob = self.finder.find()
            if not blob:
                self.state = "search"
                return 0.0, 0.0
            turn = max(-0.3, min(0.3, blob["bearing_deg"] / 30.0))
            dist = tel.tof / 1000.0 if tel.tof > 0 else 2.0
            if dist <= GRAB_DISTANCE_M + 0.01 and abs(blob["bearing_deg"]) < 4:
                self.state, self.t0 = "reach", time.monotonic()
                self.link.send("G 90")
                return 0.0, 0.0
            fwd = 0.25 if dist > 0.4 else 0.12
            return fwd - turn, fwd + turn
        if s == "reach":
            x = NOSE_FROM_TURRET_M + (tel.tof / 1000.0 if tel.tof > 0 else GRAB_DISTANCE_M) + 0.015
            try:
                q = arm_ik.solve(min(x, 0.175), 0.0, 0.03)
            except arm_ik.Unreachable:
                self.state = "approach"             # too far or too close: drive again
                return 0.0, 0.0
            self.link.arm(q["yaw"], q["shoulder"], q["elbow"], q["wrist"], 90)
            if time.monotonic() - self.t0 > 3.0:
                self.state, self.t0 = "grip", time.monotonic()
                self.link.send("G 0")
            return None
        if s == "grip":
            if time.monotonic() - self.t0 > 1.0:
                self.link.send("P carry")
                self.state = "done"
            return None
        return None
