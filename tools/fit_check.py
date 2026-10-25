#!/usr/bin/env python3
"""Check the assembled car for parts that run into each other.

Exports every placed part of hardware/scad/assembly.scad as its own STL (in
world coordinates, using -D only=NAME), then intersects every pair whose
bounding boxes touch. The arm is checked in each firmware pose at three turret
angles, plus two targets solved by the agent's inverse kinematics
(agent/solar_rover_agent/arm_ik.py), and it reports where the fingertips end up in
each, so you can see the IK agrees with the model.

    python3 tools/fit_check.py            # needs openscad, and: pip install trimesh==5.1.1 manifold3d==3.5.4
    python3 tools/fit_check.py --jobs 8
    python3 tools/fit_check.py --paths    # also every move between poses

Run tools/export_stl.sh first. Exit code 1 means a collision was found.
"""
import argparse
import itertools
import os
import re
import subprocess
import sys
import tempfile
import warnings
from concurrent.futures import ThreadPoolExecutor

import trimesh

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ASM = os.path.join(ROOT, "hardware", "scad", "assembly.scad")

CORNERS = ["fl", "fr", "rr", "rl"]
STATIC = (["deck_front", "deck_rear", "nose", "turret_base", "box_servo_yaw",
           "roof_tray_front", "roof_tray_rear", "box_panel_front", "box_panel_rear",
           "power_tray", "logic_tray", "charge_tray",
           "cradle_tb6612_a", "cradle_tb6612_b", "cradle_mp1584",
           "box_pca9685", "box_xl4015", "box_tb6612_a", "box_tb6612_b", "box_mp1584",
           "box_wago_gnd", "box_wago_main", "box_wago_5v",
           "box_esp_board", "box_pi", "box_mpu6050", "box_i2c_hub",
           "box_battery", "box_relay", "box_cn3795", "box_ina219_solar", "box_ina219_batt",
           "box_wago_chg", "box_wago_pack", "cradle_e220", "box_e220", "box_antenna"]
          + [f"{p}_{c}" for p in ("clamp", "wheel", "tire", "disc", "box_motor") for c in CORNERS]
          + [f"post_{i}" for i in range(10)] + [f"standoff_{i}" for i in range(4)])
# parts that turn with the turret, and those that also move with the arm
TURRET = ["turret_top", "shoulder_mount", "camera_mount", "box_servo_shoulder", "box_camera"]
ARM = ["upper_plate_horn", "upper_plate_idler", "crossbar", "forearm", "box_servo_elbow",
       "box_servo_wrist", "gripper_palm", "gripper_horn_arm", "finger_driven",
       "finger_idler", "box_servo_grip"]
FINGERS = ["finger_driven", "finger_idler"]
ARM_CPP = os.path.join(ROOT, "firmware", "car", "src", "arm.cpp")
YAWS = [-90, 0, 90]

# Pairs that are meant to share material, with the reason. Plain face-to-face
# contact is already below the threshold and needs no entry.
ALLOWED = {
    tuple(sorted((f"wheel_{c}", f"tire_{c}"))): "the TPU tire is 0.6 mm undersize so it grips the rim"
    for c in CORNERS
}
# each driver board sits inside its cradle
ALLOWED[("box_tb6612_a", "cradle_tb6612_a")] = "board in its cradle"
ALLOWED[("box_e220", "cradle_e220")] = "board in its cradle"
ALLOWED[("box_tb6612_b", "cradle_tb6612_b")] = "board in its cradle"
ALLOWED[("box_mp1584", "cradle_mp1584")] = "board in its cradle"
LIMIT_MM3 = 1.0
warnings.filterwarnings("ignore", category=RuntimeWarning)   # trimesh: centre of mass of empty meshes


def firmware_poses():
    """{name: [yaw, shoulder, elbow, wrist]} from the POSES table in arm.cpp."""
    src = open(ARM_CPP).read()
    return {m[1]: [float(v) for v in m[2].split(",")[:4]]
            for m in re.finditer(r'\{"(\w+)",\s*\{([^}]*)\}\}', src)}


POSES = firmware_poses()

