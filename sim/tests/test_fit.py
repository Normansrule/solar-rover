import json
import math

import numpy as np

from solar_rover_sim.env import CarParams
from solar_rover_sim.fit import fit

STEPS = [(1.0, 0, 0), (2.0, 0.3, 0.3), (1.5, 0, 0), (1.5, -0.5, 0.5), (1.5, 0, 0), (2.0, 0.5, 0.2), (1.5, 0, 0)]


def fake_log(tau=0.25, gl=0.52, gr=0.46, turn_eff=0.7, track=0.174, noise=0.01, seed=0):
    """Rows like solar_rover_agent.runlog writes, from a known first-order car."""
    rng = np.random.default_rng(seed)
    rows, t, vl, vr = [], 0.0, 0.0, 0.0
    for dur, ul, ur in STEPS:
        end = t + dur
        while t < end:
            spd = [vl, vl, vr, vr] + rng.normal(0, noise, 4)
            gz = math.degrees(turn_eff * (vr - vl) / track) + rng.normal(0, 1.0)
            rows.append({"t": round(t, 3), "cmd": [ul, ur],
                         "tel": {"spd": [float(x * 1000) for x in spd], "imu": [0, 0, 1, 0, 0, gz]}})
            dt = 0.05 + rng.uniform(-0.005, 0.005)
            a = 1 - math.exp(-dt / tau)
            vl += a * (gl * ul - vl); vr += a * (gr * ur - vr)
            t += dt
    return rows


def test_fit_recovers_known_car():
    res = fit(fake_log())
    p = res["params"]
    assert abs(p["tau"] - 0.25) <= 0.03
    assert abs(p["v_max"] - 0.49) < 0.01
    assert abs(p["turn_eff"] - 0.7) < 0.05
    lo, hi = p["strength"]
    assert lo < 0.46 / 0.49 and hi > 0.52 / 0.49      # the range covers both real sides
    assert res["fit"]["speed_rmse_m_s"] < 0.02


def test_rows_without_commands_are_skipped():
    rows = fake_log()
    for r in rows[:40]:
        r["cmd"] = None                                 # e.g. a --dry-run stretch
    res = fit(rows)
    assert res["fit"]["rows"] == len(rows) - 40


def test_fitted_params_load_into_the_sim(tmp_path):
    path = tmp_path / "car_fit.json"
    path.write_text(json.dumps(fit(fake_log())))
    p = CarParams.load(str(path))
    assert isinstance(p.strength, tuple) and abs(p.tau - 0.25) <= 0.03 and p.track == CarParams().track
