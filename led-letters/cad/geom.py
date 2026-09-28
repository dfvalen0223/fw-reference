"""2D glyph geometry shared by the planners: glyph outlines exported from
OpenSCAD as SVG, and the polygon helpers used to analyse them."""
from __future__ import annotations

import math
import re
import subprocess
import tempfile
from pathlib import Path

LETTER_SCAD = Path(__file__).resolve().parent / "channel_letter.scad"

Pt = tuple[float, float]
Loop = list[Pt]


def scad_str(s: str) -> str:
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def glyph_loops(p, ch: str, rect: tuple[float, float, float, float] | None = None) -> list[Loop]:
    """Export the glyph (optionally clipped to rect=(x0,x1,y0,y1)) as SVG and
    return its closed loops."""
    shape = f"glyph_2d({scad_str(ch)}, {p.letter_height}, {scad_str(p.font)})"
    if rect is not None:
        x0, x1, y0, y1 = rect
        shape = f"intersection() {{ {shape}; translate([{x0}, {y0}]) square([{x1 - x0}, {y1 - y0}]); }}"
    with tempfile.TemporaryDirectory() as td:
        scad = Path(td) / "slice.scad"
        svg = Path(td) / "slice.svg"
        scad.write_text(f"include <{LETTER_SCAD}>\n{shape};\n")
        r = subprocess.run(["openscad", "-o", str(svg), str(scad)], capture_output=True, text=True)
        if not svg.exists():
            if "top level object is empty" in r.stderr or "not a 2D object" in r.stderr:
                return []
            raise RuntimeError(f"openscad 2D slice failed for {ch!r}:\n{r.stderr}")
        text = svg.read_text()
    loops: list[Loop] = []
    for d in re.findall(r'd="([^"]*)"', text):
        for sub in d.split("M"):
            pts = [(float(x), -float(y)) for x, y in re.findall(r"(-?[\d.e+-]+),(-?[\d.e+-]+)", sub)]
            if len(pts) >= 3:
                loops.append(pts)
    return loops


def segments(loops: list[Loop]) -> list[tuple[Pt, Pt]]:
    return [(loop[i], loop[(i + 1) % len(loop)]) for loop in loops for i in range(len(loop))]


def inside(pt: Pt, segs: list[tuple[Pt, Pt]]) -> bool:
    x, y = pt
    hit = False
    for (x1, y1), (x2, y2) in segs:
        if (y1 > y) != (y2 > y) and x1 + (y - y1) * (x2 - x1) / (y2 - y1) > x:
            hit = not hit
    return hit


def dist_to_outline(pt: Pt, segs: list[tuple[Pt, Pt]]) -> float:
    px, py = pt
    best = math.inf
    for (x1, y1), (x2, y2) in segs:
        dx, dy = x2 - x1, y2 - y1
        L2 = dx * dx + dy * dy
        t = 0.0 if L2 == 0 else max(0.0, min(1.0, ((px - x1) * dx + (py - y1) * dy) / L2))
        best = min(best, math.hypot(px - x1 - t * dx, py - y1 - t * dy))
    return best


def outer_loops(loops: list[Loop]) -> list[Loop]:
    """Island boundaries: loops nested inside an even number of other loops
    (the rest are holes)."""
    return [loop for i, loop in enumerate(loops)
            if sum(1 for j, o in enumerate(loops) if j != i and inside(loop[0], segments([o]))) % 2 == 0]


def closest_points(a: Loop, b: Loop) -> tuple[float, Pt, Pt]:
    """Closest pair of points between two polygon boundaries."""
    def seg_point(p0: Pt, s: tuple[Pt, Pt]) -> Pt:
        (x1, y1), (x2, y2) = s
        dx, dy = x2 - x1, y2 - y1
        L2 = dx * dx + dy * dy
        t = 0.0 if L2 == 0 else max(0.0, min(1.0, ((p0[0] - x1) * dx + (p0[1] - y1) * dy) / L2))
        return (x1 + t * dx, y1 + t * dy)
    best = (math.inf, a[0], b[0])
    for src, dst, flip in ((a, b, False), (b, a, True)):
        for v in src:
            for sgm in segments([dst]):
                q = seg_point(v, sgm)
                d = math.dist(v, q)
                if d < best[0]:
                    best = (d, q, v) if flip else (d, v, q)
    return best