# Arm targets solved with the agent's inverse kinematics, to check its maths
# against the model: (label, x, z) of the fingertip in metres from the turret
# axis at ground level.
sys.path.insert(0, os.path.join(ROOT, "agent"))
from solar_rover_agent import arm_ik  # noqa: E402
IK_TARGETS = [("ik_ground_max", 0.175, 0.0), ("ik_pick_py", 0.175, 0.03)]
for label, x, z in IK_TARGETS:
    q = arm_ik.solve(x, 0.0, z)
    POSES[label] = [q["yaw"], q["shoulder"], q["elbow"], q["wrist"]]


def joint_speeds():
    """Slew limits (deg/s) of yaw, shoulder, elbow and wrist from JOINTS in config.h."""
    src = open(os.path.join(ROOT, "firmware", "car", "src", "config.h")).read()
    rows = re.findall(r'\{"(\w+)",([^}]*)\}', src[src.index("JOINTS[J_COUNT]"):])
    return [float(r[1].split(",")[-1]) for r in rows[:4]]


def pose_path(a, b, dt=0.02, every=0.2):
    """Joint angles the firmware passes through going from pose a to pose b.
    Mirrors arm::update(): yaw, shoulder and elbow move one at a time (they
    share the servo rail), yaw first, then the elbow before the shoulder when the
    elbow unfolds, otherwise the shoulder first. The wrist moves at the same time."""
    speeds = joint_speeds()
    cur, out, t, last = list(a), [], 0.0, -every
    while any(abs(c - g) > 1e-6 for c, g in zip(cur, b)):
        order = [0, 2, 1] if b[2] < cur[2] else [0, 1, 2]
        leader = next((j for j in order if abs(b[j] - cur[j]) > 1.0), -1)
        for j in range(4):
            if j < 3 and leader != -1 and j != leader:
                continue
            step = speeds[j] * dt
            cur[j] += max(-step, min(step, b[j] - cur[j]))
        t += dt
        if t - last >= every:
            out.append([round(c, 1) for c in cur])
            last = t
    return out


def config_value(name):
    src = open(os.path.join(ROOT, "hardware", "scad", "config.scad")).read()
    return float(re.search(rf"^{name}\s*=\s*([-\d.]+)", src, re.M)[1])


def export(name, pose, yaw, out):
    angles = POSES[pose] if pose else []
    cmd = ["openscad", "-q", "-o", out, "-D", f'only="{name}"', "-D", f"angles={angles}",
           "-D", f"yaw_extra={yaw}", ASM]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0 or not os.path.exists(out):
        sys.exit(f"openscad failed for {name}:\n{r.stderr}")
    return out


def load(path):
    m = trimesh.load(path, force="mesh")
    m.merge_vertices()
    return m


