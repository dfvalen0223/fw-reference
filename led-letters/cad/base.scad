// base.scad
// Freestanding plinth so a letter stands on the floor or a table with no
// screws: the letter drops into a seat shaped like its own bottom (so round
// or pointed bottoms like O, S, V are held too) and the plinth is wide and
// deep enough not to tip. generate.py sizes it for a minimum tip-over angle
// and splits it into bed-sized segments joined by vertical dovetails: slide
// the next segment down onto the previous one. Each segment carries its
// code (e.g. "H B0a") on the underside.
//
// Letter coordinates, as in channel_letter.scad: X width, Y up, Z depth
// (Z=0 back face of the letter, Z=depth its front face). gy0 is the lowest
// point of the glyph. Profile, seen from the side:
//
//        back overhang     letter seat     front ledge
//     ┌──────────────────┐            ┌┐
//     │                  │  (pocket)  │└──────────┐  <- lip (low, so little of
//     │                  └────────────┘           │     the lit face is hidden)
//     └───────────────────────────────────────────┘  <- floor_t under the letter

include <channel_letter.scad>

// Dovetail d = [axis, at, pos, ytop]: seam plane x=at (axis 0) or z=at
// (axis 1), centred at `pos` along the seam, rising from the plinth bottom
// to ytop. The male flares out into the +axis neighbour.
function dovetail_poly(d, len, root, tip, ov) =
    d[0] == 0
        ? [[d[1] - ov, d[2] - root / 2], [d[1] + len, d[2] - tip / 2],
           [d[1] + len, d[2] + tip / 2], [d[1] - ov, d[2] + root / 2]]
        : [[d[2] - root / 2, d[1] - ov], [d[2] - tip / 2, d[1] + len],
           [d[2] + tip / 2, d[1] + len], [d[2] + root / 2, d[1] - ov]];

// Extrude an XZ-plane polygon vertically between y0 and y1.
module xz_prism(y0, y1) {
    multmatrix([[1, 0, 0, 0], [0, 0, 1, y0], [0, 1, 0, 0], [0, 0, 0, 1]])
        linear_extrude(height = y1 - y0) children();
}

module base_segment(ch, letter_height, depth, font,
                     sx0, sx1, sz0, sz1, gy0,
                     seat, lip, floor_t, clr = 0.5,
                     males = [], females = [], label = "", label_pt = [],
                     dt_len = 8, dt_root = 10, dt_tip = 16, dt_clr = 0.2) {
    yb = gy0 - clr - floor_t;   // plinth bottom (floor level)
    zf = depth + clr;           // front face of the seat
    difference() {
        union() {
            if (sz0 < zf)
                translate([sx0, yb, sz0]) cube([sx1 - sx0, gy0 + seat - yb, min(sz1, zf) - sz0]);
            if (sz1 > zf)
                translate([sx0, yb, max(sz0, zf)]) cube([sx1 - sx0, gy0 + lip - yb, sz1 - max(sz0, zf)]);
            for (d = males)
                xz_prism(yb, d[3]) polygon(dovetail_poly(d, dt_len, dt_root, dt_tip, 0.6));
        }
        // Seat: the letter's own solid, grown by the clearance, up to `seat`.
        translate([0, 0, -clr])
            linear_extrude(height = depth + 2 * clr)
                intersection() {
                    offset(delta = clr) glyph_2d(ch, letter_height, font);
                    translate([-5000, gy0 - clr - 1]) square([10000, seat + clr + 2]);
                }
        // Lead-in chamfer (2 mm) around the top of the seat.
        translate([0, 0, -clr - 2])
            linear_extrude(height = depth + 2 * clr + 4)
                intersection() {
                    offset(delta = clr + 2) glyph_2d(ch, letter_height, font);
                    translate([-5000, gy0 + seat - 2]) square([10000, 3]);
                }
        for (d = females)
            xz_prism(yb - 1, d[3] + 1) offset(delta = dt_clr) polygon(dovetail_poly(d, dt_len, dt_root, dt_tip, 1));
        // Segment code debossed on the underside (the face printed against
        // the bed), mirrored so it reads correctly when the segment is flipped.
        if (len(label_pt) == 2 && len(label) > 0)
            translate([label_pt[0], yb - 1, label_pt[1]])
                rotate([-90, 0, 0])
                    linear_extrude(height = 1.6)
                        mirror([1, 0, 0])
                            text(label, size = 6, font = "Liberation Sans:style=Bold",
                                 halign = "center", valign = "center");
    }
}
