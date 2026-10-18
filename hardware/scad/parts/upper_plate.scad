// Upper arm side plate (print 2): one with variant="horn", one with variant="idler".
// 100 mm shoulder-to-elbow. Print flat, 5 walls, 40% infill.
//  horn : screwed to the shoulder servo horn (shoulder end) and to the elbow
//         servo horn (elbow end), 4x M3x8 each, heads on the outside.
//  idler: pivots on M3x10 screws (+ washer) at both ends.
// The two plates are tied together at X=60 by the crossbar spacer (M3x60 + nut).
include <../config.scad>
use <../lib.scad>

variant = "horn";
t = 4;

difference() {
    hull() {
        cylinder(r=14, h=t);
        translate([link_upper,0,0]) cylinder(r=14, h=t);
    }
    for (x=[0, link_upper]) translate([x,0,0]) {
        if (variant == "horn") mg996_horn_holes(m3_clear);
        else hole(m3_clear + 0.2);
    }
    translate([60,0,0]) hole(m3_clear);
    // lightening slots
    translate([30,0,0]) slot(16, 10);
    translate([82,0,0]) slot(4, 8);
}
