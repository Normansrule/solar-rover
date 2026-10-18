#!/usr/bin/env python3
"""Print the size of every STL and flag parts that will not fit the print bed
or do not sit on Z=0.  Usage: python3 tools/stl_check.py [bed_x bed_y bed_z]"""
import struct, sys, pathlib

def bbox(path):
    data = path.read_bytes()
    if data[:5] == b"solid" and b"facet" in data[:400]:
        pts = [tuple(map(float, l.split()[1:4])) for l in data.decode().splitlines() if l.strip().startswith("vertex")]
    else:
        n = struct.unpack_from("<I", data, 80)[0]
        pts = []
        for i in range(n):
            off = 84 + i * 50 + 12
            v = struct.unpack_from("<9f", data, off)
            pts += [v[0:3], v[3:6], v[6:9]]
    lo = [min(p[i] for p in pts) for i in range(3)]
    hi = [max(p[i] for p in pts) for i in range(3)]
    return lo, hi

bed = [float(a) for a in sys.argv[1:4]] if len(sys.argv) >= 4 else [220, 220, 250]
stl_dir = pathlib.Path(__file__).resolve().parent.parent / "hardware" / "stl"
bad = 0
for p in sorted(stl_dir.glob("*.stl")):
    lo, hi = bbox(p)
    size = [h - l for h, l in zip(hi, lo)]
    flat = sorted(size[:2])
    fits = flat[0] <= min(bed[:2]) and flat[1] <= max(bed[:2]) and size[2] <= bed[2]
    note = "" if fits else "  <-- larger than the bed"
    if abs(lo[2]) > 0.01 and not p.stem.startswith("deck"):
        note += f"  (min z {lo[2]:.1f}: lay flat in the slicer)"
    bad += not fits
    print(f"{p.stem:22s} {size[0]:7.1f} x {size[1]:7.1f} x {size[2]:6.1f} mm{note}")
sys.exit(1 if bad else 0)
