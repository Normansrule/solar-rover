"""Score a driving policy in the simulator the way the car runs it.

On the car the agent scales the policy's output by speed_scale and never
drives forward when the time-of-flight sensor reads under stop_tof_mm
(agent/solar_rover_agent/behaviours.py). train_es scores the bare policy;
this scores both, on the same seeds, so the numbers match what to expect.

    python -m solar_rover_sim.evaluate ../agent/models/drive.npz
    python -m solar_rover_sim.evaluate old.npz new.npz --episodes 100
"""
from __future__ import annotations

import argparse
from dataclasses import replace

import numpy as np

from .env import CarParams, SolarRoverEnv
from .policy import MLPPolicy


ON_CAR = dict(speed_scale=0.7, stop_tof=0.13)   # agent/config.yaml speed_scale, Navigator stop_tof_mm
BARE = dict(speed_scale=1.0, stop_tof=0.0)


def run(policy, task, episodes=50, seed0=50_000, params=None):
    out = {"success": 0, "collisions": 0, "steps": []}
    for i in range(episodes):
        env = SolarRoverEnv(task=task, params=params, seed=seed0 + i)
        obs, _ = env.reset(seed=seed0 + i)
        done, info, n = False, {}, 0
        while not done:
            obs, _, term, trunc, info = env.step(policy.act(obs))
            done, n = term or trunc, n + 1
        out["collisions"] += int(info.get("collided", False))
        if task == "goal":
            out["success"] += int(info["goal_dist"] < 0.15)
        else:
            out["success"] += int(info["light"] > 0.5)
        out["steps"].append(n)
    out["mean_s"] = round(float(np.mean(out["steps"])) * (params or CarParams()).dt, 1)
    del out["steps"]
    return out


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("policies", nargs="+", help=".npz policies")
    ap.add_argument("--episodes", type=int, default=50)
    ap.add_argument("--speed-scale", type=float, default=0.7)
    ap.add_argument("--params", help="car_fit.json from solar_rover_sim.fit")
    a = ap.parse_args(argv)
    base = CarParams.load(a.params) if a.params else CarParams()
    res = {}
    for path in a.policies:
        pol = MLPPolicy.load(path)
        res[path] = {
            "bare": run(pol, "goal", a.episodes, params=replace(base, **BARE)),
            "on_car": run(pol, "goal", a.episodes, params=replace(base, **dict(ON_CAR, speed_scale=a.speed_scale))),
        }
        for k, v in res[path].items():
            print(f"{path:40s} {k:7s} goal {v['success']:3d}/{a.episodes}  crashes {v['collisions']:3d}/{a.episodes}  "
                  f"mean episode {v['mean_s']} s")
    return res


if __name__ == "__main__":
    main()
