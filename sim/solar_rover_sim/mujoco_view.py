"""Load the 3-D MuJoCo model, drive it forward while waving the arm, and print
what the sensors read. With a display, `--view` opens the interactive viewer.

    python -m solar_rover_sim.mujoco_view [--view] [--seconds 5]
"""
import argparse
import math
import pathlib

import numpy as np

MODEL = pathlib.Path(__file__).resolve().parent.parent / "models" / "solar_rover.xml"


def main():
    import mujoco
    ap = argparse.ArgumentParser()
    ap.add_argument("--view", action="store_true")
    ap.add_argument("--seconds", type=float, default=5.0)
    a = ap.parse_args()
    m = mujoco.MjModel.from_xml_path(str(MODEL))
    d = mujoco.MjData(m)

    def control(t):
        w = 8.0  # wheel rad/s (~0.26 m/s)
        d.ctrl[0:4] = [w, w, w, w] if t < 3 else [w, w, -w, -w]   # straight, then spin right
        r = math.radians                      # arm actuators take radians
        d.ctrl[4] = r(30 * math.sin(t))       # yaw
        d.ctrl[5] = r(45 + 20 * math.sin(t))  # shoulder
        d.ctrl[6] = r(60)
        d.ctrl[7] = r(-10)
        d.ctrl[8] = r(30 if (t % 2) < 1 else 0)

    if a.view:
        import mujoco.viewer
        with mujoco.viewer.launch_passive(m, d) as v:
            while v.is_running() and d.time < a.seconds * 10:
                control(d.time); mujoco.mj_step(m, d); v.sync()
        return
    steps = int(a.seconds / m.opt.timestep)
    for i in range(steps):
        control(d.time)
        mujoco.mj_step(m, d)
    pos = d.body("chassis").xpos
    print(f"after {a.seconds:.1f}s: chassis at x={pos[0]:.2f} y={pos[1]:.2f} z={pos[2]:.3f} m, "
          f"gyro z={d.sensor('gyro').data[2]:.2f} rad/s, tof={d.sensor('tof').data[0]:.2f} m")


if __name__ == "__main__":
    main()
