// Tray standoff (print 4): the logic tray sits tray_standoff_h (32 mm) above the
// power tray, which leaves room for the jumpers plugged into the power modules.
include <../config.scad>
use <../lib.scad>
difference() { cylinder(d=8, h=tray_standoff_h); hole(m3_clear); }
