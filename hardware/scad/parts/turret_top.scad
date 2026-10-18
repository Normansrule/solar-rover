// Turret top plate (print 1). Screws to the base-yaw servo's round metal horn
// (4x M3x8 from above into the horn's threaded holes; MEASURE your horn) and
// rides on the base rim. The skirt overlaps the base to resist tipping.
// Add a little PTFE grease on the rim. Print upside down (top face on the bed,
// skirt pointing up) so nothing needs supports.
include <../config.scad>
use <../lib.scad>

plate_t = 6;
skirt_h = 6;
skirt_id = turret_d + 2*0.4;

module part() {
    difference() {
        union() {
            cylinder(d=turret_d, h=plate_t);
            translate([0,0,-skirt_h]) difference() {
                cylinder(d=skirt_id+2*2.4, h=skirt_h+plate_t);
                translate([0,0,-1]) cylinder(d=skirt_id, h=skirt_h+1);
            }
        }
        // horn screws, counterbored from the top
        for (a=[0:90:270]) rotate(a) translate([mg996_horn_hole_r,0,0]) m3_cbore(plate_t);
        hole(9);
        // module interface inserts (shoulder mount)
        for (p=turret_bolts) translate([p[0],p[1],0]) insert_m3(plate_t);
        // camera mount inserts
        for (y=[-10,10]) translate([36,y,0]) insert_m3(plate_t);
        // arm wire pass-through
        translate([-36,0,0]) hole(8);
    }
}
// printed upside down: plate face on the bed, skirt pointing up
rotate([180,0,0]) translate([0,0,-plate_t]) part();
