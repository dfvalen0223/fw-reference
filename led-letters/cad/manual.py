"""Assembly manual generator: one self-contained, printable HTML page per
project (plus a PDF when a Chromium/Chrome binary is available), built from
the same plan that produced the STL files, so every code, count, length and
special note in it matches what was printed."""
from __future__ import annotations

import datetime as dt
import html
import math
import re
import shutil
import subprocess
from pathlib import Path

from base_plan import CLR, DT_LEN, DT_ROOT, DT_TIP
from features import PORT_KIND_GLAND, PORT_KIND_H, PORT_SPACING

CHROME_CANDIDATES = ("chromium", "chromium-browser", "google-chrome", "google-chrome-stable", "chrome")
DEFAULT_GPIOS = [4, 5, 6, 7, 15, 16, 17, 18, 8, 9, 10, 11, 12, 13, 14, 21]
SOCKET_R = 7.7
PIXEL_MAX_A = 0.060

CSS = """
:root { --ink:#1d2330; --muted:#5b6475; --line:#c9ced8; --soft:#f3f5f8; --accent:#2f6fdc;
        --warn:#b4520a; --warnbg:#fff4e8; --ok:#1f7a4d; }
* { box-sizing: border-box; }
body { margin: 0; background: #fff; color: var(--ink); font: 14px/1.5 "Liberation Sans", Arial, sans-serif; }
main { max-width: 900px; margin: 0 auto; padding: 24px 20px 60px; }
h1 { font-size: 30px; margin: 0 0 4px; } h2 { font-size: 21px; margin: 34px 0 10px; border-bottom: 2px solid var(--ink); padding-bottom: 4px; }
h3 { font-size: 16px; margin: 22px 0 8px; } p { margin: 6px 0; } .muted { color: var(--muted); }
.cover-sub { color: var(--muted); font-size: 15px; margin-bottom: 18px; }
.kpis { display: grid; grid-template-columns: repeat(auto-fit, minmax(120px, 1fr)); gap: 8px; margin: 16px 0; }
.kpi { background: var(--soft); border-radius: 8px; padding: 10px 12px; }
.kpi b { display: block; font-size: 22px; } .kpi span { color: var(--muted); font-size: 12px; }
table { border-collapse: collapse; width: 100%; margin: 8px 0 12px; font-size: 13px; }
th, td { border-bottom: 1px solid var(--line); padding: 5px 6px; text-align: left; vertical-align: top; }
th { background: var(--soft); font-weight: bold; } td.n, th.n { text-align: right; white-space: nowrap; }
code { background: var(--soft); padding: 0 4px; border-radius: 3px; font-size: 12px; }
ol.steps { padding-left: 0; list-style: none; counter-reset: s; } ol.steps > li { counter-increment: s; position: relative; padding: 4px 0 6px 38px; }
ol.steps > li::before { content: counter(s); position: absolute; left: 0; top: 3px; width: 26px; height: 26px; border-radius: 50%;
  background: var(--ink); color: #fff; font-weight: bold; text-align: center; line-height: 26px; font-size: 13px; }
ul.check { list-style: none; padding-left: 0; } ul.check li::before { content: "\\2610"; margin-right: 8px; font-size: 16px; }
.note { background: var(--warnbg); border-left: 4px solid var(--warn); padding: 8px 12px; margin: 8px 0; border-radius: 4px; }
.note b { color: var(--warn); } .ok { color: var(--ok); }
.fig { margin: 10px 0 14px; } .fig svg { width: 100%; height: auto; max-height: 430px; display: block; }
.fig.side svg { max-height: 320px; } .block { break-inside: avoid; } .fig .cap { color: var(--muted); font-size: 12px; margin-top: 4px; }
.two { display: grid; grid-template-columns: 1fr 1fr; gap: 16px; align-items: start; }
.letter-head { display: flex; flex-wrap: wrap; gap: 6px 14px; align-items: baseline; }
.letter-head .big { font-size: 44px; font-weight: bold; line-height: 1; }
.pill { background: var(--soft); border-radius: 999px; padding: 2px 10px; font-size: 13px; }
.pill.out { background: var(--accent); color: #fff; font-weight: bold; }
.page { break-before: page; }
@media (max-width: 640px) { .two { grid-template-columns: 1fr; } main { padding: 16px; } h1 { font-size: 24px; } }
@page { size: A4; margin: 14mm; }
@media print { main { padding: 0; max-width: none; } h2 { break-after: avoid; } .fig, table, .note { break-inside: avoid; } }
"""


def esc(s) -> str:
    return html.escape(str(s))


def out_name(output: int) -> str:
    return f"OUT {output + 1}"


def gpio_list(kconfig_default: str | None) -> list[int]:
    if kconfig_default:
        return [int(v) for v in re.findall(r"\d+", kconfig_default)]
    return DEFAULT_GPIOS


# ---- SVG helpers -------------------------------------------------------------

