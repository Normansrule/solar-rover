// Whole-car assembly preview built from the exported STLs (run
// tools/export_stl.sh first). Open in OpenSCAD and press F5, or render a
// picture with tools/render_preview.sh. Bought parts are drawn as plain boxes.
//
// -D pose="park|ready|pick|carry" sets the arm to a firmware pose
// (firmware/car/src/arm.cpp), and -D only="name" draws one placed part on its
// own, which is how tools/fit_check.py exports each part for the collision check.
// -D step=N draws only the parts fitted by build step N of docs/assembly.md,
// which is how tools/render_anim.sh draws the build animation.
include <config.scad>
use <lib.scad>
use <parts/shoulder_mount.scad>   // servo_placed()
use <parts/forearm.scad>          // elbow_servo(), wrist_servo()

stl  = "../stl/";
only = "";        // "" = everything, otherwise one part name from below
pose = "";        // "" = the hand-set angles below
yaw_extra = 0;    // added turret yaw, degrees
angles = [];      // [yaw, shoulder, elbow, wrist] in firmware angles: overrides pose
step = 99;        // build step from docs/assembly.md: draw parts fitted up to here

// firmware poses: [yaw, shoulder, elbow, wrist] in the firmware's own angles,
// copied from firmware/car/src/arm.cpp (tools/fit_check.py reads them from there)
poses = [["park",  [0,110,120,-70]], ["ready", [0,45,60,-10]],
         ["pick",  [0,10,57,-43]],   ["carry", [0,70,120,-40]]];
pi_ = search([pose], poses);
pa  = len(angles) == 4 ? angles : (pose == "" || pi_[0] == []) ? undef : poses[pi_[0]][1];
// the model turns the shoulder and wrist the other way round from the firmware
shoulder = pa == undef ? -35 : -pa[1];   // degrees, negative = up
elbow    = pa == undef ?  70 :  pa[2];   // relative to the upper arm
wrist    = pa == undef ?   0 : -pa[3];
yaw      = (pa == undef ? 0 : pa[0]) + yaw_extra;

deck_c = [0.55,0.6,0.68];  shell_c = [0.42,0.5,0.62];  arm_c = [0.95,0.75,0.1];
blk = [0.15,0.15,0.15];

module stlp(n) import(str(stl, n, ".stl"), convexity=6);
// build step (docs/assembly.md) that fits each part, matched by name prefix
step_of = [["cradle_e220",7], ["box_e220",7], ["box_antenna",7], ["deck_",1], ["clamp_",2], ["box_motor_",2], ["disc_",2], ["wheel_",2], ["tire_",2],
           ["box_battery",3], ["charge_tray",3], ["box_relay",3], ["box_cn3795",3], ["box_ina219",3],
           ["box_wago",3], ["power_tray",3], ["standoff_",3], ["cradle_",3], ["box_pca9685",3],
           ["box_xl4015",3], ["box_tb6612",3], ["box_mp1584",3],
           ["logic_tray",4], ["box_esp",4], ["box_pi",4], ["box_mpu",4], ["box_i2c",4],
           ["turret_",5], ["box_servo_yaw",5], ["shoulder_mount",6], ["box_servo_shoulder",6],
           ["camera_mount",7], ["box_camera",7], ["nose",7], ["post_",7], ["roof_",7], ["box_panel",7]];
function step_for(n) = let(m = [for (e=step_of) if (len(n) >= len(e[0]) && [for (i=[0:len(e[0])-1]) n[i]] == [for (i=[0:len(e[0])-1]) e[0][i]]) e[1]]) len(m) ? m[0] : 6;
module show(n, c) if ((only == "" || only == n) && step_for(n) <= step) color(c) children();

