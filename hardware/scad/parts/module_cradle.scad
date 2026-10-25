// Module cradle (print 2x tb6612, 1x mp1584, 1x e220): a snap-in holder for the
// small boards that have no screw holes. The board drops in between the two end
// walls, rests on the ledges and clicks under the lips; the other two sides
// stay open for header pins and wires (solder the TB6612 headers pins-up).
// Two M2 x 8 screws go down through the floor and a pair of tray slots (9 mm
// apart, like the slot rows) into M2 nuts under the tray. Print floor down.
// The e220 variant holds the LoRa module (pins up, shield down between the
// ledges, SMA jack out of the open +Y end). It hangs upside down under the rear
// roof tray on 2x M3x8 into the tray's inserts, screwed in before the module
// clicks in; a zip tie round cradle and module stops it shaking loose.
//   -D 'variant="tb6612"' | "mp1584" | "e220"   or  -D 'board=[x,y]' for any other board
include <../config.scad>
use <../lib.scad>

variant = "tb6612";
board = variant == "mp1584" ? cradle_mp1584 : variant == "e220" ? cradle_e220 : cradle_tb6612;
// e220: two M3 holes across the cradle near the SMA end (they meet the roof
// tray inserts, see lora_boss_* in config.scad); others: M2, 9 mm apart along Y
holes = variant == "e220" ? [[-lora_boss_dy, -lora_boss_dx], [lora_boss_dy, -lora_boss_dx]] : [[0, -4.5], [0, 4.5]];
hole_d = variant == "e220" ? m3_clear : m2_clear;
board_t = 1.6;
floor_t = 2;
wall = 2;
ledge_h = 4;          // board underside above the floor (screw heads, solder tails)
ledge_w = 1.2;
lip = 0.6;
ix = board[0] + 2*tol;               // inner length between the end walls
oy = board[1];                       // cradle width = board width
h = floor_t + ledge_h + board_t + 1.4;

difference() {
    union() {
        translate([-(ix/2+wall), -oy/2, 0]) cube([ix + 2*wall, oy, floor_t]);
        for (sx=[-1,1]) {
            // end wall
            translate([sx > 0 ? ix/2 : -ix/2-wall, -oy/2, 0]) cube([wall, oy, h]);
            // ledge the board rests on
            translate([sx > 0 ? ix/2-ledge_w : -ix/2, -oy/2, 0]) cube([ledge_w, oy, floor_t + ledge_h]);
            // snap lip above the board, chamfered underneath so it prints and clicks
            hull() {
                translate([sx > 0 ? ix/2-lip : -ix/2, -oy/2, floor_t + ledge_h + board_t + 0.1 + lip]) cube([lip, oy, 0.8]);
                translate([sx > 0 ? ix/2-0.01 : -ix/2, -oy/2, floor_t + ledge_h + board_t + 0.1]) cube([0.01, oy, 0.01]);
            }
        }
    }
    // mounting holes
    for (h=holes) translate([h[0], h[1], 0]) hole(hole_d);
}
