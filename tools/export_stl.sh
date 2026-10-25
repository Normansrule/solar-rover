#!/usr/bin/env bash
# Export every printable part to hardware/stl/. Needs OpenSCAD:
#   sudo apt install openscad
# Usage: tools/export_stl.sh            (all parts)
#        tools/export_stl.sh wheel tire  (only those outputs)
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SCAD="$ROOT/hardware/scad/parts"
OUT="$ROOT/hardware/stl"
mkdir -p "$OUT"

# output-name | source file | -D overrides
PARTS=(
  "fit_test|fit_test.scad|"
  "deck_front|deck_half.scad|half=\"front\""
  "deck_rear|deck_half.scad|half=\"rear\""
  "motor_clamp_a|motor_clamp.scad|side=\"a\""
  "motor_clamp_b|motor_clamp.scad|side=\"b\""
  "wheel|wheel.scad|"
  "tire|tire.scad|"
  "encoder_disc|encoder_disc.scad|"
  "roof_post|roof_post.scad|"
  "turret_base|turret_base.scad|"
  "turret_top|turret_top.scad|"
  "shoulder_mount|shoulder_mount.scad|"
  "upper_plate_horn|upper_plate.scad|variant=\"horn\""
  "upper_plate_idler|upper_plate.scad|variant=\"idler\""
  "crossbar|crossbar.scad|"
  "forearm|forearm.scad|"
  "gripper_palm|gripper_palm.scad|"
  "gripper_horn_arm|gripper_horn_arm.scad|"
  "finger_driven|finger.scad|variant=\"driven\""
  "finger_idler|finger.scad|variant=\"idler\""
  "camera_mount|camera_mount.scad|"
  "nose|nose.scad|"
  "roof_tray_front|roof_tray.scad|variant=\"front\""
  "roof_tray_rear|roof_tray.scad|variant=\"rear\""
  "power_tray|power_tray.scad|"
  "logic_tray|logic_tray.scad|"
  "tray_standoff|tray_standoff.scad|"
  "charge_tray|charge_tray.scad|"
  "module_cradle_tb6612|module_cradle.scad|variant=\"tb6612\""
  "module_cradle_mp1584|module_cradle.scad|variant=\"mp1584\""
  "remote_case_top|remote_case.scad|variant=\"top\""
  "remote_case_bottom|remote_case.scad|variant=\"bottom\""
)

want=("$@")
for entry in "${PARTS[@]}"; do
  IFS='|' read -r name src def <<<"$entry"
  if [ ${#want[@]} -gt 0 ] && [[ ! " ${want[*]} " =~ " $name " ]]; then continue; fi
  args=()
  [ -n "$def" ] && args=(-D "$def")
  echo "==> $name"
  openscad --export-format binstl -o "$OUT/$name.stl" "${args[@]}" "$SCAD/$src" 2>&1 | grep -iE "warning|error" || true
done
echo "STLs are in $OUT"
