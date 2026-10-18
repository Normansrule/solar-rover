import json
import math

import numpy as np
import pytest

from solar_rover_agent import arm_ik
from solar_rover_agent.behaviours import Navigator
from solar_rover_agent.link import Telemetry
from solar_rover_agent.observation import build_obs as agent_obs
from solar_rover_agent.odometry import Odometry

SAMPLE = {"t": 1234, "mode": "auto", "src": "agent", "ovr": 0, "bat_v": 7.9, "bat_pct": 72, "chg_ma": 300,
          "sol_v": 11.6, "sol_mw": 2100, "temp_c": 31.5, "chg": "solar", "low": 0, "crit": 0,
          "enc": [10, 10, 12, 12], "spd": [100, 100, 120, 120], "imu": [0, 0, 1, 0, 0, 5.0], "tof": 450,
          "arm": [0, 110, 120, -70, 20]}


def test_telemetry_parse():
    t = Telemetry.parse(json.dumps(SAMPLE))
    assert t.bat_pct == 72 and t.tof == 450 and t.chg == "solar"
    assert Telemetry.parse("not json") is None


def test_observation_matches_simulator():
    pytest.importorskip("solar_rover_sim")
    from solar_rover_sim.env import build_obs as sim_obs
    kw = dict(goal_bx=1.2, goal_by=-0.4, goal_dist=math.hypot(1.2, 0.4), v_left=0.2, v_right=0.3, yaw_rate=0.5,
              tof_m=0.8, batt=0.6, solar_w=2.0, solar_delta_w=0.1, task="goal", prev_action=[0.1, -0.2])
    assert np.allclose(agent_obs(**kw), sim_obs(**kw))


@pytest.mark.parametrize("x,y,z", [(0.18, 0.0, 0.03), (0.17, 0.04, 0.05), (0.15, -0.08, 0.10)])
def test_ik_round_trip(x, y, z):
    q = arm_ik.solve(x, y, z)
    fx, fy, fz = arm_ik.forward(q["yaw"], q["shoulder"], q["elbow"], q["wrist"])
    assert math.isclose(fx, x, abs_tol=1e-6) and math.isclose(fy, y, abs_tol=1e-6) and math.isclose(fz, z, abs_tol=1e-6)


def test_ik_unreachable():
    with pytest.raises(arm_ik.Unreachable):
        arm_ik.solve(1.0, 0, 0)


def test_odometry_straight():
    o = Odometry()
    o.update([0, 0, 0, 0], 0.0, 0.1)
    o.update([40, 40, 40, 40], 0.0, 0.1)        # one wheel turn on every wheel
    assert math.isclose(o.x, math.pi * 0.065, rel_tol=1e-6) and abs(o.y) < 1e-9


class ConstPolicy:
    kind = "test"
    def __init__(self, a): self.a = np.array(a)
    def act(self, obs): return self.a


def test_navigator_blocks_forward_into_obstacle():
    nav = Navigator(ConstPolicy([1.0, 1.0]), [[2, 0]], speed_scale=1.0)
    tel = Telemetry.parse(json.dumps({**SAMPLE, "tof": 80}))
    l, r = nav.step(tel, Odometry())
    assert l + r <= 1e-9


def test_navigator_switches_to_sun_when_low():
    nav = Navigator(ConstPolicy([0.0, 0.0]), [[2, 0]])
    tel = Telemetry.parse(json.dumps({**SAMPLE, "low": 1, "bat_pct": 25}))
    nav.step(tel, Odometry())
    assert nav.task == "sun"


def test_sun_seeker_improves_light_in_sim():
    pytest.importorskip("solar_rover_sim")
    from solar_rover_sim import SolarRoverEnv
    from solar_rover_agent.behaviours import SunSeeker
    start, end = [], []
    for i in range(10):
        env = SolarRoverEnv(task="sun", seed=500 + i); env.reset(seed=500 + i); env.p.episode_s = 90
        start.append(env.light(env.s.x, env.s.y))
        ss, done, info = SunSeeker(), False, {}
        while not done:
            a = ss.step(env.solar, env.s.x, env.s.y, env.s.th, (env.s.vr - env.s.vl) / env.p.track * 0.85, env._tof(), env.p.dt)
            _, _, te, tr, info = env.step(np.array(a)); done = te or tr
        end.append(info["light"])
    assert np.mean(end) > np.mean(start)
