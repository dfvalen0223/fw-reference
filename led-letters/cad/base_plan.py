"""Freestanding plinth planning: size, tip-over stability, bed-sized segments
and the dovetails that join them (see base.scad for the geometry)."""
from __future__ import annotations

import math
from dataclasses import dataclass, field

from geom import Loop, Pt, area_centroid, axis_edges, choose_cuts, glyph_loops, x_intervals

# Must match base.scad defaults.
DT_LEN = 8.0
DT_ROOT = 10.0
DT_TIP = 16.0
CLR = 0.5
LABEL_H = 6.0


def clamp(v: float, lo: float, hi: float) -> float:
    return max(lo, min(hi, v))


@dataclass
class BaseSegment:
    letter: str
    col: int
    row: int
    sx0: float
    sx1: float
    sz0: float
    sz1: float
    males: list[tuple[int, float, float, float]] = field(default_factory=list)    # axis, at, pos, ytop
    females: list[tuple[int, float, float, float]] = field(default_factory=list)
    label_pt: Pt | None = None
    fits_bed: bool = True
    file: str = ""

    @property
    def tag(self) -> str:
        return f"B{self.row}{chr(ord('a') + self.col)}"

    @property
    def label(self) -> str:
        return f"{self.letter} {self.tag}"


@dataclass
class BasePlan:
    gy0: float
    seat: float
    lip: float
    floor_t: float
    x0: float
    x1: float
    z0: float
    z1: float
    cg: tuple[float, float]           # centroid x, height of centre of gravity above the floor
    tip_deg: dict[str, float]
    contact_mm: float                 # width of the letter actually touching the seat floor
    segments: list[BaseSegment]
    notes: list[str]
    pockets: list[tuple[float, float]] = field(default_factory=list)   # x-extent of the seat

    @property
    def height(self) -> float:
        return self.seat + CLR + self.floor_t