// ---- chassis ----
show("deck_front", deck_c) stlp("deck_front");
show("deck_rear", deck_c) stlp("deck_rear");
plate_y0 = motor_outer_y - tt_body_t - 14 - clamp_t;
module clamp_a_world() translate([axle_x, plate_y0, axle_z]) rotate([-90,0,0]) stlp("motor_clamp_a");
module clamp_b_world() mirror([0,1,0]) translate([axle_x, plate_y0, axle_z]) rotate([-90,0,0]) mirror([1,0,0]) stlp("motor_clamp_b");
show("clamp_fl", deck_c) clamp_a_world();
show("clamp_rr", deck_c) rotate(180) clamp_a_world();
show("clamp_fr", deck_c) clamp_b_world();
show("clamp_rl", deck_c) rotate(180) clamp_b_world();
module motor_box() translate([axle_x+tt_shaft_from_end-tt_body_l, motor_outer_y-tt_body_t, axle_z-tt_body_h/2]) cube([tt_body_l, tt_body_t, tt_body_h]);
module wheel_only() translate([axle_x, 100, axle_z]) rotate([90,0,0]) stlp("wheel");
module tire_only() translate([axle_x, 100, axle_z]) rotate([90,0,0]) translate([0,0,2.2]) stlp("tire");
module disc_only() translate([axle_x, motor_outer_y-tt_body_t-7.5, axle_z]) rotate([-90,0,0]) stlp("encoder_disc");
for (c=[["fl",0,0],["fr",0,1],["rr",180,0],["rl",180,1]]) rotate(c[1]) mirror([0,c[2],0]) {
    show(str("box_motor_", c[0]), [0.95,0.85,0.2]) motor_box();
    show(str("wheel_", c[0]), blk*2) wheel_only();
    show(str("tire_", c[0]), blk) tire_only();
    show(str("disc_", c[0]), blk) disc_only();
}

// ---- body ----
show("nose", shell_c) stlp("nose");
posts = concat([for (x=[40,-40,-115,-140], y=[-62,62]) [x,y]], [[-62,20],[-62,-20]]);
for (i=[0:len(posts)-1]) show(str("post_", i), shell_c) translate([posts[i][0],posts[i][1],deck_top_z]) stlp("roof_post");
roof_z = deck_top_z + roof_post_h;
show("roof_tray_front", shell_c) translate([0,0,roof_z]) stlp("roof_tray_front");
show("roof_tray_rear", shell_c) translate([-120,0,roof_z]) stlp("roof_tray_rear");
for (x=[0,-120]) show(str("box_panel_", x == 0 ? "front" : "rear"), [0.1,0.2,0.5]) translate([x-panel[0]/2, -panel[1]/2, roof_z+4]) cube(panel);
// LoRa radio: cradle hanging upside down from the rear roof tray's bosses, the
// E220 under it (pins down, with Dupont plugs), SMA jack out the back and a
// swivel antenna turned up behind the roof
show("cradle_e220", [0.3,0.3,0.3]) translate([lora_x, 0, roof_z-4]) rotate([0,0,-90]) rotate([180,0,0]) stlp("module_cradle_e220");
show("box_e220", [0.1,0.45,0.35]) translate([lora_x-cradle_e220[1]/2, -cradle_e220[0]/2, roof_z-26]) cube([cradle_e220[1], cradle_e220[0], 15]);
show("box_antenna", blk) translate([lora_x-cradle_e220[1]/2-12, -4, roof_z-14]) cube([8, 8, 120]);

