// Forearm (print 1). Holds the elbow MG996R at the elbow end (its horn screws to
// the upper horn plate) and the wrist MG90S at the far end. Pivot inserts on the
// -Y face take the M3 pivot screws from the idler plate and the gripper.
// Print with the -Y face (floor) on the bed. No supports.
include <../config.scad>
use <../lib.scad>

horn_y  = 23.2;
top_y   = horn_y - mg996_spline_h;      // 17.2
bot_y   = top_y - mg996_h;              // -19.8
floor_t = 6;
fy      = bot_y - floor_t;              // -25.8 (floor outer face)
cx      = mg996_l/2 - mg996_shaft_off;
flange_y = top_y - (mg996_h - mg996_flange_z);   // 7.7
x_lo = -mg996_shaft_off - (mg996_flange_l-mg996_l)/2;    // -16.9
x_hi = mg996_l - mg996_shaft_off + (mg996_flange_l-mg996_l)/2;  // 37.6
zw  = mg996_w/2 + 0.3;
L   = link_fore;
// wrist MG90S: horn face at y=8, case pointing back toward the elbow
w_top = 3;
w_bot = w_top - mg90_h;                 // -19.7
w_fl  = w_top - (mg90_h - mg90_flange_z);      // -3.7 underside of flange
w_cx  = mg90_l/2 - mg90_shaft_off;
wzw   = mg90_w/2 + 0.3;

module elbow_servo() { translate([0, top_y, 0]) rotate([-90,0,0]) children(); }
module wrist_servo() { translate([L, w_top, 0]) multmatrix([[-1,0,0,0],[0,0,1,0],[0,1,0,0],[0,0,0,1]]) children(); }

module part() {
    difference() {
        union() {
            // elbow box
            translate([x_lo-2.4, fy, -zw-2.4]) cube([x_hi-x_lo+4.8, floor_t+0.01, 2*(zw+2.4)]);
            for (sz=[-1,1]) translate([x_lo-2.4, bot_y, sz>0 ? zw : -zw-2.4]) cube([x_hi-x_lo+4.8, flange_y-bot_y, 2.4]);
            translate([x_lo-2.4, bot_y, -zw]) cube([-mg996_shaft_off-0.3-(x_lo-2.4), flange_y-bot_y, 2*zw]);
            translate([mg996_l-mg996_shaft_off+0.3, bot_y, -zw]) cube([x_hi+2.4-(mg996_l-mg996_shaft_off+0.3), flange_y-bot_y, 2*zw]);
            // beam
            translate([x_hi, fy, -10]) cube([L-x_hi, floor_t, 20]);
            for (sz=[-1,1]) translate([x_hi, bot_y, sz>0 ? 7.6 : -10]) cube([L-24-x_hi, 8, 2.4]);
            // wrist pocket
            translate([L-24, fy, -wzw-2.4]) cube([24+13, w_fl-fy, 2*(wzw+2.4)]);
            translate([L, fy, 0]) rotate([-90,0,0]) cylinder(r=wzw+2.4, h=0.01);
        }
        elbow_servo() mg996_case_neg(0.3);
        wrist_servo() mg90_case_neg(0.3);
        // elbow flange inserts
        for (x=[cx-mg996_hole_dx/2, cx+mg996_hole_dx/2], z=[-mg996_hole_dy/2, mg996_hole_dy/2])
            translate([x, flange_y+0.01, z]) rotate([90,0,0]) cylinder(d=m3_insert_d, h=m3_insert_h);
        // wrist flange pilot holes (MG90S ships with M2 self-tappers)
        for (x=[L-(w_cx-mg90_hole_dx/2), L-(w_cx+mg90_hole_dx/2)])
            translate([x, w_fl+0.01, 0]) rotate([90,0,0]) cylinder(d=m2_pilot, h=8);
        // pivot inserts
        for (x=[0, L]) translate([x, fy-0.01, 0]) rotate([-90,0,0]) cylinder(d=m3_insert_d, h=m3_insert_h);
        // cable channel along the beam
        translate([x_hi-1, bot_y+0.5, -4]) cube([L-24-x_hi+2, 6, 8]);
    }
}
rotate([90,0,0]) translate([0,-fy,0]) part();
