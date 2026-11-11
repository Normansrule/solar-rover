"""Drive a fixed test pattern and log it, so the simulator can be fitted to
the real car (docs/software.md, "Tune the simulator from real runs").

Put the car on a flat floor with about 1.5 m clear in front and 0.5 m to each
side, then, on the Pi or a PC:

    python3 -m solar_rover_agent.sysid --out runs/floor1.jsonl
    python3 -m solar_rover_agent.sysid --url ws://solar-rover.local/ws --out runs/floor1.jsonl

It takes about 25 s. It stops if anything comes within 30 cm of the nose or
if a person takes over. Then fit the simulator:

    cd sim && python3 -m solar_rover_sim.fit ../agent/runs/*.jsonl
"""
from __future__ import annotations

import argparse
import time

from .link import CarLink
from .runlog import RunLog

# (seconds, left, right): steps, spins, an arc and a reverse, with rests in
# between so each response starts from standstill.
PATTERN = [
    (1.0, 0.0, 0.0),
    (2.0, 0.3, 0.3), (1.5, 0.0, 0.0),
    (1.5, 0.6, 0.6), (1.5, 0.0, 0.0),
    (1.5, -0.5, 0.5), (1.5, 0.0, 0.0),
    (1.5, 0.5, -0.5), (1.5, 0.0, 0.0),
    (2.0, 0.5, 0.2), (1.5, 0.0, 0.0),
    (1.5, -0.4, -0.4), (1.5, 0.0, 0.0),
    (1.5, 0.2, 0.7), (2.0, 0.0, 0.0),
]
STOP_MM = 300
RATE_HZ = 20


def schedule(t):
    """Command (left, right) at t seconds into the pattern, or None when done."""
    for dur, l, r in PATTERN:
        if t < dur:
            return l, r
        t -= dur
    return None


def run(link, log, rate_hz=RATE_HZ, clock=time.monotonic, sleep=time.sleep):
    period = 1.0 / rate_hz
    t0 = clock()
    while True:
        cmd = schedule(clock() - t0)
        if cmd is None:
            link.stop()
            return "done"
        tel = link.latest
        if tel is not None:
            if tel.mode == "manual" or tel.ovr or tel.crit:
                link.stop()
                return "someone took over (or the battery is critical)"
            if 0 <= tel.tof < STOP_MM and (cmd[0] + cmd[1]) > 0:
                link.stop()
                return f"stopped: something {tel.tof} mm ahead"
            log.write(tel, cmd)
        link.drive(*cmd)
        sleep(period)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", default="/dev/serial0")
    ap.add_argument("--url", help="ws://solar-rover.local/ws to run from a PC over WiFi")
    ap.add_argument("--out", default=f"runs/sysid-{time.strftime('%Y%m%d-%H%M%S')}.jsonl")
    a = ap.parse_args(argv)
    link = CarLink(port=a.port, url=a.url)
    log = RunLog(a.out)
    try:
        time.sleep(0.5)
        if link.latest is None:
            print("no telemetry from the car yet; is it on and connected?")
            return 1
        link.send("M auto")
        print(f"driving the test pattern ({sum(p[0] for p in PATTERN):.0f} s), logging to {a.out}")
        print(run(link, log))
    except KeyboardInterrupt:
        print("interrupted")
    finally:
        link.close()
        log.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