def bridge_ribs(band: list[Loop], reach: float = 4.0) -> list[tuple[float, float, float, float]]:
    """Minimum spanning tree of straight ribs joining every island of a tile,
    each rib running `reach` mm into both islands so it fuses with them."""
    isl = outer_loops(band)
    if len(isl) < 2:
        return []
    joined, ribs = {0}, []
    while len(joined) < len(isl):
        _, pa, pb, j = min((closest_points(isl[i], isl[k]) + (k,)
                            for i in joined for k in range(len(isl)) if k not in joined),
                           key=lambda c: c[0])
        L = max(math.dist(pa, pb), 1e-6)
        ux, uy = (pb[0] - pa[0]) / L, (pb[1] - pa[1]) / L
        ribs.append((round(pa[0] - ux * reach, 2), round(pa[1] - uy * reach, 2),
                     round(pb[0] + ux * reach, 2), round(pb[1] + uy * reach, 2)))
        joined.add(j)
    return ribs


def axis_edges(loops: list[Loop], axis: int) -> list[float]:
    """Glyph edges parallel to a cut on `axis`: horizontal edges (their y) for
    axis=1, vertical edges (their x) for axis=0."""
    return [a[axis] for a, b in segments(loops)
            if abs(a[axis] - b[axis]) < 0.05 and abs(a[1 - axis] - b[1 - axis]) > 1.0]


def choose_cuts(lo: float, hi: float, max_len: float, edges: list[float],
                clearance: float = 20.0) -> tuple[list[float], list[str]]:
    """Uniform cuts over lo..hi, each nudged (up to +-90 mm) to the feasible
    position farthest from any parallel glyph edge, while every tile stays
    <= max_len and a feasible split of the rest remains."""
    n = max(1, math.ceil((hi - lo) / max_len))
    cuts, warnings = [lo], []
    for k in range(1, n):
        nominal = lo + (hi - lo) * k / n
        best, best_score = None, -1.0
        for off in range(-90, 91):
            cand = nominal + off
            if not (clearance < cand - cuts[-1] <= max_len):
                continue
            if hi - cand > max_len * (n - k):
                continue
            clear = min((abs(cand - e) for e in edges), default=math.inf)
            # Prefer enough clearance first, then staying close to uniform.
            score = min(clear, clearance) * 1000 - abs(off)
            if score > best_score:
                best, best_score = cand, score
        if best is None:
            best = min(nominal, cuts[-1] + max_len)
        if min((abs(best - e) for e in edges), default=math.inf) < clearance:
            warnings.append(f"cut at {best:.1f}mm is within {clearance:.0f}mm of a glyph edge")
        cuts.append(best)
    cuts.append(hi)
    return cuts, warnings


def signed_area_centroid(loop: Loop) -> tuple[float, float, float]:
    """(area, cx, cy) of one closed loop; area is unsigned."""
    a = cx = cy = 0.0
    for (x1, y1), (x2, y2) in zip(loop, loop[1:] + loop[:1]):
        c = x1 * y2 - x2 * y1
        a += c
        cx += (x1 + x2) * c
        cy += (y1 + y2) * c
    if a == 0:
        return 0.0, loop[0][0], loop[0][1]
    return abs(a / 2), cx / (3 * a), cy / (3 * a)


def area_centroid(loops: list[Loop]) -> tuple[float, float, float]:
    """Area and centroid of a shape with holes (even-odd nesting)."""
    total = sx = sy = 0.0
    outers = {id(l) for l in outer_loops(loops)}
    for loop in loops:
        a, cx, cy = signed_area_centroid(loop)
        sgn = 1.0 if id(loop) in outers else -1.0
        total += sgn * a
        sx += sgn * a * cx
        sy += sgn * a * cy
    if total <= 0:
        return 0.0, 0.0, 0.0
    return total, sx / total, sy / total


def x_intervals(loops: list[Loop]) -> list[tuple[float, float]]:
    """Merged x-extent of each island (outer loop)."""
    iv = sorted((min(x for x, _ in l), max(x for x, _ in l)) for l in outer_loops(loops))
    merged: list[tuple[float, float]] = []
    for a, b in iv:
        if merged and a <= merged[-1][1]:
            merged[-1] = (merged[-1][0], max(merged[-1][1], b))
        else:
            merged.append((a, b))
    return merged
