"""Tiny monochrome SVG drawing helpers for the IEEE report figures."""
from html import escape

FONT = "Liberation Sans, Arial, Helvetica, sans-serif"
MONO = "Liberation Mono, DejaVu Sans Mono, monospace"


class D:
    def __init__(self, w, h):
        self.w, self.h, self.items = w, h, []

    # primitives -------------------------------------------------------
    def raw(self, s):
        self.items.append(s)

    def text(self, x, y, s, size=8.5, anchor="middle", bold=False, italic=False, mono=False):
        fw = "bold" if bold else "normal"
        fs = "italic" if italic else "normal"
        fam = MONO if mono else FONT
        self.items.append(
            f'<text x="{x}" y="{y}" font-family="{fam}" font-size="{size}" font-weight="{fw}" '
            f'font-style="{fs}" text-anchor="{anchor}" fill="#000">{escape(str(s))}</text>')

    def rect(self, x, y, w, h, fill="#fff", dash=False, sw=0.9, rx=0):
        d = ' stroke-dasharray="3,2"' if dash else ""
        self.items.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{rx}" fill="{fill}" '
                          f'stroke="#000" stroke-width="{sw}"{d}/>')

    def box(self, x, y, w, h, lines, size=8.5, bold_first=False, fill="#fff", dash=False,
            rx=0, mono=False, sw=0.9):
        self.rect(x, y, w, h, fill, dash, sw, rx)
        if isinstance(lines, str):
            lines = [lines]
        lh = size * 1.22
        y0 = y + h / 2 - (len(lines) - 1) * lh / 2 + size * 0.35
        for i, s in enumerate(lines):
            self.text(x + w / 2, y0 + i * lh, s, size, bold=(bold_first and i == 0) or False, mono=mono)

    def line(self, x1, y1, x2, y2, dash=False, sw=0.9, arrow=False, both=False):
        d = ' stroke-dasharray="3,2"' if dash else ""
        m = ' marker-end="url(#ah)"' if arrow else ""
        if both:
            m += ' marker-start="url(#as)"'
        self.items.append(f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" stroke="#000" '
                          f'stroke-width="{sw}"{d}{m}/>')

    def arrow(self, x1, y1, x2, y2, label=None, dash=False, lx=None, ly=None, size=7.5, both=False):
        self.line(x1, y1, x2, y2, dash, arrow=True, both=both)
        if label:
            self.text(lx if lx is not None else (x1 + x2) / 2 + 3,
                      ly if ly is not None else (y1 + y2) / 2 - 3, label, size,
                      anchor="start" if lx is None and x1 == x2 else "middle", italic=True)

    def path(self, d, arrow=False, dash=False, fill="none", sw=0.9):
        m = ' marker-end="url(#ah)"' if arrow else ""
        ds = ' stroke-dasharray="3,2"' if dash else ""
        self.items.append(f'<path d="{d}" fill="{fill}" stroke="#000" stroke-width="{sw}"{m}{ds}/>')

    def diamond(self, cx, cy, w, h, lines, size=8):
        self.items.append(f'<polygon points="{cx},{cy-h/2} {cx+w/2},{cy} {cx},{cy+h/2} {cx-w/2},{cy}" '
                          f'fill="#fff" stroke="#000" stroke-width="0.9"/>')
        if isinstance(lines, str):
            lines = [lines]
        lh = size * 1.2
        y0 = cy - (len(lines) - 1) * lh / 2 + size * 0.35
        for i, s in enumerate(lines):
            self.text(cx, y0 + i * lh, s, size)

    def ellipse(self, cx, cy, rx, ry, lines, size=8, double=False):
        self.items.append(f'<ellipse cx="{cx}" cy="{cy}" rx="{rx}" ry="{ry}" fill="#fff" stroke="#000" stroke-width="0.9"/>')
        if double:
            self.items.append(f'<ellipse cx="{cx}" cy="{cy}" rx="{rx-2.5}" ry="{ry-2.5}" fill="none" stroke="#000" stroke-width="0.7"/>')
        if isinstance(lines, str):
            lines = [lines]
        lh = size * 1.2
        y0 = cy - (len(lines) - 1) * lh / 2 + size * 0.35
        for i, s in enumerate(lines):
            self.text(cx, y0 + i * lh, s, size)

    def marker(self, x, y, kind, r=2.6):
        if kind == "o":
            self.items.append(f'<circle cx="{x}" cy="{y}" r="{r}" fill="#fff" stroke="#000" stroke-width="0.9"/>')
        elif kind == "s":
            self.items.append(f'<rect x="{x-r}" y="{y-r}" width="{2*r}" height="{2*r}" fill="#000"/>')
        elif kind == "^":
            self.items.append(f'<polygon points="{x},{y-r*1.2} {x+r*1.1},{y+r*0.8} {x-r*1.1},{y+r*0.8}" fill="#fff" stroke="#000" stroke-width="0.9"/>')

    def svg(self):
        defs = ('<defs><marker id="ah" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="6" markerHeight="6" '
                'orient="auto-start-reverse"><path d="M0,1 L9,5 L0,9 z" fill="#000"/></marker>'
                '<marker id="as" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="6" markerHeight="6" '
                'orient="auto-start-reverse"><path d="M0,1 L9,5 L0,9 z" fill="#000"/></marker>'
                '<pattern id="hatch" width="4" height="4" patternUnits="userSpaceOnUse" patternTransform="rotate(45)">'
                '<line x1="0" y1="0" x2="0" y2="4" stroke="#000" stroke-width="0.7"/></pattern></defs>')
        return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {self.w} {self.h}" '
                f'width="100%">{defs}{"".join(self.items)}</svg>')


