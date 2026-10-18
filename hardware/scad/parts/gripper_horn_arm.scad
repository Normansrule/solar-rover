// Gripper horn arm (print 1). Screws to the wrist MG90S horn (2x M2 through the
// slots into the horn, plus the horn's centre screw) and to the palm side block
// (2x M3x8). Print flat.
include <../config.scad>
use <../lib.scad>
t = 4;
// XZ outline drawn in XY for printing (Y here = Z on the robot)
difference() {
    union() {
        // hub on the horn, then a bar that stays below the palm plate (Y < -2)
        cylinder(r=12, h=t);
        translate([0,-14,0]) cube([62, 12, t]);
    }
    hole(6);
    for (s=[-1,1]) rotate(s>0 ? 0 : 180) translate([9.5,0,0]) slot(7, m2_clear);
    for (z=[-11,-5]) translate([57.5, z, 0]) hole(m3_clear);
}
