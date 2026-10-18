"""What the agent is trying to do right now, and the safety rules around the policy."""
from __future__ import annotations

import math
import time

import numpy as np

from .observation import from_telemetry


class Navigator:
    """Runs the trained policy toward waypoints, or toward sunlight when the
    battery is low, and parks once it has found a sunny spot."""

    def __init__(self, policy, waypoints, speed_scale=0.7, stop_tof_mm=130, park_after_s=20.0, sun_mode="scripted"):
        self.policy = policy
        self.sun_mode = sun_mode
        self.sun = SunSeeker()
        self.waypoints = [tuple(w) for w in waypoints] or [(1.0, 0.0)]
        self.wp = 0
        self.speed_scale = speed_scale
        self.stop_tof_mm = stop_tof_mm
        self.park_after_s = park_after_s
        self.prev_action = np.zeros(2)
        self.prev_solar = 0.0
        self.task = "goal"
        self.parked = False
        self._sun_still_since = None
        self._best_solar = 0.0

    def choose_task(self, tel):
        if tel.low or tel.bat_pct < 30:
            if self.task != "sun":
                self.task, self.parked, self._sun_still_since, self._best_solar = "sun", False, None, 0.0
                self.sun.reset()
        elif tel.bat_pct > 60 and self.task == "sun":
            self.task, self.parked = "goal", False
        return self.task

    def step(self, tel, odo, dt=0.1):
        """Returns (left, right) wheel commands in -1..1, or None to send nothing."""
        task = self.choose_task(tel)
        if self.parked:
            return None
        if task == "sun" and self.sun_mode == "scripted":
            tof = 2.0 if tel.tof < 0 else tel.tof / 1000.0
            l, r = self.sun.step(tel.sol_mw / 1000.0, odo.x, odo.y, odo.th, math.radians(tel.imu[5]), tof, dt)
            if self.sun.parked:
                self.parked = True
            return l * self.speed_scale / 0.7, r * self.speed_scale / 0.7
        goal = self.waypoints[self.wp] if task == "goal" else None
        if goal and math.hypot(goal[0] - odo.x, goal[1] - odo.y) < 0.15:
            self.wp = (self.wp + 1) % len(self.waypoints)
            goal = self.waypoints[self.wp]
        obs = from_telemetry(tel, odo, task, goal, self.prev_action, self.prev_solar)
        act = np.clip(self.policy.act(obs), -1, 1)
        self.prev_action = act
        self.prev_solar = tel.sol_mw / 1000.0

        # safety: never drive forward into something right in front of the sensor
        if 0 <= tel.tof < self.stop_tof_mm and act[0] + act[1] > 0:
            fwd = 0.5 * (act[0] + act[1])
            act = act - fwd

        if task == "sun":
            solar = tel.sol_mw / 1000.0
            moving = abs(act[0]) + abs(act[1]) > 0.15
            if solar >= 0.9 * self._best_solar and not moving:
                self._sun_still_since = self._sun_still_since or time.monotonic()
                if time.monotonic() - self._sun_still_since > self.park_after_s:
                    self.parked = True
                    return 0.0, 0.0
            else:
                self._sun_still_since = None
            self._best_solar = max(self._best_solar, solar)
        return float(act[0] * self.speed_scale), float(act[1] * self.speed_scale)


class SunSeeker:
    """Scripted 'find the sun' routine used when the battery is low (default;
    set sun_mode: policy in config.yaml to use the learned policy instead).

    The panel lies flat on the roof, so turning on the spot tells us nothing:
    only moving changes the reading. This is a run-and-tumble search: drive a
    short leg, keep going while the solar power rises, turn ~120 degrees when
    it falls, remember the brightest spot (from odometry), and after a few
    legs without progress drive back to that spot and park."""

    SPIN = 0.35
    LEG_S = 2.0
    FWD = 0.35

    def __init__(self, max_legs=14, patience=4):
        self.max_legs, self.patience = max_legs, patience
        self.reset()

    def reset(self):
        self.state, self.parked = "leg", False
        self.legs = self.fails = 0
        self.t, self.leg_start_w = 0.0, None
        self.best = None            # (watts, x, y)
        self.turn_left = 0.0
        self.sign = 1

    def _turn(self, yaw_rate, dt):
        self.turn_left -= abs(yaw_rate) * dt
        return (-self.SPIN * self.sign, self.SPIN * self.sign) if self.turn_left > 0 else None

    def step(self, solar_w, x, y, heading, yaw_rate, tof_m, dt):
        """Returns (left, right) in -1..1. Positions in m, heading in rad, yaw_rate in rad/s."""
        if self.parked:
            return 0.0, 0.0
        if self.best is None or solar_w > self.best[0]:
            self.best = (solar_w, x, y)
        if self.state == "turn":
            cmd = self._turn(yaw_rate, dt)
            if cmd:
                return cmd
            self.state, self.t, self.leg_start_w = "leg", 0.0, solar_w
        if self.state == "leg":
            if self.leg_start_w is None:
                self.leg_start_w = solar_w
            self.t += dt
            blocked = tof_m < 0.35
            if self.t < self.LEG_S and not blocked:
                return self.FWD, self.FWD
            self.legs += 1
            improved = solar_w > self.leg_start_w * 1.03 + 0.02
            self.fails = 0 if improved else self.fails + 1
            if self.legs >= self.max_legs or self.fails >= self.patience:
                self.state = "return"
            elif improved and not blocked:
                self.t, self.leg_start_w = 0.0, solar_w        # keep going this way
                return self.FWD, self.FWD
            else:
                self.state, self.turn_left = "turn", math.radians(120)
                self.sign = -self.sign
                return 0.0, 0.0
        if self.state == "return":
            _, bx, by = self.best
            dx, dy = bx - x, by - y
            if math.hypot(dx, dy) < 0.12:
                self.parked = True
                return 0.0, 0.0
            err = (math.atan2(dy, dx) - heading + math.pi) % (2 * math.pi) - math.pi
            if abs(err) > math.radians(15):
                s = self.SPIN if err > 0 else -self.SPIN
                return -s, s
            if tof_m < 0.3:
                self.parked = True          # something is in the way: good enough
                return 0.0, 0.0
            return self.FWD * 0.7, self.FWD * 0.7
        return 0.0, 0.0
