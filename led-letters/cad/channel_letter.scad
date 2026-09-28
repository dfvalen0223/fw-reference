// channel_letter.scad
// Parametric backlit "channel letter" generator: one glyph, split into a grid
// of printer-bed-sized tiles that snap together with NO screws and NO glue.
// Two parts per tile:
//   - "carcaza"  : opaque shell (back wall + return wall) carrying the
//                  WS2812B 12 mm pixel sockets, the tile-to-tile snap joints
//                  and the zip-tie rigging points.
//   - "diffusor" : translucent front skin with a thin rim that snaps into the
//                  carcaza's open front.
//
// Axes: X = letter width, Y = letter height, Z = depth (Z=0 is the back face,
// Z=depth is the open front). Print the carcaza on its back (Z up) and the
// diffusor on its face; neither needs supports.
//
// cad/part.scad is the CLI entry point; cad/generate.py drives it and decides
// the tile grid, pixel positions, rigging points and bridge ribs.

include <lib/snap_fit.scad>

// ---- glyph ------------------------------------------------------------

// 2D glyph outline scaled so its bounding box is exactly `letter_height`
// tall, baseline at y=0, horizontally centered on x=0.
module glyph_2d(ch, letter_height, font) {
    resize([0, letter_height], auto = [true, true])
        text(ch, size = 100, font = font, halign = "center", valign = "baseline");
}

module glyph_solid(ch, letter_height, depth, font) {
    linear_extrude(height = depth)
        glyph_2d(ch, letter_height, font);
}

// Hollow shell: back wall [0..back_t] under the whole glyph, return wall
// (perimeter ring, thickness wall_t) up to the open front at z=depth.
module glyph_shell(ch, letter_height, depth, wall_t, back_t, font) {
    difference() {
        glyph_solid(ch, letter_height, depth, font);
        translate([0, 0, back_t])
            linear_extrude(height = depth - back_t + 1)
                offset(delta = -wall_t)
                    glyph_2d(ch, letter_height, font);
    }
}

// ---- interior features -------------------------------------------------

// WS2812B 12 mm pixel sockets. generate.py lays the pixels out (spacing
// ~= depth for even diffusion, narrow strokes get a single centre line) and
// passes their XY positions in `pts`. Each socket is a split tube the pixel
// pushes into from the front; the four slots let the string's wires leave
// in any direction, and the small hole through the back wall lets you push a
// pixel back out with a pen for service.
module pixel_sockets(pts, back_t, pixel_d = 12.2, wall = 1.6, height = 9, slot_w = 5.5) {
    for (pt = pts)
        translate([pt[0], pt[1], back_t - 0.2])
            difference() {
                cylinder(d = pixel_d + 2 * wall, h = height + 0.2, $fn = 32);
                translate([0, 0, -1]) cylinder(d = pixel_d, h = height + 2, $fn = 32);
                for (a = [0, 90])
                    rotate([0, 0, a])
                        translate([-(pixel_d + 2 * wall + 2) / 2, -slot_w / 2, 0.2 + 1.2])
                            cube([pixel_d + 2 * wall + 2, slot_w, height]);
            }
}

module pixel_service_holes(pts, back_t, d = 4) {
    for (pt = pts)
        translate([pt[0], pt[1], -1]) cylinder(d = d, h = back_t + 2, $fn = 16);
}

// Bridge ribs: thin (1.5 mm) straight webs flush with the back face that tie
// together regions of the glyph that are disjoint inside one tile (e.g. the
// two strokes near the apex of a tall "A"), so every tile prints and handles
// as ONE piece. generate.py computes them as a minimum spanning tree between
// the islands' nearest points and passes [[x1,y1,x2,y2], ...] in `ribs`.
module bridge_ribs(ribs, thickness = 1.5, width = 6) {
    for (r = ribs)
        linear_extrude(height = thickness)
            hull() {
                translate([r[0], r[1]]) circle(d = width, $fn = 16);
                translate([r[2], r[3]]) circle(d = width, $fn = 16);
            }
}

// Rigging points (screw-free mounting): a pad on the inside of the back wall
// pierced by two zip-tie slots stacked in Y. A zip-tie goes out one slot,
// around a truss tube / backer rail, and back in the other. The back face
// stays flat, so it also takes Dual-Lock / VHB pads as a no-tool alternative.
// generate.py picks the points (well inside the strokes, clear of pixels and
// cut lines) and passes them in `pts`.
module rigging_pads(pts, back_t, pad_w = 14, pad_h = 18, pad_t = 2.5) {
    for (pt = pts)
        translate([pt[0] - pad_w / 2, pt[1] - pad_h / 2, back_t - 0.2])
            cube([pad_w, pad_h, pad_t + 0.2]);
}

module rigging_slots(pts, back_t, pad_t = 2.5, slot_w = 6, slot_h = 2.6, slot_gap = 8) {
    for (pt = pts, s = [-1, 1])
        translate([pt[0] - slot_w / 2, pt[1] + s * slot_gap / 2 - slot_h / 2, -1])
            cube([slot_w, slot_h, back_t + pad_t + 2]);
}

// ---- parts ---------------------------------------------------------------
//
// A letter is split into a grid of tiles [x_lo..x_hi] x [y_lo..y_hi] that
// each fit the print bed. cut_left/right/bottom/top say whether that edge is
// an internal cut line (shared with a neighbouring tile). Convention: the
// right and top edges carry TONGUES, the left and bottom edges GROOVES, so
// any two neighbours always mate. Near a point where two cut lines cross the
// joint stops `corner_keepout` short, so perpendicular joints never collide.

