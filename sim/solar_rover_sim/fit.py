"""Fit the simulator's motor numbers to logs from the real car.

Record a log with the agent's test pattern (python3 -m solar_rover_agent.sysid,
see docs/software.md), then:

    python -m solar_rover_sim.fit ../agent/runs/*.jsonl --out models/car_fit.json

It fits, per the model in env.py:
  tau       motor + ramp lag, shared by both sides (first-order response)
  v_max     speed in m/s for a command of 1.0, the mean of the two sides
  strength  left/right gain relative to v_max, widened into the range the
            trainer randomises over
  turn_eff  measured yaw rate / ideal skid-steer yaw rate (gyro vs encoders)

Train against it with  python -m solar_rover_sim.train_es --params models/car_fit.json
"""
from __future__ import annotations

import argparse
import json
import math

import numpy as np

from .env import CarParams

TAUS = np.round(np.arange(0.03, 1.001, 0.01), 3)
MAX_GAP_S = 0.5     # a longer gap between rows starts a new segment


def segments(rows):
    """Split log rows into runs of consecutive commanded steps.
    Each segment is a dict of arrays: t, ul, ur (commands), vl, vr (m/s), gz (rad/s)."""
    out, cur, last_t = [], [], None
    for row in rows:
        cmd, tel = row.get("cmd"), row.get("tel") or {}
        ok = cmd is not None and "spd" in tel and "imu" in tel
        if not ok or (last_t is not None and row["t"] - last_t > MAX_GAP_S):
            if len(cur) > 5:
                out.append(cur)
            cur = []
        if ok:
            cur.append(row)
        last_t = row.get("t")
    if len(cur) > 5:
        out.append(cur)
    segs = []
    for rows in out:
        spd = np.array([r["tel"]["spd"] for r in rows], float) / 1000.0
        segs.append(dict(
            t=np.array([r["t"] for r in rows], float),
            ul=np.array([r["cmd"][0] for r in rows], float),
            ur=np.array([r["cmd"][1] for r in rows], float),
            vl=spd[:, 0:2].mean(axis=1), vr=spd[:, 2:4].mean(axis=1),
            gz=np.radians([r["tel"]["imu"][5] for r in rows]),
        ))
    return segs


def _responses(t, u, v0, tau):
    """Unit-gain first-order response to u (held between rows), and the decay of v0."""
    x = np.zeros_like(u); d = np.ones_like(u)
    for k in range(1, len(u)):
        a = 1.0 - math.exp(-(t[k] - t[k - 1]) / tau)
        x[k] = x[k - 1] + a * (u[k - 1] - x[k - 1])
        d[k] = d[k - 1] * (1 - a)
    return x, d * v0


def fit_side(segs, side, tau):
    """Best gain for one side at a given tau, and the squared error."""
    xs, ys = [], []
    for s in segs:
        x, dec = _responses(s["t"], s["u" + side], s["v" + side][0], tau)
        xs.append(x); ys.append(s["v" + side] - dec)
    x, y = np.concatenate(xs), np.concatenate(ys)
    g = float(x @ y / max(x @ x, 1e-9))
    return g, float(np.sum((y - g * x) ** 2)), len(y)


def fit(rows, track=None):
    segs = segments(rows)
    if not segs:
        raise ValueError("no commanded driving in these logs (was cmd always null?)")
    track = track or CarParams().track
    best = None
    for tau in TAUS:
        gl, el, n = fit_side(segs, "l", tau)
        gr, er, _ = fit_side(segs, "r", tau)
        if best is None or el + er < best[0]:
            best = (el + er, float(tau), gl, gr, n)
    err, tau, gl, gr, n = best
    # turn efficiency: gyro yaw vs the yaw the wheel speeds would give
    ideal = np.concatenate([(s["vr"] - s["vl"]) / track for s in segs])
    gz = np.concatenate([s["gz"] for s in segs])
    m = np.abs(ideal) > 0.3
    turn_eff = float(gz[m] @ ideal[m] / (ideal[m] @ ideal[m])) if m.sum() > 5 else CarParams().turn_eff
    v_max = 0.5 * (gl + gr)
    sl, sr = gl / v_max, gr / v_max
    return {
        "params": {"tau": round(tau, 3), "v_max": round(v_max, 3), "turn_eff": round(turn_eff, 3),
                   "strength": [round(min(sl, sr) * 0.9, 3), round(max(sl, sr) * 1.1, 3)]},
        "fit": {"rows": n, "segments": len(segs), "speed_rmse_m_s": round(math.sqrt(err / (2 * n)), 4),
                "gain_left": round(gl, 3), "gain_right": round(gr, 3), "turn_rows": int(m.sum())},
    }


def main(argv=None):
    import sys, pathlib
    sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2] / "agent"))
    from solar_rover_agent.runlog import read

    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("logs", nargs="+")
    ap.add_argument("--out", default="models/car_fit.json")
    a = ap.parse_args(argv)
    rows = []
    for p in a.logs:
        rows += read(p)
    res = fit(rows)
    res["logs"] = [str(p) for p in a.logs]
    with open(a.out, "w") as f:
        json.dump(res, f, indent=1)
    d = CarParams()
    p = res["params"]
    print(f"fitted from {res['fit']['rows']} rows in {res['fit']['segments']} segments "
          f"(speed error {res['fit']['speed_rmse_m_s'] * 1000:.0f} mm/s):")
    print(f"  tau       {d.tau:.3f} -> {p['tau']:.3f} s")
    print(f"  v_max     {d.v_max:.3f} -> {p['v_max']:.3f} m/s")
    print(f"  turn_eff  {d.turn_eff:.3f} -> {p['turn_eff']:.3f}")
    print(f"  strength  {d.strength} -> {tuple(p['strength'])}")
    print(f"saved {a.out}; retrain with: python -m solar_rover_sim.train_es --params {a.out}")


if __name__ == "__main__":
    main()
