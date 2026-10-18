// Deck half. Export with -D 'half="front"' or -D 'half="rear"'.
// Print: top face down (flat on the bed), 0.2 mm layers, 4 walls, 25% gyroid, PETG.
// The two halves bolt together through the joint flanges with 3x M3x16 + nuts.
include <../config.scad>
use <../lib.scad>

half = "front";

z0 = deck_bottom_z;
z1 = deck_top_z;

// Features in whole-deck coordinates [x, y]
motor_bosses  = [for (sx=[-1,1], sy=[-1,1], x=[63,71]) [sx*x, sy*41]];   // clamp screws, inserts from BELOW
top_inserts   = concat(
    [for (a=[45:90:315]) [turret_x+turret_deck_r*cos(a), turret_deck_r*sin(a)]],  // turret base
    [[140,40],[140,-40]],                                                // nose wedge
    [[40,50],[40,-50],[-40,50],[-40,-50]]);                              // electronics tray
// charge-tray legs: inserts flush through the deck plate, no boss underneath
// (the bosses would hit the rear motors and clamps)
charge_legs   = [for (p=charge_tray_legs) charge_tray_c + p];
roof_posts    = concat([for (x=[40,-40,-115,-140], sy=[-1,1]) [x, sy*62]], [[-62,20],[-62,-20]]);   // M3x10 from below into roof_post inserts
battery_slots = [for (x=[-126,-86], sy=[-1,1]) [x, sy*20]];   // 4-slot holder centred at x=-106
wire_holes    = [[60,0,14],[-60,0,14],[20,30,10],[20,-30,10],[-20,30,10],[-20,-30,10],[turret_x,-18,12]];

module in_half() {
    if (half == "front") translate([0,-500,-500]) cube([1000,1000,1000]);
    else translate([-1000,-500,-500]) cube([1000,1000,1000]);
}

module ribs() {
    // longitudinal
    for (y=[-12,12]) translate([-deck_l/2, y-rib_w/2, z0-rib_h]) cube([deck_l, rib_w, rib_h]);
    // perimeter (side) ribs, skipping the motor zones
    for (sy=[-1,1]) for (seg=[[-150,-118],[-42,42],[118,150]])
        translate([seg[0], sy*(deck_w/2-rib_w/2)-rib_w/2, z0-rib_h]) cube([seg[1]-seg[0], rib_w, rib_h]);
    // transverse
    for (x=[-135,-20,20,135]) translate([x-rib_w/2, -deck_w/2, z0-rib_h]) cube([rib_w, deck_w, rib_h]);
}

module joint_flange() {
    // flange on this half's side of x=0
    sx = (half == "front") ? 0 : -flange_t;
    difference() {
        translate([sx, -deck_w/2+6, z0-flange_h]) cube([flange_t, deck_w-12, flange_h]);
        for (y=[-45,0,45]) translate([0,y,z0-flange_h/2]) rotate([0,90,0]) hole(m3_clear);
        if (half == "front") for (y=[-45,0,45])
            translate([flange_t,y,z0-flange_h/2]) rotate([0,90,0]) m3_nut_pocket(0, m3_nut_h+0.5);
    }
}

module deck_body() {
    union() {
        translate([0,0,z0]) rbox([deck_l, deck_w, deck_t], r=8);
        ribs();
        joint_flange();
        for (p=motor_bosses) translate([p[0],p[1],z0-6]) cylinder(d=9, h=6.01);
        for (p=top_inserts)  translate([p[0],p[1],z0-6]) cylinder(d=9, h=6.01);
    }
}

difference() {
    union() {
        intersection() { deck_body(); in_half(); }
        // alignment pins on the front half poke into sockets in the rear half
        if (half == "front") for (y=[-60,60])
            translate([-5,y,z0-flange_h/2]) rotate([0,90,0]) cylinder(d=4, h=5.5);
    }
    for (p=motor_bosses) translate([p[0],p[1],z0-6-0.01]) cylinder(d=m3_insert_d, h=m3_insert_h);
    for (p=top_inserts)  translate([p[0],p[1],0]) insert_m3(z1);
    for (p=charge_legs)  translate([p[0],p[1],z0-0.01]) cylinder(d=m3_insert_d, h=deck_t+0.02);
    for (p=roof_posts)   translate([p[0],p[1],0]) hole(m3_clear);
    for (p=battery_slots) translate([p[0],p[1],0]) rotate(90) slot(16, m3_clear);
    for (w=wire_holes) translate([w[0],w[1],0]) hole(w[2]);
    if (half == "rear") for (y=[-60,60])
        translate([-6,y,z0-flange_h/2]) rotate([0,90,0]) cylinder(d=4.4, h=6.5);
}
