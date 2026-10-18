// Nose wedge / front bumper (print 1). Two lugs bolt to the deck front inserts
// (2x M3x8). The VL53L0X distance sensor sits behind the window in the front
// face (2x M2x6 into the pilot holes; MEASURE your module's hole spacing).
// Print standing on its back face (X=150 face down); enable supports touching the
// build plate for the two lugs.
include <../config.scad>
use <../lib.scad>

x0 = deck_l/2; x1 = x0 + 32;
zb = 30; zt = deck_top_z + 10;
w  = deck_w;
t  = 2.4;

module profile(o=0) {
    polygon([[x0+o, zb+o], [x1-o, zb+o], [x1-o, zb+8-o*0.3], [x0+o, zt-o]]);
}
difference() {
    union() {
        // shell: extrude the profile, then hollow it
        difference() {
            rotate([90,0,0]) translate([0,0,-w/2]) linear_extrude(w) profile(0);
            rotate([90,0,0]) translate([0,0,-w/2+t]) linear_extrude(w-2*t) profile(t);
        }
        // deck lugs
        for (sy=[-1,1]) translate([x0-16, sy*40-6, deck_top_z]) cube([16.01, 12, 4]);
        // sensor bosses
        for (y=[-10,10]) translate([x1-t-4, y, zb+5]) rotate([0,90,0]) cylinder(d=5, h=4.01);
    }
    for (sy=[-1,1]) translate([140, sy*40, 0]) hole(m3_clear);
    // sensor window
    translate([x1-5, 0, zb+5]) rotate([0,90,0]) cylinder(d=9, h=10);
    for (y=[-10,10]) translate([x1-t-5, y, zb+5]) rotate([0,90,0]) cylinder(d=m2_pilot, h=10);
    // open back for wiring
    translate([x0-1, -w/2+t+4, zb+t+2]) cube([t+2, w-2*t-8, zt-zb-2*t-4]);
}
