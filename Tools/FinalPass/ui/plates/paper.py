"""Paper, ink, pencil and wax crayon for the TALLOW story plates, and the dark table they lie on.

A Sheet is one paper object in its own pixel space (drawn at 2x the plate). It carries a
colour field and a shape (alpha) with cut or torn edges, tooth, fibres, age, folds and
cockle. Marks go on through `deposit`, which lets the medium decide how the paper's tooth
breaks the stroke: printer's ink sits solid, pencil catches the grain, wax crayon skips
across the valleys. A Table lays sheets on the soot under one candle and casts their
shadows. Everything is procedural; every stroke and word is placed by hand in plates.py.
"""
import math

import cairo
import numpy as np
from PIL import Image, ImageDraw, ImageFont

import look

SS = 2                                   # sheets and the table are drawn at 2x the plate
FONT_CACHE = {}


def font(path, size):
    key = (path, int(round(size)))
    if key not in FONT_CACHE:
        FONT_CACHE[key] = ImageFont.truetype(path, key[1])
    return FONT_CACHE[key]


class Wobble:
    """Smooth 1D value noise, steady for a seed, for hands that never quite hold a line."""

    def __init__(self, seed, period=211):
        self.v = np.random.default_rng(seed).uniform(-1, 1, period)
        self.p = period

    def __call__(self, t):
        t = np.asarray(t, float)
        i = np.floor(t).astype(int)
        f = t - i
        f = f * f * (3 - 2 * f)
        return self.v[i % self.p] * (1 - f) + self.v[(i + 1) % self.p] * f


def resample(P, step):
    P = np.asarray(P, float)
    d = np.r_[0, np.cumsum(np.linalg.norm(np.diff(P, axis=0), axis=1))]
    if d[-1] < 1e-6:
        return P[:1]
    n = max(2, int(math.ceil(d[-1] / step)) + 1)
    s = np.linspace(0, d[-1], n)
    return np.stack([np.interp(s, d, P[:, 0]), np.interp(s, d, P[:, 1])], 1)


def spline(points, n_per=18, closed=False):
    """Catmull-Rom through the points: how a hand moves between the places it means to go."""
    P = [np.asarray(p, float) for p in points]
    if closed:
        P = [P[-1]] + P + [P[0], P[1]]
    else:
        P = [P[0]] + P + [P[-1]]
    out = []
    for i in range(1, len(P) - 2):
        p0, p1, p2, p3 = P[i - 1], P[i], P[i + 1], P[i + 2]
        t = np.linspace(0, 1, n_per)[:, None]
        seg = .5 * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t ** 2 + (-p0 + 3 * p1 - 3 * p2 + p3) * t ** 3)
        out.append(seg if not out else seg[1:])
    return np.vstack(out)


def normals(P):
    T = np.gradient(P, axis=0)
    T /= np.maximum(1e-9, np.linalg.norm(T, axis=1))[:, None]
    return np.stack([-T[:, 1], T[:, 0]], 1)


def wander(P, seed, amp, scale=60.0):
    """Push a path sideways by a slow wobble: amp px, wavelength about `scale` px."""
    P = resample(P, 2.0)
    if len(P) < 3:
        return P
    arc = np.r_[0, np.cumsum(np.linalg.norm(np.diff(P, axis=0), axis=1))]
    w = Wobble(seed)
    return P + normals(P) * (amp * (w(arc / scale) + .35 * w(arc / (scale * .31) + 40)))[:, None]


def mask_surface(w, h):
    s = cairo.ImageSurface(cairo.FORMAT_A8, w, h)
    return s, cairo.Context(s)


def surface_array(s, w, h):
    s.flush()
    return np.ndarray((h, s.get_stride()), np.uint8, s.get_data())[:, :w].astype(np.float32) / 255.0