def strip(d, x, y, w, h, total, segs, size=7, labels_below=None):
    """Memory strip. segs = list of (start, size, label, kind) kind in free|used|sys|new."""
    sc = w / total
    for start, sz, lab, kind in segs:
        fill = {"free": "#fff", "used": "#d9d9d9", "sys": "url(#hatch)", "new": "#8c8c8c"}[kind]
        d.rect(x + start * sc, y, sz * sc, h, fill, sw=0.7)
        if lab and sz * sc > 9:
            d.text(x + (start + sz / 2) * sc, y + h / 2 + size * 0.35, lab, size,
                   bold=(kind == "new"))
    if labels_below:
        for a in labels_below:
            d.text(x + a * sc, y + h + size + 1, a, size - 1)


def chart(d, x, y, w, h, xs, series, xlabel, ylabel, ymin, ymax, yticks, xticks=None,
          legend=None, fmt=lambda v: str(v), lx=None, ly=None):
    """series: list of (name, ys, marker, dash). Plain line chart with axes."""
    d.line(x, y + h, x + w, y + h)
    d.line(x, y, x, y + h)
    xmin, xmax = min(xs), max(xs)
    px = lambda v: x + (v - xmin) / (xmax - xmin) * w
    py = lambda v: y + h - (v - ymin) / (ymax - ymin) * h
    for t in yticks:
        d.line(x - 2.5, py(t), x, py(t))
        d.line(x, py(t), x + w, py(t), dash=True, sw=0.25)
        d.text(x - 4, py(t) + 2.5, fmt(t), 7, anchor="end")
    for t in (xticks or xs):
        d.line(px(t), y + h, px(t), y + h + 2.5)
        d.text(px(t), y + h + 10, t, 7)
    d.text(x + w / 2, y + h + 21, xlabel, 7.5)
    d.raw(f'<text transform="translate({x-28},{y+h/2}) rotate(-90)" font-family="{FONT}" '
          f'font-size="7.5" text-anchor="middle">{escape(ylabel)}</text>')
    for name, ys, mk, dash in series:
        pts = " ".join(f"{px(a)},{py(b)}" for a, b in zip(xs, ys) if b is not None)
        ds = f' stroke-dasharray="{dash}"' if dash else ""
        d.raw(f'<polyline points="{pts}" fill="none" stroke="#000" stroke-width="0.9"{ds}/>')
        for a, b in zip(xs, ys):
            if b is not None:
                d.marker(px(a), py(b), mk)
    if legend:
        lx = lx if lx is not None else x + w - 70
        ly = ly if ly is not None else y + 6
        d.rect(lx - 4, ly - 7, 74, 12 * len(series) + 4, sw=0.5)
        for i, (name, ys, mk, dash) in enumerate(series):
            yy = ly + i * 12
            ds = f' stroke-dasharray="{dash}"' if dash else ""
            d.raw(f'<line x1="{lx}" y1="{yy}" x2="{lx+22}" y2="{yy}" stroke="#000" stroke-width="0.9"{ds}/>')
            d.marker(lx + 11, yy, mk)
            d.text(lx + 27, yy + 2.5, name, 7.5, anchor="start")
    return px, py
