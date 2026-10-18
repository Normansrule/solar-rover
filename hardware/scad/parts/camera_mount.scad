// Camera mount (print 1) for the Pi camera (OV5647 "Zero" board, 25x24 mm,
// 4x M2 on 21 x 12.5 mm). Bolts to the two inserts at the front of the turret top
// (2x M3x8). Looks forward, tilted 15 degrees down. Print base-down; the 15 degree lean needs no supports.
include <../config.scad>
use <../lib.scad>
tilt = 15;
difference() {
    union() {
        translate([30,-12,0]) cube([10, 24, 4]);
        translate([38,0,2]) rotate([0,tilt,0]) translate([0,-14,0]) cube([3, 28, 40]);
    }
    for (y=[-10,10]) translate([36,y,0]) hole(m3_clear);
    translate([38,0,2]) rotate([0,tilt,0]) for (y=[-10.5,10.5], zz=[22-6.25, 22+6.25])
        translate([0,y,zz]) rotate([0,90,0]) hole(m2_clear, 20);
    translate([-50,-50,-20]) cube([200,100,20]);
}