class Svg:
    """Tiny SVG builder in model units (mm), y up."""

    def __init__(self, x0: float, x1: float, y0: float, y1: float, px_w: float = 800):
        self.x0, self.x1, self.y0, self.y1 = x0, x1, y0, y1
        self.w, self.h = x1 - x0, y1 - y0
        self.s = px_w / self.w
        self.parts: list[str] = []

    def X(self, x): return (x - self.x0) * self.s  # noqa: E704
    def Y(self, y): return (self.y1 - y) * self.s  # noqa: E704

    def add(self, s: str):
        self.parts.append(s)

    def path(self, loops, **a):
        d = " ".join("M " + " L ".join(f"{self.X(x):.1f},{self.Y(y):.1f}" for x, y in loop) + " Z" for loop in loops)
        self.add(f'<path d="{d}" fill-rule="evenodd" {attrs(a)}/>')

    def rect(self, x0, y0, x1, y1, **a):
        self.add(f'<rect x="{self.X(min(x0, x1)):.1f}" y="{self.Y(max(y0, y1)):.1f}" '
                 f'width="{abs(x1 - x0) * self.s:.1f}" height="{abs(y1 - y0) * self.s:.1f}" {attrs(a)}/>')

    def line(self, x0, y0, x1, y1, **a):
        self.add(f'<line x1="{self.X(x0):.1f}" y1="{self.Y(y0):.1f}" x2="{self.X(x1):.1f}" y2="{self.Y(y1):.1f}" {attrs(a)}/>')

    def poly(self, pts, closed=True, **a):
        tag = "polygon" if closed else "polyline"
        self.add(f'<{tag} points="{" ".join(f"{self.X(x):.1f},{self.Y(y):.1f}" for x, y in pts)}" {attrs(a)}/>')

    def circle(self, x, y, r_px, **a):
        self.add(f'<circle cx="{self.X(x):.1f}" cy="{self.Y(y):.1f}" r="{r_px:.1f}" {attrs(a)}/>')

    def text(self, x, y, s, size=12, anchor="middle", **a):
        self.add(f'<text x="{self.X(x):.1f}" y="{self.Y(y):.1f}" font-size="{size}" text-anchor="{anchor}" '
                 f'dominant-baseline="middle" {attrs(a)}>{esc(s)}</text>')

    def render(self, label: str) -> str:
        return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {self.w * self.s:.0f} {self.h * self.s:.0f}" '
                f'role="img" aria-label="{esc(label)}" font-family="Liberation Sans, Arial, sans-serif">'
                + "".join(self.parts) + "</svg>")


def attrs(a: dict) -> str:
    return " ".join(f'{k.replace("_", "-")}="{v}"' for k, v in a.items())


def fig(svg: str, cap: str, cls: str = "") -> str:
    return f'<div class="fig {cls}">{svg}<div class="cap">{cap}</div></div>'


def bounds(loops, pad):
    xs = [x for l in loops for x, _ in l]
    ys = [y for l in loops for _, y in l]
    return min(xs) - pad, max(xs) + pad, min(ys) - pad, max(ys) + pad


def tile_order(tiles) -> list:
    return sorted(tiles, key=lambda t: (t.row, t.col))


def svg_tiles(g, p) -> str:
    x0, x1, y0, y1 = bounds(g.outline, p.letter_height * 0.05)
    s = Svg(x0, x1, y0, y1, 520)
    s.path(g.outline, fill="#eef1f6", stroke="#1d2330", stroke_width="1.5")
    for k, t in enumerate(tile_order(g.tiles), 1):
        s.rect(t.x_lo, t.y_lo, t.x_hi, t.y_hi, fill="none", stroke="#c33", stroke_width="1.5", stroke_dasharray="7,4")
        cx, cy = (t.x_lo + t.x_hi) / 2, (t.y_lo + t.y_hi) / 2
        s.circle(cx, cy, 17, fill="#1d2330")
        s.text(cx, cy, k, size=17, fill="#fff", font_weight="bold")
        s.text(cx, cy - 28 / s.s, t.tag, size=15, fill="#c33", font_weight="bold")
    return s.render("piezas de la letra y orden de encastre")


