"""Fast 2-D simulator of the Solar Rover for reinforcement learning.

The car is modelled as a skid-steer base with first-order motor lag, random
motor strength, wheel slip noise, one forward time-of-flight distance sensor,
a yaw-rate gyro, wheel encoders, a battery and a solar panel that reads a
random light field. The observation vector is built exactly like the one the
real car's agent builds from telemetry (agent/solar_rover_agent/observation.py),
so a policy trained here runs on the car unchanged.

Tasks
  "goal": drive to a goal point (given in the car's odometry frame) without
          hitting anything.
  "sun" : find the sunniest spot nearby and stop there (what the car does when
          its battery is low). Only the solar power reading is available, like
          on the real car.
"""
from __future__ import annotations

import math
from dataclasses import dataclass, field

import numpy as np

try:
    import gymnasium as gym
    from gymnasium import spaces
    _Base = gym.Env
except ImportError:  # the env also works without gymnasium (plain reset/step)
    gym = None
    spaces = None
    _Base = object

OBS_DIM = 14
ACT_DIM = 2


@dataclass
class CarParams:
    track: float = 0.174          # m, wheel centre to wheel centre
    radius: float = 0.16          # m, collision radius of the body
    v_max: float = 0.60           # m/s at command 1.0 (closed-loop wheel speed)
    tau: float = 0.15             # s, motor + ramp lag
    turn_eff: float = 0.85        # skid steer turns less than ideal: yaw = turn_eff * (vr - vl) / track
    dt: float = 0.1               # s, agent step (10 Hz)
    tof_max: float = 2.0          # m
    tof_noise: float = 0.02       # m
    solar_peak_w: float = 4.0
    battery_wh: float = 44.0
    world: float = 4.0            # m, square arena side
    n_obstacles: tuple = (3, 9)
    episode_s: float = 40.0
    # domain randomisation ranges
    strength: tuple = (0.75, 1.1)
    slip: float = 0.05

    @classmethod
    def load(cls, path: str) -> "CarParams":
        """Defaults overridden by the "params" of a fit file from fit.py."""
        import json
        with open(path) as f:
            d = json.load(f)
        d = d.get("params", d)
        known = {k: (tuple(v) if isinstance(v, list) else v) for k, v in d.items() if k in cls.__dataclass_fields__}
        return cls(**known)


@dataclass
class _State:
    x: float = 0.0
    y: float = 0.0
    th: float = 0.0
    vl: float = 0.0
    vr: float = 0.0
    batt: float = 0.8
    odo: np.ndarray = field(default_factory=lambda: np.zeros(3))


