# Printing and assembly

> **Status: design v0.2, not yet test-built.** Print the fit-test parts (step 0) before printing everything. Every bought-part dimension that varies between suppliers is marked **MEASURE** in `hardware/scad/config.scad`. Edit it there and re-export (`bash tools/export_stl.sh`), and every part updates.

## Print settings

- **Material:** PETG for everything (the car sits in the sun; PLA softens at 55–60 °C), and TPU 95A for the tires.
- **Slicer:** 0.4 mm nozzle, 0.2 mm layers, 4 walls, 30% gyroid infill unless the table says otherwise.
- **Bed:** all parts fit a 220 × 220 mm bed (`python3 tools/stl_check.py` checks this).

| STL | Qty | Orientation | Notes |
|---|---|---|---|
| `fit_test` | 1 | flat | print first (build step 0); about 20 g |
| `deck_front`, `deck_rear` | 1 each | flip top face down | 5 walls. Ribs and bosses face up. |
| `motor_clamp_a` | 2 | as exported | front-left + rear-right; no supports |
| `motor_clamp_b` | 2 | as exported | front-right + rear-left |
| `wheel` | 4 | as exported (outer face down) | 5 walls, 40% infill |
| `tire` | 4 | as exported | **TPU**, 25 mm/s, 3 walls, 15% infill |
| `encoder_disc` | 4 | as exported | black filament works best |
| `roof_post` | 10 | upright | 100% infill |
| `turret_base` | 1 | as exported | |
| `turret_top` | 1 | as exported (upside down) | |
| `shoulder_mount` | 1 | as exported | 40% infill |
| `upper_plate_horn`, `upper_plate_idler` | 1 each | flat | 5 walls, 40% infill |
| `crossbar` | 1 | upright | |
| `forearm` | 1 | as exported (floor down) | 40% infill |
| `gripper_palm` | 1 | as exported | supports "touching build plate" for the two small servo posts |
| `gripper_horn_arm` | 1 | flat | |
| `finger_driven`, `finger_idler` | 1 each | flat | 100% infill |
| `camera_mount` | 1 | as exported | |
| `nose` | 1 | stand on its back face | supports touching build plate for the 2 lugs |
| `roof_tray_front`, `roof_tray_rear` | 1 each | as exported | PETG |
| `power_tray`, `logic_tray` | 1 each | flat | |
| `tray_standoff` | 4 | upright | |
| `charge_tray` | 1 | as exported (plate down, legs up) | stands over the battery holder |
| `module_cradle_tb6612` | 2 | as exported (floor down) | snap-in holders for the two motor drivers. MEASURE the board (`cradle_tb6612`) |
| `module_cradle_mp1584` | 1 | as exported (floor down) | snap-in holder for the 5.1 V buck (`cradle_mp1584`) |
| `remote_case_top`, `remote_case_bottom` | 1 each | outer face down | |

**Total:** about 850 g of PETG and 100 g of TPU.

## Heat-set inserts

Use a soldering iron at about 230 °C with an insert tip, and press each insert in straight. You need about 69 M3 inserts and 8 M2.5 inserts:

| Part | M3 inserts |
|---|---|
| Deck | 8 motor-clamp bosses (from **below**), 4 turret, 2 nose, 4 tray, 4 charge tray (flush in the plate) |
| Roof posts | 2 each (both ends) |
| Turret base | 4 (servo posts) |
| Turret top | 4 shoulder mount + 2 camera |
| Shoulder mount | 4 servo + 1 pivot |
| Forearm | 4 servo + 2 pivots |
| Gripper palm | 1 idler pin + 2 horn arm |
| Remote bottom shell | 4 |

The M2.5 inserts go in the logic tray: 4 for the Pi and 4 for the PCA9685.

## Screw list

| Screw | Qty | Where |
|---|---|---|
| M3 × 8 | 45 | clamps → deck (8), LM393s (4 + nuts), roof trays (10), turret-top → yaw horn (4), arm plates → horns (8), horn arm → palm (2), camera (2), nose (2) … |
| M3 × 10 | 35 | roof posts from under the deck (10), turret base → deck (4), servo flanges (12, with washers), pivots (3, with washers), shoulder mount → turret (4) |
| M3 × 12 | 2 | idler finger pin (with washer) |
| M3 × 16 | 8 | deck joint (3 + nuts), remote case (4) |
| M3 × 30 | 4 | charge tray legs → deck |
| M3 × 45 | 12 | through the TT motors (8, nyloc nuts on the wheel side); logic tray → standoffs → power tray → deck (4) |
| M3 × 60 | 1 | crossbar (nyloc) |
| M2.5 × 6 | 8 | Pi, PCA9685 |
| M2 × 6–10 | ~36 | modules on the tray slots (with nuts), joysticks, camera, ToF, wheel retainers (M2 × 10 + washer) |
| M3 × 6 | 4 | expansion board → logic tray inserts |