def svg_pixels(g, p) -> str:
    x0, x1, y0, y1 = bounds(g.outline, p.letter_height * 0.05)
    s = Svg(x0, x1, y0, y1, 520)
    s.path(g.outline, fill="#f7f8fb", stroke="#1d2330", stroke_width="1.5")
    for t in g.tiles:
        s.rect(t.x_lo, t.y_lo, t.x_hi, t.y_hi, fill="none", stroke="#e3a4a4", stroke_width="1", stroke_dasharray="5,4")
        for rx, ry in t.rigs:
            s.rect(rx - 7, ry - 9, rx + 7, ry + 9, fill="none", stroke="#555", stroke_width="1.2")
    ext = {e["after_pixel"] - 1: e for e in g.extensions}
    for k, (a, b) in enumerate(zip(g.chain, g.chain[1:])):
        if k in ext:
            e = ext[k]
            s.poly(e["route"], closed=False, fill="none", stroke="#e07b00", stroke_width="3", stroke_dasharray="6,4")
            mx, my = e["route"][len(e["route"]) // 2]
            s.text(mx, my, f"+{e['length_mm']} mm", size=11, fill="#b4520a", font_weight="bold", anchor="start",
                   dx="10")
        else:
            s.line(*a, *b, stroke="#2f6fdc", stroke_width="1.5")
    r_px = max(7.0, SOCKET_R * s.s)
    for k, (x, y) in enumerate(g.chain, 1):
        s.circle(x, y, r_px, fill="#fff", stroke="#2f6fdc", stroke_width="1.5")
        s.text(x, y, k, size=max(8, min(11, r_px * 1.1)))
    if g.port:
        px, py, kind = g.port
        if kind == PORT_KIND_GLAND:
            s.circle(px, py, max(5, 3.5 * s.s), fill="#1f7a4d")
            s.text(px, py - 12 / s.s - 8, "CABLE", size=10, fill="#1f7a4d", font_weight="bold")
        else:
            offs = [(-PORT_SPACING / 2, 0), (PORT_SPACING / 2, 0)] if kind == PORT_KIND_H else [(0, PORT_SPACING / 2), (0, -PORT_SPACING / 2)]
            for (dx, dy), lab in zip(offs, ("5V", "DATOS")):
                s.circle(px + dx, py + dy, max(5, 6 * s.s), fill="#1f7a4d")
                s.text(px + dx + (0 if kind == PORT_KIND_H else 14 / s.s + 12),
                       py + dy - (12 / s.s + 8 if kind == PORT_KIND_H else 0), lab, size=9, fill="#1f7a4d",
                       font_weight="bold", anchor="middle" if kind == PORT_KIND_H else "start")
    return s.render("mapa de píxeles y puerto de conexión")


def svg_base_top(g, p) -> str:
    b = g.base
    pad = 18
    s = Svg(b.x0 - pad, b.x1 + pad, -(b.z1 + pad + 26), -(b.z0 - pad), 520)
    Z = lambda z: -z  # noqa: E731  front (large z) at the bottom of the drawing
    s.rect(b.x0, Z(b.z0), b.x1, Z(b.z1), fill="#eef1f6", stroke="#1d2330", stroke_width="1.5")
    s.rect(b.x0, Z(p.depth + CLR), b.x1, Z(b.z1), fill="#dfe4ec", stroke="none")
    for a, c in b.pockets:
        s.rect(a, Z(-CLR), c, Z(p.depth + CLR), fill="#fff", stroke="#2f6fdc", stroke_width="1.2")
    s.rect(g.x0, Z(0), g.x1, Z(p.depth), fill="none", stroke="#2f6fdc", stroke_width="1", stroke_dasharray="4,3")
    for sg in b.segments:
        s.rect(sg.sx0, Z(sg.sz0), sg.sx1, Z(sg.sz1), fill="none", stroke="#1d2330", stroke_width="1.2")
        s.text((sg.sx0 + sg.sx1) / 2, Z(sg.sz0) - 12 / s.s, sg.tag, size=15, font_weight="bold")
        for d in sg.males:
            axis, at, pos = d[0], d[1], d[2]
            if axis == 0:
                pts = [(at, pos - DT_ROOT / 2), (at + DT_LEN, pos - DT_TIP / 2), (at + DT_LEN, pos + DT_TIP / 2), (at, pos + DT_ROOT / 2)]
                pts = [(x, Z(z)) for x, z in pts]
            else:
                pts = [(pos - DT_ROOT / 2, at), (pos - DT_TIP / 2, at + DT_LEN), (pos + DT_TIP / 2, at + DT_LEN), (pos + DT_ROOT / 2, at)]
                pts = [(x, Z(z)) for x, z in pts]
            s.poly(pts, fill="#b4520a", stroke="none")
    s.text((b.x0 + b.x1) / 2, Z(b.z1) - 14 / s.s, "▼ FRENTE (lado del público) ▼", size=14, fill="#5b6475")
    return s.render("vista superior de la base")


def svg_side(g, p) -> str:
    b = g.base
    yb = b.gy0 - CLR - b.floor_t
    gy1 = max(y for l in g.outline for _, y in l)
    pad = p.letter_height * 0.08
    zl, zr, yl, yh = b.z0 - pad, b.z1 + pad, yb - pad * 1.3, gy1 + pad * 0.5
    min_w = (yh - yl) * 280 / 320
    if zr - zl < min_w:
        grow = (min_w - (zr - zl)) / 2
        zl, zr = zl - grow, zr + grow
    s = Svg(zl, zr, yl, yh, 320 * (zr - zl) / (yh - yl))
    s.line(zl, yb, zr, yb, stroke="#1d2330", stroke_width="2")
    zf = p.depth + CLR
    s.poly([(b.z0, yb), (b.z0, b.gy0 + b.seat), (-CLR, b.gy0 + b.seat), (-CLR, b.gy0 - CLR), (zf, b.gy0 - CLR),
            (zf, b.gy0 + b.lip), (b.z1, b.gy0 + b.lip), (b.z1, yb)], fill="#dfe4ec", stroke="#1d2330", stroke_width="1.2")
    s.rect(0, b.gy0, p.depth, gy1, fill="#eef1f6", stroke="#1d2330", stroke_width="1.2")
    s.line(p.depth, b.gy0, p.depth, gy1, stroke="#2f6fdc", stroke_width="3")
    cgy = yb + b.cg[1]
    for zz in (b.z0, b.z1):
        s.line(p.depth / 2, cgy, zz, yb, stroke="#b4520a", stroke_width="1", stroke_dasharray="4,3")
    s.circle(p.depth / 2, cgy, 5, fill="#b4520a")
    s.text(p.depth / 2 - 9 / s.s, cgy, "CG", size=12, fill="#b4520a", anchor="end", font_weight="bold")
    s.text(b.z1, yb - 14 / s.s, f"{b.tip_deg['adelante']:.0f}°", size=13, fill="#b4520a", font_weight="bold")
    s.text(b.z0, yb - 14 / s.s, f"{b.tip_deg['atrás']:.0f}°", size=13, fill="#b4520a", font_weight="bold")
    s.text(p.depth + 6 / s.s, gy1 - 14 / s.s, "frente →", size=12, fill="#2f6fdc", anchor="start")
    return s.render("vista lateral: letra en su base y centro de gravedad")


def letter_offset(l, g) -> float:
    return l["x_center_mm"] - (g.x1 - g.x0) / 2 - g.x0


def svg_word(w, p) -> str:
    items = [(l, w.glyphs[l["char"]]) for l in w.letters]
    xs, ys = [], []
    for l, g in items:
        off = letter_offset(l, g)
        xs += [off + (g.base.x0 if g.base else g.x0), off + (g.base.x1 if g.base else g.x1)]
        ys += [y for lp in g.outline for _, y in lp]
    base_h = max((g.base.height for _, g in items if g.base), default=0)
    s = Svg(min(xs) - 30, max(xs) + 30, min(ys) - base_h - p.letter_height * 0.22, max(ys) + 30, 860)
    for l, g in items:
        off = letter_offset(l, g)
        b = g.base
        if b:
            yb = b.gy0 - CLR - b.floor_t
            s.rect(off + b.x0, yb, off + b.x1, b.gy0 + b.seat, fill="#c9d1de", stroke="#1d2330", stroke_width="1")
        s.path([[(x + off, y) for x, y in lp] for lp in g.outline], fill="#fff8e0", stroke="#1d2330", stroke_width="1.5")
        if b:
            s.rect(off + b.x0, yb, off + b.x1, b.gy0 + b.lip, fill="#9aa6b8", stroke="#1d2330", stroke_width="1")
        ybot = (b.gy0 - CLR - b.floor_t) if b else min(y for lp in g.outline for _, y in lp)
        s.text(l["x_center_mm"], ybot - p.letter_height * 0.07, out_name(l["output"]), size=13, font_weight="bold", fill="#2f6fdc")
        s.text(l["x_center_mm"], ybot - p.letter_height * 0.15, f"{l['count']} px · {l['psu_a']} A", size=11, fill="#5b6475")
    return s.render("vista frontal del letrero")


def svg_wiring(w) -> str:
    n = len(w.letters)
    W = max(640, 140 * n + 40)
    parts = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} 300" font-family="Liberation Sans, Arial, sans-serif" '
             f'role="img" aria-label="conexionado">']
    cx = W / 2
    parts.append(f'<rect x="{cx - 110}" y="232" width="220" height="52" rx="8" fill="#1d2330"/>'
                 f'<text x="{cx}" y="252" fill="#fff" font-size="13" text-anchor="middle" font-weight="bold">Controlador ESP32-S3</text>'
                 f'<text x="{cx}" y="270" fill="#c9ced8" font-size="11" text-anchor="middle">USB-C 5 V · 74AHCT245</text>')
    for i, l in enumerate(w.letters):
        x = 20 + (W - 40) * (i + 0.5) / n
        parts.append(f'<rect x="{x - 52}" y="12" width="104" height="30" rx="6" fill="#fff4e8" stroke="#b4520a"/>'
                     f'<text x="{x}" y="31" font-size="11" text-anchor="middle">Fuente 5 V {l["psu_a"]} A</text>'
                     f'<line x1="{x}" y1="42" x2="{x}" y2="80" stroke="#b4520a" stroke-width="3"/>'
                     f'<rect x="{x - 40}" y="80" width="80" height="80" rx="6" fill="#eef1f6" stroke="#1d2330" stroke-width="1.5"/>'
                     f'<text x="{x}" y="128" font-size="34" text-anchor="middle" font-weight="bold">{esc(l["char"])}</text>'
                     f'<path d="M {x} 160 C {x} 200, {cx + (i - (n - 1) / 2) * 12} 200, {cx + (i - (n - 1) / 2) * 12} 232" '
                     f'fill="none" stroke="#2f6fdc" stroke-width="2.5"/>'
                     f'<text x="{x}" y="180" font-size="11" fill="#2f6fdc" font-weight="bold" text-anchor="middle" '
                     f'paint-order="stroke" stroke="#fff" stroke-width="5">{out_name(l["output"])} · {l["cable_m"]} m</text>')
    parts.append('<text x="10" y="296" font-size="10" fill="#5b6475">naranja: 5 V desde la fuente de cada letra (jack DC) · '
                 'azul: datos + GND desde el controlador (GX12)</text></svg>')
    return "".join(parts)


