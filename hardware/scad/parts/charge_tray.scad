// Charge tray (print 1). A slotted plate on four legs that stands over the
// battery holder. It carries the charging modules: the CN3795 charger, the
// charger relay, both INA219s and the CHG_IN and PACK+ lever connectors, so the
// short, heavy charging wires stay next to the pack. 4x M3x30 go down through
// the hollow legs into inserts in the deck, so with the rear roof tray off, the
// tray lifts out from above to get at the cells. Print plate-down: the legs grow
// upward, no supports.
include <../config.scad>
use <../lib.scad>
t = 3;
L = charge_tray_size[0]; W = charge_tray_size[1];
leg_d = 10;

module part() {
    difference() {
        union() {
            rbox([L, W, t], r=5);
            // legs hang down to the deck (the part is drawn plate-up, legs down)
            for (p=charge_tray_legs) translate([p[0], p[1], -charge_tray_z]) cylinder(d=leg_d, h=charge_tray_z+0.01);
        }
        for (p=charge_tray_legs) translate([p[0], p[1], 0]) hole(m3_clear);
        // module slots: 9 rows, two 24 mm slots each
        for (y=[-36:9:36], sx=[-1,1]) translate([sx*17, y, 0]) slot(24, module_slot_w);
        // cable slot down the middle, between the slot columns
        translate([0, 0, 0]) rbox([6, 70, 50], r=2);
    }
}
// printed upside down: plate on the bed, legs pointing up
rotate([180,0,0]) translate([0,0,-t]) part();
