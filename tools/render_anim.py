#!/usr/bin/env python3
"""Render the README's animated GIFs from the 3D model (run export_stl.sh first).

    python3 tools/render_anim.py          # all three
    python3 tools/render_anim.py build    # just one: build, spin or arm

  docs/img/anim/build.gif  the car going together, one build step of docs/assembly.md at a time
  docs/img/anim/spin.gif   the finished car turning round
  docs/img/anim/arm.gif    the arm moving through its firmware poses

Needs OpenSCAD and Pillow (pip install pillow). Headless machines use
xvfb-run if it's installed (sudo apt install xvfb).
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCAD = os.path.join(ROOT, "hardware", "scad", "assembly.scad")
OUT = os.path.join(ROOT, "docs", "img", "anim")
W, H = 640, 400
CENTRE = (-10, 0, 85)          # camera look-at point, mm
DIST = 1080

# build steps from docs/assembly.md, matching step_of in assembly.scad
STEPS = [(1, "Deck"), (2, "Motors, encoders and wheels"), (3, "Battery, charger and power tray"),
         (4, "Logic tray: ESP32, Pi, IMU"), (5, "Turret and yaw servo"), (6, "Arm and gripper"),
         (7, "Nose, camera and solar roof")]


def font(size, bold=False):
    name = "DejaVuSans-Bold.ttf" if bold else "DejaVuSans.ttf"
    for d in ("/usr/share/fonts/truetype/dejavu", "/usr/share/fonts/dejavu", "/Library/Fonts"):
        if os.path.exists(os.path.join(d, name)):
            return ImageFont.truetype(os.path.join(d, name), size)
    return ImageFont.load_default()


def firmware_poses():
    """{name: [yaw, shoulder, elbow, wrist]} read from firmware/car/src/arm.cpp."""
    src = open(os.path.join(ROOT, "firmware", "car", "src", "arm.cpp")).read()
    poses = {}
    for name, nums in re.findall(r'\{\s*"(\w+)"\s*,\s*\{([^}]*)\}', src):
        vals = [float(v) for v in nums.split(",")[:4]]
        if len(vals) == 4:
            poses[name] = vals
    return poses


def render(png, rz, rx=62, **defs):
    cmd = ["openscad", f"--imgsize={W},{H}", "--colorscheme=Tomorrow",
           f"--camera={CENTRE[0]},{CENTRE[1]},{CENTRE[2]},{rx},0,{rz % 360:.1f},{DIST}", "-o", png]
    for k, v in defs.items():
        cmd += ["-D", f"{k}={v}"]
    cmd.append(SCAD)
    if shutil.which("xvfb-run"):
        cmd = ["xvfb-run", "-a"] + cmd
    subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return Image.open(png).convert("RGB")


def caption(img, title, sub="", done=None):
    """Title bar along the bottom, plus a row of step dots when done=(n, total)."""
    d = ImageDraw.Draw(img, "RGBA")
    d.rectangle([0, H - 54, W, H], fill=(20, 28, 38, 225))
    d.text((16, H - 48), title, font=font(20, True), fill=(255, 255, 255))
    if sub:
        d.text((16, H - 22), sub, font=font(13), fill=(255, 196, 61))
    if done:
        n, total = done
        for i in range(total):
            x = W - 22 - (total - 1 - i) * 22
            col = (46, 204, 113) if i < n else (90, 100, 112)
            d.ellipse([x - 7, H - 34, x + 7, H - 20], fill=col)
    return img


def save_gif(frames, path, ms):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    pal = [f.quantize(colors=96, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE) for f in frames]
    pal[0].save(path, save_all=True, append_images=pal[1:], duration=ms, loop=0, optimize=True)
    print(f"wrote {os.path.relpath(path, ROOT)} ({len(frames)} frames, {os.path.getsize(path) // 1024} KB)")


def build(tmp):
    frames, ms, rz = [], [], 20.0
    for n, title in STEPS:
        for k in range(6):                      # slow turn while each step holds
            img = render(os.path.join(tmp, "f.png"), rz, step=n, pose='"carry"')
            frames.append(caption(img, f"Step {n}: {title}", "docs/assembly.md", (n, len(STEPS))))
            ms.append(900 if k == 0 else 140)
            rz += 4
    for k in range(18):                         # then one lap of the finished car
        img = render(os.path.join(tmp, "f.png"), rz, pose='"carry"')
        frames.append(caption(img, "Solar Rover, fully built", "31 printed parts, off-the-shelf modules", (7, 7)))
        ms.append(120)
        rz += 20
    save_gif(frames, os.path.join(OUT, "build.gif"), ms)


def spin(tmp):
    frames = []
    for k in range(36):
        frames.append(render(os.path.join(tmp, "f.png"), 20 + k * 10, pose='"carry"'))
    save_gif(frames, os.path.join(OUT, "spin.gif"), 90)


def arm(tmp):
    p = firmware_poses()
    park, ready, pick, carry = (p.get(k) for k in ("park", "ready", "pick", "carry"))
    if None in (park, ready, pick, carry):
        sys.exit("couldn't read the park/ready/pick/carry poses from arm.cpp")
    look = lambda a, yaw: [yaw, a[1], a[2], a[3]]
    keys = [("park", park), ("ready", ready), ("pick", pick), ("pick", pick), ("carry", carry),
            ("carry, turret left", look(carry, 60)), ("carry, turret right", look(carry, -60)),
            ("carry", carry), ("park", park)]
    frames, ms = [], []
    for (name_a, a), (name_b, b) in zip(keys, keys[1:]):
        for k in range(6):
            t = k / 6
            ang = [round(x + (y - x) * t, 1) for x, y in zip(a, b)]
            img = render(os.path.join(tmp, "f.png"), 345, rx=78, angles=str(ang))
            label = name_a if k == 0 or name_a == name_b else f"{name_a} -> {name_b}"
            frames.append(caption(img, f"Arm pose: {label}", "yaw / shoulder / elbow / wrist = "
                                  + " / ".join(f"{v:.0f}" for v in ang)))
            ms.append(700 if k == 0 else 110)
    save_gif(frames, os.path.join(OUT, "arm.gif"), ms)


if __name__ == "__main__":
    jobs = {"build": build, "spin": spin, "arm": arm}
    pick_ = sys.argv[1:] or list(jobs)
    with tempfile.TemporaryDirectory() as tmp:
        for j in pick_:
            jobs[j](tmp)
