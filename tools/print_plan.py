#!/usr/bin/env python3
"""Plan the prints: filament and time per part, packed onto bed-sized plates.

Reads the print table in docs/assembly.md (quantity, walls, infill, TPU) and
the STLs in hardware/stl, estimates how much filament each part takes and
how long it prints, and packs the parts onto plates that fit the bed and
finish overnight.  Writes docs/print-plan.md.

    python3 tools/print_plan.py            # rewrite docs/print-plan.md
    python3 tools/print_plan.py --check    # fail if the plan is out of date,
                                           # a part is missing from the table,
                                           # or the BOM filament is too low

The numbers are estimates (checked against PrusaSlicer: filament within 7%, time
within about 20%).  The slicer has the final word, but this is enough to buy filament
and plan evenings.  Needs trimesh (pip install trimesh==5.1.1).
"""
import math, pathlib, re, sys

import trimesh

ROOT = pathlib.Path(__file__).resolve().parent.parent
STL = ROOT / "hardware" / "stl"
ASSEMBLY = ROOT / "docs" / "assembly.md"
BOM = ROOT / "docs" / "bom.csv"
OUT = ROOT / "docs" / "print-plan.md"

BED = 220.0          # mm, square bed
GAP = 6.0            # mm between parts on a plate
NIGHT_H = 11.0       # a plate should finish overnight

# Calibrated against PrusaSlicer 2.7 (generic profile, settings as in
# assembly.md) on every part in hardware/stl: filament within 7% per part,
# 3% on average; time within about 20%.
WALL_T = 0.405       # mm of side wall per perimeter
SKIN_T = 0.64        # mm of top or bottom skin (4 layers, partly overlapping the walls)
MATERIALS = {
    #        g/cm3, mm3/s, $/kg
    "PETG": (1.27, 2.4, 20.0),
    "TPU":  (1.21, 1.4, 25.0),
}
PART_MIN = 9         # minutes per part on top of extrusion (travel, slow first layer)
PLATE_SETUP_MIN = 8  # heat-up, bed level, purge


def read_table(text):
    """Rows of the assembly.md print table -> {stl: dict(qty, walls, infill, material)}."""
    rows = {}
    for line in text.splitlines():
        if not line.startswith("| `"):
            continue
        cells = [c.strip() for c in line.strip("|").split("|")]
        if len(cells) < 4 or not cells[1].split()[0].isdigit():
            continue
        names = re.findall(r"`([a-z0-9_]+)`", cells[0])
        qty = int(cells[1].split()[0])
        notes = cells[3]
        walls = re.search(r"(\d+) walls", notes)
        infill = re.search(r"(\d+)% infill", notes)
        for n in names:
            rows[n] = dict(
                qty=qty,
                walls=int(walls.group(1)) if walls else 4,
                infill=int(infill.group(1)) / 100 if infill else 0.30,
                material="TPU" if "TPU" in notes else "PETG",
            )
    return rows


def estimate(mesh, walls, infill, material):
    """Grams and minutes for one copy of a part.

    The part is a solid shell filled with sparse infill.  Faces that point up
    or down get top/bottom skin, the rest get the side walls; on thin parts
    the shell is the whole part.
    """
    vol = mesh.volume                                         # mm3
    flat = abs(mesh.face_normals[:, 2]) > 0.7
    area = mesh.area_faces
    shell = min(vol, area[flat].sum() * SKIN_T + area[~flat].sum() * walls * WALL_T)
    plastic = shell + infill * (vol - shell)                  # mm3
    dens, rate, _ = MATERIALS[material]
    return plastic / 1000 * dens, plastic / rate / 60 + PART_MIN


def footprint(mesh):
    lo, hi = mesh.bounds
    return float(hi[0] - lo[0]), float(hi[1] - lo[1]), float(hi[2] - lo[2])


