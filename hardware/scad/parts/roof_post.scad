// Roof post (print 10). Stands on the deck; M3x10 from under the deck into the
// bottom insert, M3x8 from the roof tray into the top insert.
// Print upright, 100% infill or 6 walls.
include <../config.scad>
use <../lib.scad>

difference() {
    cylinder(d=10, h=roof_post_h);
    insert_m3(roof_post_h);
    translate([0,0,m3_insert_h]) rotate([180,0,0]) insert_m3(m3_insert_h);
}
