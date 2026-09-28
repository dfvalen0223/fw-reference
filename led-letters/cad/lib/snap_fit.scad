// snap_fit.scad
// Screw-free joints that follow the letter's own outline, so they work for
// any glyph without knowing its shape in advance. Every module takes the 2D
// outline as children().
//
// Tile-to-tile joint (at any vertical or horizontal cut line):
//   - The wall is locally thickened inward into a "collar" around the cut.
//   - The tile on the - side grows a continuous TONGUE out of the collar (a thin
//     ring centred in the wall) with a small BEAD near its tip.
//   - The tile on the + side has the matching GROOVE: a narrow mouth followed by a
//     wider relief chamber. The bead is slightly wider than the mouth, so it
//     clicks past it and stays locked -- the same continuous snap lip used on
//     3D-printed enclosure lids. Pry apart to disassemble.
//
// Diffuser-to-carcaza joint (along Z, at the open front face):
//   - A thin rim on the diffuser, with an outward bead, snaps into a shallow
//     groove on the inner face of the return wall.
//
// Every step here lies in the XY plane and the carcaza prints standing on its
// back (Z up), so none of these features needs supports.

// Region between the outline inset by `a` and inset by `b` (a < b).
module ring2d(a, b) {
    difference() {
        offset(delta = -a) children();
        offset(delta = -b) children();
    }
}

// Band of the plane between lo..hi along `axis` (0 = X, 1 = Y), unbounded
// along the other axis.
module band2d(lo, hi, axis = 1) {
    if (axis == 1) translate([-5000, lo]) square([10000, hi - lo]);
    else           translate([lo, -5000]) square([hi - lo, 10000]);
}

// A joint runs along a cut line at coordinate `at` on `axis` (0 = vertical
// cut at x=at, 1 = horizontal cut at y=at). `span_lo..span_hi` limits it
// along the cut line (the tile's extent on the other axis, minus corner
// keep-outs where two cut lines cross).

// Local inward thickening of the wall around the cut line. The tiny outward
// offset merges parts of the region that would otherwise touch along a
// zero-width line (e.g. at the sharp apex of a counter), which exports as
// non-manifold edges.
module joint_collar(at, axis, span_lo, span_hi, wall_t, collar_t, collar_h, z0, z1) {
    translate([0, 0, z0])
        linear_extrude(height = z1 - z0)
            offset(delta = 0.01) intersection() {
                ring2d(0, wall_t + collar_t) children();
                band2d(at - collar_h, at + collar_h, axis);
                band2d(span_lo, span_hi, 1 - axis);
            }
}

// Tongue, protruding in +axis from the cut line (distances measured inward
// from the outer outline):
//   t1..t2      : tongue faces (tongue thickness = t2 - t1)
//   bead        : extra width per side at the bead
//   lead        : width reduction per side at the tip (lead-in)
//   len         : total protrusion past the cut line
//   bead_y0/y1  : range (relative to the cut) that carries the bead
module joint_tongue(at, axis, span_lo, span_hi, z0, z1, t1, t2, len, bead, lead,
                    bead_y0, bead_y1, overlap = 0.6) {
    translate([0, 0, z0])
        linear_extrude(height = z1 - z0)
            intersection() {
                union() {
                    intersection() {
                        ring2d(t1, t2) children();
                        band2d(at - overlap, at + bead_y0 + 0.01, axis);
                    }
                    intersection() {
                        ring2d(t1 - bead, t2 + bead) children();
                        band2d(at + bead_y0, at + bead_y1 + 0.01, axis);
                    }
                    intersection() {
                        ring2d(t1 + lead, t2 - lead) children();
                        band2d(at + bead_y1, at + len, axis);
                    }
                }
                band2d(span_lo, span_hi, 1 - axis);
            }
}

// Matching groove, cut into the tile whose -axis edge is at `at`.
module joint_groove(at, axis, span_lo, span_hi, z0, z1, t1, t2, len, bead, bead_y0,
                    clearance, slack = 0.6) {
    mouth_end = at + bead_y0 - 0.2;
    translate([0, 0, z0])
        linear_extrude(height = z1 - z0)
            intersection() {
                union() {
                    intersection() {
                        ring2d(t1 - clearance, t2 + clearance) children();
                        band2d(at - 1, mouth_end + 0.01, axis);
                    }
                    intersection() {
                        ring2d(t1 - bead - clearance, t2 + bead + clearance) children();
                        band2d(mouth_end, at + len + slack, axis);
                    }
                }
                band2d(span_lo - clearance, span_hi + clearance, 1 - axis);
            }
}

// Shallow groove on the inner face of the return wall, near the front, that
// the diffuser rim's bead snaps into.
module diffusor_groove(wall_t, depth_into_wall, z_center, groove_h) {
    translate([0, 0, z_center - groove_h / 2])
        linear_extrude(height = groove_h)
            ring2d(wall_t - depth_into_wall, wall_t + 0.01) children();
}

// Small printable calibration coupon: the same joint applied to a 40x30 mm
// rectangular "letter". Print one at a few clearances and keep the value
// that clicks together firmly by hand and can still be pried apart.
module snap_test_coupon(wall_t = 2.4, collar_t = 3.4, t1 = 1.8, t2 = 3.9,
                         len = 5, bead = 0.35, lead = 0.3, bead_y0 = 2.2,
                         bead_y1 = 3.6, clearance = 0.18) {
    h = 12;           // height of each half
    d = 20;           // depth (Z)
    back_t = 2;
    z0 = back_t;
    z1 = d - 2;

    module outline() { translate([-20, -15]) square([40, 30]); }
    module shell_band(y0, y1) {
        intersection() {
            union() {
                linear_extrude(height = d) ring2d(0, wall_t) outline();
                linear_extrude(height = back_t) outline();
            }
            translate([-50, y0, -1]) cube([100, y1 - y0, d + 2]);
        }
        intersection() {
            joint_collar(0, 1, -100, 100, wall_t, collar_t, h, z0, z1) outline();
            translate([-50, y0, -1]) cube([100, y1 - y0, d + 2]);
        }
    }

    // Lower half (tongue) -- only the band just below the cut is printed.
    translate([-30, 0, 0]) {
        intersection() {
            union() {
                shell_band(-h, 0);
                joint_tongue(0, 1, -100, 100, z0 + 0.3, z1 - 0.3, t1, t2, len, bead, lead, bead_y0, bead_y1) outline();
            }
            translate([-50, -h, -1]) cube([100, h + len + 1, d + 2]);
        }
    }
    // Upper half (groove), printed next to it.
    translate([30, 0, 0]) {
        difference() {
            shell_band(0, h);
            joint_groove(0, 1, -100, 100, z0, z1, t1, t2, len, bead, bead_y0, clearance) outline();
        }
    }
}