# ---- content -----------------------------------------------------------------

def tile_steps(g) -> list[str]:
    tiles = tile_order(g.tiles)
    by_row: dict[int, list] = {}
    for t in tiles:
        by_row.setdefault(t.row, []).append(t)
    steps = []
    rows = sorted(by_row)
    for r in rows:
        row = by_row[r]
        if len(row) == 1:
            steps.append(f"Fila {r + 1}: pieza <code>{row[0].tag}</code> sola.")
        else:
            seq = " → ".join(f"<code>{t.tag}</code>" for t in row)
            steps.append(f"Fila {r + 1}: unir {seq} de izquierda a derecha. La lengüeta del borde derecho de cada "
                         f"pieza entra en la ranura de la siguiente; presionar a lo largo de toda la costura hasta "
                         f"escuchar el clic.")
        if r != rows[0]:
            steps.append(f"Apoyar la fila {r + 1} sobre la fila {r}: las lengüetas del borde superior de la fila de "
                         f"abajo entran en las ranuras de la de arriba. Presionar de a tramos hasta el clic.")
    return steps


def base_steps(g) -> list[str]:
    b = g.base
    rows: dict[int, list] = {}
    for sg in b.segments:
        rows.setdefault(sg.row, []).append(sg)
    steps = []
    for r in sorted(rows):
        row = sorted(rows[r], key=lambda s: s.col)
        if len(row) > 1:
            steps.append(f"Fila {'trasera' if r == 0 else r + 1} de la base: apoyar <code>{row[0].tag}</code> en el piso y "
                         + ", ".join(f"deslizar <code>{sg.tag}</code> desde arriba sobre las colas de milano de la anterior"
                                     for sg in row[1:]) + ".")
        else:
            steps.append(f"Apoyar <code>{row[0].tag}</code> en el piso.")
        if r > 0:
            steps.append("Bajar esta fila sobre las colas de milano del borde delantero de la fila anterior.")
    steps.append("Bajar la letra armada dentro del hueco de la base (el hueco tiene la forma de la parte de abajo "
                 "de la letra) hasta que apoye en el fondo. Queda trabada sin tornillos; para sacarla, levantarla.")
    return steps


