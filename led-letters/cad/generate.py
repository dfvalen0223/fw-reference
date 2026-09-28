#!/usr/bin/env python3
"""Automated generator for modular, screw-free backlit channel letters.

Given a word/phrase and a letter height (150-1000 mm) this script:

  1. Slices each glyph with OpenSCAD (fast 2D SVG export) and, in Python:
       - splits the letter into a grid of tiles that each fit the print bed,
         nudging every cut line off horizontal/vertical glyph edges (a cut
         along an edge would leave paper-thin slivers) and skipping empty
         tiles,
       - lays out the WS2812B 12 mm pixels (spacing ~= depth for even
         diffusion; narrow strokes get a single centre line),
       - orders them into one continuous data chain per letter and flags hops
         longer than the pixel string's lead wire,
       - picks zip-tie rigging points clear of pixels and cut lines,
       - joins disjoint islands inside a tile with bridge ribs so each tile
         prints as one piece.
  2. Calls OpenSCAD headlessly (in parallel) to export every
     (letter, tile, part) STL: "carcaza" (shell) + "diffusor" (front).
  3. Writes manifest.json and bom.csv.
  4. Regenerates firmware/main/letters_config.h (per-LED map) so the ESP32-S3
     firmware always matches the CAD that was just generated.

Usage:
    python3 generate.py --text "HOLA" --letter-height 500 --output-dir ../build/HOLA
"""
from __future__ import annotations

import argparse
import csv
import heapq
import json
import math
import re
import string
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import asdict, dataclass, field
from pathlib import Path

from base_plan import BasePlan, BaseSegment, plan_base
from features import PORT_KIND_GLAND, choose_label, choose_port, port_rect, rect_around
from manual import write_manual
from geom import (Loop, Pt, axis_edges, bridge_ribs, choose_cuts, dist_to_outline, glyph_loops, inside,
                  outer_loops, scad_str, segments)

SCRIPT_DIR = Path(__file__).resolve().parent
PART_SCAD = SCRIPT_DIR / "part.scad"
FIRMWARE_CONFIG = SCRIPT_DIR.parent / "firmware" / "main" / "letters_config.h"
KCONFIG = SCRIPT_DIR.parent / "firmware" / "main" / "Kconfig.projbuild"

# Must match the defaults in channel_letter.scad / lib/snap_fit.scad.
COLLAR_H = 10.0
TONGUE_LEN = 5.0
SOCKET_R = (12.2 + 2 * 1.6) / 2       # pixel socket outer radius
PAD_HALF_DIAG = math.hypot(7, 9)      # rigging pad 14 x 18 mm
CUT_EDGE_CLEARANCE = 40.0             # min distance from a cut to a parallel glyph edge
LETTER_SPACING = 0.12                 # gap between letters, fraction of height
BASE_GAP = 5.0                        # min gap between neighbouring plinths, mm
PIXEL_MAX_A = 0.060                   # WS2812B at full white
PSU_SIZES_A = (2, 3, 4, 5, 6, 8, 10, 15, 20)
PSU_BUDGET = 0.8                      # firmware keeps each letter under 80 % of its supply
MAX_OUTPUTS = 16                      # parallel LCD-bus lanes on the ESP32-S3

@dataclass
class Params:
    text: str
    letter_height: float
    depth: float
    led_pitch: float
    wall_t: float = 2.4
    back_t: float = 2.0
    bed_x: float = 220.0
    bed_y: float = 220.0
    bed_z: float = 250.0
    bridge_islands: bool = True
    font: str = "Liberation Sans:style=Bold"
    joint_clearance: float = 0.18
    pixel_lead: float = 100.0   # wire length between pixels on the string
    rig_pitch: float = 150.0
    stand: bool = True          # freestanding plinth under each letter
    tip_angle: float = 15.0     # minimum tip-over angle the plinth must give

    @property
    def tile_max(self) -> float:
        # Square tiles fit the bed in either orientation, leaving room for the
        # tongues that protrude past the tile edges.
        return min(self.bed_x, self.bed_y) - TONGUE_LEN - 5


@dataclass
class TileJob:
    letter: str
    letter_index: int
    col: int
    row: int
    x_lo: float
    x_hi: float
    y_lo: float
    y_hi: float
    cut_left: bool
    cut_right: bool
    cut_bottom: bool
    cut_top: bool
    islands: int = 1
    size_mm: tuple[float, float] = (0.0, 0.0)
    fits_bed: bool = True
    leds: list[Pt] = field(default_factory=list)
    rigs: list[Pt] = field(default_factory=list)
    ribs: list[tuple[float, float, float, float]] = field(default_factory=list)
    port: tuple[float, float, int] | None = None
    label_pt: tuple[float, float, float] | None = None
    carcaza_file: str = ""
    diffusor_file: str = ""

    @property
    def tag(self) -> str:
        return f"r{self.row}c{self.col}"

    @property
    def label(self) -> str:
        return f"{self.letter} {self.tag}"


@dataclass
class GlyphPlan:
    tiles: list[TileJob]
    chain: list[Pt]          # pixel positions in data-chain order (glyph coords)
    x0: float
    x1: float
    extensions: list[dict]   # {"after_pixel", "length_mm", "route"}
    outline: list[Loop] = field(default_factory=list)
    base: BasePlan | None = None
    port: tuple[float, float, int] | None = None
    notes: list[str] = field(default_factory=list)   # special notes for the manual (Spanish)


