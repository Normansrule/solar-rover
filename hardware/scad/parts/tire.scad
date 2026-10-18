// Tire (print 4) in TPU 95A. 0.2 mm layers, 3 walls, 15% infill, slow (25 mm/s).
// Stretches over the wheel rim between the two lips.
include <../config.scad>
use <../lib.scad>

tire_t = 5;
id     = wheel_d - 2*tire_t - 0.6;   // slightly undersize so it grips the rim
w      = wheel_w - 4 - 0.4;

difference() {
    cylinder(d=wheel_d, h=w, $fn=120);
    translate([0,0,-1]) cylinder(d=id, h=w+2, $fn=120);
    // chevron-ish tread: alternating grooves
    for (a=[0:10:350]) rotate(a) translate([wheel_d/2, 0, (a/10)%2==0 ? 2 : w/2+1])
        cube([2.4, 2.2, w/2-3], center=false);
}
