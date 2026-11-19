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


# ---------- the front-corner sensors and the side guard ----------

def test_corner_sensors_see_their_own_side():
    env = _arena(CarParams(), [(0.25, 0.45, 0.05)])   # a post ahead-left, well off the front sensor's cone
    left, right = env.side_tofs()
    assert left < 0.5 and right > 1.0
    assert env._tof() > 1.0
    assert _arena(CarParams(side_tof_deg=0), [(0.25, 0.45, 0.05)]).side_tofs() == (2.0, 2.0)


def test_side_guard_slows_and_turns_away():
    from solar_rover_sim.env import side_guard
    np.testing.assert_allclose(side_guard([0.6, 0.6], 2.0, 2.0, 0.3), [0.6, 0.6])      # nothing near
    np.testing.assert_allclose(side_guard([-0.5, 0.5], 0.1, 2.0, 0.3), [-0.5, 0.5])    # turning on the spot
    l, r = side_guard([0.6, 0.6], 0.15, 2.0, 0.3)                                       # left corner close
    assert l + r < 1.2 and l > r                                                        # slower, turning right
    l, r = side_guard([0.6, 0.6], 2.0, 0.15, 0.3)
    assert r > l                                                                        # right close: turns left
    l, r = side_guard([0.3, 0.7], 2.0, 0.15, 0.3)                                       # already turning left: kept
    assert abs((r - l) - 0.4) < 1e-9 and abs((l + r) - 0.5) < 1e-9                     # its own turn, half speed


def test_sim_and_agent_guards_agree():
    import importlib.util, pathlib
    from solar_rover_sim.env import side_guard
    path = pathlib.Path(__file__).resolve().parents[2] / "agent/solar_rover_agent/safety.py"
    spec = importlib.util.spec_from_file_location("agent_safety", path)
    mod = importlib.util.module_from_spec(spec); spec.loader.exec_module(mod)
    rng = np.random.default_rng(1)
    for _ in range(200):
        a, l, r = rng.uniform(-1, 1, 2), *rng.uniform(0, 0.6, 2)
        np.testing.assert_allclose(side_guard(a, l, r, 0.3), mod.side_guard(a, l, r, 0.3))


def test_side_guard_cuts_crashes_of_the_shipped_policy():
    import pathlib
    from solar_rover_sim.evaluate import ONE_SENSOR
    pol = MLPPolicy.load(pathlib.Path(__file__).resolve().parents[2] / "agent/models/drive.npz")
    one = run(pol, "goal", episodes=40, params=replace(CarParams(), **ONE_SENSOR))
    both = run(pol, "goal", episodes=40, params=replace(CarParams(), **ON_CAR))
    assert both["collisions"] < one["collisions"] / 2
    assert both["success"] >= one["success"]
