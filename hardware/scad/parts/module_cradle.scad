// Module cradle (print 2x tb6612, 1x mp1584): a snap-in holder for the small
// boards that have no screw holes. The board drops in between the two end
// walls, rests on the ledges and clicks under the lips; the other two sides
// stay open for header pins and wires (solder the TB6612 headers pins-up).
// Two M2 x 8 screws go down through the floor and a pair of tray slots (9 mm
// apart, like the slot rows) into M2 nuts under the tray. Print floor down.
//   -D 'variant="tb6612"' | "mp1584"   or  -D 'board=[x,y]' for any other board
include <../config.scad>
use <../lib.scad>

variant = "tb6612";
board = variant == "mp1584" ? cradle_mp1584 : cradle_tb6612;
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
    // mounting holes, 9 mm apart to land in two neighbouring tray slots
    for (sy=[-1,1]) translate([0, sy*4.5, 0]) hole(m2_clear);
}
