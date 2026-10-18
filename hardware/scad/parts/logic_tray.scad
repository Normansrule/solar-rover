// Logic tray (print 1). Carries the Raspberry Pi Zero 2 W, the ESP32 DevKit
// plugged into a 38-pin screw-terminal expansion board, and (on the slotted
// strip at +X) the MPU-6050 and the passive I2C hub. Sits on 4 tray standoffs
// above the power tray. No soldered boards: every module screws or zip-ties on.
// Inserts: M2.5 for the Pi, M3 for the expansion board. Print flat.
include <../config.scad>
use <../lib.scad>
t = 3;
tray_holes = [[40,50],[-40,50],[40,-50],[-40,-50]];
pi_c  = [0, -40];
exp_c = [0, 16.5];      // expansion board turned 90 deg: 54 mm across X, 77 mm along Y
strip_x = 36.5;         // centre of the slotted strip for the small modules
difference() {
    union() {
        rbox([90, 110, t], r=5);
        for (sx=[-1,1], sy=[-1,1]) {
            translate([pi_c[0]+sx*pi0_holes[0]/2, pi_c[1]+sy*pi0_holes[1]/2, 0]) cylinder(d=6, h=t+4);
            translate([exp_c[0]+sx*esp_exp_holes[0]/2, exp_c[1]+sy*esp_exp_holes[1]/2, 0]) cylinder(d=8, h=t+3);
        }
    }
    for (p=tray_holes) translate([p[0],p[1],0]) hole(m3_clear);
    for (sx=[-1,1], sy=[-1,1]) {
        translate([pi_c[0]+sx*pi0_holes[0]/2, pi_c[1]+sy*pi0_holes[1]/2, 0]) insert_m25(t+4);
        translate([exp_c[0]+sx*esp_exp_holes[0]/2, exp_c[1]+sy*esp_exp_holes[1]/2, 0]) insert_m3(t+3);
    }
    // slotted strip for MPU-6050 + I2C hub (M2 screw + nut, or zip ties)
    for (y=[-16:8:40]) translate([strip_x, y, 0]) slot(8, module_slot_w);
    // cable windows: under the expansion board and along the -X edge
    translate([0, 16.5, 0]) rbox([30, 50, 50], r=4);
    translate([-38, 10, 0]) rbox([10, 60, 50], r=3);
}
