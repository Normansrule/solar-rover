// Solar Rover - shared helper modules
include <config.scad>

// Rounded rectangle plate, centred in XY, sitting on Z=0.
module rbox(size, r=4) {
    x = size[0]; y = size[1]; z = size[2];
    hull() for (sx=[-1,1], sy=[-1,1])
        translate([sx*(x/2-r), sy*(y/2-r), 0]) cylinder(r=r, h=z);
}

// Vertical hole through everything (subtract). Centred on the origin.
module hole(d, h=200) { translate([0,0,-h/2]) cylinder(d=d, h=h); }

// Heat-set insert pocket opening upward at Z=top.
module insert_m3(top) { translate([0,0,top-m3_insert_h]) cylinder(d=m3_insert_d, h=m3_insert_h+0.01); }
module insert_m25(top) { translate([0,0,top-4.5]) cylinder(d=m25_insert_d, h=4.51); }

// Boss for an insert: cylinder from z0 to z1, with insert pocket at the top.
module insert_boss_m3(z0, z1, od=8) {
    difference() {
        translate([0,0,z0]) cylinder(d=od, h=z1-z0);
        insert_m3(z1);
    }
}

// Slot of length l (centre-to-centre) along X, diameter d.
module slot(l, d, h=200) {
    hull() for (sx=[-1,1]) translate([sx*l/2,0,-h/2]) cylinder(d=d, h=h);
}

// Counterbored M3 clearance hole, head at Z=top going down.
module m3_cbore(top, depth=m3_head_h) {
    hole(m3_clear);
    translate([0,0,top-depth]) cylinder(d=m3_head_d, h=depth+50);
}

// Captive hex nut pocket, opening at Z=z (nut goes in from that face).
module m3_nut_pocket(z, depth=m3_nut_h+0.4) {
    translate([0,0,z-depth]) cylinder(d=m3_nut_af/cos(30), h=depth+0.01, $fn=6);
}

// Pair of zip-tie slots (for modules without mounting holes), spaced s apart along X.
module zip_slots(s=30, w=4, l=2.2) {
    for (sx=[-1,1]) translate([sx*s/2,0,0]) cube([l, w, 50], center=true);
}

// D-shaped bore for TT motor shafts.
module d_bore(h=50, extra=0) {
    intersection() {
        cylinder(d=tt_shaft_d+extra, h=h);
        translate([-(tt_shaft_d+extra)/2, -(tt_shaft_flat+extra)/2, 0])
            cube([tt_shaft_d+extra, tt_shaft_flat+extra, h]);
    }
}

// Simple spur gear (trapezoid teeth), module m, z teeth, thickness t. Good enough
// for a slow servo gripper; not a precision involute.
module spur_gear(m=1.25, z=16, t=6, bore=0) {
    rp = m*z/2; ra = rp + m; rf = rp - 1.25*m;
    difference() {
        linear_extrude(t) union() {
            circle(r=rf, $fn=z*4);
            for (i=[0:z-1]) rotate(i*360/z)
                polygon([[rf*cos(-90/z*1.1), rf*sin(-90/z*1.1)],
                         [ra*cos(-90/z*0.45), ra*sin(-90/z*0.45)],
                         [ra*cos( 90/z*0.45), ra*sin( 90/z*0.45)],
                         [rf*cos( 90/z*1.1), rf*sin( 90/z*1.1)]]);
        }
        if (bore > 0) hole(bore);
    }
}

// Servo pocket for an MG996R lying with its output axis along +Z.
// Case centred so the OUTPUT SHAFT is at the origin; case top at Z=0.
module mg996_case_neg(extra=tol) {
    translate([-mg996_shaft_off-extra, -mg996_w/2-extra, -mg996_h-extra])
        cube([mg996_l+2*extra, mg996_w+2*extra, mg996_h+extra+0.01]);
}
module mg996_flange_holes(d=m3_clear, h=60) {
    cx = mg996_l/2 - mg996_shaft_off;
    for (sx=[-1,1], sy=[-1,1])
        translate([cx+sx*mg996_hole_dx/2, sy*mg996_hole_dy/2, -h/2]) cylinder(d=d, h=h);
}
module mg90_case_neg(extra=tol) {
    translate([-mg90_shaft_off-extra, -mg90_w/2-extra, -mg90_h-extra])
        cube([mg90_l+2*extra, mg90_w+2*extra, mg90_h+extra+0.01]);
}
module mg90_flange_holes(d=m2_pilot, h=60) {
    cx = mg90_l/2 - mg90_shaft_off;
    for (sx=[-1,1]) translate([cx+sx*mg90_hole_dx/2, 0, -h/2]) cylinder(d=d, h=h);
}
// Round MG996R horn screw pattern (4 holes) + centre relief.
module mg996_horn_holes(d=m3_clear) {
    hole(9);
    for (a=[0:90:270]) rotate(a) translate([mg996_horn_hole_r,0,0]) hole(d);
}
