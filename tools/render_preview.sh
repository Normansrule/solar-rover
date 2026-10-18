#!/usr/bin/env bash
# Render docs/img/assembly.png from the exported STLs (run export_stl.sh first).
# Headless machines: sudo apt install xvfb, the script uses xvfb-run if present.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
mkdir -p "$ROOT/docs/img"
RUN=(); command -v xvfb-run >/dev/null && RUN=(xvfb-run -a)
render() { "${RUN[@]}" openscad --imgsize=1600,1000 --colorscheme=Tomorrow \
  --camera="$2" -D 'pose="carry"' -o "$ROOT/docs/img/$1.png" "$ROOT/hardware/scad/assembly.scad"; }
render assembly      40,0,100,62,0,35,820
render assembly_side 25,0,110,90,0,0,860
render assembly_top  25,0,40,0,0,0,860
echo "Images in $ROOT/docs/img"
