"""White-line engraving on soot: the drawing engine behind the TALLOW interface marks.

Every mark is cut, not painted. A line's width carries the light that falls on the
form (one candle, upper left), so a plane in light is a field of swelling ivory lines
and a plane in shadow is soot with a thin lost edge. Lines follow the form like a
wood engraver's graver: along a cylinder, across a board, round a disc.

Drawn with numpy + pycairo at 4x and box-filtered down. No sampled imagery.
"""
import math

import cairo
import numpy as np
from PIL import Image, PngImagePlugin

SS = 4                                      # supersampling factor
LIGHT = np.array([-0.55, -0.62, 0.56])      # toward the light: up-left and in front
LIGHT = LIGHT / np.linalg.norm(LIGHT)


# ---------------------------------------------------------------- noise ---
class Noise1:
    """Smooth value noise along a line; steady for a seed so a mark never crawls."""

    def __init__(self, seed, period=257):
        rng = np.random.default_rng(seed)
        self.v = rng.uniform(-1.0, 1.0, period)
        self.p = period

    def __call__(self, t):
        t = np.asarray(t, float)
        i = np.floor(t).astype(int)
        f = t - i
        f = f * f * (3 - 2 * f)
        a = self.v[i % self.p]
        b = self.v[(i + 1) % self.p]
        return a + (b - a) * f