class SolarRoverEnv(_Base):
    metadata = {"render_modes": []}

    def __init__(self, task: str = "mixed", params: CarParams | None = None, seed: int | None = None):
        self.p = params or CarParams()
        self.task_mode = task
        self.rng = np.random.default_rng(seed)
        if spaces is not None:
            self.observation_space = spaces.Box(-5, 5, (OBS_DIM,), np.float32)
            self.action_space = spaces.Box(-1, 1, (ACT_DIM,), np.float32)
        self.s = _State()

    # ------------------------------------------------------------------ world
    def _make_world(self):
        p, r = self.p, self.rng
        half = p.world / 2
        n = r.integers(p.n_obstacles[0], p.n_obstacles[1] + 1)
        self.obs_c = r.uniform(-half + 0.3, half - 0.3, (n, 2))
        self.obs_r = r.uniform(0.08, 0.3, n)
        # light field: a few bright patches + ambient; obstacles cast short shadows
        k = r.integers(2, 5)
        self.sun_c = r.uniform(-half + 0.4, half - 0.4, (k, 2))
        self.sun_w = r.uniform(0.4, 1.0, k)
        self.sun_s = r.uniform(0.3, 0.9, k)
        self.sun_dir = r.uniform(0, 2 * math.pi)
        self.ambient = r.uniform(0.02, 0.12)

    def light(self, x: float, y: float) -> float:
        d2 = np.sum((self.sun_c - [x, y]) ** 2, axis=1)
        val = self.ambient + float(np.max(self.sun_w * np.exp(-d2 / (2 * self.sun_s ** 2))))
        # shadow: obstacle within 0.4 m toward the sun
        sx, sy = math.cos(self.sun_dir), math.sin(self.sun_dir)
        rel = self.obs_c - [x, y]
        along = rel @ np.array([sx, sy])
        perp = np.abs(rel @ np.array([-sy, sx]))
        if np.any((along > 0) & (along < 0.4 + self.obs_r) & (perp < self.obs_r)):
            val *= 0.25
        return min(val, 1.0)

    def _free(self, x, y, margin):
        half = self.p.world / 2 - margin
        if abs(x) > half or abs(y) > half:
            return False
        return bool(np.all(np.hypot(*(self.obs_c - [x, y]).T) > self.obs_r + margin))

    def _sample_free(self, margin):
        for _ in range(500):
            x, y = self.rng.uniform(-self.p.world / 2, self.p.world / 2, 2)
            if self._free(x, y, margin):
                return x, y
        return 0.0, 0.0

    def _tof(self) -> float:
        p, s = self.p, self.s
        ox = s.x + math.cos(s.th) * 0.15
        oy = s.y + math.sin(s.th) * 0.15
        dx, dy = math.cos(s.th), math.sin(s.th)
        best = p.tof_max
        # obstacles (circles)
        rel = self.obs_c - [ox, oy]
        b = rel @ np.array([dx, dy])
        c = np.sum(rel ** 2, axis=1) - self.obs_r ** 2
        disc = b * b - c
        hit = (disc > 0) & (b > 0)
        if np.any(hit):
            t = b[hit] - np.sqrt(disc[hit])
            t = t[t > 0]
            if t.size:
                best = min(best, float(t.min()))
        # walls
        half = p.world / 2
        for comp, d in ((ox, dx), (oy, dy)):
            if abs(d) > 1e-6:
                t = ((half if d > 0 else -half) - comp) / d
                if t > 0:
                    best = min(best, t)
        return best

    # ------------------------------------------------------------------ gym api
    def reset(self, *, seed: int | None = None, options: dict | None = None):
        if seed is not None:
            self.rng = np.random.default_rng(seed)
        p, r = self.p, self.rng
        self._make_world()
        x, y = self._sample_free(p.radius + 0.15)
        self.s = _State(x=x, y=y, th=r.uniform(-math.pi, math.pi), batt=r.uniform(0.2, 1.0))
        self.s.odo = np.zeros(3)
        self.strength = r.uniform(*p.strength, size=2)
        self.task = self.task_mode if self.task_mode != "mixed" else ("goal" if r.random() < 0.6 else "sun")
        gx, gy = self._sample_free(p.radius + 0.1)
        for _ in range(50):
            if 0.8 < math.hypot(gx - x, gy - y) < 3.0:
                break
            gx, gy = self._sample_free(p.radius + 0.1)
        self.goal_w = np.array([gx, gy])
        self.t = 0
        self.prev_action = np.zeros(2)
        self.solar = self.light(x, y) * p.solar_peak_w
        self.prev_solar = self.solar
        self.best_light = self.light(x, y)
        self.prev_dist = self._goal_dist()
        return self._obs(), {"task": self.task}

    def _goal_dist(self):
        return float(math.hypot(*(self.goal_w - [self.s.x, self.s.y])))

    def _obs(self):
        p, s = self.p, self.s
        # goal relative to the car body frame (from true pose; on the car this
        # comes from odometry, whose drift is modelled by the noise below)
        dx, dy = self.goal_w - [s.x, s.y]
        c, si = math.cos(-s.th), math.sin(-s.th)
        bx, by = c * dx - si * dy, si * dx + c * dy
        bx += self.rng.normal(0, 0.03); by += self.rng.normal(0, 0.03)
        dist = math.hypot(bx, by)
        is_goal = 1.0 if self.task == "goal" else 0.0
        if not is_goal:
            bx = by = dist = 0.0
        gz = (s.vr - s.vl) / p.track + self.rng.normal(0, 0.02)
        tof = min(self._tof() + self.rng.normal(0, p.tof_noise), p.tof_max)
        return build_obs(
            goal_bx=bx, goal_by=by, goal_dist=dist,
            v_left=s.vl / p.v_max, v_right=s.vr / p.v_max, yaw_rate=gz,
            tof_m=tof, batt=s.batt, solar_w=self.solar, solar_delta_w=self.solar - self.prev_solar,
            task=self.task, prev_action=self.prev_action,
        )

    def step(self, action):
        p, s = self.p, self.s
        a = np.clip(np.asarray(action, dtype=float), -1, 1)
        # motor lag + strength + slip
        alpha = 1 - math.exp(-p.dt / p.tau)
        tl = a[0] * p.v_max * self.strength[0]
        tr = a[1] * p.v_max * self.strength[1]
        s.vl += alpha * (tl - s.vl)
        s.vr += alpha * (tr - s.vr)
        slip = 1 + self.rng.normal(0, p.slip, 2)
        v = 0.5 * (s.vl * slip[0] + s.vr * slip[1])
        w = (s.vr * slip[1] - s.vl * slip[0]) / p.track * p.turn_eff   # skid-steer turns less than the ideal
        nx = s.x + v * math.cos(s.th) * p.dt
        ny = s.y + v * math.sin(s.th) * p.dt
        s.th = (s.th + w * p.dt + math.pi) % (2 * math.pi) - math.pi
        collided = not self._free(nx, ny, p.radius)
        if not collided:
            s.x, s.y = nx, ny
        # energy: ~3 W base + up to 9 W for the motors
        watts = 3.0 + 9.0 * 0.5 * (abs(a[0]) + abs(a[1]))
        self.prev_solar = self.solar
        lit = self.light(s.x, s.y)
        self.solar = lit * p.solar_peak_w * (1 + self.rng.normal(0, 0.03))
        s.batt = float(np.clip(s.batt + (self.solar * 0.85 - watts) * p.dt / 3600 / p.battery_wh, 0, 1))
        self.t += 1

        reward, terminated = 0.0, False
        energy = 0.004 * (abs(a[0]) + abs(a[1])) + 0.01 * abs(a[0] - self.prev_action[0]) + 0.01 * abs(a[1] - self.prev_action[1])
        if self.task == "goal":
            d = self._goal_dist()
            reward += 4.0 * (self.prev_dist - d) - 0.005 - energy
            self.prev_dist = d
            if d < 0.15:
                reward += 5.0
                terminated = True
        else:
            # sun: reward the light where the car is, mostly once it has stopped
            speed = abs(v) + 0.1 * abs(w)
            reward += 0.04 * lit * (1.0 if speed < 0.05 else 0.3) - energy
            reward += 0.5 * max(0.0, lit - self.best_light)
            self.best_light = max(self.best_light, lit)
        if collided:
            reward -= 2.0
            terminated = True
        truncated = self.t >= int(p.episode_s / p.dt)
        self.prev_action = a
        info = {"task": self.task, "collided": collided, "light": lit, "goal_dist": self._goal_dist()}
        return self._obs(), float(reward), terminated, truncated, info


def build_obs(*, goal_bx, goal_by, goal_dist, v_left, v_right, yaw_rate, tof_m, batt,
              solar_w, solar_delta_w, task, prev_action):
    """Shared observation layout (14 floats). Keep in sync with the agent."""
    gd = min(goal_dist, 4.0)
    scale = gd / goal_dist if goal_dist > 1e-6 else 0.0
    return np.array([
        goal_bx * scale / 2.0, goal_by * scale / 2.0, gd / 2.0,
        v_left, v_right, yaw_rate / 4.0,
        min(tof_m, 2.0) / 2.0,
        batt,
        solar_w / 4.0, np.clip(solar_delta_w, -1, 1),
        1.0 if task == "goal" else 0.0, 1.0 if task == "sun" else 0.0,
        float(prev_action[0]), float(prev_action[1]),
    ], dtype=np.float32)
