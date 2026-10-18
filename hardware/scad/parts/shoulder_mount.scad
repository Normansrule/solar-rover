// Shoulder mount (print 1). Bolts to the turret top with 4x M3x10 into the
// turret-top inserts. Holds the shoulder MG996R with its case pointing down and
// its output shaft along +Y. The upper-arm horn plate is screwed to the servo
// horn; the idler plate pivots on an M3x10 screw in the -Y face insert.
// Print base-down, 4 walls, 40% infill. No supports needed (short bridges).
include <../config.scad>
use <../lib.scad>

horn_y  = 23.2;                         // horn face = inner face of the horn plate
top_y   = horn_y - mg996_spline_h;      // servo case top (17.2)
bot_y   = top_y - mg996_h;              // servo case bottom (-19.8)
floor_t = 6;
zs      = shoulder_z;
wall_t  = 2.4;
cx      = mg996_l/2 - mg996_shaft_off;
flange_y = top_y - (mg996_h - mg996_flange_z);           // underside of the flange
tab_lo  = [zs-(mg996_l-mg996_shaft_off)-(mg996_flange_l-mg996_l)/2, zs-(mg996_l-mg996_shaft_off)];
tab_hi  = [zs+mg996_shaft_off, zs+mg996_shaft_off+(mg996_flange_l-mg996_l)/2];
holes_z = [zs-(cx+mg996_hole_dx/2), zs-(cx-mg996_hole_dx/2)];
xw      = mg996_w/2 + 0.3;

module servo_placed() {
    // lib frame -> mount frame: x' = -y, y' = z, z' = -x
    translate([0, top_y, zs]) multmatrix([[0,-1,0,0],[0,0,1,0],[-1,0,0,0],[0,0,0,1]]) children();
}

difference() {
    union() {
        // base plate
        translate([0,0,0]) rbox([60, 70, 5], r=5);
        // floor (pivot side)
        translate([-xw-wall_t, bot_y-floor_t, 0]) cube([2*(xw+wall_t), floor_t, tab_hi[1]+wall_t]);
        // side walls
        for (sx=[-1,1]) translate([sx>0 ? xw : -xw-wall_t, bot_y-0.01, 0]) cube([wall_t, flange_y-bot_y, tab_hi[1]+wall_t]);
        // lower and upper ledges behind the flange tabs
        translate([-xw, bot_y-0.01, 0]) cube([2*xw, flange_y-bot_y, tab_lo[1]]);
        translate([-xw, bot_y-0.01, tab_hi[0]]) cube([2*xw, flange_y-bot_y, tab_hi[1]-tab_hi[0]+wall_t]);
    }
    // servo body clearance
    servo_placed() mg996_case_neg(0.3);
    // flange screw inserts (from the flange face toward -Y)
    for (z=holes_z, x=[-mg996_hole_dy/2, mg996_hole_dy/2])
        translate([x, flange_y+0.01, z]) rotate([90,0,0]) cylinder(d=m3_insert_d, h=m3_insert_h);
    // pivot insert on the outer face of the floor
    translate([0, bot_y-floor_t-0.01, zs]) rotate([-90,0,0]) cylinder(d=m3_insert_d, h=m3_insert_h);
    // turret-top screws (counterbored heads on the base plate)
    for (p=turret_bolts) translate([p[0],p[1],0]) { hole(m3_clear); translate([0,0,5-1.5]) cylinder(d=m3_head_d, h=10); }
    // servo cable notch through the lower ledge
    translate([-5, bot_y-0.02, 5]) cube([10, 12, tab_lo[1]]);
}
