"""The simulator's sensor cone and the car's forward stop."""
import math
from dataclasses import replace

import numpy as np

from solar_rover_sim.env import CarParams, SolarRoverEnv
from solar_rover_sim.evaluate import BARE, ON_CAR, run
from solar_rover_sim.policy import MLPPolicy, init_params


def _arena(params, obstacles):
    env = SolarRoverEnv(task="goal", params=params, seed=0)
    env.reset(seed=0)
    env.obs_c = np.array([o[:2] for o in obstacles], float).reshape(-1, 2)
    env.obs_r = np.array([o[2] for o in obstacles], float)
    env.s.x, env.s.y, env.s.th = 0.0, 0.0, 0.0
    return env


def test_tof_cone_sees_an_object_off_axis():
    # a post 0.6 m ahead and 0.12 m to the side: outside a single ray, inside 25 degrees
    obstacles = [(0.75, 0.12, 0.03)]
    assert _arena(CarParams(tof_fov_deg=0), obstacles)._tof() > 1.0
    assert _arena(CarParams(), obstacles)._tof() < 0.7


def test_forward_stop_keeps_the_car_off_a_wall():
    p = replace(CarParams(), slip=0.0, strength=(1.0, 1.0), **ON_CAR)
    env = _arena(p, [(0.6, 0.0, 0.15)])          # obstacle face 0.45 m ahead
    for _ in range(60):
        _, _, term, _, info = env.step(np.array([1.0, 1.0]))
        assert not info["collided"]
        if term:
            break
    assert env._tof() < 0.2                       # it got close, then stopped


def test_without_the_stop_it_crashes():
    p = replace(CarParams(), slip=0.0, strength=(1.0, 1.0), **BARE)
    env = _arena(p, [(0.6, 0.0, 0.15)])
    crashed = False
    for _ in range(60):
        _, _, term, _, info = env.step(np.array([1.0, 1.0]))
        crashed |= info["collided"]
        if term:
            break
    assert crashed


def test_evaluate_reports_counts():
    pol = MLPPolicy(init_params(np.random.default_rng(0)))
    r = run(pol, "goal", episodes=3, params=replace(CarParams(), **ON_CAR))
    assert 0 <= r["success"] <= 3 and 0 <= r["collisions"] <= 3 and r["mean_s"] > 0


def test_footprint_is_the_real_car_not_a_circle():
    env = _arena(CarParams(), [(0.0, 0.14, 0.03)])   # a thin post 4 cm from the wheel
    assert env._body_free(0.0, 0.0, 0.0)
    assert not env._body_free(0.0, 0.0, math.pi / 2)  # turned 90 degrees, the nose hits it
    assert not env._body_free(0.0, 0.05, 0.0)
