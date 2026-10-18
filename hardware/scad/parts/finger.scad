// Gripper finger with gear (print 2): variant="driven" (fits the MG90S horn) and
// variant="idler" (spins on an M3x12 pin + washer). Print flat, 100% infill.
// Glue a strip of TPU or rubber on the inner face for grip.
include <../config.scad>
use <../lib.scad>

variant = "driven";
m = 1.25; z = 16; t = 6;
s = (variant == "driven") ? 1 : -1;   // driven finger sits at +Y of the idler

difference() {
    union() {
        rotate(variant == "driven" ? 0 : 180/z) spur_gear(m, z, t);
        // finger: from the gear to a tip 40 mm ahead. The axes are 20 mm apart, so a
        // tip centre 6 mm toward the other finger leaves a 1 mm gap when closed.
        hull() {
            cylinder(r=7, h=t);
            translate([40, -s*6, 0]) cylinder(r=3.5, h=t);
        }
    }
    if (variant == "driven") {
        hole(2.6);                                // horn screw
        translate([0,0,t-3]) cylinder(d=7.6, h=4); // horn hub
        translate([0,0,t-1.8]) hull() { cylinder(d=6, h=2); translate([15,0,0]) cylinder(d=3.5, h=2); } // single-arm horn
    } else hole(m3_clear);
}