module tile_box(x_lo, x_hi, y_lo, y_hi, grow = 0) {
    translate([x_lo - grow, y_lo - grow, -50])
        cube([x_hi - x_lo + 2 * grow, y_hi - y_lo + 2 * grow, 5000]);
}

// Z range used by tile joints: above the back wall, and stopping short of
// the front zone reserved for the diffuser rim.
function joint_z1(depth, rim_depth) = depth - rim_depth - 1;

module carcaza_tile(ch, letter_height, depth, wall_t, back_t, font,
                     x_lo, x_hi, y_lo, y_hi,
                     cut_left, cut_right, cut_bottom, cut_top,
                     ribs = [],
                     collar_t = 3.4, collar_h = 10,
                     tongue_t1 = 1.8, tongue_t2 = 3.9, tongue_len = 5,
                     bead = 0.35, lead = 0.3, bead_y0 = 2.2, bead_y1 = 3.6,
                     joint_clearance = 0.18,
                     leds = [], rigs = [],
                     rim_depth = 6, diffusor_bead = 0.7, rim_clearance = 0.25) {
    z0 = back_t;
    z1 = joint_z1(depth, rim_depth);
    keep = tongue_len + 3;
    // Span of each joint along its cut line, with corner keep-outs.
    xs_lo = x_lo + (cut_left ? keep : 0);
    xs_hi = x_hi - (cut_right ? keep : 0);
    ys_lo = y_lo + (cut_bottom ? keep : 0);
    ys_hi = y_hi - (cut_top ? keep : 0);

    difference() {
        union() {
            intersection() {
                union() {
                    glyph_shell(ch, letter_height, depth, wall_t, back_t, font);
                    if (cut_bottom)
                        joint_collar(y_lo, 1, x_lo, x_hi, wall_t, collar_t, collar_h, z0, z1)
                            glyph_2d(ch, letter_height, font);
                    if (cut_top)
                        joint_collar(y_hi, 1, x_lo, x_hi, wall_t, collar_t, collar_h, z0, z1)
                            glyph_2d(ch, letter_height, font);
                    if (cut_left)
                        joint_collar(x_lo, 0, y_lo, y_hi, wall_t, collar_t, collar_h, z0, z1)
                            glyph_2d(ch, letter_height, font);
                    if (cut_right)
                        joint_collar(x_hi, 0, y_lo, y_hi, wall_t, collar_t, collar_h, z0, z1)
                            glyph_2d(ch, letter_height, font);
                    bridge_ribs(ribs, thickness = min(1.5, back_t));
                    rigging_pads(rigs, back_t);
                    pixel_sockets(leds, back_t);
                }
                tile_box(x_lo, x_hi, y_lo, y_hi);
            }
            if (cut_top)
                joint_tongue(y_hi, 1, xs_lo, xs_hi, z0 + 0.3, z1 - 0.3, tongue_t1, tongue_t2,
                             tongue_len, bead, lead, bead_y0, bead_y1)
                    glyph_2d(ch, letter_height, font);
            if (cut_right)
                joint_tongue(x_hi, 0, ys_lo, ys_hi, z0 + 0.3, z1 - 0.3, tongue_t1, tongue_t2,
                             tongue_len, bead, lead, bead_y0, bead_y1)
                    glyph_2d(ch, letter_height, font);
        }
        if (cut_bottom)
            joint_groove(y_lo, 1, xs_lo, xs_hi, z0, z1, tongue_t1, tongue_t2, tongue_len,
                         bead, bead_y0, joint_clearance)
                glyph_2d(ch, letter_height, font);
        if (cut_left)
            joint_groove(x_lo, 0, ys_lo, ys_hi, z0, z1, tongue_t1, tongue_t2, tongue_len,
                         bead, bead_y0, joint_clearance)
                glyph_2d(ch, letter_height, font);
        rigging_slots(rigs, back_t);
        pixel_service_holes(leds, back_t);
        // Snap groove for the diffuser rim bead.
        intersection() {
            diffusor_groove(wall_t, diffusor_bead - rim_clearance + 0.15,
                            depth - rim_depth / 2, 1.4)
                glyph_2d(ch, letter_height, font);
            tile_box(x_lo, x_hi, y_lo, y_hi, grow = 1);
        }
    }
}

// Diffuser: 1.6 mm translucent skin flush with the open front, plus a thin
// rim (not a solid plug, so it doesn't eat light) that drops inside the return
// wall and carries an outward bead that clicks into the carcaza's groove.
// Split on the same grid as the carcaza.
module diffusor_tile(ch, letter_height, depth, wall_t, font, x_lo, x_hi, y_lo, y_hi,
                      diffusor_t = 1.6, rim_depth = 6, rim_t = 1.2,
                      rim_clearance = 0.25, diffusor_bead = 0.7) {
    rim_in = wall_t + rim_clearance;
    intersection() {
        union() {
            translate([0, 0, depth - diffusor_t])
                linear_extrude(height = diffusor_t)
                    glyph_2d(ch, letter_height, font);
            translate([0, 0, depth - rim_depth])
                linear_extrude(height = rim_depth)
                    ring2d(rim_in, rim_in + rim_t) glyph_2d(ch, letter_height, font);
            // Bead, centred on the carcaza groove (depth - rim_depth/2).
            translate([0, 0, depth - rim_depth / 2 - 0.5])
                linear_extrude(height = 1.0)
                    ring2d(rim_in - (diffusor_bead - rim_clearance), rim_in + 0.01)
                        glyph_2d(ch, letter_height, font);
        }
        tile_box(x_lo, x_hi, y_lo, y_hi);
    }
}
