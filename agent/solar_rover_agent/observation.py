"""Build the policy observation from telemetry, exactly like the simulator
(sim/solar_rover_sim/env.py: build_obs). Keep the two in sync."""
from __future__ import annotations

import math

import numpy as np

V_MAX_MM_S = 600.0


def build_obs(*, goal_bx, goal_by, goal_dist, v_left, v_right, yaw_rate, tof_m, batt,
              solar_w, solar_delta_w, task, prev_action):
    gd = min(goal_dist, 4.0)
    scale = gd / goal_dist if goal_dist > 1e-6 else 0.0
    return np.array([
        goal_bx * scale / 2.0, goal_by * scale / 2.0, gd / 2.0,
        v_left, v_right, yaw_rate / 4.0,
        min(tof_m, 2.0) / 2.0,
        batt,
        solar_w / 4.0, float(np.clip(solar_delta_w, -1, 1)),
        1.0 if task == "goal" else 0.0, 1.0 if task == "sun" else 0.0,
        float(prev_action[0]), float(prev_action[1]),
    ], dtype=np.float32)


def from_telemetry(tel, odo, task: str, goal_xy, prev_action, prev_solar_w: float):
    if task == "goal" and goal_xy is not None:
        bx, by = odo.to_body(*goal_xy)
        dist = math.hypot(bx, by)
    else:
        bx = by = dist = 0.0
    vl = 0.5 * (tel.spd[0] + tel.spd[1]) / V_MAX_MM_S
    vr = 0.5 * (tel.spd[2] + tel.spd[3]) / V_MAX_MM_S
    tof = 2.0 if tel.tof < 0 else tel.tof / 1000.0
    solar = tel.sol_mw / 1000.0
    return build_obs(goal_bx=bx, goal_by=by, goal_dist=dist, v_left=vl, v_right=vr,
                     yaw_rate=math.radians(tel.imu[5]), tof_m=tof, batt=tel.bat_pct / 100.0,
                     solar_w=solar, solar_delta_w=solar - prev_solar_w, task=task, prev_action=prev_action)