class Sheet:
    """One sheet of paper. Coordinates are local pixels at 2x the plate, y down."""

    def __init__(self, w, h, seed, tone=(214, 203, 176), tooth=1.0):
        self.w, self.h, self.seed = int(w), int(h), seed
        self.tone = np.array(tone, np.float32) / 255.0
        n = look.noise
        mottle = n(self.w, self.h, seed, 2.4) * .045
        cloud = n(self.w, self.h, seed + 1, 3.2) * .03
        tz = look.value_noise(self.w, self.h, seed + 2, 2) - .5
        fine = look.value_noise(self.w, self.h, seed + 3, 5) - .5
        self.tooth = np.clip(.5 + tz * .9 + fine * .6, 0, 1)        # 0 valley .. 1 peak, for media
        fib = fibres(self.w, self.h, seed + 4, count=self.w * self.h // 2200)
        v = 1 + mottle + cloud + (self.tooth - .5) * .05 * tooth - fib * .035 + fib * .02
        self.rgb = self.tone[None, None, :] * v[..., None]
        self.alpha = np.ones((self.h, self.w), np.float32)
        self.height = n(self.w, self.h, seed + 5, 3.4) * 1.0       # cockle, for the light
        self.creases = []
        self.speck = {c: look.value_noise(self.w, self.h, seed + 10 + c, c) for c in (3, 9, 14)}

    # ---------------------------------------------------------------- shape ---
    def shape(self, edges, corner=6.0):
        """edges: four (kind, amp) for top, right, bottom, left; kind 'cut' or 'torn'. Sets alpha."""
        w, h = self.w, self.h
        pts = []
        corners = [(0, 0), (w, 0), (w, h), (0, h)]
        rim = np.zeros((h, w), np.float32)
        for i, (kind, amp) in enumerate(edges):
            a, b = np.array(corners[i], float), np.array(corners[(i + 1) % 4], float)
            L = np.linalg.norm(b - a)
            t = np.linspace(0, 1, max(8, int(L / 3)))
            P = a + (b - a) * t[:, None]
            inward = np.array([-(b - a)[1], (b - a)[0]]) / L
            wb = Wobble(self.seed * 13 + i * 7)
            if kind == 'torn':
                off = amp * (.55 + .45 * (wb(t * L / 70) + .5 * wb(t * L / 19 + 9) + .25 * wb(t * L / 5 + 3)))
            else:
                off = amp * (.5 + .5 * wb(t * L / 400)) + .6 * wb(t * L / 9 + 5)
            off = np.maximum(off, 0)
            # keep the corners where they are, so the sheet keeps its cut size
            P = P + inward * off[:, None]
            pts.append(P[:-1])
        poly = np.vstack(pts)
        s, cr = mask_surface(w, h)
        cr.move_to(*poly[0])
        for q in poly[1:]:
            cr.line_to(*q)
        cr.close_path()
        cr.set_source_rgba(1, 1, 1, 1)
        cr.fill()
        a = surface_array(s, w, h)
        # round the corners a hair
        if corner > 0:
            a = np.clip((look.blur(a, corner * .5) - .5) * 3 + .5, 0, 1) * a
        self.alpha = a
        # a torn edge shows pale, fibrous paper core a few px in
        torn_any = any(k == 'torn' for k, _ in edges)
        if torn_any:
            inner = np.clip((look.blur(a, 3) - .55) * 4, 0, 1)
            band = np.clip(a - inner, 0, 1)
            sides = np.zeros((h, w), np.float32)
            for i, (kind, amp) in enumerate(edges):
                if kind != 'torn':
                    continue
                yy, xx = np.mgrid[0:h, 0:w]
                d = [yy, w - xx, h - yy, xx][i].astype(np.float32)
                sides = np.maximum(sides, (d < amp * 2.6 + 8).astype(np.float32))
            core = band * sides * (.6 + .4 * self.tooth)
            self.rgb = self.rgb * (1 - core[..., None]) + np.minimum(1, self.tone * 1.12)[None, None, :] * core[..., None]

    # ----------------------------------------------------------------- age ---
    def age(self, amount=1.0, edge=60.0, colour=(150, 120, 80)):
        """Edges and handled places brown first."""
        e = 1 - look.blur(self.alpha, edge * .5)
        e = np.clip(e * 2.2, 0, 1) ** 1.3
        c = np.array(colour, np.float32) / 255.0
        k = (amount * .45 * e)[..., None]
        self.rgb = self.rgb * (1 - k) + self.rgb * c[None, None, :] * 1.6 * k

    def fox(self, spots):
        """Foxing: (x, y, r, strength) brown spots, placed where damp and hands would leave them."""
        yy, xx = np.mgrid[0:self.h, 0:self.w].astype(np.float32)
        for x, y, r, k in spots:
            d = np.hypot(xx - x, yy - y) / r
            m = np.clip(1 - d, 0, 1) ** 1.5 * k
            ring = np.exp(-((d - .85) / .12) ** 2) * k * .5
            m = m + ring
            self.rgb = self.rgb * (1 - .35 * m[..., None]) + np.array([.52, .38, .22])[None, None, :] * .35 * m[..., None]

    def stain(self, x, y, rx, ry, seed, tint=(1.05, 1.02, .9), gloss=.18, opacity=.8):
        """A tallow drip: a translucent, slightly glossy pool with a darker rim."""
        yy, xx = np.mgrid[0:self.h, 0:self.w].astype(np.float32)
        wob = look.blur(look.noise(self.w, self.h, seed, 2.2), 2) * .35
        d = np.hypot((xx - x) / rx, (yy - y) / ry) + wob
        body = np.clip((1 - d) * 6, 0, 1) * opacity
        rim = np.exp(-((d - .97) / .05) ** 2) * opacity
        t = np.array(tint, np.float32)
        self.rgb = self.rgb * (1 - body[..., None]) + np.minimum(1, self.rgb * t[None, None, :] + gloss * .25) * body[..., None]
        self.rgb = self.rgb * (1 - .22 * rim[..., None])
        self.height = self.height + body * .6

    def crease(self, a, b, depth=1.0):
        """A fold line from a to b (local px). Shading comes from the light at placement time."""
        self.creases.append((np.array(a, float), np.array(b, float), depth))
        yy, xx = np.mgrid[0:self.h, 0:self.w].astype(np.float32)
        a, b = np.array(a, float), np.array(b, float)
        d = b - a
        L = np.linalg.norm(d)
        n = np.array([-d[1], d[0]]) / L
        s = (xx - a[0]) * n[0] + (yy - a[1]) * n[1]
        # a roof-shaped ridge in the height field, plus a fine broken line of crushed fibre
        self.height = self.height + depth * 1.4 * np.exp(-np.abs(s) / 30.0)
        line = np.exp(-(s / 1.3) ** 2) * (.5 + .5 * self.tooth) * depth
        self.rgb = self.rgb * (1 - .12 * line[..., None])

    def rules(self, y0, pitch, n, x0=None, x1=None, colour=(120, 140, 160), opacity=.45, width=1.6, seed=3):
        """Printed ruling, a little uneven in ink."""
        m = np.zeros((self.h, self.w), np.float32)
        x0 = 0 if x0 is None else x0
        x1 = self.w if x1 is None else x1
        for i in range(n):
            y = y0 + i * pitch
            if y > self.h - 4:
                break
            yi = int(y)
            for k in range(-2, 3):
                yy = yi + k
                if 0 <= yy < self.h:
                    m[yy, int(x0):int(x1)] = np.maximum(m[yy, int(x0):int(x1)], math.exp(-((yy - y) / (width * .5)) ** 2))
        self.deposit(m, colour, opacity, 'print', seed)

    def vrule(self, x, y0=0, y1=None, colour=(160, 60, 50), opacity=.55, width=2.0, seed=4):
        m = np.zeros((self.h, self.w), np.float32)
        y1 = self.h if y1 is None else y1
        for k in range(-3, 4):
            xx = int(x) + k
            if 0 <= xx < self.w:
                m[int(y0):int(y1), xx] = math.exp(-((xx - x) / (width * .5)) ** 2)
        self.deposit(m, colour, opacity, 'print', seed)

    # ---------------------------------------------------------------- marks ---
    def deposit(self, m, colour, opacity=1.0, medium='ink', seed=1, pressure=None):
        """Lay a mark (coverage mask 0..1) on the sheet in the given medium."""
        ys, xs = np.nonzero(m > .002)
        if len(ys) == 0:
            return
        y0, y1 = max(0, ys.min() - 6), min(self.h, ys.max() + 7)
        x0, x1 = max(0, xs.min() - 6), min(self.w, xs.max() + 7)
        sl = (slice(y0, y1), slice(x0, x1))
        m = m[sl]
        c = np.array(colour, np.float32) / 255.0
        t = self.tooth[sl]
        if medium == 'print':
            spread = look.blur(m, 1)
            m = np.clip(np.maximum(m, spread * 1.15), 0, 1) * (.86 + .14 * self.speck[9][sl])
        elif medium == 'ink':
            m = np.clip(m * 1.05, 0, 1) * (.9 + .1 * self.speck[14][sl])
        elif medium == 'pencil':
            # graphite catches the tooth's peaks and leaves the valleys
            p = .55 if pressure is None else pressure
            m = m * np.clip((t - (.62 - .55 * p)) / .3, 0, 1) * (.55 + .45 * p)
        elif medium == 'crayon':
            # wax rides the tooth: it only lands where the pressure reaches down into the paper,
            # so the valleys stay paper even under a hard push and the edges break up
            p = 1.0 if pressure is None else pressure
            depth = np.clip((1 - t) * .8 + (self.speck[3][sl] - .5) * .5 + .1, 0, 1)
            m = np.clip((m * (.55 + .4 * p) - depth) / .12, 0, 1)
        m = np.clip(m * opacity, 0, 1)[..., None]
        rgb = self.rgb[sl]
        if medium == 'crayon':
            # wax is opaque and lies on top; heavy wax catches a dull sheen along the drag
            sheen = (self.speck[9][sl] - .5)[..., None] * .05
            self.rgb[sl] = rgb * (1 - m) + np.clip(c[None, None, :] + sheen, 0, 1) * m
        else:
            # ink and graphite stain into the sheet: multiply toward the colour
            self.rgb[sl] = rgb * (1 - m) + (rgb * c[None, None, :] / np.maximum(self.tone, .3)[None, None, :] * .55 + c * .45) * m

    def strokes_mask(self, paths, width, alpha=None, tip=1, seed=1, tip_spread=.6):
        """Rasterise paths into a coverage mask. width: scalar or per-path. alpha: per-path opacity.
        tip > 1 draws a blunt crayon tip as several parallel lines a little apart."""
        s, cr = mask_surface(self.w, self.h)
        cr.set_line_cap(cairo.LINE_CAP_ROUND)
        cr.set_line_join(cairo.LINE_JOIN_ROUND)
        rng = np.random.default_rng(seed)
        for i, P in enumerate(paths):
            P = np.asarray(P, float)
            if len(P) < 2:
                continue
            wdt = width[i] if isinstance(width, (list, tuple, np.ndarray)) else width
            a = 1.0 if alpha is None else alpha[i]
            N = normals(P)
            for k in range(tip):
                off = 0 if tip == 1 else (k / (tip - 1) - .5) * wdt * tip_spread
                Q = P + N * off
                cr.set_line_width(wdt if tip == 1 else wdt * (1 - tip_spread * .8))
                cr.set_source_rgba(1, 1, 1, a * (1 if tip == 1 else rng.uniform(.5, .85)))
                cr.move_to(*Q[0])
                for q in Q[1:]:
                    cr.line_to(*q)
                cr.stroke()
        return surface_array(s, self.w, self.h)

    def crayon(self, paths, colour, width=9, pressure=1.0, seed=1, tip=4, alpha=None):
        m = self.strokes_mask(paths, width, alpha, tip, seed)
        self.deposit(m, colour, 1.0, 'crayon', seed, pressure)

    def pencil(self, paths, width=2.4, pressure=.6, seed=1, colour=(70, 68, 66)):
        m = self.strokes_mask(paths, width, None, 1, seed)
        self.deposit(m, colour, .95, 'pencil', seed, pressure)

    # ----------------------------------------------------------------- type ---
    def text_mask(self, lines, fnt, x, y, leading, align='left', spacing=0.0):
        """Printed text: whole lines, kerning kept. (x, y) is the first baseline's left (or centre)."""
        img = Image.new('L', (self.w, self.h), 0)
        d = ImageDraw.Draw(img)
        for i, line in enumerate(lines):
            yy = y + i * leading
            if spacing:
                xx = x - (self._spaced_len(line, fnt, spacing) * (.5 if align == 'center' else 1 if align == 'right' else 0))
                for ch in line:
                    d.text((xx, yy), ch, font=fnt, fill=255, anchor='ls')
                    xx += fnt.getlength(ch) + spacing
            else:
                anchor = {'left': 'ls', 'center': 'ms', 'right': 'rs'}[align]
                d.text((x, yy), line, font=fnt, fill=255, anchor=anchor)
        return np.asarray(img, np.float32) / 255.0

    @staticmethod
    def _spaced_len(line, fnt, spacing):
        return sum(fnt.getlength(c) for c in line) + spacing * (len(line) - 1)

    def print_text(self, lines, path, size, x, y, leading=None, colour=(34, 29, 26), opacity=.94, align='left', spacing=0.0, seed=1):
        f = font(path, size)
        m = self.text_mask(lines if isinstance(lines, (list, tuple)) else [lines], f, x, y, leading or size * 1.25, align, spacing)
        self.deposit(m, colour, opacity, 'print', seed)
        return m

    def wrap(self, text, path, size, width):
        f = font(path, size)
        out, line = [], ''
        for word in text.split(' '):
            trial = (line + ' ' + word).strip()
            if f.getlength(trial) > width and line:
                out.append(line)
                line = word
            else:
                line = trial
        if line:
            out.append(line)
        return out

    def hand_mask(self, lines, path, size, x, y, leading, seed, rot=2.5, bob=.06, grow=.06, drift=0.0, slope=0.0, track=0.0):
        """Handwriting: every glyph placed by itself, with the hand's small turns, rises and size changes.
        rot degrees, bob and grow as fractions of the size, drift px of baseline wander per line,
        slope px the line climbs over its length."""
        out = np.zeros((self.h, self.w), np.float32)
        f = font(path, size)
        w1, w2 = Wobble(seed), Wobble(seed + 77)
        k = 0
        for li, line in enumerate(lines):
            if isinstance(line, tuple):
                line, dx = line
            else:
                dx = 0
            base = y + li * leading
            total = max(1.0, f.getlength(line))
            for i, ch in enumerate(line):
                k += 1
                if ch == ' ':
                    continue
                gx = x + dx + f.getlength(line[:i]) + track * i
                u = (gx - x) / total
                gy = base + drift * w2(li * 3.1 + u * 2.2) + slope * u + size * bob * w1(k * .9)
                sz = size * (1 + grow * w1(k * .53 + 31))
                gf = font(path, sz)
                pad = int(sz * 1.6)
                g = Image.new('L', (pad * 2, pad * 2), 0)
                ImageDraw.Draw(g).text((pad, pad), ch, font=gf, fill=255, anchor='ls')
                g = g.rotate(rot * w1(k * .71 + 9), resample=Image.BICUBIC, center=(pad, pad))
                ga = np.asarray(g, np.float32) / 255.0
                X0, Y0 = int(round(gx)) - pad, int(round(gy)) - pad
                xs, ys = max(0, X0), max(0, Y0)
                xe, ye = min(self.w, X0 + pad * 2), min(self.h, Y0 + pad * 2)
                if xe <= xs or ye <= ys:
                    continue
                out[ys:ye, xs:xe] = np.maximum(out[ys:ye, xs:xe], ga[ys - Y0:ye - Y0, xs - X0:xe - X0])
        return out

    def rgba(self):
        return np.dstack([np.clip(self.rgb, 0, 1), self.alpha])


def fibres(w, h, seed, count, length=(8, 30)):
    rng = np.random.default_rng(seed)
    s, cr = mask_surface(w, h)
    for _ in range(count):
        x, y = rng.uniform(0, w), rng.uniform(0, h)
        a = rng.uniform(0, math.pi)
        L = rng.uniform(*length)
        cr.set_source_rgba(1, 1, 1, rng.uniform(.3, 1))
        cr.set_line_width(rng.uniform(.6, 1.3))
        cr.move_to(x, y)
        b = rng.uniform(-.3, .3)
        mx, my = x + math.cos(a) * L * .5 - math.sin(a) * L * b, y + math.sin(a) * L * .5 + math.cos(a) * L * b
        cr.curve_to(mx, my, mx, my, x + math.cos(a) * L, y + math.sin(a) * L)
        cr.stroke()
    return surface_array(s, w, h)


# --------------------------------------------------------------------- table ---
class Table:
    """The dark table a plate is laid on, under one candle. side is the caption side ('L' or 'R');
    the candle hangs above the art on that side so the art falls off into the dark away from it."""

    def __init__(self, side, seed, light=None, reach=1150.0, peak=1.0):
        self.side, self.seed = side, seed
        self.W, self.H = look.W * SS, look.H * SS
        g = look.soot_ground(look.W, look.H, seed)
        self.ground = np.stack([np.asarray(Image.fromarray(g[..., c].astype(np.float32), 'F').resize((self.W, self.H), Image.BICUBIC), np.float32) for c in range(3)], -1)
        self.obj = np.zeros((self.H, self.W, 3), np.float32)      # premultiplied object colour
        self.cov = np.zeros((self.H, self.W), np.float32)         # object coverage
        if light is None:
            light = (1180, -220) if side == 'R' else (740, -220)
        self.light = np.array(light, float) * SS
        self.reach = reach * SS
        self.peak = peak
        self.chroma = .72          # paper colour kept quiet: the thread is the only saturated thing

    def _lightmap(self):
        return look.candle(self.W, self.H, self.light, self.reach, self.peak, ambient=.05)

    def place(self, sheet, cx, cy, angle=0.0, lift=1.0, shade=True):
        """Lay a sheet with its centre at (cx, cy) in plate px, turned `angle` degrees (counter-clockwise)."""
        rgba = sheet.rgba() if isinstance(sheet, Sheet) else sheet
        h, w = rgba.shape[:2]
        # light on the sheet's own relief (cockle, creases, wax), from the candle's direction
        if isinstance(sheet, Sheet) and shade:
            ldir = self.light / SS - np.array([cx, cy])
            ldir = ldir / max(1e-6, np.linalg.norm(ldir))
            a = math.radians(angle)
            # light direction in the sheet's local frame (undo the rotation; y down)
            lx = ldir[0] * math.cos(a) - ldir[1] * math.sin(a)
            ly = ldir[0] * math.sin(a) + ldir[1] * math.cos(a)
            gy, gx = np.gradient(look.blur(sheet.height, 2))
            relief = 1 + np.clip(-(gx * lx + gy * ly) * 1.6, -.22, .22)
            rgba = rgba.copy()
            rgba[..., :3] *= relief[..., None]
        prem = np.dstack([rgba[..., :3] * rgba[..., 3:4], rgba[..., 3]])
        chans = []
        for c in range(4):
            im = Image.fromarray(prem[..., c].astype(np.float32), 'F').rotate(angle, resample=Image.BICUBIC, expand=True)
            chans.append(np.asarray(im, np.float32))
        prem = np.stack(chans, -1)
        hh, ww = prem.shape[:2]
        X0, Y0 = int(round(cx * SS - ww / 2)), int(round(cy * SS - hh / 2))
        full = np.zeros((self.H, self.W, 4), np.float32)
        xs, ys, xe, ye = max(0, X0), max(0, Y0), min(self.W, X0 + ww), min(self.H, Y0 + hh)
        if xe <= xs or ye <= ys:
            return
        full[ys:ye, xs:xe] = prem[ys - Y0:ye - Y0, xs - X0:xe - X0]
        A = np.clip(full[..., 3], 0, 1)
        self._shadow(A, cx, cy, lift)
        self.obj = self.obj * (1 - A[..., None]) + full[..., :3]
        self.cov = self.cov * (1 - A) + A
        a = math.radians(angle)

        def to_plate(lx, ly):
            """A point on the sheet (local 2x px) to plate px, after the turn."""
            dx, dy = lx - w / 2, ly - h / 2
            return np.array([cx + (dx * math.cos(a) + dy * math.sin(a)) / SS, cy + (-dx * math.sin(a) + dy * math.cos(a)) / SS])
        return to_plate

    def _shadow(self, A, cx, cy, lift):
        away = np.array([cx, cy]) * SS - self.light
        away = away / max(1e-6, np.linalg.norm(away))
        def shifted(a, d):
            dx, dy = int(round(away[0] * d)), int(round(away[1] * d))
            return np.roll(np.roll(a, dy, 0), dx, 1)
        soft = look.blur(shifted(A, 22 * lift * SS), 18 * lift * SS) * .62
        contact = look.blur(shifted(A, 2.5 * SS), 2.5 * SS) * .55
        sh = 1 - np.clip(soft + contact - soft * contact, 0, .85) * (1 - A)
        self.ground *= sh[..., None]
        self.obj *= sh[..., None]

    def thread(self, paths, width=2.6, colour=look.THREAD, lift=.25, seed=5):
        """The red thread: a twisted cotton line with a sheen on the turns and a fine shadow."""
        s, cr = mask_surface(self.W, self.H)
        cr.set_line_cap(cairo.LINE_CAP_ROUND)
        cr.set_line_join(cairo.LINE_JOIN_ROUND)
        for P in paths:
            P = np.asarray(P, float) * SS
            cr.set_line_width(width * SS)
            cr.set_source_rgba(1, 1, 1, 1)
            cr.move_to(*P[0])
            for q in P[1:]:
                cr.line_to(*q)
            cr.stroke()
        A = surface_array(s, self.W, self.H)
        cx, cy = np.mean(np.vstack(paths), 0)
        self._shadow(A, cx, cy, lift)
        ys, xs = np.nonzero(A > 0)
        sl = (slice(ys.min(), ys.max() + 1), slice(xs.min(), xs.max() + 1))
        yy, xx = np.mgrid[sl].astype(np.float32)
        # twist: fine diagonal sheen along the yarn
        twist = .5 + .5 * np.sin((xx + yy) * .9 / SS)
        col = np.array(colour, np.float32)[None, None, :] * (.75 + .5 * twist[..., None])
        a = A[sl]
        self.obj[sl] = self.obj[sl] * (1 - a[..., None]) + col * a[..., None]
        self.cov[sl] = self.cov[sl] * (1 - a) + a

    def render(self, seed=None):
        L = self._lightmap()
        warm = look.FLAME[None, None, :]
        ground = self.ground * (.8 + .45 * L)[..., None]
        obj = self.obj * (L[..., None] * warm)
        ol = look.lum(obj)[..., None]
        obj = ol + self.chroma * (obj - ol)
        rgb = ground * (1 - self.cov[..., None]) + obj
        # down to the plate size, box-filtered
        rgb = rgb.reshape(look.H, SS, look.W, SS, 3).mean((1, 3))
        # paper never glares: a soft shoulder on the highlights
        rgb = np.where(rgb > .62, .62 + (rgb - .62) * .55, rgb)
        rgb = look.finish(rgb, self.side, seed if seed is not None else self.seed)
        return rgb