**Tools:** 2.5 mm hex key (all the M3 screws), 2.0 mm (M2.5), 1.5 mm or a small Phillips (M2), a soldering iron (only for the heat-set inserts and the two inline diodes), a wire stripper, and a multimeter.

## Build steps

0. **Fit test.** Print `fit_test` first (one flat plate, about 35 minutes). Each fit comes in a few sizes, and the engraved number under the one that fits is the value to put in `hardware/scad/config.scad`:

   | On the coupon | Check | Set in `config.scad` |
   |---|---|---|
   | 4 D-shaft bosses (−0.1 … +0.2) | Press a TT motor shaft into each. Pick the firmest one you can still push on by hand. | Add that number to `tt_shaft_d` and `tt_shaft_flat` (e.g. +0.1 → 5.5 and 3.8) |
   | TT hole pattern | Hold a motor flat on it, shaft in the big hole: an M3 screw must pass through both small holes into the motor. | `tt_holes` |
   | M3 inserts 3.9 / 4.1 / 4.3 | Set an insert in each with the iron. Pick the one that goes in straight and doesn't push out with a screw. | `m3_insert_d` |
   | M2.5 inserts 3.3 / 3.5 / 3.7 | The same, for the Pi and PCA9685 inserts. | `m25_insert_d` |
   | M2 pilots 1.5 / 1.7 / 1.9 | Drive an M2 screw into each. Pick the one that bites without splitting the boss. | `m2_pilot` |
   | MG996R and MG90S windows | The servo body should drop in with a little play. Too tight or loose: measure the servo. | `mg996_l`, `mg996_w`, `mg90_l`, `mg90_w` (or `tol`) |
   | M3 holes 3.2 / 3.4 / 3.6, hex pocket | An M3 screw slides through; a nut presses into the pocket. | `m3_clear`, `m3_nut_af` |

   Then re-export (`bash tools/export_stl.sh`). If you changed the TT values, print one `wheel` and `encoder_disc` to confirm before the rest.
1. **Deck.** Set the inserts. Bolt the two halves together through the joint flanges: 3 × M3 × 16 with the nuts in the front-half pockets. The alignment pins locate the halves.
2. **Drive.** For each wheel:
   1. Press the encoder disc onto the motor's inner shaft (hub toward the motor).
   2. Bolt the motor to its clamp: 2 × M3 × 45, head on the clamp side, nyloc nut on the wheel side.
   3. Screw the LM393 onto the shelf so the disc runs in the middle of the fork. Shim with washers if needed.
   4. Bolt the clamp to the deck underside: 2 × M3 × 8 into the inserts.
   5. Stretch a tire onto a wheel, push the wheel on, and fit the M2 × 10 retaining screw + washer.
