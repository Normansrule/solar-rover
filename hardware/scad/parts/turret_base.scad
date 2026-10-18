// Turret base (print 1). Bolts to the deck with 4x M3x10 into deck inserts.
// The base-yaw MG996R hangs inside on two posts, output shaft up, at the centre.
// Print upright (floor on the bed), 4 walls, 30% infill. No supports.
include <../config.scad>
use <../lib.scad>

floor_t = 4;
wall_t  = 3;
z_rim   = turret_h;                 // local: 0 = deck top
horn_z  = z_rim;                    // horn face level with the rim
case_top = horn_z - mg996_spline_h; // 42
flange_bot = case_top - (mg996_h - mg996_flange_z);   // 32.5

cx = mg996_l/2 - mg996_shaft_off;   // case centre relative to the shaft

difference() {
    union() {
        difference() {
            cylinder(d=turret_d, h=z_rim);
            translate([0,0,floor_t]) cylinder(d=turret_d-2*wall_t, h=z_rim);
        }
        // servo flange posts (one per flange tab)
        for (sx=[-1,1]) translate([cx+sx*mg996_hole_dx/2-4, -mg996_w/2, 0])
            cube([8, mg996_w, flange_bot]);
    }
    // servo flange screw inserts (M3 into the posts, screws from above through the flange)
    for (sx=[-1,1], sy=[-1,1]) translate([cx+sx*mg996_hole_dx/2, sy*mg996_hole_dy/2, 0]) insert_m3(flange_bot);
    // deck screws (heads on the floor)
    for (a=[45:90:315]) rotate(a) translate([turret_deck_r,0,0]) { hole(m3_clear); translate([0,0,floor_t-0.01]) cylinder(d=m3_head_d+0.6, h=60); }
    // wire hole (matches the deck hole)
    translate([0,-18,0]) hole(12);
    // case clearance under the servo (body bottom sits just above the floor)
    translate([0,0,case_top]) mg996_case_neg(0.6);
}
