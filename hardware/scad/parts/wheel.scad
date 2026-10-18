// Wheel hub (print 4). Outer face down on the bed. PETG, 5 walls, 40% infill.
// Pushes onto the TT D-shaft from the dished (inner) side; retained by an
// M2x10 thread-forming screw + washer into the shaft's centre hole.
// The dish leaves room for the motor through-bolt nuts.
include <../config.scad>
use <../lib.scad>

tire_t = 5;
rim_od = wheel_d - 2*tire_t;   // 55
lip    = 2;

difference() {
    union() {
        // web (outer face)
        cylinder(d=rim_od-2, h=4);
        // rim tube
        difference() { cylinder(d=rim_od, h=wheel_w); translate([0,0,-1]) cylinder(d=rim_od-6, h=wheel_w+2); }
        // tire retaining lips
        for (z=[0, wheel_w-lip]) translate([0,0,z]) difference() {
            cylinder(d=rim_od+3, h=lip); translate([0,0,-1]) cylinder(d=rim_od-6, h=lip+2);
        }
        // hub boss
        cylinder(d=14, h=wheel_w);
        // spokes
        for (a=[0:60:300]) rotate(a) translate([0,-2,0]) cube([rim_od/2-2, 4, 12]);
    }
    // D-bore from the inner face, 8 mm deep
    translate([0,0,wheel_w-8]) d_bore(9, 0.15);
    // retaining screw + counterbore on the outer face
    hole(m2_clear);
    translate([0,0,-1]) cylinder(d=5, h=3);
    // lightening holes in the web
    for (a=[30:60:330]) rotate(a) translate([16,0,0]) hole(9);
    // tire grip grooves on the rim
    for (a=[0:15:345]) rotate(a) translate([rim_od/2, 0, lip]) cylinder(d=1.2, h=wheel_w-2*lip, $fn=8);
}