// ---- electronics ----
// Stand-ins for the bought modules: footprints are typical sizes (MEASURE yours),
// and heights include the jumpers or servo plugs pushed onto their pins.
module mod(n, c, p, size) show(n, c) translate(p) cube(size);
pt = deck_top_z + 3;                              // power tray top
lt = deck_top_z + 3 + tray_standoff_h;            // logic tray underside
ct = deck_top_z + charge_tray_z + 3;              // charge tray top
cc = charge_tray_c;
show("power_tray", [0.3,0.3,0.3]) translate([0,0,deck_top_z]) stlp("power_tray");
so = [for (x=[-40,40], y=[-50,50]) [x,y]];
for (i=[0:3]) show(str("standoff_", i), [0.3,0.3,0.3]) translate([so[i][0],so[i][1],pt]) stlp("tray_standoff");
show("logic_tray", [0.3,0.3,0.3]) translate([0,0,lt]) stlp("logic_tray");
show("charge_tray", [0.3,0.3,0.3]) translate([cc[0],cc[1],ct]) rotate([180,0,0]) stlp("charge_tray");
show("cradle_tb6612_a", [0.3,0.3,0.3]) translate([-2.5,-10.5,pt]) stlp("module_cradle_tb6612");
show("cradle_tb6612_b", [0.3,0.3,0.3]) translate([24,-10.5,pt]) stlp("module_cradle_tb6612");
show("cradle_mp1584", [0.3,0.3,0.3]) translate([-1.5,16.5,pt]) stlp("module_cradle_mp1584");
// power tray
mod("box_pca9685", [0.1,0.3,0.7], [-31,27.5,deck_top_z+7], [62,25,21.6]);   // servo plugs on top
mod("box_xl4015", blk, [-44,-30,pt], [26,51,16]);
mod("box_tb6612_a", [0.6,0.1,0.5], [-12.75,-20.75,pt+6], [20.5,20.5,21.6]); // jumpers on top
mod("box_tb6612_b", [0.6,0.1,0.5], [13.75,-20.75,pt+6], [20.5,20.5,21.6]);
mod("box_mp1584", [0.6,0.1,0.5], [-12.7,8,pt+6], [22.4,17,5]);
mod("box_wago_gnd", [0.9,0.9,0.9], [14,0,pt], [29.8,18.3,8.1]);
mod("box_wago_main", [0.9,0.9,0.9], [-15,-41,pt], [29.8,18.3,8.1]);
mod("box_wago_5v", [0.9,0.9,0.9], [15,-41,pt], [29.8,18.3,8.1]);
// logic tray
mod("box_esp_board", [0.1,0.4,0.6], [-27,-22,lt+6], [54,77,21.6]);    // expansion board + DevKit + jumpers
mod("box_pi", [0.2,0.6,0.3], [-32.5,-55,lt+7], [65,30,21.6]);         // Pi Zero + jumpers on the GPIO header
mod("box_mpu6050", blk, [29,-14,lt+3], [16,21,21.6]);
mod("box_i2c_hub", blk, [29,12,lt+3], [16,24,12]);
// battery holder on the deck, charging modules on the charge tray above it
mod("box_battery", [0.2,0.2,0.6], [cc[0]-cell_holder[0]/2, cc[1]-cell_holder[1]/2, deck_top_z], cell_holder);
mod("box_relay", blk, [cc[0]-28,cc[1]+22,ct], [50,26,18.5]);
mod("box_cn3795", blk, [cc[0]-37,cc[1]-4,ct], [45,25,15]);
mod("box_ina219_solar", blk, [cc[0]+11,cc[1]-2,ct], [25,22,21.6]);
mod("box_ina219_batt", blk, [cc[0]-36,cc[1]-30,ct], [25,22,21.6]);
mod("box_wago_chg", [0.9,0.9,0.9], [cc[0]-7,cc[1]-28,ct], [18.9,18.3,8.1]);
mod("box_wago_pack", [0.9,0.9,0.9], [cc[0]+14,cc[1]-28,ct], [18.9,18.3,8.1]);

// ---- turret + arm ----
tz = deck_top_z + turret_h;
translate([turret_x, 0, 0]) {
    show("turret_base", shell_c) translate([0,0,deck_top_z]) stlp("turret_base");
    show("box_servo_yaw", blk) translate([0,0,deck_top_z+turret_h-mg996_spline_h]) mg996_case_neg(0);
    rotate(yaw) {
        show("turret_top", shell_c) translate([0,0,tz+6]) rotate([180,0,0]) stlp("turret_top");
        show("shoulder_mount", arm_c) translate([0,0,tz+6]) stlp("shoulder_mount");
        show("camera_mount", blk) translate([0,0,tz+6]) stlp("camera_mount");
        show("box_servo_shoulder", blk) translate([0,0,tz+6]) servo_placed() mg996_case_neg(0);
        show("box_camera", [0.1,0.5,0.2]) translate([0,0,tz+6]) translate([38,0,2]) rotate([0,15,0]) translate([3.2,-12.5,10]) cube([1.6,25,24]);
        translate([0,0,tz+6+shoulder_z]) rotate([0,shoulder,0]) {
            show("upper_plate_horn", arm_c) translate([0,27.2,0]) rotate([90,0,0]) stlp("upper_plate_horn");
            show("upper_plate_idler", arm_c) translate([0,-26.8,0]) rotate([90,0,0]) stlp("upper_plate_idler");
            show("crossbar", arm_c) translate([60,-26.8,0]) rotate([-90,0,0]) stlp("crossbar");
            translate([link_upper,0,0]) rotate([0,elbow,0]) {
                show("forearm", arm_c) translate([0,-25.8,0]) rotate([-90,0,0]) stlp("forearm");
                show("box_servo_elbow", blk) elbow_servo() mg996_case_neg(0);
                show("box_servo_wrist", blk) wrist_servo() mg90_case_neg(0);
                translate([link_fore,0,0]) rotate([0,wrist,0]) {
                    show("gripper_palm", arm_c) translate([0,-30.8,0]) rotate([-90,0,0]) stlp("gripper_palm");
                    show("box_servo_grip", blk) translate([30,1.4,-2]) mg90_case_neg(0);
                    show("gripper_horn_arm", arm_c) translate([0,12,0]) rotate([90,0,0]) stlp("gripper_horn_arm");
                    show("finger_driven", blk) translate([30,1.4,2]) stlp("finger_driven");
                    show("finger_idler", blk) translate([30,-18.6,2]) stlp("finger_idler");
                }
            }
        }
    }
}