# ---- glyph planning --------------------------------------------------------

def candidate_grid(segs, x0, x1, y0, y1, step, min_d) -> list[tuple[float, Pt]]:
    """(distance-to-outline, point) for grid points inside the glyph with at
    least `min_d` of clearance, most interior first."""
    out = []
    y = y0 + step / 2
    while y < y1:
        x = x0 + step / 2
        while x < x1:
            if inside((x, y), segs):
                d = dist_to_outline((x, y), segs)
                if d >= min_d:
                    out.append((d, (x, y)))
            x += step
        y += step
    out.sort(key=lambda c: -c[0])
    return out


def near_cut(pt: Pt, xcuts: list[float], ycuts: list[float], clear: float) -> bool:
    return (any(abs(pt[0] - c) < clear for c in xcuts[1:-1])
            or any(abs(pt[1] - c) < clear for c in ycuts[1:-1]))


def hop_inside(a: Pt, b: Pt, segs, clear: float) -> bool:
    """True if the wire between two pixels can run straight inside the letter
    (it can't cross a counter or leave the glyph: there's a wall in the way)."""
    n = max(2, int(math.dist(a, b) // 8))
    for k in range(1, n):
        q = (a[0] + (b[0] - a[0]) * k / n, a[1] + (b[1] - a[1]) * k / n)
        if not inside(q, segs) or dist_to_outline(q, segs) < clear:
            return False
    return True


class Geodesic:
    """Wire distances between pixels measured *inside* the letter: a
    visibility graph between nearby pixels (the wire can't cross a counter or
    leave the glyph) plus all-pairs Dijkstra. Pixels in separate islands get a
    penalised straight-line distance."""

    def __init__(self, pts: list[Pt], segs, clear: float, radius: float):
        n = len(pts)
        self.pts = pts
        adj: list[list[tuple[int, float]]] = [[] for _ in range(n)]
        for i in range(n):
            for j in range(i + 1, n):
                d = math.dist(pts[i], pts[j])
                if d <= radius and hop_inside(pts[i], pts[j], segs, clear):
                    adj[i].append((j, d))
                    adj[j].append((i, d))
        self.dist = [[math.inf] * n for _ in range(n)]
        self.prev = [[-1] * n for _ in range(n)]
        for s0 in range(n):
            dist, prev = self.dist[s0], self.prev[s0]
            dist[s0] = 0.0
            heap = [(0.0, s0)]
            while heap:
                d, u = heapq.heappop(heap)
                if d > dist[u]:
                    continue
                for v, w in adj[u]:
                    if d + w < dist[v]:
                        dist[v], prev[v] = d + w, u
                        heapq.heappush(heap, (d + w, v))

    def cost(self, i: int, j: int) -> float:
        d = self.dist[i][j]
        return d if d < math.inf else 3 * math.dist(self.pts[i], self.pts[j]) + 10000.0

    def route(self, i: int, j: int) -> list[Pt]:
        if self.dist[i][j] == math.inf:
            return [self.pts[i], self.pts[j]]
        path = [j]
        while path[-1] != i:
            path.append(self.prev[i][path[-1]])
        return [self.pts[k] for k in reversed(path)]

    def length(self, i: int, j: int) -> float:
        r = self.route(i, j)
        return sum(math.dist(a, b) for a, b in zip(r, r[1:]))


def order_chain(g: Geodesic) -> list[int]:
    """Shortest open path through every pixel (nearest neighbour from the
    bottom-left pixel, then 2-opt) using wire distance inside the letter, so
    branching glyphs (A, E, H...) backtrack along the cheapest stroke."""
    pts = g.pts
    n = len(pts)
    if n < 3:
        return sorted(range(n), key=lambda i: (pts[i][1], pts[i][0]))
    start = min(range(n), key=lambda i: (pts[i][1], pts[i][0]))
    path, rest = [start], set(range(n)) - {start}
    while rest:
        nxt = min(rest, key=lambda j: g.cost(path[-1], j))
        path.append(nxt)
        rest.remove(nxt)
    improved = True
    while improved:
        improved = False
        for i in range(1, n - 1):
            for j in range(i + 1, n):
                a, b, c = path[i - 1], path[i], path[j]
                d = path[j + 1] if j + 1 < n else None
                old = g.cost(a, b) + (g.cost(c, d) if d is not None else 0)
                new = g.cost(a, c) + (g.cost(b, d) if d is not None else 0)
                if new < old - 1e-6:
                    path[i:j + 1] = reversed(path[i:j + 1])
                    improved = True
    return path


def layout_pixels(segs, x0, x1, y0, y1, pitch, min_d, excluded) -> list[Pt]:
    """Hexagonal lattice at `pitch` where the strokes are wide, then a
    most-interior-first fill so narrow strokes still get a centre line."""
    row_h = pitch * math.sqrt(3) / 2
    best = None
    # Try a few lattice offsets and keep the one that fits the most pixels.
    for ox in (0.0, 0.33, 0.66):
        for oy in (0.0, 0.5):
            pts = []
            r = 0
            y = y0 + oy * row_h
            while y < y1:
                x = x0 + ((0.5 if r % 2 else 0.0) + ox) * pitch
                while x < x1:
                    q = (x, y)
                    if not excluded(q) and inside(q, segs) and dist_to_outline(q, segs) >= min_d:
                        pts.append(q)
                    x += pitch
                y += row_h
                r += 1
            if best is None or len(pts) > len(best):
                best = pts
    leds = list(best or [])
    step = max(2.0, min(5.0, pitch / 6))
    for _, q in candidate_grid(segs, x0, x1, y0, y1, step, min_d):
        if not excluded(q) and all(math.dist(q, o) >= pitch * 0.75 for o in leds):
            leds.append(q)
    return leds


def plan_glyph(p: Params, ch: str) -> tuple[GlyphPlan | None, list[str]]:
    full = glyph_loops(p, ch)
    if not full:
        return None, [f"{ch!r} has no outline in font {p.font!r}; skipped"]
    segs = segments(full)
    xs = [x for loop in full for x, _ in loop]
    ys = [y for loop in full for _, y in loop]
    gx0, gx1 = min(xs), max(xs)
    # Round letters (O, S, C...) overshoot the baseline, so use the real extent.
    gy0, gy1 = min(ys), max(ys)
    m = 1.0  # keep outer tile edges clear of the outline
    xcuts, wx = choose_cuts(gx0 - m, gx1 + m, p.tile_max, axis_edges(full, 0), CUT_EDGE_CLEARANCE)
    ycuts, wy = choose_cuts(gy0 - m, gy1 + m, p.tile_max, axis_edges(full, 1), CUT_EDGE_CLEARANCE)
    warnings = [f"{ch!r}: {w}" for w in wx + wy]

    led_min_d = SOCKET_R + p.wall_t + 0.6
    leds = layout_pixels(segs, gx0, gx1, gy0, gy1, p.led_pitch, led_min_d,
                         lambda q: near_cut(q, xcuts, ycuts, SOCKET_R + 1.5))
    if not leds:
        warnings.append(f"{ch!r}: strokes are too narrow for 12 mm pixel sockets at this size")
    geo = Geodesic(leds, segs, p.wall_t + 1, radius=2.5 * p.led_pitch)
    order = order_chain(geo)
    chain = [leds[i] for i in order]
    # An extension is needed wherever the wire, routed inside the letter,
    # is longer than the string's lead.
    extensions: list[dict] = []
    for k, (i, j) in enumerate(zip(order, order[1:])):
        length = geo.length(i, j)
        if length > p.pixel_lead * 0.9:
            extensions.append({"after_pixel": k + 1, "length_mm": int(math.ceil(length * 1.1 / 10) * 10),
                               "route": [(round(x, 1), round(y, 1)) for x, y in geo.route(i, j)]})
    step = max(2.0, min(5.0, p.led_pitch / 6))
    cands = candidate_grid(segs, gx0, gx1, gy0, gy1, step, 0.0)

    # Rigging: well inside the stroke, clear of pixels, cut lines and each other.
    rig_min_d = PAD_HALF_DIAG + p.wall_t + 0.5
    rig_cands = [pt for d, pt in cands if d >= rig_min_d
                 and not near_cut(pt, xcuts, ycuts, 12.0)
                 and all(math.dist(pt, q) >= SOCKET_R + PAD_HALF_DIAG + 1 for q in leds)]

    tiles: list[TileJob] = []
    ncols, nrows = len(xcuts) - 1, len(ycuts) - 1
    for row in range(nrows):
        for col in range(ncols):
            x_lo, x_hi, y_lo, y_hi = xcuts[col], xcuts[col + 1], ycuts[row], ycuts[row + 1]
            band = glyph_loops(p, ch, rect=(x_lo, x_hi, y_lo, y_hi))
            if not band:
                continue
            bx = [x for loop in band for x, _ in loop]
            by = [y for loop in band for _, y in loop]
            cut_left, cut_right, cut_bottom, cut_top = col > 0, col < ncols - 1, row > 0, row < nrows - 1
            w = max(bx) - min(bx) + (TONGUE_LEN if cut_right else 0)
            h = max(by) - min(by) + (TONGUE_LEN if cut_top else 0)
            fits = ((w <= p.bed_x and h <= p.bed_y) or (w <= p.bed_y and h <= p.bed_x)) and p.depth <= p.bed_z
            in_tile = lambda q: x_lo <= q[0] < x_hi and y_lo <= q[1] < y_hi  # noqa: E731
            want = max(1, round((x_hi - x_lo) / p.rig_pitch))
            rigs: list[Pt] = []
            for pt in rig_cands:
                if len(rigs) >= want:
                    break
                if in_tile(pt) and all(math.dist(pt, q) >= p.rig_pitch * 0.6 for q in rigs):
                    rigs.append(pt)
            islands = len(outer_loops(band))
            tile = TileJob(
                letter=ch, letter_index=-1, col=col, row=row,
                x_lo=round(x_lo, 3), x_hi=round(x_hi, 3), y_lo=round(y_lo, 3), y_hi=round(y_hi, 3),
                cut_left=cut_left, cut_right=cut_right, cut_bottom=cut_bottom, cut_top=cut_top,
                islands=islands,
                ribs=bridge_ribs(band) if p.bridge_islands and islands > 1 else [],
                size_mm=(round(w, 1), round(h, 1)), fits_bed=fits,
                leds=[q for q in leds if in_tile(q)], rigs=rigs,
            )
            tiles.append(tile)
            if not fits:
                warnings.append(f"{ch!r} tile {tile.tag}: {w:.0f}x{h:.0f}x{p.depth:.0f}mm does not fit "
                                f"the {p.bed_x:.0f}x{p.bed_y:.0f}x{p.bed_z:.0f}mm build volume")
    notes: list[str] = []
    base = None
    if p.stand:
        base = plan_base(p, ch, full, gx0, gx1, gy0, p.tile_max, p.tip_angle)
        notes += base.notes
        for sg in base.segments:
            if not sg.fits_bed:
                warnings.append(f"{ch!r} base segment {sg.tag} does not fit the bed")

    # Connection port: just above the plinth, near pixel 1.
    rig_rects = [rect_around(r, 14, 18) for t in tiles for r in t.rigs]
    y_min = gy0 + (base.seat + 8 if base else 10)
    port = choose_port(cands, segs, p.wall_t, y_min, leds, SOCKET_R, rig_rects,
                       xcuts, ycuts, chain[0] if chain else None)
    if port is None:
        warnings.append(f"{ch!r}: no room for the connection port")
        notes.append("No hay lugar para el puerto de conexión: pasar los cables por uno de los agujeros "
                     "de amarre y fijarlos con un precinto.")
    else:
        for t in tiles:
            if t.x_lo <= port[0] < t.x_hi and t.y_lo <= port[1] < t.y_hi:
                t.port = port
        if port[2] == PORT_KIND_GLAND:
            notes.append("Trazos angostos: en lugar de conectores de panel, esta letra tiene un agujero "
                         "de 7 mm para sacar un cable corto con los conectores en la punta.")

    # Tile codes on the back of every carcaza.
    busy = rig_rects + ([port_rect(port)] if port else [])
    for t in tiles:
        t.label_pt = choose_label(t.label, (t.x_lo, t.x_hi, t.y_lo, t.y_hi), cands, segs, busy, leds)

    if extensions:
        warnings.append(f"{ch!r}: {len(extensions)} extension wire(s) needed where the string must "
                        f"run further than its {p.pixel_lead:.0f}mm lead (see assembly_*.svg / manifest)")
        for e in extensions:
            notes.append(f"Extensión de cable de {e['length_mm']} mm entre los píxeles {e['after_pixel']} y "
                         f"{e['after_pixel'] + 1} (recorrido naranja en el mapa de píxeles).")
    for t in tiles:
        if t.ribs:
            notes.append(f"La pieza {t.tag} tiene {t.islands} partes unidas por nervaduras finas en el dorso: "
                         f"no cortarlas, mantienen la pieza en una sola. Su difusor sale en {t.islands} partes "
                         f"separadas: cada una va en su zona.")
        if min(t.size_mm) < 35:
            notes.append(f"La pieza {t.tag} es angosta ({min(t.size_mm):.0f} mm): despegarla de la cama y "
                         f"encastrarla con cuidado.")
    for w_ in wx + wy:
        notes.append(f"Un corte entre piezas quedó cerca de un borde de la letra ({w_.split('cut at ')[-1]}); "
                     f"revisar bien esa costura al encastrar.")
    if len(chain) > 100:
        notes.append(f"{len(chain)} píxeles: además de la entrada, llevar 5 V y GND también al último píxel "
                     f"(inyección) para que no bajen el brillo ni el color al final de la tira.")
    return GlyphPlan(tiles, chain, gx0, gx1, extensions, full, base, port, notes), warnings


@dataclass
class WordPlan:
    tiles: list[TileJob]
    letters: list[dict]          # per placed letter: char, index, first_led, count
    led_map: list[tuple[int, int, int]]  # x_mm, y_mm (word coords), letter slot
    warnings: list[str]
    extension_count: int
    glyphs: dict[str, GlyphPlan] = field(default_factory=dict)


def plan_word(p: Params) -> WordPlan:
    tiles: list[TileJob] = []
    letters: list[dict] = []
    led_map: list[tuple[int, int, int]] = []
    warnings: list[str] = []
    extension_count = 0
    cache: dict[str, GlyphPlan | None] = {}
    x_cursor = 0.0
    prev_base_right = None   # right edge of the previous plinth, word coords
    for li, ch in enumerate(p.text):
        if ch.isspace():
            x_cursor += p.letter_height * 0.35
            prev_base_right = None
            continue
        if ch not in cache:
            cache[ch], w = plan_glyph(p, ch)
            warnings += w
        g = cache[ch]
        if g is None:
            continue
        if g.base and prev_base_right is not None:
            left = x_cursor + g.base.x0 - g.x0
            if left < prev_base_right + BASE_GAP:
                x_cursor += prev_base_right + BASE_GAP - left
        if g.base:
            prev_base_right = x_cursor + g.base.x1 - g.x0
        extension_count += len(g.extensions)
        slot = len(letters)
        n = len(g.chain)
        psu = next((a for a in PSU_SIZES_A if a >= n * PIXEL_MAX_A * 0.75), PSU_SIZES_A[-1])
        letters.append({"char": ch, "index": li, "first_led": len(led_map), "count": n,
                        "output": slot, "psu_a": psu, "max_ma": int(psu * 1000 * PSU_BUDGET),
                        "x_center_mm": round(x_cursor + (g.x1 - g.x0) / 2),
                        "port": g.port, "notes": list(g.notes),
                        "extensions": [{k: e[k] for k in ("after_pixel", "length_mm")} for e in g.extensions]})
        for x, y in g.chain:
            led_map.append((round(x_cursor + x - g.x0), round(y), slot))
        for t in g.tiles:
            tiles.append(TileJob(**{**asdict(t), "letter_index": li}))
        x_cursor += (g.x1 - g.x0) + p.letter_height * LETTER_SPACING
    if len(letters) > MAX_OUTPUTS:
        warnings.append(f"{len(letters)} letters but the controller drives at most {MAX_OUTPUTS} outputs")
    # Controller sits behind the middle of the word; data cable = horizontal run + 1 m slack.
    mid = (x_cursor - p.letter_height * LETTER_SPACING) / 2
    for l in letters:
        l["cable_m"] = math.ceil((abs(l["x_center_mm"] - mid) / 1000 + 1.0) * 2) / 2
    return WordPlan(tiles, letters, led_map, warnings, extension_count,
                    {k: v for k, v in cache.items() if v is not None})


# ---- 3D export -----------------------------------------------------------

def scad_points(pts: list[Pt]) -> str:
    return "[" + ",".join(f"[{x:.2f},{y:.2f}]" for x, y in pts) + "]"


def scad_defines(p: Params, job: TileJob, part: str) -> list[str]:
    defs = {
        "ch": scad_str(job.letter),
        "font": scad_str(p.font),
        "letter_height": p.letter_height,
        "depth": p.depth,
        "wall_t": p.wall_t,
        "back_t": p.back_t,
        "x_lo": job.x_lo, "x_hi": job.x_hi, "y_lo": job.y_lo, "y_hi": job.y_hi,
        "cut_left": str(job.cut_left).lower(),
        "cut_right": str(job.cut_right).lower(),
        "cut_bottom": str(job.cut_bottom).lower(),
        "cut_top": str(job.cut_top).lower(),
        "ribs": "[" + ",".join(f"[{a},{b},{c},{d}]" for a, b, c, d in job.ribs) + "]",
        "joint_clearance": p.joint_clearance,
        "leds": scad_points(job.leds),
        "rigs": scad_points(job.rigs),
        "port": f"[{job.port[0]},{job.port[1]},{job.port[2]}]" if job.port else "[]",
        "label": scad_str(job.label),
        "label_pt": f"[{job.label_pt[0]},{job.label_pt[1]},{job.label_pt[2]}]" if job.label_pt else "[]",
        "part": scad_str(part),
    }
    out: list[str] = []
    for k, v in defs.items():
        out += ["-D", f"{k}={v}"]
    return out


def base_defines(p: Params, g: GlyphPlan, sg: BaseSegment) -> list[str]:
    b = g.base
    dts = lambda ds: "[" + ",".join(f"[{a},{c},{d},{e}]" for a, c, d, e in ds) + "]"  # noqa: E731
    defs = {
        "ch": scad_str(sg.letter), "font": scad_str(p.font),
        "letter_height": p.letter_height, "depth": p.depth,
        "seg": f"[{sg.sx0:.2f},{sg.sx1:.2f},{sg.sz0:.2f},{sg.sz1:.2f}]",
        "gy0": b.gy0, "seat": b.seat, "lip": b.lip, "floor_t": b.floor_t,
        "males": dts(sg.males), "females": dts(sg.females),
        "label": scad_str(sg.label),
        "label_pt": f"[{sg.label_pt[0]},{sg.label_pt[1]}]" if sg.label_pt else "[]",
        "part": scad_str("base"),
    }
    out: list[str] = []
    for k, v in defs.items():
        out += ["-D", f"{k}={v}"]
    return out


def run_openscad(out_path: Path, args: list[str]) -> None:
    out_path.parent.mkdir(parents=True, exist_ok=True)
    cmd = ["openscad", "-o", str(out_path), *args, str(PART_SCAD)]
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0 or not out_path.exists() or out_path.stat().st_size < 100:
        raise RuntimeError(f"openscad failed for {out_path.name}\ncmd: {' '.join(cmd)}\n{result.stderr}")


def safe_name(ch: str, letter_index: int) -> str:
    tag = ch if ch in string.ascii_letters + string.digits else f"u{ord(ch):04x}"
    return f"{letter_index:02d}_{tag}"


def assign_files(w: "WordPlan", fmt: str) -> None:
    """Name every part up front; repeated letters share their files."""
    first_index = {l["char"]: l["index"] for l in reversed(w.letters)}
    for t in w.tiles:
        stem = f"{safe_name(t.letter, first_index[t.letter])}_{t.tag}"
        t.carcaza_file, t.diffusor_file = f"{stem}_carcaza.{fmt}", f"{stem}_diffusor.{fmt}"
    for ch, g in w.glyphs.items():
        for sg in (g.base.segments if g.base else []):
            sg.file = f"{safe_name(ch, first_index[ch])}_base_{sg.tag}.{fmt}"


def build(p: Params, w: "WordPlan", output_dir: Path, jobs: int) -> None:
    tasks: dict[str, list[str]] = {}
    for t in w.tiles:
        tasks.setdefault(t.carcaza_file, scad_defines(p, t, "carcaza"))
        tasks.setdefault(t.diffusor_file, scad_defines(p, t, "diffusor"))
    for g in w.glyphs.values():
        for sg in (g.base.segments if g.base else []):
            tasks.setdefault(sg.file, base_defines(p, g, sg))
    with ThreadPoolExecutor(max_workers=jobs) as ex:
        futures = {ex.submit(run_openscad, output_dir / f, args): f for f, args in tasks.items()}
        for fut in as_completed(futures):
            fut.result()
            print(f"  ok  {futures[fut]}")


# ---- outputs ---------------------------------------------------------------

def internal_joints(tiles: list[TileJob]) -> int:
    present = {(t.letter_index, t.col, t.row) for t in tiles}
    return sum(((t.letter_index, t.col + 1, t.row) in present) + ((t.letter_index, t.col, t.row + 1) in present)
               for t in tiles)


def base_segments(w: WordPlan) -> list[BaseSegment]:
    """One entry per printed base segment (repeated letters repeat them)."""
    return [sg for l in w.letters for sg in (w.glyphs[l["char"]].base.segments if w.glyphs[l["char"]].base else [])]


def totals(p: Params, w: WordPlan) -> dict:
    n = len(w.led_map)
    bases = base_segments(w)
    return {
        "letters": len(w.letters),
        "tiles": len(w.tiles),
        "base_segments": len(bases),
        "printed_parts": len(w.tiles) * 2 + len(bases),
        "pixels": n,
        "screws": 0,
        "snap_joints": internal_joints(w.tiles),
        "base_dovetails": sum(len(sg.males) for sg in bases),
        "zip_tie_points": sum(len(t.rigs) for t in w.tiles),
        "controller_outputs": len(w.letters),
        "power_supplies_5v_a": [l["psu_a"] for l in w.letters],
        "data_cables_m": [l["cable_m"] for l in w.letters],
        "extension_wires_needed": w.extension_count,
        "amps_at_full_white_5v": round(n * PIXEL_MAX_A, 1),
    }


def print_list(w: WordPlan) -> dict[str, int]:
    """How many copies of each STL to print (repeated letters share files)."""
    out: dict[str, int] = {}
    files = [f for t in w.tiles for f in (t.carcaza_file, t.diffusor_file)] + [sg.file for sg in base_segments(w)]
    for f in files:
        if f:
            out[f] = out.get(f, 0) + 1
    return out


def base_summary(b: BasePlan | None) -> dict | None:
    if b is None:
        return None
    return {
        "size_mm": [round(b.x1 - b.x0), round(b.z1 - b.z0), round(b.height)],
        "seat_mm": round(b.seat), "front_lip_mm": round(b.lip),
        "tip_deg": {k: round(v, 1) for k, v in b.tip_deg.items()},
        "cg_height_mm": round(b.cg[1]),
        "segments": [{"tag": sg.tag, "file": sg.file, "dovetails_out": len(sg.males),
                      "dovetails_in": len(sg.females), "fits_bed": sg.fits_bed} for sg in b.segments],
    }


def write_manifest(p: Params, w: WordPlan, output_dir: Path) -> Path:
    manifest = {
        "params": asdict(p),
        "totals": totals(p, w),
        "warnings": w.warnings,
        "letters": [{**l, "base": base_summary(w.glyphs[l["char"]].base)} for l in w.letters],
        "print_list": print_list(w),
        "tiles": [{**asdict(t), "tag": t.tag, "led_count": len(t.leds)} for t in w.tiles],
    }
    path = output_dir / "manifest.json"
    path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False))
    return path


