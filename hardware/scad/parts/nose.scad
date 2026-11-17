// Nose wedge / front bumper (print 1). Two lugs bolt to the deck front inserts
// (2x M3x8). The VL53L0X distance sensor sits behind the window in the front
// face (2x M2x6 into the pilot holes; MEASURE your module's hole spacing).
// Two corner pods carry the side sensors, each aimed corner_aim degrees out from
// straight ahead: the module screws onto the pod face, chip outward (2x M2x6),
// and its wires go through the slot into the nose. Turn the pods off with
// corner_pods=false if you only fit the front sensor.
// Print standing on its back face (X=150 face down); enable supports touching the
// build plate for the two lugs.
include <../config.scad>
use <../lib.scad>

x0 = deck_l/2; x1 = x0 + 32;
zb = 30; zt = deck_top_z + 10;
w  = deck_w;
t  = 2.4;
corner_pods = true;
corner_aim  = 70;              // degrees out from straight ahead (the simulator's side_tof_deg)
pod_zc = zb + 8;               // sensor centre height
pod_h  = 15;                   // pod height (the module is 10.7 x 25)
pod_half = 13;                 // half the face width along the module's long side

// a pod's footprint, top view, for the left side (mirrored for the right)
function pod_c() = [x1 - 13, w/2 + 2.5];
function pod_n() = [cos(corner_aim), sin(corner_aim)];       // face normal
function pod_t() = [sin(corner_aim), -cos(corner_aim)];      // along the face, toward the nose tip
module pod_2d() {
    c = pod_c(); tt = pod_t();
    a = c + pod_half * tt; b = c - pod_half * tt;
    // back from the face to the side wall; the rear edge is 45 degrees so it prints without support
    polygon([a, b, [b[0] - (b[1] - (w/2 - t)), w/2 - t], [a[0], w/2 - t]]);
}
module pod_cuts() {
    c = pod_c(); n = pod_n(); tt = pod_t();
    // sensor window and two M2 pilots along the face (holes 20 mm apart like the front one)
    translate([c[0], c[1], pod_zc]) rotate([0, 0, atan2(n[1], n[0])]) {
        translate([-4, 0, 0]) rotate([0, 90, 0]) cylinder(d = 9, h = 10);
        for (s = [-10, 10]) translate([-8, s, 0]) rotate([0, 90, 0]) cylinder(d = m2_pilot, h = 10);
        // slot for the header pins / wires, under the window, through into the nose
        translate([-30, -9, -pod_h/2 + 1.2]) cube([31, 18, 3.2]);
    }
}
module pods() {
    for (m = [0, 1]) mirror([0, m, 0])
        translate([0, 0, pod_zc - pod_h/2]) linear_extrude(pod_h) pod_2d();
}
module pods_cut() { for (m = [0, 1]) mirror([0, m, 0]) pod_cuts(); }

module profile(o=0) {
    polygon([[x0+o, zb+o], [x1-o, zb+o], [x1-o, zb+8-o*0.3], [x0+o, zt-o]]);
}
difference() {
    union() {
        // shell: extrude the profile, then hollow it
        difference() {
            rotate([90,0,0]) translate([0,0,-w/2]) linear_extrude(w) profile(0);
            rotate([90,0,0]) translate([0,0,-w/2+t]) linear_extrude(w-2*t) profile(t);
        }
        // deck lugs
        for (sy=[-1,1]) translate([x0-16, sy*40-6, deck_top_z]) cube([16.01, 12, 4]);
        // sensor bosses
        for (y=[-10,10]) translate([x1-t-4, y, zb+5]) rotate([0,90,0]) cylinder(d=5, h=4.01);
        if (corner_pods) intersection() {
            pods();
            translate([x0, -w, zb]) cube([x1 - x0, 2 * w, zt - zb]);   // stay inside the nose's height and length
        }
    }
    if (corner_pods) pods_cut();
    for (sy=[-1,1]) translate([140, sy*40, 0]) hole(m3_clear);
    // sensor window
    translate([x1-5, 0, zb+5]) rotate([0,90,0]) cylinder(d=9, h=10);
    for (y=[-10,10]) translate([x1-t-5, y, zb+5]) rotate([0,90,0]) cylinder(d=m2_pilot, h=10);
    // open back for wiring, between the two deck lugs (wider would cut the wall
    // the lugs hang from and they would print loose)
    translate([x0-1, -30, zb+t+2]) cube([t+2, 60, zt-zb-2*t-4]);
}
