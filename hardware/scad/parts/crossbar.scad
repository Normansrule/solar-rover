// Crossbar spacer between the two upper-arm plates (print 1). M3x60 + nyloc nut.
include <../config.scad>
use <../lib.scad>
difference() { cylinder(d=10, h=50); hole(m3_clear); }