def pack(items):
    """Pack (name, w, d, minutes) onto plates: shelf packing, at most NIGHT_H hours.

    First-fit decreasing by footprint: each item goes on the first plate that
    has room and time left, else a new plate.  Good enough for ~50 parts.
    """
    plates = []   # dict(items, shelves=[(y, height, x_used)], minutes)
    for it in sorted(items, key=lambda i: -max(i[1], i[2]) * min(i[1], i[2])):
        name, w, d, mins = it
        placed = False
        for p in plates:
            if p["minutes"] + mins > NIGHT_H * 60:
                continue
            if place(p, w, d) or place(p, d, w):
                p["items"].append(it)
                p["minutes"] += mins
                placed = True
                break
        if not placed:
            p = dict(items=[it], shelves=[], minutes=PLATE_SETUP_MIN + mins)
            if not (place(p, w, d) or place(p, d, w)):
                raise SystemExit(f"{name} does not fit a {BED:.0f} mm bed")
            plates.append(p)
    return plates


def place(p, w, d):
    """Put a w x d box on plate p (shelves along Y).  True if it fits."""
    for i, (y, h, x) in enumerate(p["shelves"]):
        if d <= h and x + w <= BED:
            p["shelves"][i] = (y, h, x + w + GAP)
            return True
    y = sum(h + GAP for _, h, _ in p["shelves"])
    if y + d <= BED and w <= BED:
        p["shelves"].append((y, d, w + GAP))
        return True
    return False


def bom_grams():
    """Grams of filament the BOM budgets, per material (from the '~800 g' notes)."""
    out = {}
    for line in BOM.read_text().splitlines():
        if "filament" not in line:
            continue
        m = re.search(r"~(\d+) g", line)
        mat = "TPU" if "TPU" in line else "PETG"
        if m:
            out[mat] = int(m.group(1))
    return out


def plan():
    table = read_table(ASSEMBLY.read_text())
    stls = sorted(p.stem for p in STL.glob("*.stl"))
    missing = [s for s in stls if s not in table]
    unknown = [s for s in table if s not in stls]

    parts = []   # name, qty, material, grams_each, minutes_each, (w, d, h)
    for s in stls:
        if s not in table:
            continue
        r = table[s]
        mesh = trimesh.load(STL / f"{s}.stl")
        g, m = estimate(mesh, r["walls"], r["infill"], r["material"])
        parts.append((s, r["qty"], r["material"], g, m, footprint(mesh)))

    groups = {}
    for s, qty, mat, g, m, (w, d, h) in parts:
        if s.startswith("fit_test"):
            continue                       # printed on its own first
        for k in range(qty):
            groups.setdefault(mat, []).append((s, w, d, m))
    plates = {mat: pack(items) for mat, items in groups.items()}
    return parts, plates, missing, unknown