def write_bom(w: WordPlan, output_dir: Path) -> Path:
    path = output_dir / "bom.csv"
    fields = ["letter", "letter_index", "tag", "x_lo", "x_hi", "y_lo", "y_hi", "size_mm",
              "fits_bed", "islands", "bridge_ribs", "led_count", "zip_tie_points",
              "carcaza_file", "diffusor_file"]
    with path.open("w", newline="") as f:
        wr = csv.DictWriter(f, fieldnames=fields, extrasaction="ignore")
        wr.writeheader()
        for t in w.tiles:
            wr.writerow({**asdict(t), "tag": t.tag, "led_count": len(t.leds), "zip_tie_points": len(t.rigs),
                         "bridge_ribs": len(t.ribs)})
    return path


def write_assembly_svg(p: Params, ch: str, g: GlyphPlan, path: Path) -> None:
    """Front view of one letter: tiles, pixel sockets numbered in string order,
    where an extension wire is needed, and zip-tie points."""
    pad = p.letter_height * 0.06
    xs = [x for loop in g.outline for x, _ in loop]
    ys = [y for loop in g.outline for _, y in loop]
    x0, x1, y0, y1 = min(xs) - pad, max(xs) + pad, min(ys) - pad, max(ys) + pad
    fs = max(4.0, p.letter_height / 45)
    sx = lambda x: f"{x - x0:.1f}"          # noqa: E731
    sy = lambda y: f"{y1 - y:.1f}"          # noqa: E731
    out = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {x1 - x0:.0f} {y1 - y0 + 3 * fs:.0f}" '
           f'width="{(x1 - x0) * 2:.0f}" font-family="sans-serif" font-size="{fs:.1f}">',
           '<rect width="100%" height="100%" fill="white"/>']
    d = " ".join("M " + " L ".join(f"{sx(x)},{sy(y)}" for x, y in loop) + " Z" for loop in g.outline)
    out.append(f'<path d="{d}" fill="#eee" stroke="#333" stroke-width="{fs / 6:.2f}" fill-rule="evenodd"/>')
    for t in g.tiles:
        out.append(f'<rect x="{sx(t.x_lo)}" y="{sy(t.y_hi)}" width="{t.x_hi - t.x_lo:.1f}" '
                   f'height="{t.y_hi - t.y_lo:.1f}" fill="none" stroke="#c33" stroke-dasharray="{fs},{fs / 2}" '
                   f'stroke-width="{fs / 5:.2f}"/>')
        out.append(f'<text x="{float(sx(t.x_lo)) + fs / 2:.1f}" y="{float(sy(t.y_hi)) + fs * 1.2:.1f}" '
                   f'fill="#c33">{t.tag}</text>')
        for rx, ry in t.rigs:
            out.append(f'<rect x="{float(sx(rx)) - 7:.1f}" y="{float(sy(ry)) - 9:.1f}" width="14" height="18" '
                       f'fill="none" stroke="black" stroke-width="{fs / 5:.2f}"/>')
    ext = {e["after_pixel"] - 1: e for e in g.extensions}
    for k, (a, b) in enumerate(zip(g.chain, g.chain[1:])):
        if k in ext:
            e = ext[k]
            pts = " ".join(f"{sx(x)},{sy(y)}" for x, y in e["route"])
            out.append(f'<polyline points="{pts}" fill="none" stroke="#e80" stroke-width="{fs / 3:.2f}" '
                       f'stroke-dasharray="{fs / 2},{fs / 3}"/>')
            mx, my = e["route"][len(e["route"]) // 2]
            out.append(f'<text x="{float(sx(mx)) + SOCKET_R + 1:.1f}" y="{sy(my)}" fill="#e80" '
                       f'font-weight="bold">+{e["length_mm"]}mm</text>')
        else:
            out.append(f'<line x1="{sx(a[0])}" y1="{sy(a[1])}" x2="{sx(b[0])}" y2="{sy(b[1])}" '
                       f'stroke="#36c" stroke-width="{fs / 6:.2f}"/>')
    for k, (x, y) in enumerate(g.chain):
        out.append(f'<circle cx="{sx(x)}" cy="{sy(y)}" r="{SOCKET_R:.1f}" fill="white" stroke="#36c" '
                   f'stroke-width="{fs / 5:.2f}"/>')
        out.append(f'<text x="{sx(x)}" y="{float(sy(y)) + fs * 0.35:.1f}" text-anchor="middle" '
                   f'font-size="{fs * 0.9:.1f}">{k + 1}</text>')
    if g.chain:
        x, y = g.chain[0]
        out.append(f'<text x="{float(sx(x)) + SOCKET_R + 2:.1f}" y="{float(sy(y)) + SOCKET_R + fs:.1f}" '
                   f'fill="#36c" font-weight="bold">DATA IN</text>')
    out.append(f'<text x="{fs / 2:.1f}" y="{y1 - y0 + 2.2 * fs:.1f}">"{ch}"  front view  |  '
               f'{len(g.chain)} pixels (string order)  |  red: tiles  |  orange dashed: extension wire  |  '
               f'black: zip-tie points</text>')
    out.append("</svg>")
    path.write_text("\n".join(out))


def c_char(ch: str) -> str:
    return "'\\''" if ch == "'" else "'\\\\'" if ch == "\\" else f"'{ch}'" if ch.isascii() else "'?'"


def write_firmware_config(p: Params, w: WordPlan, path: Path) -> None:
    width = max((x for x, _, _ in w.led_map), default=0)
    lines = [
        "// AUTO-GENERATED by cad/generate.py -- do not edit by hand.",
        f"// Source: text={p.text!r} letter_height={p.letter_height:.0f}mm led_pitch={p.led_pitch:.0f}mm",
        "// LEDs are listed in data-chain order: letters left to right, and inside",
        "// each letter in the order the pixel string visits the sockets.",
        "// Include from exactly one .c file (it defines static arrays).",
        "#pragma once",
        '#include "led_layout.h"',
        "",
        f"#define LED_LETTER_COUNT {len(w.letters)}",
        f"#define LED_TOTAL_COUNT {len(w.led_map)}",
        f"#define LED_WORD_WIDTH_MM {int(width)}",
        f"#define LED_WORD_HEIGHT_MM {int(round(p.letter_height))}",
        "",
        f"#define LED_OUTPUT_COUNT {len(w.letters)}",
        "",
        "// glyph, first LED, LED count, controller output (OUT n), current budget (mA)",
        "static const led_letter_t LED_LETTERS[LED_LETTER_COUNT] = {",
        *[f"    {{ {c_char(l['char'])}, {l['first_led']}, {l['count']}, {l['output']}, {l['max_ma']} }},"
          for l in w.letters],
        "};",
        "",
        "static const led_pos_t LED_MAP[LED_TOTAL_COUNT] = {",
        *[f"    {{ {x}, {y}, {s} }}," for x, y, s in w.led_map],
        "};",
        "",
    ]
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines))


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--text", required=True, help='word/phrase, e.g. "HOLA"')
    ap.add_argument("--letter-height", type=float, required=True, help="mm, 150-1000")
    ap.add_argument("--depth", type=float, help="mm; default clamp(height*0.16, 40, 150)")
    ap.add_argument("--led-pitch", type=float, help="mm between pixels; default ~= depth")
    ap.add_argument("--pixel-lead", type=float, default=100.0, help="wire length between pixels on the string, mm")
    ap.add_argument("--wall-t", type=float, default=2.4)
    ap.add_argument("--back-t", type=float, default=2.0)
    ap.add_argument("--bed-x", type=float, default=220.0, help="print bed X, mm")
    ap.add_argument("--bed-y", type=float, default=220.0, help="print bed Y, mm")
    ap.add_argument("--bed-z", type=float, default=250.0, help="max print height, mm (limits depth)")
    ap.add_argument("--no-stand", action="store_true",
                    help="no freestanding plinth (letters hung or stuck to a wall)")
    ap.add_argument("--tip-angle", type=float, default=15.0,
                    help="minimum tip-over angle of a letter on its plinth, degrees")
    ap.add_argument("--no-bridge-islands", action="store_true",
                    help="leave disjoint islands in a tile as separate pieces")
    ap.add_argument("--font", default="Liberation Sans:style=Bold")
    ap.add_argument("--joint-clearance", type=float, default=0.18, help="from the snap test coupon")
    ap.add_argument("--output-dir", required=True, type=Path)
    ap.add_argument("--jobs", type=int, default=4, help="parallel openscad workers")
    ap.add_argument("--format", default="stl", choices=["stl", "3mf", "off"])
    ap.add_argument("--plan-only", action="store_true", help="analyse and write manifest, skip 3D export")
    ap.add_argument("--firmware-config", type=Path, default=FIRMWARE_CONFIG,
                    help="where to write letters_config.h")
    ap.add_argument("--no-firmware-config", action="store_true")
    ap.add_argument("--no-manual", action="store_true", help="skip the assembly manual")
    ap.add_argument("--chrome", help="Chromium/Chrome binary used to print the manual to PDF")
    args = ap.parse_args()

    if not 150 <= args.letter_height <= 1000:
        print(f"warning: {args.letter_height}mm is outside the 150-1000mm design range", file=sys.stderr)
    depth = args.depth if args.depth is not None else min(max(args.letter_height * 0.16, 40.0), 150.0)
    pitch = args.led_pitch if args.led_pitch is not None else min(max(depth, 25.0), args.pixel_lead / 1.4)
    p = Params(text=args.text, letter_height=args.letter_height, depth=depth, led_pitch=pitch,
               wall_t=args.wall_t, back_t=args.back_t,
               bed_x=args.bed_x, bed_y=args.bed_y, bed_z=args.bed_z,
               bridge_islands=not args.no_bridge_islands, font=args.font,
               stand=not args.no_stand, tip_angle=args.tip_angle,
               joint_clearance=args.joint_clearance, pixel_lead=args.pixel_lead)

    out = args.output_dir
    out.mkdir(parents=True, exist_ok=True)
    print(f"Planning {p.text!r}: {p.letter_height:.0f}mm tall, {depth:.0f}mm deep, "
          f"tiles <= {p.tile_max:.0f}mm, pixel pitch {pitch:.0f}mm")
    w = plan_word(p)
    for t in w.tiles:
        print(f"  {t.letter} {t.tag}: {t.size_mm[0]:4.0f}x{t.size_mm[1]:4.0f}mm  islands {t.islands}  "
              f"pixels {len(t.leds):3d}  zip-tie points {len(t.rigs)}")
    for msg in w.warnings:
        print(f"  WARNING {msg}", file=sys.stderr)

    assign_files(w, args.format)
    if not args.plan_only:
        build(p, w, out, args.jobs)
    write_manifest(p, w, out)
    write_bom(w, out)
    for ch, g in w.glyphs.items():
        tag = ch if ch in string.ascii_letters + string.digits else f"u{ord(ch):04x}"
        write_assembly_svg(p, ch, g, out / f"assembly_{tag}.svg")
    if not args.no_firmware_config:
        write_firmware_config(p, w, args.firmware_config)
        print(f"Updated {args.firmware_config}")
    if not args.no_manual:
        kc = KCONFIG.read_text() if KCONFIG.exists() else ""
        m = re.search(r'config LETTERS_OUT_GPIOS.*?default "([^"]+)"', kc, re.S)
        html_path, pdf_path = write_manual(p, w, totals(p, w), print_list(w), out,
                                           None if args.plan_only else out, m.group(1) if m else None, args.chrome)
        print(f"Manual: {html_path}" + (f" + {pdf_path.name}" if pdf_path else " (no Chrome found for the PDF)"))
    tot = totals(p, w)
    print(f"Done: {tot['tiles']} tiles, {tot['printed_parts']} printed parts, {tot['pixels']} pixels, "
          f"{tot['snap_joints']} snap joints, 0 screws. Output: {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