def plan_base(p, ch: str, full: list[Loop], gx0: float, gx1: float, gy0: float,
              tile_max: float, tip_min_deg: float) -> BasePlan:
    H, depth = p.letter_height, p.depth
    seat = clamp(0.05 * H, 15, 50)
    lip = clamp(0.015 * H, 6, 15)
    floor_t = clamp(0.012 * H, 4, 8)
    notes: list[str] = []

    _, cx, cy = area_centroid(full)
    h = floor_t + CLR + (cy - gy0)
    t = math.tan(math.radians(tip_min_deg))
    front = max(10.0, t * h - depth / 2 - CLR)
    back = max(30.0, t * h - depth / 2 - CLR)  # room for dovetails and the label
    side_l = max(10.0, t * h - (cx - gx0))
    side_r = max(10.0, t * h - (gx1 - cx))
    x0, x1 = gx0 - CLR - side_l, gx1 + CLR + side_r
    z0, z1 = -CLR - back, depth + CLR + front
    zf = depth + CLR
    tip = {
        "adelante": math.degrees(math.atan((z1 - depth / 2) / h)),
        "atrás": math.degrees(math.atan((depth / 2 - z0) / h)),
        "izquierda": math.degrees(math.atan((cx - x0) / h)),
        "derecha": math.degrees(math.atan((x1 - cx) / h)),
    }

    band = glyph_loops(p, ch, rect=(gx0 - 1, gx1 + 1, gy0 - 1, gy0 + seat))
    pockets = [(a - CLR - 0.5, b + CLR + 0.5) for a, b in x_intervals(band)]
    contact = glyph_loops(p, ch, rect=(gx0 - 1, gx1 + 1, gy0 - 1, gy0 + 2))
    contact_mm = sum(b - a for a, b in x_intervals(contact))

    def in_pocket(xa: float, xb: float, za: float, zb: float) -> bool:
        if zb < -CLR - 0.5 or za > zf + 0.5:
            return False
        return any(xa < b and xb > a for a, b in pockets)

    xedges = [v for a, b in pockets for v in (a, b)] + axis_edges(band, 0)
    xcuts, _ = choose_cuts(x0, x1, tile_max - DT_LEN, xedges, clearance=15.0)
    zcuts, _ = choose_cuts(z0, z1, tile_max - DT_LEN, [-CLR, zf], clearance=15.0)

    segs = {(c, r): BaseSegment(ch, c, r, xcuts[c], xcuts[c + 1], zcuts[r], zcuts[r + 1])
            for r in range(len(zcuts) - 1) for c in range(len(xcuts) - 1)}

    def top_for(za: float, zb: float) -> float | None:
        if zb <= zf:
            return gy0 + seat
        if za >= zf:
            return gy0 + lip
        return None  # straddles the step between seat and front ledge

    def pick(lo: float, hi: float, valid) -> list[tuple[float, float]]:
        cands = []
        edge = DT_TIP / 2 + 2
        pos = lo + edge
        while pos <= hi - edge:
            ytop = valid(pos)
            if ytop is not None:
                cands.append((pos, ytop))
            pos += 2
        n = max(1, round((hi - lo) / 110))
        chosen: list[tuple[float, float]] = []
        for k in range(n):
            target = lo + (hi - lo) * (k + 0.5) / n
            for c in sorted(cands, key=lambda c: abs(c[0] - target)):
                if all(abs(c[0] - o[0]) >= 30 for o in chosen):
                    chosen.append(c)
                    break
        return chosen

    for (c, r), sg in segs.items():
        if (c + 1, r) in segs:  # seam at x = sg.sx1
            xc = sg.sx1

            def v_x(pos, xc=xc):
                za, zb = pos - DT_TIP / 2 - 1, pos + DT_TIP / 2 + 1
                if in_pocket(xc - 1, xc + DT_LEN + 1, za, zb):
                    return None
                return top_for(za, zb)
            got = pick(sg.sz0, sg.sz1, v_x)
            for pos, ytop in got:
                d = (0, round(xc, 2), round(pos, 2), round(ytop, 2))
                sg.males.append(d)
                segs[(c + 1, r)].females.append(d)
            if not got:
                notes.append(f"La unión {sg.tag}–{segs[(c + 1, r)].tag} queda bajo la letra y no lleva cola "
                             f"de milano: la propia letra mantiene alineados esos segmentos.")
        if (c, r + 1) in segs:  # seam at z = sg.sz1
            zc = sg.sz1

            def v_z(pos, zc=zc):
                xa, xb = pos - DT_TIP / 2 - 1, pos + DT_TIP / 2 + 1
                if in_pocket(xa, xb, zc - 1, zc + DT_LEN + 1):
                    return None
                return top_for(zc - 1, zc + DT_LEN + 1)
            got = pick(sg.sx0, sg.sx1, v_z)
            for pos, ytop in got:
                d = (1, round(zc, 2), round(pos, 2), round(ytop, 2))
                sg.males.append(d)
                segs[(c, r + 1)].females.append(d)
            if not got:
                notes.append(f"La unión {sg.tag}–{segs[(c, r + 1)].tag} queda bajo la letra y no lleva cola "
                             f"de milano: la propia letra mantiene alineados esos segmentos.")

    # Segment code debossed on the underside (always solid), clear of the
    # female dovetail cavities that run through the full height.
    for sg in segs.values():
        lw = len(sg.label) * LABEL_H * 0.62 + 4
        cx_, cz_ = (sg.sx0 + sg.sx1) / 2, (sg.sz0 + sg.sz1) / 2
        best = None
        for dz in range(0, 200, 4):
            for dx in range(0, 200, 4):
                for x, z in ((cx_ + dx, cz_ + dz), (cx_ - dx, cz_ - dz), (cx_ + dx, cz_ - dz), (cx_ - dx, cz_ + dz)):
                    xa, xb, za, zb = x - lw / 2, x + lw / 2, z - LABEL_H, z + LABEL_H
                    if xa < sg.sx0 + 2 or xb > sg.sx1 - 2 or za < sg.sz0 + 2 or zb > sg.sz1 - 2:
                        continue
                    if any((d[0] == 0 and xb > d[1] - 2 and za < d[2] + DT_TIP and zb > d[2] - DT_TIP) or
                           (d[0] == 1 and zb > d[1] - 2 and xa < d[2] + DT_TIP and xb > d[2] - DT_TIP)
                           for d in sg.females):
                        continue
                    best = (round(x, 1), round(z, 1))
                    break
                if best:
                    break
            if best:
                break
        sg.label_pt = best
        w = sg.sx1 - sg.sx0 + (DT_LEN if any(d[0] == 0 for d in sg.males) else 0)
        d_ = sg.sz1 - sg.sz0 + (DT_LEN if any(d[0] == 1 for d in sg.males) else 0)
        sg.fits_bed = max(w, d_) <= tile_max + 10

    if contact_mm < 0.25 * (gx1 - gx0):
        notes.append(f"La letra apoya sobre una zona angosta ({contact_mm:.0f} mm): el hueco de la base copia "
                     f"su forma y la sostiene, pero hay que asentarla bien hasta el fondo antes de soltarla.")
    if H >= 700:
        notes.append(f"Letra alta ({H:.0f} mm): si hay público cerca o es al aire libre, poner peso sobre "
                     f"la parte trasera de la base (p. ej. una bolsa de arena de 2–3 kg) o atarla a una "
                     f"estructura con precintos por los puntos de amarre.")

    return BasePlan(gy0=gy0, seat=seat, lip=lip, floor_t=floor_t, x0=x0, x1=x1, z0=z0, z1=z1,
                    cg=(cx, h), tip_deg=tip, contact_mm=contact_mm,
                    segments=[segs[k] for k in sorted(segs, key=lambda k: (k[1], k[0]))], notes=notes,
                    pockets=pockets)
