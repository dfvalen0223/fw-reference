"""Placement of the small features on a letter's back wall: the connection
port (DC jack + data connector) and the debossed tile codes."""
from __future__ import annotations

import math

from geom import Pt, dist_to_outline, inside

# Must match channel_letter.scad.
PORT_SPACING = 24.0
PORT_PAD = (50.0, 24.0)       # horizontal pair; a vertical pair is the transpose
GLAND_D = 7.0
PORT_KIND_H, PORT_KIND_V, PORT_KIND_GLAND = 0, 1, 2
LABEL_CHAR_W = 0.62           # Liberation Sans Bold advance, as a fraction of size


Rect = tuple[float, float, float, float]  # x0, x1, y0, y1


def rect_around(c: Pt, w: float, h: float) -> Rect:
    return (c[0] - w / 2, c[0] + w / 2, c[1] - h / 2, c[1] + h / 2)


def rect_fits(r: Rect, segs, clear: float) -> bool:
    """Rectangle inside the glyph with `clear` mm to the outline, checked on
    its corners, edge midpoints and centre (outlines are locally straight at
    this scale)."""
    x0, x1, y0, y1 = r
    xm, ym = (x0 + x1) / 2, (y0 + y1) / 2
    for q in ((x0, y0), (x1, y0), (x0, y1), (x1, y1), (xm, y0), (xm, y1), (x0, ym), (x1, ym), (xm, ym)):
        if not inside(q, segs) or dist_to_outline(q, segs) < clear:
            return False
    return True


def rect_point_dist(r: Rect, q: Pt) -> float:
    dx = max(r[0] - q[0], 0, q[0] - r[1])
    dy = max(r[2] - q[1], 0, q[1] - r[3])
    return math.hypot(dx, dy)


def rects_apart(a: Rect, b: Rect, gap: float) -> bool:
    return a[1] + gap <= b[0] or b[1] + gap <= a[0] or a[3] + gap <= b[2] or b[3] + gap <= a[2]


def near_cuts(r: Rect, xcuts: list[float], ycuts: list[float], clear: float) -> bool:
    return (any(r[0] - clear < c < r[1] + clear for c in xcuts[1:-1])
            or any(r[2] - clear < c < r[3] + clear for c in ycuts[1:-1]))


def choose_port(cands: list[tuple[float, Pt]], segs, wall_t: float, y_min: float,
                pixels: list[Pt], socket_r: float, rig_rects: list[Rect],
                xcuts: list[float], ycuts: list[float], data_in: Pt | None) -> tuple[float, float, int] | None:
    """Best spot for the port: low on the letter (just above the plinth) and
    close to pixel 1, clear of pixels, rigging pads and tile seams. Falls back
    to a single cable-gland hole where no stroke is wide enough for the pair."""
    options = []
    for kind, (w, h) in ((PORT_KIND_H, PORT_PAD), (PORT_KIND_V, PORT_PAD[::-1]), (PORT_KIND_GLAND, (GLAND_D + 6, GLAND_D + 6))):
        for d, c in cands:
            if d < min(w, h) / 2 + wall_t + 0.8:
                continue
            r = rect_around(c, w, h)
            if r[2] < y_min:
                continue
            if near_cuts(r, xcuts, ycuts, 12.0):
                continue
            if any(rect_point_dist(r, q) < socket_r + 3 for q in pixels):
                continue
            if any(not rects_apart(r, rr, 4) for rr in rig_rects):
                continue
            if not rect_fits(r, segs, wall_t + 0.8):
                continue
            score = r[2] + (0.5 * math.dist(c, data_in) if data_in else 0) + (1000 if kind == PORT_KIND_GLAND else 0)
            options.append((score, c, kind))
    if not options:
        return None
    _, c, kind = min(options, key=lambda o: o[0])
    return (round(c[0], 2), round(c[1], 2), kind)


def port_rect(port: tuple[float, float, int]) -> Rect:
    x, y, kind = port
    if kind == PORT_KIND_H:
        return rect_around((x, y), *PORT_PAD)
    if kind == PORT_KIND_V:
        return rect_around((x, y), *PORT_PAD[::-1])
    return rect_around((x, y), GLAND_D + 6, GLAND_D + 6)


def choose_label(text: str, tile: Rect, cands: list[tuple[float, Pt]], segs, busy_rects: list[Rect],
                 holes: list[Pt]) -> tuple[float, float, float] | None:
    """Spot for the tile code on the back face, as close to the tile centre as
    possible, shrinking the text for thin strokes. Returns (x, y, size)."""
    tc = ((tile[0] + tile[1]) / 2, (tile[2] + tile[3]) / 2)
    in_tile = [c for c in cands if tile[0] < c[1][0] < tile[1] and tile[2] < c[1][1] < tile[3]]
    for size in (7.0, 5.5, 4.0):
        w, h = len(text) * size * LABEL_CHAR_W + 2, size + 2
        best = None
        for d, c in in_tile:
            if d < h / 2 + 1.5:
                continue
            r = rect_around(c, w, h)
            if r[0] < tile[0] + 2 or r[1] > tile[1] - 2 or r[2] < tile[2] + 2 or r[3] > tile[3] - 2:
                continue
            if any(rect_point_dist(r, q) < 4 for q in holes):
                continue
            if any(not rects_apart(r, b, 2) for b in busy_rects):
                continue
            if not rect_fits(r, segs, 1.5):
                continue
            dd = math.dist(c, tc)
            if best is None or dd < best[0]:
                best = (dd, c)
        if best:
            return (round(best[1][0], 2), round(best[1][1], 2), size)
    return None
