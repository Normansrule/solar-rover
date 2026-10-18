// Encoder disc (print 4). Black PLA/PETG works best for the optical sensor.
// Presses onto the TT motor's INNER shaft; 20 slots = 20 counts per wheel turn
// on one edge (40 if the firmware counts both edges, which it does).
include <../config.scad>
use <../lib.scad>

difference() {
    union() {
        cylinder(d=enc_disc_d, h=enc_disc_t);
        cylinder(d=9, h=6);
    }
    translate([0,0,-1]) d_bore(10, 0.05);
    for (i=[0:enc_slots-1]) rotate(i*360/enc_slots)
        translate([enc_disc_d/2-1.5-3.0, -0.75, -1]) cube([3.0, 1.5, enc_disc_t+2]);
}
