// Fit-test coupon (print 1, before anything else). About 35 min and 20 g.
// One flat plate that checks every MEASURE fit that matters, with a few sizes
// of each so you can read off the right value instead of reprinting parts:
//   - TT motor D-shaft bores at 4 clearances (row 1, left)
//   - TT motor hole pattern: hold a motor against it (row 1, right)
//   - M3 and M2.5 heat-set insert holes, M2 screw pilot holes (row 2)
//   - MG996R and MG90S case windows, M3 clearance holes, M3 nut pocket (row 3)
// The engraved numbers are the value to put in config.scad for that size.
// See docs/assembly.md, step 0. Print flat, 0.2 mm layers, 4 walls, PETG.
include <../config.scad>
use <../lib.scad>

plate = [132, 84, 2.4];
boss_h = 8;
label_size = 3;

module label(txt, at) {
    translate([at[0], at[1], plate[2] - 0.6])
        linear_extrude(1) text(txt, size=label_size, halign="center", valign="center",
                               font="Liberation Sans:style=Bold");
}

// boss standing on the plate, hole cut by children() from its top
module boss(at, d) {
    translate([at[0], at[1], 0]) cylinder(d=d, h=boss_h);
}

// --- row 1: TT shaft bores (extra = clearance on the D) and the motor holes
shaft_extra = [-0.1, 0, 0.1, 0.2];
shaft_x = [12, 30, 48, 66];
row1 = 66;
tt_shaft = [104, row1];

// --- row 2: insert and pilot holes
m3_ins = [3.9, 4.1, 4.3];
m25_ins = [3.3, 3.5, 3.7];
m2_pil = [1.5, 1.7, 1.9];
row2 = 40;

// --- row 3: servo windows, clearance holes, nut pocket
row3 = 14;
frame = 3;

difference() {
    union() {
        translate([plate[0]/2, plate[1]/2, 0]) rbox(plate, r=4);
        for (i=[0:3]) boss([shaft_x[i], row1], 12);
        for (i=[0:2]) boss([10 + i*13, row2], 9);
        for (i=[0:2]) boss([52 + i*12, row2], 8);
        for (i=[0:2]) boss([94 + i*11, row2], 7);
        // servo window frames
        translate([4, row3 - (mg996_w + 2*tol)/2 - frame, 0])
            cube([mg996_l + 2*tol + 2*frame, mg996_w + 2*tol + 2*frame, boss_h]);
        translate([58, row3 - (mg90_w + 2*tol)/2 - frame, 0])
            cube([mg90_l + 2*tol + 2*frame, mg90_w + 2*tol + 2*frame, boss_h]);
        translate([118, row3, 0]) cylinder(d=11, h=boss_h);
    }
    // D-shaft bores, from the top 7 mm deep
    for (i=[0:3]) translate([shaft_x[i], row1, 1]) rotate(90) d_bore(h=20, extra=2*tol + shaft_extra[i]);
    // TT motor pattern: shaft clearance + the two through holes (motor face on the plate)
    translate(tt_shaft) hole(tt_shaft_d + 1.5);
    for (h = tt_holes) translate([tt_shaft[0] + h[0], tt_shaft[1] + h[1]]) hole(m3_clear);
    // inserts and pilots, 1 mm floor left under each
    for (i=[0:2]) translate([10 + i*13, row2, 1]) cylinder(d=m3_ins[i], h=20);
    for (i=[0:2]) translate([52 + i*12, row2, 1]) cylinder(d=m25_ins[i], h=20);
    for (i=[0:2]) translate([94 + i*11, row2, 1]) cylinder(d=m2_pil[i], h=20);
    // servo windows (through), at the parts' own tolerance
    translate([4 + frame, row3 - (mg996_w + 2*tol)/2, -1]) cube([mg996_l + 2*tol, mg996_w + 2*tol, 20]);
    translate([58 + frame, row3 - (mg90_w + 2*tol)/2, -1]) cube([mg90_l + 2*tol, mg90_w + 2*tol, 20]);
    // M3 clearance holes and a nut pocket from the top
    for (i=[0:2]) translate([90 + i*8.5, row3 + 4]) hole([3.2, 3.4, 3.6][i]);
    translate([118, row3, 0]) { hole(m3_clear); m3_nut_pocket(boss_h); }
    // labels
    for (i=[0:3]) label(str(shaft_extra[i] >= 0 ? "+" : "", shaft_extra[i]), [shaft_x[i], row1 - 10]);
    label("TT", [tt_shaft[0] - 10, row1 + 11]);
    for (i=[0:2]) label(str(m3_ins[i]), [10 + i*13, row2 - 8.5]);
    for (i=[0:2]) label(str(m25_ins[i]), [52 + i*12, row2 - 8]);
    for (i=[0:2]) label(str(m2_pil[i]), [94 + i*11, row2 - 7.5]);
    label("M3", [23, row2 + 8.5]); label("M2.5", [64, row2 + 8]); label("M2", [105, row2 + 7.5]);
    for (i=[0:2]) label(str([3.2, 3.4, 3.6][i]), [90 + i*8.5, row3 - 4.5]);
}
