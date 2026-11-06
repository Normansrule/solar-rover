"""The sysid pattern driven against a simulated car, then fitted back."""
import math

import pytest

from solar_rover_agent import sysid
from solar_rover_agent.link import Telemetry
from solar_rover_agent.runlog import RunLog, read

sim = pytest.importorskip("solar_rover_sim")
from solar_rover_sim.env import CarParams, SolarRoverEnv  # noqa: E402
from solar_rover_sim.fit import fit  # noqa: E402


class SimCar:
    """Looks like CarLink to sysid.run; one env step per sleep()."""

    def __init__(self, params, tof=-1):
        self.env = SolarRoverEnv(task="goal", params=params, seed=1)
        self.env.reset(seed=1)
        self.env.obs_c = self.env.obs_c[:0]; self.env.obs_r = self.env.obs_r[:0]   # empty floor
        self.env.strength[:] = (1.0, 0.9)        # the right side is 10 % weaker
        self.env.p.slip = 0.0
        self.cmd, self.now, self.tof, self.stopped = (0.0, 0.0), 0.0, tof, False
        self.latest = self._tel()

    def _tel(self):
        s, p = self.env.s, self.env.p
        gz = math.degrees(p.turn_eff * (s.vr - s.vl) / p.track)
        return Telemetry(spd=[s.vl * 1000, s.vl * 1000, s.vr * 1000, s.vr * 1000], imu=[0, 0, 1, 0, 0, gz], tof=self.tof)

    def drive(self, l, r): self.cmd = (l, r)
    def stop(self): self.stopped = True; self.cmd = (0.0, 0.0)
    def clock(self): return self.now

    def sleep(self, dt):
        self.env.step(self.cmd)
        self.now += dt
        self.latest = self._tel()


def test_sysid_pattern_fits_back_to_the_sim(tmp_path):
    truth = CarParams(dt=1 / sysid.RATE_HZ, tau=0.3, v_max=0.5, turn_eff=0.72)
    car = SimCar(truth)
    log = RunLog(tmp_path / "run.jsonl", clock=car.clock)
    assert sysid.run(car, log, clock=car.clock, sleep=car.sleep) == "done"
    log.close()
    assert car.stopped
    p = fit(read(tmp_path / "run.jsonl"))["params"]
    assert abs(p["tau"] - 0.3) <= 0.04
    assert abs(p["v_max"] - 0.475) < 0.015        # mean of 0.5 and 0.45
    assert abs(p["turn_eff"] - 0.72) < 0.05


def test_sysid_stops_for_an_obstacle(tmp_path):
    car = SimCar(CarParams(dt=1 / sysid.RATE_HZ), tof=200)
    log = RunLog(tmp_path / "run.jsonl")
    msg = sysid.run(car, log, clock=car.clock, sleep=car.sleep)
    assert "200 mm" in msg and car.stopped


def test_sysid_yields_to_a_person(tmp_path):
    car = SimCar(CarParams(dt=1 / sysid.RATE_HZ))
    car.latest.ovr = 1
    assert "took over" in sysid.run(car, RunLog(tmp_path / "r.jsonl"), clock=car.clock, sleep=car.sleep)
