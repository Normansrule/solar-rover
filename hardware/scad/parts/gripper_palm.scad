// Gripper palm (print 1) with the wrist pivot arm. The gripper MG90S hangs under
// the palm with its spline up through the driven-finger hole. The horn arm
// (gripper_horn_arm.scad) screws onto the side block with 2x M3x8.
// Print with the pivot-arm face (Y = -30.8) on the bed; enable supports
// "touching build plate" for the two small servo posts.
include <../config.scad>
use <../lib.scad>

g1 = [30,  1.4];     // driven finger axis
g2 = [30, -18.6];    // idler finger axis (20 mm centre distance)
py0 = -30.8; py1 = -26.8;      // pivot arm (washer gap to the forearm floor at -25.8)
cx = mg90_l/2 - mg90_shaft_off;
fl_top = -2 - (mg90_h - mg90_flange_z) + mg90_flange_t;   // flange top face (-6.2)

module part() {
    difference() {
        union() {
            // palm plate
            translate([16, py0, -2]) cube([62-16, 12-py0, 4]);
            // pivot arm
            hull() {
                translate([0, py0, 0]) rotate([-90,0,0]) cylinder(r=12, h=py1-py0);
                translate([16, py0, -12]) cube([8, py1-py0, 14]);
            }
            // servo flange posts
            for (x=[g1[0]+cx-mg90_hole_dx/2, g1[0]+cx+mg90_hole_dx/2])
                translate([x-3, g1[1]-3, fl_top]) cube([6, 6, -2-fl_top+0.01]);
            // idler boss
            translate([g2[0], g2[1], -6]) cylinder(d=9, h=4.01);
            // side block for the horn arm
            translate([53, 2, -14]) cube([9, 6, 14]);
        }
        // pivot hole
        translate([0,0,0]) rotate([90,0,0]) hole(m3_clear+0.2);
        // driven spline / horn hub clearance
        translate([g1[0], g1[1], 0]) hole(10);
        // servo flange pilot holes
        for (x=[g1[0]+cx-mg90_hole_dx/2, g1[0]+cx+mg90_hole_dx/2]) translate([x, g1[1], 0]) hole(m2_pilot);
        // idler pin insert
        translate([g2[0], g2[1], 0]) insert_m3(2);
        // servo case clearance under the palm
        translate([g1[0], g1[1], -2]) mg90_case_neg(0.3);
        // horn-arm inserts in the side block (from the +Y face)
        for (z=[-11,-5]) translate([57.5, 8.01, z]) rotate([90,0,0]) cylinder(d=m3_insert_d, h=m3_insert_h);
    }
}
rotate([90,0,0]) translate([0,-py0,0]) part();