def overlap(a, b):
    lo = [max(x, y) for x, y in zip(a.bounds[0], b.bounds[0])]
    hi = [min(x, y) for x, y in zip(a.bounds[1], b.bounds[1])]
    if any(h - l <= 0.05 for l, h in zip(lo, hi)):
        return 0.0
    try:
        inter = trimesh.boolean.intersection([a, b], engine="manifold")
    except Exception as e:  # non-manifold input: report it rather than hide it
        print(f"  ! could not intersect: {e}")
        return float("nan")
    return 0.0 if inter.is_empty else abs(inter.volume)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--jobs", type=int, default=os.cpu_count() or 4)
    ap.add_argument("--keep", help="folder to keep the exported part STLs in")
    ap.add_argument("--paths", action="store_true",
                    help="also check the arm along every move between firmware poses (slower)")
    args = ap.parse_args()
    tmp = args.keep or tempfile.mkdtemp(prefix="fitcheck_")
    os.makedirs(tmp, exist_ok=True)

    jobs = [(n, "", 0) for n in STATIC]
    first = next(iter(POSES))
    jobs += [(n, first, y) for n in TURRET for y in YAWS]
    jobs += [(n, p, y) for n in ARM for p in POSES for y in YAWS]
    moves = {}
    if args.paths:
        names = [p for p in POSES if not p.startswith("ik_")]
        for a, b in itertools.permutations(names, 2):
            steps = pose_path(POSES[a], POSES[b])
            moves[(a, b)] = []
            for q in steps:
                key = "path_" + "_".join(f"{v:g}" for v in q)
                POSES[key] = q
                moves[(a, b)].append(key)
        keys = sorted({k for v in moves.values() for k in v})
        jobs += [(n, k, 0) for n in ARM for k in keys]
    paths = {}
    print(f"Exporting {len(jobs)} placed parts with openscad ...")
    with ThreadPoolExecutor(args.jobs) as ex:
        futs = {j: ex.submit(export, *j, os.path.join(tmp, f"{j[0]}__{j[1] or 'x'}__{j[2]}.stl")) for j in jobs}
        for j, f in futs.items():
            paths[j] = f.result()
    meshes = {j: load(p) for j, p in paths.items()}

    static = {n: meshes[(n, "", 0)] for n in STATIC}
    ground = min(static[f"tire_{c}"].bounds[0][2] for c in CORNERS)
    bad = []

    def check(a_name, a, b_name, b, where):
        key = tuple(sorted((a_name, b_name)))
        v = overlap(a, b)
        if v != v or v > LIMIT_MM3:
            if key in ALLOWED:
                return
            bad.append((where, a_name, b_name, v))
            print(f"  COLLISION {where}: {a_name} x {b_name}: {v:.1f} mm3")

    print("Checking the fixed parts against each other ...")
    for a, b in itertools.combinations(STATIC, 2):
        check(a, static[a], b, static[b], "fixed")

    for y in YAWS:
        turret = {n: meshes[(n, first, y)] for n in TURRET}
        print(f"Checking the turret at yaw {y:+d} ...")
        for a, b in itertools.combinations(TURRET, 2):
            check(a, turret[a], b, turret[b], f"yaw {y:+d}")
        for a in TURRET:
            for b in STATIC:
                check(a, turret[a], b, static[b], f"yaw {y:+d}")
        for p in POSES:
            if p.startswith("path_"):
                continue
            arm = {n: meshes[(n, p, y)] for n in ARM}
            print(f"Checking the arm in '{p}' at yaw {y:+d} ...")
            for a, b in itertools.combinations(ARM, 2):
                check(a, arm[a], b, arm[b], f"{p} yaw {y:+d}")
            for a in ARM:
                for b, m in list(static.items()) + list(turret.items()):
                    check(a, arm[a], b, m, f"{p} yaw {y:+d}")

    if moves:
        turret = {n: meshes[(n, first, 0)] for n in TURRET}
        for (a, b), keys in moves.items():
            print(f"Checking the move {a} -> {b} ({len(keys)} steps) ...")
            for k in keys:
                arm = {n: meshes[(n, k, 0)] for n in ARM}
                for x, y in itertools.combinations(ARM, 2):
                    check(x, arm[x], y, arm[y], f"{a}->{b} at {POSES[k]}")
                for x in ARM:
                    for y, m in list(static.items()) + list(turret.items()):
                        check(x, arm[x], y, m, f"{a}->{b} at {POSES[k]}")

    print()
    axis_x = config_value("turret_x")
    nose_x = static["nose"].bounds[1][0]
    for p in POSES:
        if p.startswith("path_"):
            continue
        lo = min(meshes[(f, p, 0)].bounds[0][2] for f in FINGERS) - ground
        fx = sum(meshes[(f, p, 0)].bounds[:, 0].mean() for f in FINGERS) / len(FINGERS)
        line = f"{p:15s} fingertips {lo:+6.1f} mm above the ground, {fx - nose_x:+6.1f} mm ahead of the nose"
        for label, x, z in IK_TARGETS:
            if p == label:
                line += f" (IK aimed at {z * 1000:+.0f} mm, {x * 1000 - (nose_x - axis_x):+.0f} mm)"
        print(line)
        if lo < -1.0:
            bad.append((p, "fingers", "ground", -lo))
            print(f"  COLLISION: the fingers go {-lo:.1f} mm into the ground")

    if bad:
        print(f"\n{len(bad)} collision(s) found.")
        return 1
    print("\nNo collisions.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
