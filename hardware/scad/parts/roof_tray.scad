// Solar roof tray (print 2): variant="front" (spans X -60..60) and variant="rear"
// (spans X -180..-60). Holds one 6V 2W panel (MEASURE yours; edit `panel` in
// config.scad). Screws down onto its roof posts with M3x8 (counterbored so the
// heads sit under the panel). The panel snaps under the four lips.
// Print floor-down in PETG (it sits in the sun). No supports.
include <../config.scad>
use <../lib.scad>

variant = "front";
L = 120; W = deck_w;
floor_t = 4;
pk = [panel[0]+0.6, panel[1]+0.6, panel[2]+0.4];
H = floor_t + pk[2];
posts = (variant == "front") ? [[40,62],[-40,62],[40,-62],[-40,-62]]
                             : [[5,62],[-20,62],[5,-62],[-20,-62],[58,20],[58,-20]];   // relative to tray centre (-120)

difference() {
    union() {
        rbox([L, W, H], r=5);
        // snap lips (overhang the panel edge by 1 mm)
        for (sy=[-1,1], x=[-35,25]) translate([x, sy*(pk[1]/2) + (sy>0 ? -1 : -1.4), H]) cube([10, 2.4, 1.2]);
    }
    // panel pocket
    translate([0,0,floor_t]) rbox([pk[0], pk[1], pk[2]+0.01], r=1);
    // vent / wiring window (leaves an 8 mm ledge)
    translate([0,0,-1]) rbox([pk[0]-16, pk[1]-16, 20], r=4);
    // post screws, counterbored so the heads sit below the panel
    for (p=posts) translate([p[0],p[1],0]) { hole(m3_clear); translate([0,0,floor_t-3.2]) cylinder(d=m3_head_d, h=10); }
    // panel lead exit
    translate([L/2-10, 0, 0]) hole(6);
}
