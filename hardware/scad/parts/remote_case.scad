// Handheld remote case (print 2): variant="top" and variant="bottom".
// Holds an ESP32 DevKit, 2x KY-023 analog joysticks, 2x 12 mm tact buttons,
// a single 18650 holder, a TP4056 USB-C charger board, an MT3608 boost and a
// slide switch. Halves join with 4x M3x16 into inserts in the bottom shell.
// Print both shells face-down (outer face on the bed). MEASURE the joystick
// hole spacing (26.5 x 20.5 mm here).
include <../config.scad>
use <../lib.scad>

variant = "top";
L = 160; W = 75; H_top = 14; H_bot = 24; t = 2.4;
stick_c = [[-50, 0], [50, 0]];
btn_c   = [[-18, 18], [18, 18]];
screws  = [[-L/2+7, W/2-7], [L/2-7, W/2-7], [-L/2+7, -W/2+7], [L/2-7, -W/2+7]];

module shell(h) {
    difference() {
        rbox([L, W, h], r=10);
        translate([0,0,t]) rbox([L-2*t, W-2*t, h], r=8);
    }
}

if (variant == "top") {
    difference() {
        union() {
            shell(H_top);
            for (p=screws) translate([p[0],p[1],0]) cylinder(d=8, h=H_top);
            // joystick standoffs (PCB sits 10 mm under the lid)
            for (c=stick_c, sx=[-1,1], sy=[-1,1]) translate([c[0]+sx*26.5/2, c[1]+sy*20.5/2, 0]) cylinder(d=6, h=10);
        }
        for (p=screws) translate([p[0],p[1],0]) { hole(m3_clear); translate([0,0,-1]) cylinder(d=m3_head_d, h=4.2); }
        for (c=stick_c) translate([c[0],c[1],0]) hole(28);
        for (c=stick_c, sx=[-1,1], sy=[-1,1]) translate([c[0]+sx*26.5/2, c[1]+sy*20.5/2, 0]) translate([0,0,10-5]) cylinder(d=m2_pilot+0.6, h=6);
        for (c=btn_c) translate([c[0],c[1],0]) hole(12.5);
        // status LED window
        translate([0,-22,0]) hole(5);
    }
} else {
    difference() {
        union() {
            shell(H_bot);
            for (p=screws) translate([p[0],p[1],0]) cylinder(d=8, h=H_bot);
            // 18650 holder cradle
            for (sy=[-1,1]) translate([-40, sy*12-1, 0]) cube([80, 2, 8]);
            // ESP32 DevKit rails
            for (sy=[-1,1]) translate([-28, -26+sy*14.5-1, 0]) cube([56, 2, 6]);
        }
        for (p=screws) translate([p[0],p[1],0]) insert_m3(H_bot);
        // USB-C charge port (TP4056) and ESP32 USB for flashing
        translate([L/2-1, 20, t+4]) cube([6, 10, 5], center=true);
        translate([-L/2+1, -26, t+5]) cube([6, 12, 7], center=true);
        // slide switch
        translate([L/2-1, -10, t+6]) cube([6, 9, 4.5], center=true);
    }
}