def grain(w, h, seed, scales=((3, .45), (9, .35), (27, .2))):
    """Paper tooth: a few octaves of value noise, 0..1, at output resolution."""
    rng = np.random.default_rng(seed)
    acc = np.zeros((h, w), np.float32)
    for cell, amp in scales:
        gw, gh = max(2, w // cell + 2), max(2, h // cell + 2)
        g = Image.fromarray((rng.uniform(0, 1, (gh, gw)) * 255).astype(np.uint8))
        g = g.resize((gw * cell, gh * cell), Image.BICUBIC).crop((0, 0, w, h))
        acc += np.asarray(g, np.float32) / 255.0 * amp
    acc -= acc.min()
    return acc / max(1e-6, acc.max())


def fibres(w, h, seed, count=None, length=(6, 22)):
    """Short pale fibres laid in the sheet, as in rag paper."""
    rng = np.random.default_rng(seed)
    count = count or (w * h) // 900
    surf = cairo.ImageSurface(cairo.FORMAT_A8, w, h)
    cr = cairo.Context(surf)
    for _ in range(count):
        x, y = rng.uniform(0, w), rng.uniform(0, h)
        a = rng.uniform(0, math.pi)
        L = rng.uniform(*length)
        bend = rng.uniform(-.25, .25)
        cr.set_source_rgba(1, 1, 1, rng.uniform(.25, 1))
        cr.set_line_width(rng.uniform(.5, 1.0))
        cr.move_to(x, y)
        mx, my = x + math.cos(a) * L * .5 + math.cos(a + 1.57) * L * bend, y + math.sin(a) * L * .5 + math.sin(a + 1.57) * L * bend
        cr.curve_to(mx, my, mx, my, x + math.cos(a) * L, y + math.sin(a) * L)
        cr.stroke()
    surf.flush()
    stride = surf.get_stride()
    return np.ndarray((h, stride), np.uint8, surf.get_data())[:, :w].astype(np.float32) / 255.0


# ------------------------------------------------------------- geometry ---
def bezier(p0, p1, p2, p3, n=40):
    t = np.linspace(0, 1, n)[:, None]
    p0, p1, p2, p3 = map(lambda p: np.asarray(p, float), (p0, p1, p2, p3))
    return (1 - t) ** 3 * p0 + 3 * (1 - t) ** 2 * t * p1 + 3 * (1 - t) * t ** 2 * p2 + t ** 3 * p3


def spline(points, n_per=24, closed=False, tension=.5):
    """Catmull-Rom through the given points; the hand's natural curve."""
    P = [np.asarray(p, float) for p in points]
    if closed:
        P = [P[-1]] + P + [P[0], P[1]]
    else:
        P = [P[0] + (P[0] - P[1]) * .0] + P + [P[-1]]
    out = []
    for i in range(1, len(P) - 2):
        p0, p1, p2, p3 = P[i - 1], P[i], P[i + 1], P[i + 2]
        c1 = p1 + (p2 - p0) * tension / 3
        c2 = p2 - (p3 - p1) * tension / 3
        seg = bezier(p1, c1, c2, p2, n_per)
        out.append(seg if not out else seg[1:])
    return np.vstack(out)


def ellipse(c, rx, ry, a0=0.0, a1=2 * math.pi, n=96, rot=0.0):
    t = np.linspace(a0, a1, n)
    x, y = np.cos(t) * rx, np.sin(t) * ry
    cr, sr = math.cos(rot), math.sin(rot)
    return np.stack([c[0] + x * cr - y * sr, c[1] + x * sr + y * cr], 1)


def resample(P, step):
    P = np.asarray(P, float)
    if len(P) < 2:
        return P
    d = np.r_[0, np.cumsum(np.linalg.norm(np.diff(P, axis=0), axis=1))]
    if d[-1] < 1e-9:
        return P[:1]
    n = max(2, int(math.ceil(d[-1] / step)) + 1)
    s = np.linspace(0, d[-1], n)
    return np.stack([np.interp(s, d, P[:, 0]), np.interp(s, d, P[:, 1])], 1)


def normals(P):
    T = np.gradient(P, axis=0)
    T /= np.maximum(1e-9, np.linalg.norm(T, axis=1))[:, None]
    return np.stack([-T[:, 1], T[:, 0]], 1), T


def lerp(a, b, t):
    return np.asarray(a, float) + (np.asarray(b, float) - np.asarray(a, float)) * t


def shade(n3, gamma=1.0, ambient=0.0):
    """Lambert toward the candle; n3 are surface normals (x right, y down, z out of the page)."""
    n3 = np.asarray(n3, float)
    n3 = n3 / np.maximum(1e-9, np.linalg.norm(n3, axis=-1, keepdims=True))
    return np.clip(ambient + (1 - ambient) * np.clip(n3 @ LIGHT, 0, 1), 0, 1) ** gamma


# ---------------------------------------------------------------- plate ---
class Plate:
    """One small engraving. Design units: the plate is `design_w` wide, y down."""

    def __init__(self, w_px, h_px, design_w=100.0, seed=1):
        self.W, self.H = int(w_px), int(h_px)
        self.k = self.W / design_w           # design units -> output px
        self.s = self.k * SS                  # design units -> supersampled px
        self.unit = 1.0 / self.k              # one output pixel, in design units
        self.surf = cairo.ImageSurface(cairo.FORMAT_ARGB32, self.W * SS, self.H * SS)
        self.cr = cairo.Context(self.surf)
        self.cr.set_fill_rule(cairo.FILL_RULE_WINDING)
        self.seed = seed
        self._n = 0

    # -- low level
    def _poly(self, P):
        cr = self.cr
        P = np.asarray(P, float) * self.s
        cr.move_to(*P[0])
        for q in P[1:]:
            cr.line_to(*q)
        cr.close_path()

    def _src(self, value, alpha):
        v = float(np.clip(value, 0, 1))
        self.cr.set_source_rgba(v, v, v, float(np.clip(alpha, 0, 1)))

    def fill(self, poly, value=1.0, alpha=1.0):
        self._poly(poly)
        self._src(value, alpha)
        self.cr.fill()

    def cut(self, poly):
        """Scrape back to soot: clears whatever ink lies inside."""
        cr = self.cr
        cr.save()
        cr.set_operator(cairo.OPERATOR_CLEAR)
        self._poly(poly)
        cr.fill()
        cr.restore()

    def clip(self, poly):
        self.cr.save()
        self._poly(poly)
        self.cr.clip()

    def unclip(self):
        self.cr.restore()

    def min_w(self, px):
        return px * self.unit

    # -- the graver
    def line(self, pts, width, value=1.0, alpha=1.0, taper=(.18, .22), tremor=.35, swell=.10,
             floor=.22, minpx=0.0, cap=True, seed=None, cut=False):
        """A cut line. `width` is a scalar or one value per point (design units, full width).
        Ends taper to `floor` of full width; `tremor` wanders the line sideways in design units."""
        P = np.asarray(pts, float)
        if len(P) < 2:
            return
        src_t = np.linspace(0, 1, len(P))
        Q = resample(P, step=max(.12, .35 * self.unit))
        n = len(Q)
        if n < 3:
            return
        t = np.linspace(0, 1, n)
        wa = np.atleast_1d(np.asarray(width, float))
        w = np.interp(t, np.linspace(0, 1, len(wa)), wa) if len(wa) > 1 else np.full(n, wa[0])
        self._n += 1
        rnd = Noise1((self.seed * 7919 + self._n * 104729) if seed is None else seed)
        N, _ = normals(Q)
        arc = np.r_[0, np.cumsum(np.linalg.norm(np.diff(Q, axis=0), axis=1))]
        Q = Q + N * (tremor * rnd(arc / 6.0 + 3.1))[:, None]
        prof = np.ones(n)
        a, b = taper
        if a > 0:
            prof *= np.clip(t / a, 0, 1) ** .7
        if b > 0:
            prof *= np.clip((1 - t) / b, 0, 1) ** .7
        prof = floor + (1 - floor) * prof
        half = .5 * w * prof * (1 + swell * rnd(arc / 2.3 + 17.7))
        half = np.maximum(half, .5 * minpx * self.unit)
        keep = half > .5 * .06 * self.unit
        if keep.sum() < 2:
            return
        N, _ = normals(Q)
        L = Q + N * half[:, None]
        R = Q - N * half[:, None]
        cr = self.cr
        if cut:
            cr.save()
            cr.set_operator(cairo.OPERATOR_CLEAR)
        self._poly(np.vstack([L, R[::-1]]))
        if cap:
            for i in (0, n - 1):
                cr.new_sub_path()
                cr.arc(Q[i, 0] * self.s, Q[i, 1] * self.s, half[i] * self.s, 0, 2 * math.pi)
        self._src(value, alpha)
        cr.fill()
        if cut:
            cr.restore()

    def contour(self, pts, weight, closed=False, value=1.0, lost=.35, minpx=1.0, taper=(.04, .04), tremor=.25, **kw):
        """A silhouette line that thickens where it faces the light and thins where it turns away."""
        P = np.asarray(pts, float)
        if closed:
            P = np.vstack([P, P[:1]])
        P = resample(P, .5)
        N, _ = normals(P)
        # outward normal: sign chosen so the curve's interior is on the left for closed CCW paths;
        # facing is measured against the light in the picture plane.
        L2 = LIGHT[:2] / np.linalg.norm(LIGHT[:2])
        face = np.clip(-(N @ L2) if kw.pop('flip', False) else (N @ L2), -1, 1)
        w = weight * (lost + (1 - lost) * (.5 + .5 * face))
        self.line(P, w, value=value, taper=taper if not closed else (0, 0), tremor=tremor, minpx=minpx, **kw)

    def hatch(self, region, angle, spacing, width_fn, value=1.0, jitter=.18, taper=(.25, .25), minpx=.0,
              cut_below=.06, tremor=.2, bend=0.0):
        """Parallel cuts across `region` (polygon), each one's width given by width_fn(points)."""
        R = np.asarray(region, float)
        c = R.mean(0)
        d = np.array([math.cos(angle), math.sin(angle)])
        nrm = np.array([-d[1], d[0]])
        proj = (R - c) @ nrm
        along = (R - c) @ d
        rng = np.random.default_rng(self.seed * 31 + self._n)
        self.clip(R)
        o = proj.min() + spacing * .5
        while o < proj.max():
            oo = o + rng.uniform(-jitter, jitter) * spacing
            a0, a1 = along.min() - spacing, along.max() + spacing
            s = np.linspace(a0, a1, 48)
            pts = c + np.outer(s, d) + nrm * oo + np.outer(np.sin((s - a0) / (a1 - a0) * math.pi), nrm) * bend
            w = np.asarray(width_fn(pts), float)
            if w.max() > cut_below:
                self.line(pts, np.where(w > cut_below, w, 0), value=value, taper=taper, tremor=tremor, minpx=minpx, floor=.0)
            o += spacing
        self.unclip()

    # -- output
    def pixels(self, grain_amt=.18, grain_seed=None, value_floor=0.0):
        """Box-filter to output size, un-premultiply, lay the soot tooth into the ink."""
        self.surf.flush()
        h, w = self.H * SS, self.W * SS
        a = np.ndarray((h, self.surf.get_stride() // 4, 4), np.uint8, self.surf.get_data())[:, :w].astype(np.float32) / 255.0
        a = a.reshape(self.H, SS, self.W, SS, 4).mean((1, 3))
        alpha = a[..., 3]
        val = np.where(alpha > 1e-4, a[..., 2] / np.maximum(alpha, 1e-4), 0)
        g = grain(self.W, self.H, (grain_seed or self.seed) + 11, scales=((2, .5), (5, .3), (13, .2)))
        alpha = alpha * (1 - grain_amt * g * (1 - .6 * alpha))   # thin edges break up more than full cuts
        return np.clip(val, value_floor, 1), np.clip(alpha, 0, 1)

    def save(self, path, provenance, grain_amt=.18, tint=(1.0, .985, .955)):
        val, alpha = self.pixels(grain_amt)
        rgb = np.stack([val * tint[0], val * tint[1], val * tint[2]], -1)
        out = np.dstack([rgb, alpha])
        img = Image.fromarray((np.clip(out, 0, 1) * 255 + .5).astype(np.uint8), 'RGBA')
        save_png(img, path, provenance)
        return img


def save_png(img, path, provenance):
    info = PngImagePlugin.PngInfo()
    info.add_text('provenance', provenance)
    img.save(path, pnginfo=info)