def render(parts, plates):
    by = {p[0]: p for p in parts}
    tot_g = {}
    tot_m = {}
    for s, qty, mat, g, m, _ in parts:
        tot_g[mat] = tot_g.get(mat, 0) + g * qty
        tot_m[mat] = tot_m.get(mat, 0) + m * qty
    L = []
    L.append("# Print plan")
    L.append("")
    L.append("> Generated by `python3 tools/print_plan.py` from the print table in "
             "[assembly.md](assembly.md) and the STLs. Don't edit by hand. "
             "Grams match PrusaSlicer within 7% and hours within about 20% (generic i3-style printer; a fast CoreXY printer takes about half the time). Your slicer has the final word.")
    L.append("")
    L.append(f"Settings as in assembly.md: 0.4 mm nozzle, 0.2 mm layers, {BED:.0f} × {BED:.0f} mm bed. "
             f"Plates are packed to finish in under {NIGHT_H:.0f} hours so each one can run overnight "
             "or while you are out.")
    L.append("")
    L.append("## Totals")
    L.append("")
    L.append("| Material | Parts | Filament | Print time | Cost |")
    L.append("|---|---|---|---|---|")
    for mat in MATERIALS:
        if mat not in tot_g:
            continue
        n = sum(q for _, q, mm, *_ in parts if mm == mat)
        cost = tot_g[mat] / 1000 * MATERIALS[mat][2]
        L.append(f"| {mat} | {n} | {tot_g[mat]:.0f} g | {tot_m[mat] / 60:.0f} h | ${cost:.0f} |")
    L.append("")
    spools = math.ceil(tot_g.get("PETG", 0) * 1.1 / 1000)   # 10% for purge, brims, a reprint
    L.append(f"Buy {spools} × 1 kg of PETG and a small spool (250 g is plenty) of TPU 95A. "
             "The extra PETG covers purge lines, brims and a reprint or two.")
    L.append("")
    L.append("## Plates")
    L.append("")
    L.append("Print plate 0 (the fit test) first and set the MEASURE values before the rest "
             "(build step 0). After that the order does not matter, but plates 1 and 2 hold "
             "the deck halves, which you need first when building.")
    L.append("")
    ft = by.get("fit_test")
    if ft:
        L.append(f"**Plate 0 · PETG · {ft[3]:.0f} g · {ft[4] / 60 + PLATE_SETUP_MIN / 60:.1f} h**: `fit_test`")
        L.append("")
    n = 0
    for mat in MATERIALS:
        for p in sorted(plates.get(mat, []), key=lambda p: (not any(i[0].startswith("deck") for i in p["items"]), -p["minutes"])):
            n += 1
            counts = {}
            for name, *_ in p["items"]:
                counts[name] = counts.get(name, 0) + 1
            g = sum(by[name][3] * c for name, c in counts.items())
            parts_txt = ", ".join(f"`{k}`" + (f" ×{c}" if c > 1 else "") for k, c in counts.items())
            L.append(f"**Plate {n} · {mat} · {g:.0f} g · {p['minutes'] / 60:.1f} h**: {parts_txt}")
            L.append("")
    L.append("## Per part")
    L.append("")
    L.append("| Part | Qty | Material | Size (mm) | Each | Total | Time each |")
    L.append("|---|---|---|---|---|---|---|")
    for s, qty, mat, g, m, (w, d, h) in sorted(parts, key=lambda p: -p[3] * p[1]):
        L.append(f"| `{s}` | {qty} | {mat} | {w:.0f} × {d:.0f} × {h:.0f} | {g:.0f} g | {g * qty:.0f} g | "
                 f"{m / 60:.1f} h |" if m >= 60 else
                 f"| `{s}` | {qty} | {mat} | {w:.0f} × {d:.0f} × {h:.0f} | {g:.0f} g | {g * qty:.0f} g | "
                 f"{m:.0f} min |")
    L.append("")
    return "\n".join(L), tot_g


def main():
    parts, plates, missing, unknown = plan()
    text, tot_g = render(parts, plates)
    errors = []
    if missing:
        errors.append("STLs not in the assembly.md print table: " + ", ".join(missing))
    if unknown:
        errors.append("assembly.md lists parts with no STL: " + ", ".join(unknown))
    budget = bom_grams()
    for mat, g in tot_g.items():
        if mat in budget and budget[mat] < 0.95 * g:
            errors.append(f"bom.csv budgets ~{budget[mat]} g of {mat}, the parts need ~{g:.0f} g")
    if "--check" in sys.argv:
        if not OUT.exists() or OUT.read_text() != text:
            errors.append("docs/print-plan.md is out of date: run python3 tools/print_plan.py")
    else:
        OUT.write_text(text)
        print(f"wrote {OUT.relative_to(ROOT)}")
    for mat, g in tot_g.items():
        print(f"{mat}: {g:.0f} g")
    for e in errors:
        print("ERROR:", e)
    sys.exit(1 if errors else 0)


if __name__ == "__main__":
    main()
