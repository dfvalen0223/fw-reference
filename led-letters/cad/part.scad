// part.scad
// Command-line entry point. generate.py invokes openscad against this file
// once per (letter, tile, part), passing parameters with -D. Example:
//   openscad -o out.stl -D 'ch="A"' -D letter_height=500 -D depth=80 \
//            -D x_lo=-250 -D x_hi=0 -D y_lo=0 -D y_hi=166.7 \
//            -D cut_right=true -D cut_top=true -D 'part="carcaza"' part.scad
// Calibration coupon for the snap joint:
//   openscad -o coupon.stl -D 'part="snap_test_coupon"' -D joint_clearance=0.18 part.scad

include <base.scad>

// ---- parameters (overridden via -D) ----
ch = "A";
font = "Liberation Sans:style=Bold";
letter_height = 500;   // mm, overall glyph height
depth = 80;            // mm, front-to-back depth
wall_t = 2.4;          // mm, return wall
back_t = 2.0;          // mm, back wall
x_lo = -1000;          // mm, tile bounds (default: whole letter)
x_hi = 1000;
y_lo = 0;
y_hi = 1000;
cut_left = false;      // true when that edge is shared with a neighbour tile
cut_right = false;
cut_bottom = false;
cut_top = false;
ribs = [];             // [[x1,y1,x2,y2],...] bridges between disjoint islands

// tile-to-tile snap joint
collar_t = 3.4;
collar_h = 10;
tongue_t1 = 1.8;
tongue_t2 = 3.9;
tongue_len = 5;
bead = 0.35;
joint_clearance = 0.18;

// interior: XY points chosen by generate.py
leds = [];      // WS2812B 12 mm pixel sockets
rigs = [];      // zip-tie rigging points
port = [];      // [x, y, kind]: 0/1 connector pair side by side/stacked, 2 cable gland; [] none
label = "";     // tile code debossed on the back, e.g. "H r0c1"
label_pt = [];  // [x, y, text size]

// diffuser
diffusor_t = 1.6;
rim_depth = 6;
rim_t = 1.2;
rim_clearance = 0.25;
diffusor_bead = 0.7;

// base (plinth) segment, see base.scad
seg = [0, 100, 0, 100];   // [sx0, sx1, sz0, sz1] footprint in letter coords
gy0 = 0;                   // lowest point of the glyph
seat = 20;
lip = 8;
floor_t = 5;
males = [];                // dovetails [[axis, at, pos, ytop], ...]
females = [];

part = "carcaza"; // "carcaza" | "diffusor" | "base" | "snap_test_coupon"

if (part == "carcaza") {
    carcaza_tile(ch, letter_height, depth, wall_t, back_t, font,
                    x_lo, x_hi, y_lo, y_hi, cut_left, cut_right, cut_bottom, cut_top,
                    ribs = ribs,
                    collar_t = collar_t, collar_h = collar_h,
                    tongue_t1 = tongue_t1, tongue_t2 = tongue_t2, tongue_len = tongue_len,
                    bead = bead, joint_clearance = joint_clearance,
                    leds = leds, rigs = rigs, port = port, label = label, label_pt = label_pt,
                    rim_depth = rim_depth, diffusor_bead = diffusor_bead,
                    rim_clearance = rim_clearance);
} else if (part == "diffusor") {
    diffusor_tile(ch, letter_height, depth, wall_t, font, x_lo, x_hi, y_lo, y_hi,
                     diffusor_t = diffusor_t, rim_depth = rim_depth, rim_t = rim_t,
                     rim_clearance = rim_clearance, diffusor_bead = diffusor_bead);
} else if (part == "base") {
    // Exported lying on its bottom, as it prints.
    rotate([90, 0, 0])
        base_segment(ch, letter_height, depth, font, seg[0], seg[1], seg[2], seg[3], gy0,
                     seat, lip, floor_t, males = males, females = females,
                     label = label, label_pt = label_pt);
} else if (part == "snap_test_coupon") {
    snap_test_coupon(wall_t = wall_t, collar_t = collar_t, t1 = tongue_t1, t2 = tongue_t2,
                     len = tongue_len, bead = bead, clearance = joint_clearance);
} else {
    assert(false, str("Unknown part: ", part));
}