def filament_g(stl: Path, infill: float) -> float | None:
    """Rough filament mass from the mesh volume (ASCII STL)."""
    try:
        txt = stl.read_text()
    except OSError:
        return None
    v = [tuple(map(float, m)) for m in re.findall(r"vertex\s+(\S+)\s+(\S+)\s+(\S+)", txt)]
    vol = 0.0
    for i in range(0, len(v) - 2, 3):
        (ax, ay, az), (bx, by, bz), (cx, cy, cz) = v[i], v[i + 1], v[i + 2]
        vol += (ax * (by * cz - bz * cy) - ay * (bx * cz - bz * cx) + az * (bx * cy - by * cx)) / 6
    return abs(vol) / 1000 * 1.27 * infill  # cm3 * PETG density * fill factor


def write_manual(p, w, totals: dict, print_list: dict, out_dir: Path, stl_dir: Path | None,
                 gpio_default: str | None = None, chrome: str | None = None) -> tuple[Path, Path | None]:
    gp = gpio_list(gpio_default)
    today = dt.date.today().strftime("%d/%m/%Y")
    H = p.letter_height
    h: list[str] = []
    a = h.append
    a(f"<!doctype html><html lang='es'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
      f"<title>Manual de ensamble {esc(p.text)}</title><style>{CSS}</style></head><body><main>")

    # Cover
    a(f"<h1>Manual de ensamble — «{esc(p.text)}»</h1>")
    a(f"<div class='cover-sub'>Letras de {H:.0f} mm de alto · {p.depth:.0f} mm de profundidad · generado el {today} "
      f"con <code>cad/generate.py</code>. Todos los códigos, cantidades y largos de este manual corresponden a los "
      f"archivos STL generados en la misma corrida.</div>")
    a(fig(svg_word(w, p), "Vista frontal. Cada letra lleva su propia fuente de 5 V y un cable de datos a su salida del controlador."))
    a("<div class='kpis'>" + "".join(f"<div class='kpi'><b>{v}</b><span>{k}</span></div>" for k, v in [
        ("letras", totals["letters"]), ("piezas impresas", totals["printed_parts"]), ("píxeles WS2812B", totals["pixels"]),
        ("uniones a presión", totals["snap_joints"]), ("colas de milano", totals.get("base_dovetails", 0)),
        ("tornillos", 0)]) + "</div>")
    all_notes = [(l, n) for l in w.letters for n in l["notes"]]
    if all_notes:
        a(f"<div class='note'><b>{len(all_notes)} notas especiales</b> en este letrero: están en la sección de cada "
          f"letra, resaltadas como esta.</div>")
    else:
        a("<p class='ok'>Este letrero no tiene notas especiales.</p>")

    # 1. Materials
    a("<h2>1. Materiales</h2><h3>Piezas impresas</h3>")
    a("<table><tr><th>Archivo</th><th class='n'>Cant.</th><th>Material</th><th class='n'>Filamento aprox.</th></tr>")
    tot_g = 0.0
    for f, n in sorted(print_list.items()):
        kind = "base" if "_base_" in f else ("difusor" if "diffusor" in f else "carcaza")
        mat = {"carcaza": "PETG opaco", "difusor": "PETG natural/blanco translúcido", "base": "PETG o PLA, 10 % relleno"}[kind]
        gr = filament_g(stl_dir / f, 0.25 if kind == "base" else 0.9) if stl_dir else None
        if gr:
            tot_g += gr * n
        a(f"<tr><td><code>{esc(f)}</code></td><td class='n'>{n}</td><td>{mat}</td>"
          f"<td class='n'>{f'{gr * n:.0f} g' if gr else '—'}</td></tr>")
    a(f"<tr><th>Total</th><th class='n'>{sum(print_list.values())}</th><th></th>"
      f"<th class='n'>{f'{tot_g / 1000:.1f} kg' if tot_g else '—'}</th></tr></table>")

    a("<h3>Electrónica y cables</h3><table><tr><th>Ítem</th><th class='n'>Cant.</th><th>Detalle</th></tr>")
    rows = [
        ("Píxeles WS2812B 12 mm en tira-cable, 5 V", sum(math.ceil(l["count"] * 1.1) for l in w.letters),
         f"{totals['pixels']} + 10 % de repuesto; cable entre píxeles ≥ {p.pixel_lead:.0f} mm"),
        ("Fuente enchufable 5 V con plug 5,5 × 2,1 mm", len(w.letters),
         ", ".join(f"{esc(l['char'])}: {l['psu_a']} A" for l in w.letters)),
        ("Jack DC de panel 5,5 × 2,1 mm (agujero 11 mm)", sum(1 for l in w.letters if l["port"] and l["port"][2] != PORT_KIND_GLAND), "uno por letra"),
        ("Conector GX12 3 pines, par panel + aéreo", len(w.letters), "datos: pin 1 DATOS, pin 2 GND"),
        ("Cable de datos 2 × 0,5 mm² (o micrófono blindado)", len(w.letters),
         ", ".join(f"{esc(l['char'])}: {l['cable_m']} m" for l in w.letters)),
        ("Capacitor 1000 µF 10 V", len(w.letters), "en el jack DC de cada letra"),
        ("ESP32-S3 DevKitC-1 (N8 o mayor)", 1, "controlador"),
        ("74AHCT245 + resistencias 100 Ω", 1 if len(w.letters) <= 8 else 2,
         f"{len(w.letters)} resistencias, una por salida"),
        ("Cargador USB-C 5 V 1 A", 1, "alimenta el controlador"),
    ]
    ext_total = sum(e["length_mm"] for l in w.letters for e in l["extensions"])
    if ext_total:
        rows.append(("Cable de 3 hilos para extensiones", totals["extension_wires_needed"], f"{ext_total / 1000:.1f} m en total (ver cada letra)"))
    for name, n, det in rows:
        a(f"<tr><td>{name}</td><td class='n'>{n}</td><td>{det}</td></tr>")
    a("</table><h3>Herramientas</h3><p>Ninguna para el armado mecánico. Soldador, pinza y tester sólo para cablear "
      "los conectores de cada letra (una vez). Destornillador plano para desarmar uniones haciendo palanca.</p>")

    # 2. Printing
    a("<h2>2. Impresión</h2><ol class='steps'>")
    a(f"<li>Imprimir el <b>cupón de calibración</b> de la unión a presión con el mismo material y probar: tiene que "
      f"entrar con presión de la mano, hacer clic y salir haciendo palanca. Holgura usada en esta corrida: "
      f"<code>{p.joint_clearance} mm</code>; si no calza, regenerar con otro <code>--joint-clearance</code>.</li>")
    a("<li><b>Carcazas</b>: apoyadas sobre el dorso, sin soportes, 3 perímetros, 15–20 % de relleno.</li>"
      "<li><b>Difusores</b>: apoyados sobre la cara, sin soportes, 2 perímetros.</li>"
      "<li><b>Bases</b>: apoyadas sobre la cara inferior, sin soportes, 3 perímetros, 10 % de relleno gyroid.</li>"
      "<li>Cada pieza sale con su <b>código grabado</b> en el dorso (carcazas, p. ej. <code>H r0c1</code>) o en la cara "
      "inferior (bases, p. ej. <code>H B0a</code>). Los códigos de este manual son esos.</li></ol>")

    # 3. Letters
    for li, l in enumerate(w.letters):
        g = w.glyphs[l["char"]]
        a("<section class='page'>" + ("<h2>3. Armado de cada letra</h2>" if li == 0 else "")
          + f"<div class='letter-head'><span class='big'>{esc(l['char'])}</span>"
          f"<span class='pill out'>{out_name(l['output'])}</span><span class='pill'>{l['count']} píxeles</span>"
          f"<span class='pill'>fuente 5 V {l['psu_a']} A</span><span class='pill'>cable de datos {l['cable_m']} m</span>"
          f"<span class='pill'>{len(g.tiles)} piezas + {len(g.base.segments) if g.base else 0} de base</span></div>")
        if l["notes"]:
            for n in l["notes"]:
                a(f"<div class='note'><b>Nota especial:</b> {esc(n)}</div>")
        else:
            a("<p class='ok'>Sin notas especiales para esta letra.</p>")

        a("<div class='block'><h3>a. Encastrar las piezas</h3><div class='two'>")
        a(fig(svg_tiles(g, p), "Vista frontal. Número = orden; código rojo = el grabado en el dorso de la carcaza."))
        a("<div><ol class='steps'>" + "".join(f"<li>{st}</li>" for st in tile_steps(g)) + "</ol>"
          "<p class='muted'>Armar boca abajo sobre una mesa. En las esquinas donde se cruzan dos cortes no hay unión: "
          "es normal una pequeña separación.</p></div></div></div>")

        a("<div class='block'><h3>b. Colocar los píxeles y el puerto</h3><div class='two'>")
        a(fig(svg_pixels(g, p), "Vista frontal. Círculos: zócalos en el orden de la tira. Naranja: extensión. "
                                "Verde: puerto de conexión (se ve desde atrás)."))
        steps = [
            "Dar vuelta la letra (cara abierta hacia arriba).",
            "Empezando por el zócalo <b>1</b>, empujar cada píxel en su zócalo siguiendo los números. Los cables "
            "salen por cualquiera de las 4 ranuras del zócalo.",
        ]
        for e in l["extensions"]:
            steps.append(f"Entre los píxeles {e['after_pixel']} y {e['after_pixel'] + 1}: intercalar una extensión de "
                         f"<b>{e['length_mm']} mm</b> (3 hilos) siguiendo el recorrido naranja.")
        if l["port"] and l["port"][2] != PORT_KIND_GLAND:
            steps.append("Montar el <b>jack DC</b> en el agujero marcado <code>5V</code> y el <b>GX12</b> en el marcado "
                         "<code>DATOS</code> (tuerca por dentro). Soldar: jack centro (+) → 5 V del píxel 1, jack "
                         "exterior → GND; GX12 pin 1 → DATOS del píxel 1, pin 2 → GND. Soldar el capacitor de "
                         "1000 µF entre 5 V y GND en el jack (respetar la polaridad).")
        elif l["port"]:
            steps.append("Pasar un cable corto de 4 hilos por el agujero de 7 mm y terminarlo afuera en un plug DC "
                         "hembra y un GX12 aéreo; adentro, al 5 V / GND / DATOS del píxel 1.")
        if l["count"] > 100:
            steps.append("Llevar también 5 V y GND desde el jack al <b>último píxel</b> (inyección).")
        steps.append("Probar antes de cerrar (sección 5, efecto <code>chain_test</code>).")
        steps.append("Cerrar con los difusores: cada uno lleva el mismo código que su carcaza; presionar el borde "
                     "hasta el clic.")
        a("<div><ol class='steps'>" + "".join(f"<li>{st}</li>" for st in steps) + "</ol></div></div></div>")

        if g.base:
            b = g.base
            a("<div class='block'><h3>c. Base</h3><div class='two'>")
            a(fig(svg_base_top(g, p), f"Vista superior ({b.x1 - b.x0:.0f} × {b.z1 - b.z0:.0f} × {b.height:.0f} mm). "
                                     "Azul: hueco para la letra. Marrón: colas de milano."))
            a(fig(svg_side(g, p), f"Vista lateral. Ángulo de vuelco: adelante {b.tip_deg['adelante']:.0f}°, atrás "
                                  f"{b.tip_deg['atrás']:.0f}°, costados {min(b.tip_deg['izquierda'], b.tip_deg['derecha']):.0f}° "
                                  f"(mínimo pedido {p.tip_angle:.0f}°).", "side"))
            a("</div><ol class='steps'>" + "".join(f"<li>{st}</li>" for st in base_steps(g)) + "</ol></div>")

        a("<div class='block'><h3>Piezas de esta letra</h3><table><tr><th>Código</th><th>Carcaza</th><th>Difusor</th>"
          "<th class='n'>Píxeles</th><th>Detalles</th></tr>")
        placed = [t for t in w.tiles if t.letter_index == l["index"]]
        for t in tile_order(placed):
            det = []
            if t.port:
                det.append("puerto de conexión")
            if t.ribs:
                det.append(f"{t.islands} partes unidas por nervaduras")
            if t.rigs:
                det.append(f"{len(t.rigs)} amarre(s)")
            a(f"<tr><td><b>{t.tag}</b></td><td><code>{esc(t.carcaza_file or '—')}</code></td>"
              f"<td><code>{esc(t.diffusor_file or '—')}</code></td><td class='n'>{len(t.leds)}</td><td>{', '.join(det)}</td></tr>")
        a("</table>")
        if g.base:
            a("<table><tr><th>Base</th><th>Archivo</th><th class='n'>Colas de milano (salen / entran)</th></tr>")
            for sg in g.base.segments:
                a(f"<tr><td><b>{sg.tag}</b></td><td><code>{esc(sg.file or '—')}</code></td>"
                  f"<td class='n'>{len(sg.males)} / {len(sg.females)}</td></tr>")
            a("</table>")
        a("</div></section>")

    # 4. Electronics
    a("<h2 class='page'>4. Controlador y conexionado</h2>")
    a(fig(svg_wiring(w), "Cada letra se enchufa a su propia fuente; el controlador sólo manda datos."))
    a("<table><tr><th>Salida</th><th>GPIO</th><th>Letra</th><th class='n'>Píxeles</th><th class='n'>Límite de corriente</th>"
      "<th class='n'>Cable</th></tr>")
    for l in w.letters:
        gpio = gp[l["output"]] if l["output"] < len(gp) else "—"
        a(f"<tr><td><b>{out_name(l['output'])}</b></td><td>{gpio}</td><td>{esc(l['char'])}</td><td class='n'>{l['count']}</td>"
          f"<td class='n'>{l['max_ma'] / 1000:.1f} A</td><td class='n'>{l['cable_m']} m</td></tr>")
    a("</table>")
    bus = 8 if len(w.letters) <= 8 else 16
    a(f"<ol class='steps'><li>Cablear el 74AHCT245: VCC y DIR a 5 V, GND y OE a GND. Entradas A1…A{min(8, len(w.letters))} "
      f"a los GPIO de la tabla; cada salida B por una resistencia de 100 Ω al pin 1 de su GX12 de panel "
      f"(<code>OUT n</code>); pin 2 de todos los GX12 a GND.</li>"
      f"<li>El bus paralelo usa {bus} líneas: los GPIO de salidas sin letra quedan en bajo, no conectarles nada. "
      f"GPIO 2 (reloj) y GPIO 1 (D/C) se configuran pero quedan sin conectar.</li>"
      f"<li>Compilar y grabar el firmware (<code>firmware/README.md</code>). <code>firmware/main/letters_config.h</code> "
      f"ya contiene el mapa de esta corrida y el límite de corriente de cada letra.</li></ol>")

    # 5. On site
    a("<h2>5. Montaje en el lugar</h2><ul class='check'>")
    order = " – ".join(f"{esc(l['char'])} ({out_name(l['output'])})" for l in w.letters)
    a(f"<li>Ubicar las bases de izquierda a derecha: {order}. Separación entre centros de letras según la vista frontal "
      f"(espacio entre letras ≈ {H * 0.12:.0f} mm).</li>")
    a("<li>Armar cada base (si tiene segmentos) y bajar la letra dentro del hueco.</li>"
      "<li>Enchufar la fuente de cada letra en su jack <code>5V</code>.</li>"
      "<li>Conectar el cable de datos de cada letra entre su GX12 <code>DATOS</code> y la salida indicada del "
      "controlador.</li>"
      "<li>Encender el controlador. Desde el celular: red <code>LETRAS-LED</code>, abrir <code>http://192.168.4.1</code>.</li>"
      "<li>Efecto <code>letters</code>: cada letra de un color distinto. Si una letra no enciende o tiene el color de "
      "otra, revisar su cable de datos / salida.</li>"
      "<li>Efecto <code>chain_test</code>: un píxel por vez recorre cada letra en el orden del mapa. Si salta, hay un "
      "píxel en el zócalo equivocado.</li>")
    if any("Letra alta" in n for _, n in all_notes):
        a("<li>Colocar el lastre en las letras altas (ver notas especiales).</li>")
    a("<li>Elegir el efecto final. La configuración queda guardada al apagar.</li></ul>")

    a("<h2>6. Desmontaje y transporte</h2><ol class='steps'><li>Desenchufar fuentes y cables de datos.</li>"
      "<li>Levantar cada letra de su base: sale sin herramientas.</li>"
      "<li>Las letras se transportan armadas (la tira de píxeles cruza las costuras). Las bases se desarman "
      "levantando cada segmento: las colas de milano deslizan hacia arriba.</li></ol>")

    # Extension wires are already spelled out per letter; list only the rest.
    other = [x for x in w.warnings if "extension wire" not in x]
    if other:
        a("<h2>Anexo: avisos técnicos del generador</h2><ul>" + "".join(f"<li class='muted'>{esc(x)}</li>" for x in other) + "</ul>")
    a("</main></body></html>")

    html_path = out_dir / "manual_ensamble.html"
    html_path.write_text("".join(h), encoding="utf-8")
    pdf_path = None
    exe = chrome or next((shutil.which(c) for c in CHROME_CANDIDATES if shutil.which(c)), None)
    if exe:
        pdf = out_dir / "manual_ensamble.pdf"
        r = subprocess.run([exe, "--headless", "--no-sandbox", "--disable-gpu", "--no-pdf-header-footer",
                            f"--print-to-pdf={pdf}", html_path.resolve().as_uri()], capture_output=True, text=True, timeout=180)
        if r.returncode == 0 and pdf.exists():
            pdf_path = pdf
    return html_path, pdf_path
