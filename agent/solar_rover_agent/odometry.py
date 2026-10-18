"""Dead reckoning from wheel encoders + gyro."""
from __future__ import annotations

import math

TRACK_M = 0.174
MM_PER_EDGE = math.pi * 65.0 / 40


class Odometry:
    def __init__(self):
        self.x = self.y = self.th = 0.0
        self._prev = None

    def reset(self):
        self.x = self.y = self.th = 0.0
        self._prev = None

    def update(self, enc, gyro_z_dps: float, dt: float):
        if self._prev is None:
            self._prev = list(enc)
            return
        d = [(e - p) * MM_PER_EDGE / 1000.0 for e, p in zip(enc, self._prev)]
        self._prev = list(enc)
        dl = 0.5 * (d[0] + d[1])
        dr = 0.5 * (d[2] + d[3])
        ds = 0.5 * (dl + dr)
        # heading from the gyro (skid steer makes wheel-based heading unreliable)
        dth = math.radians(gyro_z_dps) * dt if dt > 0 else (dr - dl) / TRACK_M
        self.th = (self.th + dth + math.pi) % (2 * math.pi) - math.pi
        self.x += ds * math.cos(self.th)
        self.y += ds * math.sin(self.th)

    def to_body(self, gx: float, gy: float):
        dx, dy = gx - self.x, gy - self.y
        c, s = math.cos(-self.th), math.sin(-self.th)
        return c * dx - s * dy, s * dx + c * dy