3. **Battery, charge tray and power tray.** `assembly.scad` shows this layout, and `tools/fit_check.py` has checked it with typical module sizes and the jumpers plugged in. **Set the bucks and the charger with a multimeter before connecting the pack** (wiring.md section 6).
   1. Wire the 2S2P pack and BMS (wiring.md section 5) and screw the holder to the deck slots at the rear.
   2. Charge tray (left, right, front and rear as seen from the driver's seat): fix the relay module along the left edge, the CN3795 (rear) and the "solar" INA219 (front) across the middle, and the "batt" INA219 (rear) and the CHG_IN and PACK+ lever connectors (front) along the right edge. Use M2 + nuts through the slots, zip ties or foam tape. Wire them per wiring.md section 5, then stand the tray over the holder and screw it down through its legs (4 × M3 × 30).
   3. Power tray: screw the PCA9685 into its inserts along the left edge. Stand the XL4015 across the car at the rear edge. Press the two TB6612 drivers and the MP1584 into their printed cradles and screw each cradle down through two neighbouring slots (2 × M2 × 8 + nuts): the MP1584 in the middle next to the PCA9685, and the two TB6612s one behind the other right of centre. The GND connector goes at the front, and MAIN+ and 5V along the right edge. Solder the TB6612 header pins pointing **up** so the jumpers plug in from above. Wire it per wiring.md sections 3 and 5, then screw the tray to the deck.
4. **Logic tray.** Plug the ESP32 DevKit into the expansion board and screw the board into the tray inserts (4 × M3 × 6). Screw the Pi into its M2.5 inserts. Fix the MPU-6050 and the I2C hub on the slotted strip (M2 + nut, or zip ties). Connect everything with Dupont jumpers per wiring.md sections 2–4. Stack the logic tray on the 4 standoffs and fix the stack with 4 × M3 × 45.
5. **Turret and yaw servo.** Centre the yaw servo first (step 9). Screw it onto the posts inside the turret base, output shaft up. Bolt the base to the deck (4 × M3 × 10). Screw the turret top to the servo's round metal horn (4 × M3 × 8 from above), facing forward. Feed the servo cable down through the floor hole.
6. **Arm.**
   1. Fit the shoulder servo into the shoulder mount: case pointing down, horn side +Y (the car's left).
   2. Bolt the mount to the turret top (4 × M3 × 10).
   3. Screw the horn plate to the shoulder horn. Hang the idler plate on the mount's pivot (M3 × 10 + washer).
   4. Put the elbow servo in the forearm and screw its horn to the far end of the horn plate. Pivot the idler plate on the forearm's insert.
   5. Fit the crossbar (M3 × 60).
   6. Wrist servo into the forearm, then the gripper palm (pivot) and horn arm, then the gripper servo and fingers.
   7. Run all servo cables along the arm and down the turret-top hole, with a slack loop inside the turret base for ±90° of yaw.
7. **Sensors and body.** Fit the VL53L0X behind the nose window, then bolt the nose to the deck. Fit the camera to its mount on the turret top. Fit the roof posts (M3 × 10 from under the deck), drop the panels into the roof trays (they snap under the lips), and screw the trays down. Panel leads go through the hole at each tray's front.
8. **First power-up.**
   1. Fit the 7.5 A fuse **with the arm resting on something**.
   2. Switch on, keep the car still for 1 s (gyro calibration), and open the serial monitor (115200).
   3. Check `Solar Rover ready. arm=ok imu=ok tof=ok`.
   4. Put the car on the floor with about 50 cm clear all round and send `TEST WHEELS`. It runs each wheel on its own for under a second and prints one line per wheel, then `TEST done 4/4 ok`. Fix whatever it reports: `reversed` means swap that motor's two wires at the TB6612; `encoder swapped` names the two encoder jumpers to swap. Send `S` to stop it at any time. (On a stand the wheels can't turn the car, so it can only say `direction unknown`; then check by eye that each wheel rolls forward.)
9. **Servo calibration.**
   1. Before fitting each horn, send `P park` so the servos sit at their park angles, then fit the horn at the matching position.
   2. Fine-tune each joint with `CAL <joint> <offset_deg> <dir>`, e.g. `CAL shoulder 92 1`. The values are saved in flash.
   3. Logical zeros: yaw 0 = forward; shoulder 0 = horizontal; elbow 0 = straight; wrist 0 = in line; gripper 0 = closed.
10. **Charging check.** Plug in the 12 V adapter. Telemetry should show `"chg":"wall"` and a charge current around 1–1.5 A. Unplug it and put the car in the sun: `"chg":"solar"`. Warm the DS18B20 probe past 45 °C with a hair dryer and check that charging stops and the relay clicks off (`"chg":"off_temp"`).
11. **Handheld remote.**
   1. Set the MT3608 boost to **5.0 V** with a multimeter before connecting anything to it.
   2. Screw the joysticks to the lid standoffs (M2) and push the two buttons into their 12 mm holes.
   3. Lay the ESP32 between the rails in the bottom shell and the 18650 holder in its cradle. Stick the TP4056, MT3608 and the 0–25 V voltage-sensor module to the floor with foam tape: the TP4056's USB-C port goes at the right-hand slot, and the slide switch in its slot.
   4. Wire it per wiring.md section 8 with Dupont jumpers, then close the case with 4 × M3 × 16.
   5. Flash it (software.md section 2) and pair it with the car.
