// TT motor clamp + encoder sensor shelf. Print 4: two with side="a"
// (front-left, rear-right) and two with side="b" (front-right, rear-left).
// Modelled in place for the front-left wheel, then laid flat for printing.
//
// Assembly: motor bolts to the clamp with 2x M3x45 through the motor's own
// holes (head on the clamp side, nyloc nut on the wheel side; the nut sits in
// the wheel's dish). The flange bolts to the deck with 2x M3x8 into inserts.
// The LM393 slot sensor screws to the shelf (1x M3x8 + nut) with its fork
// straddling the encoder disc on the motor's inner shaft.
include <../config.scad>
use <../lib.scad>

side  = "a";
print = true;

motor_in_y = motor_outer_y - tt_body_t;   // 49.5
spacer     = 14;
plate_y0   = motor_in_y - spacer - clamp_t;  // 32.5
plate_y1   = plate_y0 + clamp_t;             // 35.5
flange_z1  = deck_bottom_z - 6;              // meets the deck bosses (38.6)
flange_z0  = flange_z1 - 4;
holes_abs  = [for (h=tt_holes) [axle_x + h[0], axle_z + h[1]]];

module clamp() {
    difference() {
        union() {
            // vertical plate (ends 0.8 mm short of the deck's end rib at x = 135)
            translate([axle_x-45, plate_y0, 11]) cube([78, clamp_t, 38.0-11]);
            // deck flange (on the motor side of the plate, clear of the encoder disc)
            translate([58, plate_y1-0.01, flange_z0]) cube([28, 46-plate_y1, flange_z1-flange_z0]);
            // spacer bosses to the motor face
            for (h=holes_abs) translate([h[0], plate_y1-0.01, h[1]]) rotate([-90,0,0]) cylinder(d=8, h=spacer+0.01);
            // sensor shelf under the encoder disc
            translate([axle_x-8, plate_y0, 11]) cube([36, motor_in_y-plate_y0-1, 3]);
        }
        // motor through-bolts
        for (h=holes_abs) translate([h[0], 0, h[1]]) rotate([-90,0,0]) hole(m3_clear, 400);
        // deck screws (heads under the flange)
        for (x=[63,71]) translate([x, 41, 0]) { hole(m3_clear, 200); translate([0,0,flange_z0-20]) cylinder(d=m3_head_d, h=20); }
        // shaft / encoder disc clearance through the plate
        translate([axle_x, 0, axle_z]) rotate([-90,0,0]) hole(enc_disc_d+4, 400);
        // sensor mounting slot (module hole ~20 mm from the fork centre)
        translate([axle_x+20, plate_y0+7, 0]) slot(6, m3_clear, 100);
        // wire exit
        translate([axle_x-30, 0, 26]) rotate([-90,0,0]) hole(6, 400);
    }
}

module placed() { if (side == "a") clamp(); else mirror([0,1,0]) clamp(); }

if (print) {
    // lay the plate flat: plate face (y = plate_y0) onto the bed
    // plate face down on the bed, everything else grows upward: no supports
    if (side == "a") rotate([90,0,0]) translate([-axle_x, -plate_y0, -axle_z]) clamp();
    else mirror([1,0,0]) rotate([90,0,0]) translate([-axle_x, -plate_y0, -axle_z]) clamp();
} else placed();
