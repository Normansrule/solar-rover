"""Solar Rover agent main loop.

On the Pi:   python3 -m solar_rover_agent.main --config config.yaml
From a PC:   python3 -m solar_rover_agent.main --url ws://solar-rover.local/ws
"""
from __future__ import annotations

import argparse
import os
import pathlib
import subprocess
import time

from .link import CarLink
from .odometry import Odometry
from .policy import Policy
from .behaviours import Navigator
from .runlog import RunLog

HERE = pathlib.Path(__file__).resolve().parent.parent


def load_config(path):
    cfg = {"port": "/dev/serial0", "policy": str(HERE / "models" / "drive.npz"), "rate_hz": 10,
           "waypoints": [[1.0, 0.0], [1.0, 1.0], [0.0, 1.0], [0.0, 0.0]], "speed_scale": 0.7,
           "task": "navigate", "shutdown_cmd": ["sudo", "systemctl", "poweroff"]}
    if path and os.path.exists(path):
        import yaml
        with open(path) as f:
            cfg.update(yaml.safe_load(f) or {})
    return cfg


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--config", default=str(HERE / "config.yaml"))
    ap.add_argument("--url", help="ws://solar-rover.local/ws to run from a PC over WiFi")
    ap.add_argument("--port")
    ap.add_argument("--dry-run", action="store_true", help="read telemetry, print actions, send nothing")
    ap.add_argument("--log", help="record commands + telemetry here (JSON lines) for sim/solar_rover_sim/fit.py")
    a = ap.parse_args(argv)
    cfg = load_config(a.config)
    link = CarLink(port=a.port or cfg["port"], url=a.url)
    policy = Policy(cfg["policy"])
    nav = Navigator(policy, cfg["waypoints"], speed_scale=cfg["speed_scale"], sun_mode=cfg.get("sun_mode", "scripted"))
    pick = None
    if cfg.get("task") == "pick":
        from .vision import BlobFinder
        from .pick import PickRoutine
        pick = PickRoutine(link, BlobFinder())
    odo = Odometry()
    log = RunLog(a.log) if a.log else None
    period = 1.0 / cfg["rate_hz"]
    print(f"agent running: policy={cfg['policy']} ({policy.kind}), task={cfg.get('task')}")
    last = time.monotonic()
    try:
        while True:
            t0 = time.monotonic()
            dt, last = t0 - last, t0
            if link.shutdown_requested:
                print("car asked us to shut down (battery critical)")
                link.stop()
                if not a.dry_run:
                    subprocess.call(cfg["shutdown_cmd"])
                return
            tel = link.latest
            if tel is None or t0 - tel.received_at > 0.5:
                time.sleep(period)
                continue
            odo.update(tel.enc, tel.imu[5], dt)
            if tel.mode == "manual" or tel.ovr or tel.crit:
                time.sleep(period)            # a person is driving (or the car parked itself)
                continue
            cmd = pick.step(tel) if pick and not tel.low else nav.step(tel, odo, dt)
            if log:
                log.write(tel, None if a.dry_run else cmd)
            if a.dry_run:
                print(f"task={nav.task} cmd={cmd} bat={tel.bat_pct:.0f}% sol={tel.sol_mw:.0f}mW tof={tel.tof}")
            elif cmd is None:
                link.send("H")
            else:
                link.drive(*cmd)
            time.sleep(max(0.0, period - (time.monotonic() - t0)))
    except KeyboardInterrupt:
        pass
    finally:
        link.close()
        if log:
            log.close()


if __name__ == "__main__":
    main()
