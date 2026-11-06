// Solar Rover - shared dimensions (all mm)
// Coordinate system: X = forward, Y = left, Z = up. Origin = deck centre, ground at Z=0.
//
// Values marked MEASURE come from typical datasheets/clones and vary between
// suppliers. Measure your parts with calipers and edit them here before printing;
// every part reads its dimensions from this file.

$fn = 64;

// ---------- print / fit ----------
tol        = 0.25;   // general clearance per side
wall       = 2.4;    // default wall (6 perimeters at 0.4 mm nozzle)

// ---------- fasteners ----------
m3_clear     = 3.4;   // M3 clearance hole
m3_insert_d  = 4.1;   // hole for M3x5x4 brass heat-set insert (OD ~4.6)
m3_insert_h  = 5.5;   // insert hole depth
m3_head_d    = 6.2;   // socket head counterbore
m3_head_h    = 3.2;
m3_nut_af    = 5.6;   // nut across flats (+clearance)
m3_nut_h     = 2.6;
m25_clear    = 2.9;
m25_insert_d = 3.5;   // M2.5 insert hole (OD ~4.0)
m2_clear     = 2.4;
m2_pilot     = 1.7;   // pilot hole for M2 thread-forming screw

// ---------- deck ----------
deck_l     = 300;     // printed as two 150 mm halves
deck_w     = 146;
deck_t     = 4;
rib_h      = 8;
rib_w      = 2.4;
flange_h   = 14;      // joint flange (bolts the halves together)
flange_t   = 5;

// ---------- drive ----------
wheel_d       = 65;
wheel_w       = 26;
axle_z        = wheel_d/2;    // axle height above ground
axle_x        = 100;          // front axle at +axle_x, rear at -axle_x
// TT gear motor (dual-shaft, 1:48). MEASURE.
tt_body_l     = 64;           // along X (gearbox + motor can)
tt_body_t     = 22.5;         // along Y (shaft direction)
tt_body_h     = 18.8;         // along Z
tt_shaft_from_end = 11.3;     // shaft centre to gearbox front end
tt_shaft_d    = 5.4;          // D-shaft across round
tt_shaft_flat = 3.7;          // D-shaft across flats
tt_shaft_len  = 9;            // per side, beyond the body face
// two through-holes parallel to the shaft, positions relative to shaft centre [x, z]. MEASURE.
tt_holes      = [[-20.0, 5.5], [-20.0, -5.5]];
motor_outer_y = 72;           // motor face nearest the wheel
clamp_t       = 3;            // clamp plate thickness
deck_bottom_z = axle_z + tt_body_h/2 + clamp_t;   // 44.6
deck_top_z    = deck_bottom_z + deck_t;

// ---------- encoder ----------
enc_slots   = 20;
enc_disc_d  = 22;
enc_disc_t  = 1.6;

// ---------- turret / arm ----------
turret_x      = 108;
turret_d      = 84;           // base OD; top plate skirt overlaps it
turret_h      = 48;           // fixed base height above deck (servo hangs inside)
turret_deck_r = 30;           // 4x M3 at 45/135/225/315 deg: turret base to deck
turret_bolts  = [[20,28],[-20,28],[20,-28],[-20,-28]];  // module interface on the turret top (4x M3)
shoulder_z    = 45;           // shoulder axis above the turret top plate
// MG996R servo. MEASURE.
mg996_l   = 40.7;  mg996_w = 19.7;  mg996_h = 37.0;   // case (h = to top of case)
mg996_flange_l = 54.5; mg996_flange_z = 27.5;  mg996_flange_t = 2.5;
mg996_hole_dx = 49.5; mg996_hole_dy = 10.0; mg996_hole_d = 4.5;
mg996_shaft_off = 10.0;       // output shaft centre from case end
mg996_spline_h  = 6.0;        // case top to horn face
mg996_horn_d    = 22;         // round metal horn
mg996_horn_hole_r = 7.5;      // horn screw circle radius (MEASURE: usually 4 holes)
// MG90S servo. MEASURE.
mg90_l = 22.8; mg90_w = 12.2; mg90_h = 22.7;
mg90_flange_l = 32.5; mg90_flange_z = 16.0; mg90_flange_t = 2.5;
mg90_hole_dx = 27.6; mg90_hole_d = 2.0;
mg90_shaft_off = 6.0;
mg90_spline_h = 5.0;
mg90_horn_d = 7;              // horn hub (single-arm horn)

link_upper = 100;   // shoulder -> elbow
link_fore  = 90;    // elbow -> wrist
link_hand  = 70;    // wrist -> fingertip

// ---------- power / electronics ----------
cell_holder = [77, 80, 21];   // 4x18650 holder footprint. MEASURE.
pi0_holes   = [58, 23];       // Raspberry Pi Zero 2 W, M2.5
pca_holes   = [55.9, 19.0];   // Adafruit-style PCA9685, M2.5. MEASURE (clones vary)
esp_exp_holes = [47, 70];     // ESP32 38-pin screw-terminal expansion board (77x54 mm), M3. MEASURE.
cradle_tb6612 = [20.5, 20.5];  // TB6612FNG breakout board outline. MEASURE.
cradle_mp1584 = [22.4, 17.0];  // MP1584 mini buck outline. MEASURE.
cradle_e220   = [21.0, 36.0];  // Ebyte E220-900T22D LoRa module, without the SMA jack. MEASURE.
lora_x        = -162;          // E220 cradle centre: hangs under the rear roof tray's overhang,
lora_boss_dx  = -13;           //   SMA jack out the back; its 2 screws sit lora_boss_dx behind
lora_boss_dy  = 6;             //   the centre, +-lora_boss_dy to the sides (M3 inserts in the tray)
module_slot_w = 2.6;          // module-plate slots: M2/M2.5 screw + nut, or a 2.5 mm zip tie
tray_standoff_h = 32;         // power tray top to logic tray underside
// charge tray: a slotted plate on 4 legs over the battery holder
charge_tray_c    = [-106, 0];         // centre on the deck (same as the battery holder)
charge_tray_size = [76, 102];         // X, Y
charge_tray_z    = 24;                // deck top to plate underside (holder is 21 mm tall)
charge_tray_legs = [for (x=[-34,28], sy=[-1,1]) [x, sy*46]];   // relative to the centre; the front pair clears the motor-clamp inserts

// ---------- body / solar ----------
panel     = [110, 136, 3];    // 6V 2W panel: X, Y, thickness. MEASURE.
roof_post_h = 75;             // deck top to roof tray underside (room for plugged-in jumpers)
tray_frame  = 4;
roof_front_x = 60;            // front tray spans -60..60, rear tray -180..-60
