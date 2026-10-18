// Power tray (print 1). A slotted module plate on the deck between the roof
// posts. It carries the PCA9685 servo board, the XL4015 and MP1584 bucks, the
// two TB6612 drivers and the MAIN+, GND and 5V lever connectors. The charging
// modules are on the charge tray over the battery. assembly.scad draws a layout
// that tools/fit_check.py has checked.
// Modules fix with M2/M2.5 screws + nuts through the slots, zip ties, or
// foam tape; the PCA9685 gets M2.5 inserts. 4x M3x45 go through the logic tray,
// the standoffs and this tray into the deck inserts. Print flat.
include <../config.scad>
use <../lib.scad>
t = 3;
tray_holes = [[40,50],[-40,50],[40,-50],[-40,-50]];
pca_c = [0, 40];
difference() {
    union() {
        rbox([90, 110, t], r=5);
        for (sx=[-1,1], sy=[-1,1]) translate([pca_c[0]+sx*pca_holes[0]/2, pca_c[1]+sy*pca_holes[1]/2, 0]) cylinder(d=6, h=t+4);
    }
    for (p=tray_holes) translate([p[0],p[1],0]) hole(m3_clear);
    for (sx=[-1,1], sy=[-1,1]) translate([pca_c[0]+sx*pca_holes[0]/2, pca_c[1]+sy*pca_holes[1]/2, 0]) insert_m25(t+4);
    // module slots: 8 rows, two 30 mm slots each
    for (y=[-42:9:21], sx=[-1,1]) translate([sx*21, y, 0]) slot(30, module_slot_w);
    // cable windows between the standoffs
    translate([0, -50, 0]) rbox([26, 10, 50], r=3);
    translate([0, 25.5, 0]) rbox([60, 5, 50], r=2);
}
