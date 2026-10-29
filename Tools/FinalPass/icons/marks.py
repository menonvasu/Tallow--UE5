"""The TALLOW interface marks, each one drawn as a small white-line engraving (engrave.py).

Every function draws into a Plate whose design box is 100 units wide. `o` is the optical
size: how many pixels wide the plate is shown at 1080p. Line pitch and the thinnest cut
are set from it, so a 40 px HUD candle gets three bold cuts where a 108 px inventory
candle gets eight fine ones. One light, upper left, for every mark.
"""
import math

import numpy as np

from engrave import Plate, Noise1, spline, ellipse, bezier, lerp, shade, resample, normals, LIGHT


class Optic:
    def __init__(self, o):
        self.o = float(o)
        self.px = 100.0 / self.o                 # one displayed pixel, in design units

    def pitch(self, design):
        """Line pitch: never closer than ~2.4 displayed px, so tone survives filtering."""
        return max(design, 2.4 * self.px)

    def w(self, design, min_px=.9):
        return np.maximum(design, min_px * self.px)


# ------------------------------------------------------------ the candle ---
def candle_mark(p, o, height=1.0, flame='lit', cx=50.0, base=86.0, radius=11.0, max_h=52.0, dish=True, dish_w=2.35):
    """A tallow candle in a tin chamberstick. height 0..1 of a full candle; flame lit/gutter/out."""
    op = Optic(o)
    r = radius
    H = max(9.0, max_h * height)
    top = base - H
    ry_top = r * .30

    # ---- the dish: a shallow tin saucer, rim catching the light on the left
    if dish:
        drx, dry = r * dish_w, r * .50
        dy = base + 1.5
        rim_t = max(2.2, 1.6 * op.px)
        # inner floor, seen from a little above: concentric cuts on the lit half
        for i, f in enumerate(np.linspace(.42, .92, 3 if op.o < 70 else 5)):
            arc = ellipse((cx, dy), drx * f, dry * f, math.pi * .55, math.pi * 1.45, 40)
            arc = arc[arc[:, 1] < dy + .2]
            if len(arc) > 2:
                lit = shade(np.c_[-(arc[:, 0] - cx) / drx * .2, np.full(len(arc), -.9), np.full(len(arc), .5)])
                p.line(arc, op.w(.9 * (.4 + lit)), taper=(.3, .3), floor=0)
        # rim: top lip ellipse, thick to the left
        lip = ellipse((cx, dy), drx, dry, 0, 2 * math.pi, 120)
        p.contour(lip, op.w(1.9), closed=True, lost=.25, minpx=.9)
        # rim front face: short vertical cuts swelling where the band turns to the light
        band = rim_t
        n = int(drx * 2 / op.pitch(2.6))
        for i in range(n + 1):
            u = -1 + 2 * (i + .5) / (n + 1)
            x = cx + u * drx
            yy = dy + dry * math.sqrt(max(0, 1 - u * u))
            lit = float(shade([u, .15, math.sqrt(max(0, 1 - u * u))], 1.2))
            if lit > .1:
                p.line([[x, yy + .3], [x, yy + band]], op.w(1.5 * lit, .7), taper=(0, .35), floor=.5, tremor=.05)
        bottom = ellipse((cx, dy + band), drx * .98, dry * .98, 0, math.pi, 60)
        p.contour(bottom, op.w(1.1), lost=.2, minpx=.7, flip=True)
        # finger ring on the shadow side
        hx = cx + drx + r * .32
        p.line([[cx + drx - .4, dy + band * .3], [hx - r * .28, dy + band * .3]], op.w(1.0, .8), taper=(0, 0), tremor=.05)
        ring = ellipse((hx, dy + band * .3), r * .34, r * .26, 0, 2 * math.pi, 48)
        p.contour(ring, op.w(1.1), closed=True, lost=.3, minpx=.8)

    # ---- the body: cuts run up the cylinder, wide in the light, gone in the shade
    body = np.vstack([[cx - r, base], [cx - r, top], [cx + r, top], [cx + r, base]])
    p.cut(np.vstack([[cx - r - .6, base + 1], [cx - r - .6, top], [cx + r + .6, top], [cx + r + .6, base + 1]]))
    pitch = op.pitch(2.3)
    n = max(3, int(2 * r / pitch))
    wav = Noise1(int(H * 13) + 5)
    ys = np.linspace(top + ry_top * .5, base + ry_top * .4, 40)
    rad = r * (1 + .035 * wav(ys / 7.0))                                     # a dipped candle is never true
    us = [(-1 + 2 * (i + .5) / n) * .97 for i in range(n)]
    lits = [float(shade([u, -.05, math.sqrt(max(0, 1 - u * u))], 2.0)) for u in us]
    peak = max(lits)
    for u, lit in zip(us, lits):
        lit /= peak
        nz = math.sqrt(max(0, 1 - u * u))
        glow = .74 + .26 * np.clip(1 - (ys - top) / max(1, H), 0, 1) ** 2   # the flame warms the top
        w = pitch * 1.02 * lit * glow
        if w.max() > .35 * op.px:
            p.line(np.c_[cx + u * rad, ys], w, taper=(.0, .12), floor=.3, tremor=.1, swell=.08)
    # silhouette: heavy on the lit left, a hair on the right
    p.line(np.c_[cx - rad, ys], op.w(1.6), taper=(0, .1), tremor=.1)
    p.line(np.c_[cx + rad, ys], op.w(.55, .7), taper=(.1, .1), tremor=.1)

    # ---- the top: a melted lip with a pool, wick standing up out of it
    lip = ellipse((cx, top), r, ry_top, 0, 2 * math.pi, 80)
    p.cut(ellipse((cx, top), r * .93, ry_top * .8, 0, 2 * math.pi, 60))
    p.fill(ellipse((cx - r * .18, top - ry_top * .05), r * .72, ry_top * .52, 0, 2 * math.pi, 50), value=.85, alpha=.55)
    p.contour(lip, op.w(1.15), closed=True, lost=.4, minpx=.8)

    # ---- drips: tallow runs down the front and the lit side
    drips = [(-.72, .55), (-.18, .28), (.38, .42)] if height > .35 else [(-.6, .7), (.3, .5)]
    for u, length in drips:
        x = cx + u * r
        y0 = top + ry_top * math.sqrt(max(0, 1 - u * u)) * .9
        L = min(H * .8, length * max_h * .55)
        wd = r * .26
        y1 = y0 + L
        # tongue polygon: narrow neck widening to a round bead
        left = spline([[x - wd * .45, y0], [x - wd * .40, y0 + L * .45], [x - wd * .70, y1 - wd * .7]], 10)
        bead = ellipse((x, y1 - wd * .7), wd * .70, wd * .78, math.pi, 0, 20)
        right = spline([[x + wd * .70, y1 - wd * .7], [x + wd * .38, y0 + L * .5], [x + wd * .42, y0]], 10)
        poly = np.vstack([left, bead[::-1] if bead[0, 0] > bead[-1, 0] else bead, right])
        lit = float(shade([u, -.1, math.sqrt(max(0, 1 - u * u))], 1.0))
        p.cut(np.vstack([left - [op.px * .9, 0], bead + [0, op.px * .9], right + [op.px * .9, 0]]))
        if lit > .45:
            p.fill(poly, value=.75 + .25 * lit)
        else:
            p.contour(np.vstack([left, bead, right]), op.w(.9), lost=.6, minpx=.7)

    # ---- the flame, the wick, or the smoke
    wick_top = top - (3.2 if flame != 'out' else 2.6)
    if flame in ('lit', 'gutter'):
        fh = 21.0 if flame == 'lit' else 12.0
        fw = 4.9 if flame == 'lit' else 4.2
        lean = 0.0 if flame == 'lit' else 3.2
        fb = top - 1.0
        tip = (cx + lean + 1.0, fb - fh)
        shell = np.vstack([
            spline([[cx, fb + .6], [cx - fw, fb - fh * .30], [cx - fw * .55, fb - fh * .62], [tip[0], tip[1]]], 16),
            spline([[tip[0], tip[1]], [cx + fw * .65 + lean * .4, fb - fh * .58], [cx + fw * .95, fb - fh * .28], [cx, fb + .6]], 16)[1:],
        ])
        p.cut(shell + (shell - shell.mean(0)) * .28)
        p.fill(shell, value=1.0)
        # the dark heart round the wick
        core = np.vstack([
            spline([[cx, fb + .2], [cx - fw * .42, fb - fh * .16], [cx + lean * .3, fb - fh * .36]], 8),
            spline([[cx + lean * .3, fb - fh * .36], [cx + fw * .40, fb - fh * .15], [cx, fb + .2]], 8)[1:],
        ])
        if op.o > 60:
            p.cut(core)
        p.line([[cx, top + .2], [cx + .25, fb - fh * .14]], op.w(.9), value=.0, alpha=1, cut=True, taper=(0, .3))
        if flame == 'gutter':
            # a torn-off tongue of flame drifting above: the draught has it
            bit = ellipse((tip[0] + 1.2, tip[1] - 3.6), 1.1, 1.9, 0, 2 * math.pi, 18, rot=.5)
            p.fill(bit, value=.95)
    else:
        # snuffed: the black wick and one thread of smoke
        p.line([[cx, top + .2], [cx + .5, wick_top]], op.w(1.2, 1.0), value=.55, taper=(0, .2))
        smoke = spline([[cx + .5, wick_top - 1.0], [cx + 3.5, wick_top - 7], [cx - 1.5, wick_top - 14], [cx + 3.0, wick_top - 21], [cx - .5, wick_top - 28]], 14)
        p.line(smoke, op.w(1.3, .9) * np.linspace(1.0, .25, len(smoke)) / 1.0, value=.78, taper=(.1, .5), floor=0, tremor=.25)


# ------------------------------------------------------- shared form cuts ---
def plane(p, op, quad, lit, along=0, pitch=2.4, wobble=.0, value=1.0, fade=None, keep_edge=True):
    """Cut a flat face. quad = p00, p10, p11, p01; lines run p00->p10 (along=0) or p00->p01 (along=1).
    `lit` 0..1 sets line width, so the face's tone is its light. `fade(t)` darkens across the face."""
    q = [np.asarray(v, float) for v in quad]
    if along:
        q = [q[0], q[3], q[2], q[1]]
    span = max(np.linalg.norm(q[3] - q[0]), np.linalg.norm(q[2] - q[1]))
    pt = op.pitch(pitch)
    n = max(2, int(span / pt))
    for i in range(n):
        v = (i + .5) / n
        a = lerp(q[0], q[3], v)
        b = lerp(q[1], q[2], v)
        s = np.linspace(0, 1, 18)[:, None]
        pts = a + (b - a) * s
        f = lit * (fade(v) if fade else 1.0)
        w = pt * .95 * f * (1 + wobble * np.sin(s[:, 0] * 7 + i))
        if np.max(w) > .3 * op.px:
            p.line(pts, w, value=value, taper=(.06, .06), floor=.4, tremor=.08, swell=.12)


def tube(p, op, A, B, rA, rB, pitch=2.3, gamma=1.8, roll=0.0, ends=(True, True), bright=1.0, contour=True, min_px=.0, value=1.0, taper=(.08, .08)):
    """A cylinder lying from A to B: cuts run along it, swelling on the side turned to the light."""
    A, B = np.asarray(A, float), np.asarray(B, float)
    ax = B - A
    L = np.linalg.norm(ax)
    d = ax / L
    c = np.array([-d[1], d[0]])                 # across the tube, in the picture
    pt = op.pitch(pitch)
    rmax = max(rA, rB)
    n = max(2, int(2 * rmax / pt))
    for i in range(n):
        u = (-1 + 2 * (i + .5) / n) * .96
        nz = math.sqrt(max(0, 1 - u * u))
        lit = float(shade([c[0] * u, c[1] * u, nz], gamma)) * bright
        s = np.linspace(0, 1, 24)[:, None]
        pts = A + ax * s + np.outer(u * (rA + (rB - rA) * s[:, 0]), c)
        w = pt * 1.0 * lit * np.ones(len(s))
        if lit * pt > .3 * op.px:
            p.line(pts, w, taper=taper, floor=.3, tremor=.06, swell=.08, value=value, cap=taper[0] > 0)
    if contour:
        for sgn in (-1, 1):
            e0, e1 = A + c * sgn * rA, B + c * sgn * rB
            facing = float(np.dot(c * sgn, LIGHT[:2] / np.linalg.norm(LIGHT[:2])))
            p.line([e0, e1], op.w(.55 + .9 * max(0, facing), max(.6, min_px)), taper=(.05, .05), tremor=.08)


def disc_rings(p, op, c, rx, ry, r_in, r_out, pitch=2.2, bright=1.0, tilt=(0.0, -.35)):
    """Concentric cuts on a flat round face, brighter on the lit side of each ring."""
    pt = op.pitch(pitch)
    n = max(1, int((r_out - r_in) / pt))
    for i in range(n):
        f = r_in + (r_out - r_in) * (i + .5) / n
        t = np.linspace(0, 2 * math.pi, 120)
        pts = np.c_[c[0] + np.cos(t) * rx * f, c[1] + np.sin(t) * ry * f]
        # a slightly dished face: the side toward the light reads brighter
        nrm = np.c_[np.cos(t) * tilt[1] * -1, np.sin(t) * tilt[1] * -1, np.ones_like(t)]
        lit = shade(nrm, 2.0)
        lit = lit / max(1e-6, lit.max()) * bright
        w = np.clip(pt * .9 * lit, 0, pt * .95)
        # break the ring where it would vanish, so the shade side is soot
        segs = np.split(np.arange(len(t)), np.where(np.diff((w > .3 * op.px).astype(int)) != 0)[0] + 1)
        for sg in segs:
            if len(sg) > 3 and w[sg].max() > .3 * op.px:
                p.line(pts[sg], w[sg], taper=(.15, .15), floor=0, tremor=.08)


def stipple(p, op, region, density, seed, size=.9, value=1.0):
    """Flick cuts: short ticks scattered in a region, for rough grounds (a striking strip, a cloudy glass)."""
    R = np.asarray(region, float)
    rng = np.random.default_rng(seed)
    lo, hi = R.min(0), R.max(0)
    area = (hi - lo).prod()
    count = int(area * density / (op.px ** 2) * .02)
    p.clip(R)
    for _ in range(count):
        q = lo + rng.uniform(0, 1, 2) * (hi - lo)
        a = rng.uniform(0, math.pi)
        L = op.px * rng.uniform(.9, 1.8) * size
        d = np.array([math.cos(a), math.sin(a)]) * L * .5
        p.line([q - d, q + d], op.w(.8 * size, .7), taper=(.3, .3), floor=.2, tremor=0, value=value)
    p.unclip()


class Box:
    """A box seen from above at three quarters. ex: length axis, ez: depth axis (into the picture), up: height."""

    def __init__(self, origin, ex, ez, up=(0, -1)):
        self.o = np.asarray(origin, float)
        self.ex, self.ez, self.up = np.asarray(ex, float), np.asarray(ez, float), np.asarray(up, float)

    def __call__(self, x, y, z):
        return self.o + self.ex * x + self.up * y + self.ez * z

    def lit(self, axis):
        """Light on a face whose outward normal runs along the picture-plane direction `axis`."""
        if axis == 'top':
            return float(shade([0, -1, .45], 1.0))
        v = {'x': self.ex, 'z': -self.ez, '-x': -self.ex, '-z': self.ez}[axis]
        v = v / np.linalg.norm(v)
        return float(shade([v[0], v[1], .55], 1.0))


def tube_path(p, op, path, radius, pitch=2.2, gamma=1.8, bright=1.0, contour=True, rscale=None):
    """A bar bent along `path` (Nx2). Cuts follow the bend; the light turns with it."""
    P = resample(np.asarray(path, float), .6)
    N, T = normals(P)
    n = max(2, int(2 * radius / op.pitch(pitch)))
    rs = radius * (np.ones(len(P)) if rscale is None else np.interp(np.linspace(0, 1, len(P)), np.linspace(0, 1, len(rscale)), rscale))
    for i in range(n):
        u = (-1 + 2 * (i + .5) / n) * .95
        nz = math.sqrt(max(0, 1 - u * u))
        nrm = np.c_[N[:, 0] * u, N[:, 1] * u, np.full(len(P), nz)]
        lit = shade(nrm, gamma) * bright
        w = op.pitch(pitch) * lit
        if w.max() > .3 * op.px:
            p.line(P + N * (u * rs)[:, None], w, taper=(.05, .05), floor=.2, tremor=.05, swell=.06)
    if contour:
        L2 = LIGHT[:2] / np.linalg.norm(LIGHT[:2])
        for sgn in (-1, 1):
            facing = np.clip((N * sgn) @ L2, 0, 1)
            p.line(P + N * (sgn * rs)[:, None], op.w(.5, .7) + op.w(1.0) * facing, taper=(.04, .04), tremor=.06)


# ------------------------------------------------------------ the things ---
def matches_mark(p, o):
    """A matchbox, the tray pushed out with the heads showing; the sleeve's label and striking side."""
    op = Optic(o)
    b = Box((10, 64), (.78, .17), (.31, -.37))
    Lx, Hy, Dz = 64, 13, 44
    # outer sleeve faces
    top = [b(0, Hy, 0), b(Lx, Hy, 0), b(Lx, Hy, Dz), b(0, Hy, Dz)]
    front = [b(0, 0, 0), b(Lx, 0, 0), b(Lx, Hy, 0), b(0, Hy, 0)]
    p.cut(np.array([b(-1, -1, -1), b(Lx + 1, -1, -1), b(Lx + 1, Hy + 1, -1), b(Lx + 1, Hy + 1, Dz + 1), b(-1, Hy + 1, Dz + 1), b(-1, Hy + 1, -1)]))
    plane(p, op, top, .95, along=0, pitch=2.4, fade=lambda v: .75 + .25 * (1 - v))
    # the label: an oval cartouche with a border, cut back to soot and re-engraved finer
    lc = b(Lx * .5, Hy, Dz * .5)
    lab = ellipse(lc, 15, 7.5, 0, 2 * math.pi, 64, rot=math.atan2(b.ex[1], b.ex[0]) * .9)
    p.cut(lab * 1.0 + (lab - lc) * .22)
    p.contour(lab, op.w(1.1), closed=True, lost=.6, minpx=.8)
    if op.o > 70:
        inner = ellipse(lc, 10, 4.4, 0, 2 * math.pi, 48, rot=math.atan2(b.ex[1], b.ex[0]) * .9)
        p.contour(inner, op.w(.6), closed=True, lost=.7, minpx=.6)
        for k in (-1, 0, 1):
            q0 = lc + b.ex * -6 + np.array([0, k * 2.0]) + b.ez * 0
            p.line([q0, q0 + b.ex * (12 - abs(k) * 4)], op.w(.7), taper=(.2, .2), tremor=.05)
    # front: the striking side, rough with flicks, a band across its middle
    plane(p, op, front, .42, along=0, pitch=2.6)
    strip = [b(2, 3.0, 0), b(Lx - 2, 3.0, 0), b(Lx - 2, Hy - 3.0, 0), b(2, Hy - 3.0, 0)]
    p.cut(np.array(strip))
    stipple(p, op, np.array(strip), 2.2, 7, size=.9, value=.85)
    # the tray, pushed out of the far end
    t0, t1, ins = Lx - 2, Lx + 24, 1.6
    tr_front = [b(t0, ins, ins), b(t1, ins, ins), b(t1, Hy - ins, ins), b(t0, Hy - ins, ins)]
    tr_end = [b(t1, ins, ins), b(t1, ins, Dz - ins), b(t1, Hy - ins, Dz - ins), b(t1, Hy - ins, ins)]
    tr_floor = [b(Lx, ins + 1.4, ins), b(t1, ins + 1.4, ins), b(t1, ins + 1.4, Dz - ins), b(Lx, ins + 1.4, Dz - ins)]
    p.cut(np.array([b(Lx, ins - .8, ins - .8), b(t1 + .8, ins - .8, ins - .8), b(t1 + .8, Hy - ins + .8, ins - .8), b(t1 + .8, Hy - ins + .8, Dz - ins + .8), b(Lx, Hy - ins + .8, Dz - ins + .8), b(Lx, Hy + 1, 0)]))
    plane(p, op, [b(Lx, ins, ins), b(t1, ins, ins), b(t1, Hy - ins, ins), b(Lx, Hy - ins, ins)], .5, along=0, pitch=2.6)
    plane(p, op, tr_end, .12, along=0, pitch=2.8)
    # matches lying in the tray, heads at the open end
    n = 6 if op.o > 70 else 4
    for k in range(n):
        z = ins + 3.5 + (Dz - 2 * ins - 7) * k / (n - 1)
        s0, s1 = b(Lx - 1, Hy - ins - 1.2, z), b(t1 - 4.2, Hy - ins - 1.0, z)
        p.line([s0, s1], op.w(1.35, 1.0), value=.88, taper=(0, 0), tremor=.04)
        head = b(t1 - 3.0, Hy - ins - .6, z)
        hd = ellipse(head, 3.0, 2.0, 0, 2 * math.pi, 24, rot=math.atan2(b.ex[1], b.ex[0]))
        p.cut(hd + (hd - head) * .45)
        p.fill(hd, value=.93)
        p.cut(ellipse(head + np.array([.6, .7]), 1.1, .7, 0, 2 * math.pi, 12))
    p.contour(np.array(tr_front + [tr_front[0]])[:, :], op.w(.9), lost=.4, minpx=.7)
    p.contour(np.array([b(t1, ins, ins), b(t1, ins, Dz - ins), b(t1, Hy - ins, Dz - ins)]), op.w(.7), lost=.3, minpx=.6)
    # sleeve silhouette
    p.contour(np.array([b(0, 0, 0), b(0, Hy, 0), b(0, Hy, Dz), b(Lx, Hy, Dz)]), op.w(1.5), lost=.35, minpx=.9)
    p.contour(np.array([b(0, 0, 0), b(Lx, 0, 0), b(Lx, Hy, 0)]), op.w(1.1), lost=.35, minpx=.8, flip=True)
    p.line([b(0, Hy, 0), b(Lx, Hy, 0)], op.w(1.0, .8), taper=(0, 0), tremor=.06)


def crowbar_mark(p, o):
    """A gooseneck bar, the claw end curled over, a chisel at the foot. Cold iron, one bright edge."""
    op = Optic(o)
    shaft = spline([[11, 88.5], [18, 88], [26, 80], [44, 57], [63, 33], [70, 23], [76, 15.5], [84, 13.5], [88.5, 18.5], [87.5, 26]], 14, tension=.55)
    r = 3.7 if op.o > 70 else 4.6
    rs = np.r_[np.linspace(.55, .95, 5), np.ones(17), np.linspace(1.0, 1.35, 6)]
    p.cut(np.vstack([shaft + normals(shaft)[0] * (r + 1.2), (shaft - normals(shaft)[0] * (r + 1.2))[::-1]]))
    tube_path(p, op, shaft, r, pitch=2.0 if op.o > 70 else 2.6, gamma=1.6, rscale=rs)
    # the claw: a split at the curled end
    end = shaft[-1]
    nrm, tan = normals(shaft)
    tip = end + tan[-1] * 1.2
    p.line([end - tan[-1] * 8.5, tip], op.w(1.6, 1.1), cut=True, taper=(.7, 0), floor=0)
    p.cut(np.array([tip + nrm[-1] * 1.6, tip - nrm[-1] * 1.6, tip - tan[-1] * 3.2]))
    # the chisel foot: flattened, a bright bevel
    foot = shaft[0]
    d = tan[0]
    nn = nrm[0]
    bevel = np.array([foot + nn * r * .62, foot - nn * r * .62, foot - d * 4.2 - nn * r * .15, foot - d * 4.2 + nn * r * .3])
    p.fill(bevel, value=.9)
    p.contour(np.vstack([bevel, bevel[:1]]), op.w(.7), lost=.4, minpx=.6)


def fuse_mark(p, o):
    """A cartridge fuse: brass caps, cloudy glass, the wire run through it."""
    op = Optic(o)
    A, B = np.array([22.0, 80.0]), np.array([78.0, 24.0])
    d = (B - A) / np.linalg.norm(B - A)
    c = np.array([-d[1], d[0]])
    r = 9.0 if op.o > 70 else 10.0
    rc = r * 1.08
    k0, k1 = .24, .76
    C0, C1 = A + (B - A) * k0, A + (B - A) * k1
    outline = np.array([A + c * rc, B + c * rc, B - c * rc, A - c * rc])
    p.cut(outline + (outline - outline.mean(0)) * .06)
    # glass: only its edges, two long reflections, the wire, a little cloud
    for sgn in (-1, 1):
        e = np.array([C0 + c * sgn * r, C1 + c * sgn * r])
        facing = float(np.dot(c * sgn, LIGHT[:2] / np.linalg.norm(LIGHT[:2])))
        p.line(e, op.w(.6 + .7 * max(0, facing), .7), taper=(0, 0), tremor=.06)
    for u, f0, f1, wd in ((-.62, .10, .78, 1.6), (-.30, .18, .50, .9), (.55, .55, .9, .55)):
        if op.o < 70 and u > 0:
            continue
        side = c * u * r * (-1 if np.dot(c, LIGHT[:2]) > 0 else 1)      # u < 0 is the side toward the light
        p.line([lerp(C0, C1, f0) + side, lerp(C0, C1, f1) + side], op.w(wd, .8), taper=(.35, .35), floor=0, value=.95)
    if op.o > 70:
        glass = np.array([C0 + c * r * .9, C1 + c * r * .9, C1 - c * r * .9, C0 - c * r * .9])
        stipple(p, op, glass, .55, 13, size=.8, value=.6)
    wire = np.array([lerp(C0, C1, t) + c * (math.sin(t * math.pi * 3) * r * .16 + (t - .5) * .8) for t in np.linspace(0, 1, 30)])
    p.line(wire, op.w(.85, .8), taper=(0, 0), tremor=.05, value=.9)
    # the brass caps, each a short tube with two turned grooves
    for (P0, P1) in ((A, C0 + d * .4), (C1 - d * .4, B)):
        tube(p, op, P0, P1, rc, rc, pitch=2.1, gamma=2.2, taper=(0, 0))
        for f in (.28, .72) if op.o > 70 else (.5,):
            g = lerp(P0, P1, f)
            p.line([g + c * rc * .96, g - c * rc * .96], op.w(.9, .8), cut=True, taper=(0, 0), tremor=.02)
        # the end of the cap, a small turned face
    end = ellipse(B, rc * .32, rc, 0, 2 * math.pi, 48, rot=math.atan2(d[1], d[0]))
    p.contour(end, op.w(.8), closed=True, lost=.3, minpx=.7)
    back = ellipse(A, rc * .32, rc, math.pi / 2, 3 * math.pi / 2, 30, rot=math.atan2(d[1], d[0]))
    p.line(back, op.w(.9, .7), taper=(0, 0), tremor=.03)
    for P0 in (C0, C1):
        rim = ellipse(P0, rc * .30, rc, -math.pi / 2, math.pi / 2, 30, rot=math.atan2(d[1], d[0]))
        p.line(rim, op.w(.8, .7), taper=(0, 0), tremor=.03)


def reel_mark(p, o):
    """A small film reel: a pressed flange with four windows onto the wound film, a tail of film let out."""
    op = Optic(o)
    c = np.array([47.0, 47.0])
    R = 36.0
    ry = .94
    # the film's tail, curling away bottom right with its sprocket holes
    tail = spline([[c[0] + R * .55, c[1] + R * .55], [c[0] + R * .95, c[1] + R * .82], [93, 88], [96, 97]], 12)
    nrm, _ = normals(tail)
    wtail = 8.0
    band = np.vstack([tail + nrm * wtail * .5, (tail - nrm * wtail * .5)[::-1]])
    p.cut(band + (band - band.mean(0)) * .08)
    p.fill(band, value=.62, alpha=.9)
    for t in np.linspace(.18, .92, 5 if op.o > 70 else 3):
        i = int(t * (len(tail) - 1))
        q = tail[i]
        for s in (-1, 1):
            h = q + nrm[i] * s * wtail * .32
            p.cut(ellipse(h, .9 if op.o > 70 else 1.2, .7 if op.o > 70 else 1.0, 0, 2 * math.pi, 10))
    # flange: concentric turnings, cut back first so the tail tucks behind
    p.cut(ellipse(c, R + 1.2, (R + 1.2) * ry, 0, 2 * math.pi, 120))
    disc_rings(p, op, c, 1.0, ry, R * .30, R * .97, pitch=2.3, bright=1.0)
    rim = ellipse(c, R, R * ry, 0, 2 * math.pi, 160)
    p.contour(rim, op.w(1.6), closed=True, lost=.25, minpx=.9)
    p.contour(ellipse(c, R * .9, R * .9 * ry, 0, 2 * math.pi, 140), op.w(.6), closed=True, lost=.4, minpx=.6)
    # four windows: the film's edge seen through them as thin arcs
    for k in range(4):
        a0 = k * math.pi / 2 + math.pi / 4 - .52
        a1 = a0 + 1.04
        outer = ellipse(c, R * .80, R * .80 * ry, a0, a1, 24)
        inner = ellipse(c, R * .44, R * .44 * ry, a1, a0, 18)
        win = np.vstack([outer, inner])
        p.cut(win)
        p.contour(np.vstack([win, win[:1]]), op.w(.8), lost=.3, minpx=.7)
        if op.o > 70:
            p.clip(win)
            for f in np.linspace(.50, .76, 4):
                arc = ellipse(c, R * f, R * f * ry, a0, a1, 24)
                p.line(arc, op.w(.55, .6), taper=(.1, .1), value=.6, tremor=.03)
            p.unclip()
    # the hub with its square key
    p.cut(ellipse(c, R * .22, R * .22 * ry, 0, 2 * math.pi, 48))
    p.contour(ellipse(c, R * .20, R * .20 * ry, 0, 2 * math.pi, 48), op.w(1.2), closed=True, lost=.25, minpx=.8)
    sq = R * .07
    p.fill(np.array([c + [-sq, -sq], c + [sq, -sq], c + [sq, sq], c + [-sq, sq]]) + [.3, .2], value=.0, alpha=0)
    p.contour(np.array([c + [-sq, -sq], c + [sq, -sq], c + [sq, sq], c + [-sq, sq], c + [-sq, -sq]]), op.w(.8), lost=.4, minpx=.7)


def map_mark(p, o):
    """A plan folded in three, half open: lit leaves carry the pencilled rooms as dark lines."""
    op = Optic(o)
    topz = [[10, 30], [37, 21], [63, 30], [90, 21]]
    botz = [[10, 81], [37, 72], [63, 81], [90, 72]]
    lits = [.80, .26, .80]
    leaves = []
    for k in range(3):
        q = [np.array(topz[k], float), np.array(topz[k + 1], float), np.array(botz[k + 1], float), np.array(botz[k], float)]
        leaves.append(q)
    p.cut(np.array(topz + botz[::-1]) + np.array([[-1, -1]] * 4 + [[1, 1]] * 4))
    for k, q in enumerate(leaves):
        plane(p, op, q, lits[k], along=0, pitch=2.3, fade=(lambda v: .8 + .2 * v) if k != 1 else None)
    # the pencilled plan, cut through the lit leaves
    def on_leaf(k, x, y):
        q = leaves[k]
        return lerp(lerp(q[0], q[1], x), lerp(q[3], q[2], x), y)
    wall = op.w(1.2, 1.0)
    rooms = [
        (0, [(.18, .16), (.86, .16), (.86, .52), (.18, .52), (.18, .16)]),
        (0, [(.18, .52), (.18, .86), (.86, .86), (.86, .52)]),
        (0, [(.52, .52), (.52, .70)]),
        (2, [(.12, .16), (.80, .16), (.80, .86), (.12, .86), (.12, .16)]),
        (2, [(.12, .45), (.55, .45), (.55, .86)]),
    ]
    for k, pts in rooms:
        P = np.array([on_leaf(k, x, y) for x, y in pts])
        p.line(P, wall, cut=True, taper=(0, 0), tremor=.18)
    if op.o > 70:
        for i in range(5):                                   # a stair, drawn as treads
            y = .58 + i * .055
            p.line([on_leaf(2, .62, y), on_leaf(2, .78, y)], op.w(.7), cut=True, taper=(0, 0), tremor=.05)
    xm = on_leaf(2, .34, .66)                                # a cross where the bolt was
    for s in (-1, 1):
        p.line([xm + [-3.2, -3.2 * s], xm + [3.2, 3.2 * s]], op.w(1.5, 1.1), cut=True, taper=(.1, .1), tremor=.05)
    # the middle leaf, turned from the light: a faint pencil line or two
    p.line([on_leaf(1, .2, .35), on_leaf(1, .8, .35)], op.w(.6), value=.7, taper=(.2, .2))
    p.line([on_leaf(1, .5, .35), on_leaf(1, .5, .8)], op.w(.6), value=.7, taper=(.2, .2))
    # folds and edges
    for k in range(4):
        facing = .8 if k in (0, 2) else .4
        p.line([topz[k], botz[k]], op.w(.7 + facing), taper=(.03, .03), tremor=.1)
    p.contour(np.array(topz), op.w(1.1), lost=.5, minpx=.8)
    p.contour(np.array(botz), op.w(.9), lost=.5, minpx=.7)


def book_mark(p, o):
    """Maisie's book: a cloth notebook lying closed, a paper label on the board, a ribbon let out."""
    op = Optic(o)
    b = Box((12, 74), (.74, .20), (.42, -.52))
    Lx, Hy, Dz = 60, 14, 82
    ov = 1.6                                                    # boards overhang the pages
    p.cut(np.array([b(-1, -1, -1), b(Lx + 1, -1, -1), b(Lx + 1, Hy + 1, -1), b(Lx + 1, Hy + 1, Dz + 1), b(-1, Hy + 1, Dz + 1), b(-1, Hy + 1, -1)]))
    # page block: fine leaves on the tail and the fore-edge
    tail = [b(0, ov, 0), b(Lx - ov, ov, 0), b(Lx - ov, Hy - ov, 0), b(0, Hy - ov, 0)]
    fore = [b(Lx - ov, ov, 0), b(Lx - ov, ov, Dz - ov), b(Lx - ov, Hy - ov, Dz - ov), b(Lx - ov, Hy - ov, 0)]
    plane(p, op, [tail[0], tail[3], tail[2], tail[1]], .55, along=1, pitch=1.9)
    plane(p, op, [fore[0], fore[3], fore[2], fore[1]], .22, along=1, pitch=1.9)
    # lower board edge
    p.line([b(0, 0, 0), b(Lx, 0, 0), b(Lx, 0, Dz)], op.w(1.0), taper=(0, 0), tremor=.06)
    # the cover: cloth grain cut across the board, brighter toward the spine
    cover = [b(0, Hy, 0), b(Lx, Hy, 0), b(Lx, Hy, Dz), b(0, Hy, Dz)]
    plane(p, op, cover, .80, along=1, pitch=2.3, fade=lambda v: .78 + .22 * (1 - v))
    # a blind-stamped rule round the board
    inset = [b(4, Hy, 4), b(Lx - 4, Hy, 4), b(Lx - 4, Hy, Dz - 4), b(4, Hy, Dz - 4), b(4, Hy, 4)]
    p.line(np.array(inset), op.w(.8, .7), cut=True, taper=(0, 0), tremor=.05)
    # the label: a pasted slip, lit, with two written lines cut into it
    lab = [b(14, Hy, 30), b(Lx - 12, Hy, 30), b(Lx - 12, Hy, 50), b(14, Hy, 50)]
    p.cut(np.array(lab) + (np.array(lab) - np.mean(lab, 0)) * .12)
    p.fill(np.array(lab), value=.82)
    for z, x1 in ((36, Lx - 16), (44, Lx - 24)):
        p.line([b(17, Hy, z), b(x1, Hy, z)], op.w(.9, .8), cut=True, taper=(.1, .3), tremor=.25)
    # spine: rounded, catching the light, two raised bands
    sp = [b(0, 0, 0), b(0, Hy, 0), b(0, Hy, Dz), b(0, 0, Dz)]
    p.line([b(0, Hy, 0), b(0, Hy, Dz)], op.w(1.7), taper=(0, 0), tremor=.08)
    # the ribbon, let out of the tail and falling over the edge
    r0 = b(Lx * .55, Hy * .5, 0)
    rib = spline([r0, r0 + [1.2, 7.0], r0 + [-1.5, 13.5], r0 + [.8, 19]], 10)
    nrm, _ = normals(rib)
    wr = 2.6
    band = np.vstack([rib + nrm * wr * .5, (rib - nrm * wr * .5)[::-1]])
    p.cut(band + (band - band.mean(0)) * .25)
    p.fill(band, value=.9)
    end = rib[-1]
    p.cut(np.array([end + [-wr * .5, -.2], end + [wr * .5, -.2], end + [0, -2.2]]))
    # silhouette
    p.contour(np.array([b(0, 0, 0), b(0, Hy, 0), b(0, Hy, Dz), b(Lx, Hy, Dz)]), op.w(1.4), lost=.35, minpx=.9)
    p.contour(np.array([b(Lx, Hy, Dz), b(Lx, 0, Dz), b(Lx, 0, 0)]), op.w(.8), lost=.3, minpx=.7)


def letter_mark(p, o):
    """An envelope with a sealed flap, the letter's corner standing out of it."""
    op = Optic(o)
    rot = -.16
    cR, sR = math.cos(rot), math.sin(rot)
    def q(x, y):
        return np.array([50 + x * cR - y * sR, 54 + x * sR + y * cR])
    W, H = 38, 25
    # the letter's corner, behind
    sheet = [q(-24, -38), q(16, -40), q(18, -10), q(-22, -8)]
    p.cut(np.array(sheet) + (np.array(sheet) - np.mean(sheet, 0)) * .05)
    plane(p, op, sheet, .45, along=0, pitch=2.5)
    for k, x1 in ((0, 10), (1, 4), (2, 12)):
        y = -32 + k * 6
        p.line([q(-18, y), q(x1, y + .5)], op.w(.9, .8), cut=True, taper=(.1, .3), tremor=.3)
    p.contour(np.array(sheet + [sheet[0]]), op.w(.9), lost=.4, minpx=.7)
    # envelope body
    env = [q(-W, -H + 6), q(W, -H + 6), q(W, H), q(-W, H)]
    p.cut(np.array(env) + (np.array(env) - np.mean(env, 0)) * .04)
    plane(p, op, env, .66, along=0, pitch=2.4, fade=lambda v: .85 + .15 * (1 - v))
    # the folded flaps as lit and shaded triangles
    apex = q(0, 6)
    left = [q(-W, H), apex, q(-W, -H + 6)]
    p.cut(np.array([q(-W, -H + 6), q(W, -H + 6), apex]))
    plane(p, op, [q(-W, -H + 6), q(W, -H + 6), apex + (q(W, -H + 6) - q(-W, -H + 6)) * .001, apex], .82, along=0, pitch=2.2)
    for a, bq in ((q(-W, -H + 6), apex), (apex, q(W, -H + 6)), (q(-W, H), q(-6, 2)), (q(W, H), q(6, 2))):
        p.line([a, bq], op.w(1.0), cut=False, taper=(.05, .05), tremor=.06, value=.6)
        p.line([a + [0, .9], bq + [0, .9]], op.w(.7), cut=True, taper=(.05, .05), tremor=.06)
    # the seal: a pressed disc, its stamp cut in
    s = apex + np.array([0, -.5])
    seal = ellipse(s, 6.3, 5.9, 0, 2 * math.pi, 40)
    p.cut(seal * 1.0 + (seal - s) * .25)
    blob = np.array([s + [math.cos(t) * (6.0 + .6 * math.sin(t * 5)), math.sin(t) * (5.6 + .5 * math.cos(t * 4))] for t in np.linspace(0, 2 * math.pi, 40)])
    p.fill(blob, value=.92)
    p.contour(ellipse(s, 3.4, 3.1, 0, 2 * math.pi, 30), op.w(.9), closed=True, value=0, cut=True, lost=1.0, minpx=.7)
    if op.o > 70:
        p.line([s + [-1.2, 1.6], s + [0, -1.8], s + [1.2, 1.6]], op.w(.8), cut=True, taper=(.1, .1), tremor=.02)
    p.contour(np.array(env + [env[0]]), op.w(1.3), lost=.35, minpx=.9)


def cutting_mark(p, o):
    """A torn cutting from the paper: a black headline, a column rule, close-set lines of print."""
    op = Optic(o)
    rot = .08
    cR, sR = math.cos(rot), math.sin(rot)
    def q(x, y):
        return np.array([50 + x * cR - y * sR, 50 + x * sR + y * cR])
    rng = np.random.default_rng(3)
    top = [q(-30, -40), q(30, -40)]
    right = [q(30 + rng.uniform(-1.4, 1.4), y) for y in np.linspace(-40, 38, 14)]
    bottom = [q(x, 38 + rng.uniform(-1.8, 1.8)) for x in np.linspace(30, -30, 12)]
    left = [q(-30, 38), q(-30, -40)]
    sheet = np.array(top + right + bottom + left)
    p.cut(sheet + (sheet - sheet.mean(0)) * .05)
    p.fill(sheet, value=.56)
    # headline: two bold bars cut out of the paper
    for y, x1 in ((-32, 22), (-24.5, 8)):
        p.line([q(-24, y), q(x1, y)], op.w(3.6, 2.0), cut=True, taper=(0, 0), tremor=.05, floor=1)
    p.line([q(-26, -17.5), q(26, -17.5)], op.w(.8, .7), cut=True, taper=(0, 0), tremor=.04)
    p.line([q(0, -13), q(0, 34)], op.w(.7, .6), cut=True, taper=(0, 0), tremor=.04)
    pitch = op.pitch(3.0)
    for col, (x0, x1) in enumerate(((-25, -3), (3, 25))):
        y = -12.0
        while y < 33:
            xe = x1 - (rng.uniform(0, 8) if rng.uniform() < .2 else 0)
            p.line([q(x0, y), q(xe, y)], op.w(1.05, .8), cut=True, taper=(0, 0), tremor=.03, floor=1)
            y += pitch
    p.contour(np.vstack([sheet, sheet[:1]]), op.w(.9), lost=.4, minpx=.7)


def photo_mark(p, o):
    """A cabinet photograph: a thick mount, an oval window, a sitter who is only a shape."""
    op = Optic(o)
    rot = -.07
    cR, sR = math.cos(rot), math.sin(rot)
    def q(x, y):
        return np.array([50 + x * cR - y * sR, 50 + x * sR + y * cR])
    W, H = 31, 41
    card = [q(-W, -H), q(W, -H), q(W, H), q(-W, H)]
    p.cut(np.array(card) + (np.array(card) - np.mean(card, 0)) * .05)
    plane(p, op, card, .66, along=0, pitch=2.3, fade=lambda v: .9 + .1 * (1 - v))
    oc = q(0, -5)
    ov = ellipse(oc, 21, 28, 0, 2 * math.pi, 80, rot=rot)
    p.cut(ov + (ov - oc) * .07)
    # inside the oval: a dim ground, a lit shoulder and head, the face left dark
    p.clip(ov)
    hatch_pitch = op.pitch(2.6)
    p.hatch(ov, math.radians(-50), hatch_pitch, lambda pts: np.full(len(pts), .28 * hatch_pitch), value=.8)
    bust = np.vstack([
        spline([q(-19, 25), q(-15, 10), q(-7, 5), q(-6, -1)], 10),
        ellipse(q(0, -10), 7.5, 9.5, math.pi * .62, math.pi * 2.38, 40, rot=rot),
        spline([q(6, -1), q(7, 5), q(15, 10), q(19, 25)], 10),
    ])
    p.cut(bust)
    plane(p, op, [q(-19, 25), q(-6, 3), q(-6, 6), q(-19, 26)], .0)
    for sgn in (-1,):
        p.contour(bust, op.w(1.2), lost=.2, minpx=.8)
    p.unclip()
    p.contour(ov, op.w(1.3), closed=True, lost=.4, minpx=.9, flip=True)
    p.contour(ellipse(oc, 23.5, 30.5, 0, 2 * math.pi, 80, rot=rot), op.w(.6), closed=True, cut=True, lost=1, minpx=.6)
    # the studio's line under the oval
    p.line([q(-12, 33), q(12, 33)], op.w(.9, .8), cut=True, taper=(.2, .2), tremor=.05)
    p.contour(np.array(card + [card[0]]), op.w(1.3), lost=.35, minpx=.9)


def sewing_mark(p, o):
    """A cotton reel with its thread, a needle run through the winding."""
    op = Optic(o)
    c = np.array([46.0, 54.0])
    rw, hh = 15.0, 26.0
    ry = 5.2
    # flanges and the wound thread between them
    p.cut(np.array([c + [-rw - 6, -hh - ry - 2], c + [rw + 6, -hh - ry - 2], c + [rw + 6, hh + ry + 2], c + [-rw - 6, hh + ry + 2]]))
    wind_top, wind_bot = c[1] - hh + 4, c[1] + hh - 4
    pt = op.pitch(1.9)
    y = wind_top
    while y < wind_bot:
        us = np.linspace(-1, 1, 30) * .98
        xs = c[0] + us * rw * .92
        ys = y + np.sqrt(np.clip(1 - us ** 2, 0, 1)) * ry * .55 + us * .9
        lit = shade(np.c_[us, np.full_like(us, -.1), np.sqrt(np.clip(1 - us ** 2, 0, 1))], 1.6)
        w = pt * .95 * lit
        p.line(np.c_[xs, ys], w, taper=(.0, .0), floor=0, tremor=.04)
        y += pt
    p.line([[c[0] - rw * .92, wind_top], [c[0] - rw * .92, wind_bot + ry * .5]], op.w(1.1), taper=(0, 0), tremor=.05)
    p.line([[c[0] + rw * .92, wind_top], [c[0] + rw * .92, wind_bot + ry * .5]], op.w(.5, .6), taper=(0, 0), tremor=.05)
    # wooden flanges: a turned rim top and bottom
    for yy, front in ((c[1] - hh, True), (c[1] + hh, True)):
        top_face = ellipse((c[0], yy - 2.2), rw + 4.5, ry, 0, 2 * math.pi, 64)
        p.cut(top_face + [0, 0])
        p.cut(np.array([[c[0] - rw - 4.6, yy - 2.2], [c[0] + rw + 4.6, yy - 2.2], [c[0] + rw + 4.6, yy + 2.2], [c[0] - rw - 4.6, yy + 2.2]]))
        p.cut(ellipse((c[0], yy + 2.2), rw + 4.6, ry, 0, math.pi, 32))
        disc_rings(p, op, (c[0], yy - 2.2), rw + 4.5, ry, .25, .98, pitch=2.0, bright=.95, tilt=(0, -.6))
        p.contour(ellipse((c[0], yy - 2.2), rw + 4.5, ry, 0, 2 * math.pi, 64), op.w(1.1), closed=True, lost=.35, minpx=.8)
        n = int((rw + 4.5) * 2 / op.pitch(2.2))
        for i in range(n + 1):
            u = -1 + 2 * (i + .5) / (n + 1)
            x = c[0] + u * (rw + 4.5)
            y0 = yy - 2.2 + ry * math.sqrt(max(0, 1 - u * u))
            lit = float(shade([u, .1, math.sqrt(max(0, 1 - u * u))], 1.4))
            if lit > .12:
                p.line([[x, y0], [x, y0 + 4.4]], op.w(1.4 * lit, .7), taper=(0, .2), floor=.5, tremor=.03)
        p.contour(ellipse((c[0], yy + 2.2), rw + 4.5, ry, 0, math.pi, 32), op.w(.9), lost=.3, minpx=.7, flip=True)
    p.cut(ellipse((c[0], c[1] - hh - 2.2), 3.0, 1.2, 0, 2 * math.pi, 20))
    # the needle, run in at an angle, its eye catching
    n0, n1 = np.array([68.0, 20.0]), np.array([38.0, 66.0])
    d = (n1 - n0) / np.linalg.norm(n1 - n0)
    nn = np.array([-d[1], d[0]])
    p.line([n0 + nn * 1.2, n1 - d * 10 + nn * .4], op.w(.9), cut=True, taper=(0, 0))
    p.line([n0, n0 + d * 27], op.w(1.8, 1.1) * np.linspace(1, .55, 12), taper=(0, .4), floor=.2, tremor=.03)
    p.cut(ellipse(n0 + d * 3.5, .6, 1.8, 0, 2 * math.pi, 14, rot=math.atan2(d[1], d[0]) + math.pi / 2))
    # a loose end of thread through the eye, looping off
    tail = spline([n0 + d * 3.5, n0 + [7, -3], [86, 22], [88, 40], [80, 50]], 10)
    p.line(tail, op.w(1.1, .8), taper=(.0, .4), floor=.1, tremor=.15, value=.9)


def drawing_mark(p, o):
    """A child's drawing on a sheet: a figure with too many arms, pressed hard in black crayon."""
    op = Optic(o)
    rot = .1
    cR, sR = math.cos(rot), math.sin(rot)
    def q(x, y):
        return np.array([50 + x * cR - y * sR, 50 + x * sR + y * cR])
    W, H = 36, 30
    sheet = [q(-W, -H), q(W, -H), q(W, H), q(-W, H)]
    p.cut(np.array(sheet) + (np.array(sheet) - np.mean(sheet, 0)) * .05)
    plane(p, op, sheet, .64, along=0, pitch=2.2, fade=lambda v: .85 + .15 * (1 - v))
    cw = op.w(2.2, 1.5)
    body = q(-5, -3)
    rng = np.random.default_rng(21)
    shag = np.array([body + [math.cos(t) * (11 + rng.uniform(-1.6, 1.6)), math.sin(t) * (10 + rng.uniform(-1.6, 1.6))] for t in np.linspace(0, 2 * math.pi, 34, endpoint=False)])
    for k in range(10):                                          # ten arms, each with an elbow and a hand
        a0 = math.pi * 2 * k / 10 + .3
        r0 = body + np.array([math.cos(a0) * 10, math.sin(a0) * 9])
        el = body + np.array([math.cos(a0 + .25) * 17, math.sin(a0 + .25) * 15])
        hd = body + np.array([math.cos(a0 - .15) * 23, math.sin(a0 - .15) * 20])
        if hd[1] > q(0, H - 3)[1] or hd[1] < q(0, -H + 3)[1] or hd[0] < q(-W + 3, 0)[0]:
            continue
        p.line([r0, el, hd], cw * .75, cut=True, taper=(0, .2), tremor=.25)
        for f in (-.5, 0, .5):
            p.line([hd, hd + np.array([math.cos(a0 - .15 + f), math.sin(a0 - .15 + f)]) * 2.6], cw * .45, cut=True, taper=(0, .3))
    p.cut(shag)
    for ex in (-4.2, 3.2):                                         # two great eyes
        p.fill(ellipse(body + [ex, -3], 2.8, 3.0, 0, 2 * math.pi, 16), value=.95)
        p.cut(ellipse(body + [ex + .6, -2.6], 1.1, 1.3, 0, 2 * math.pi, 10))
    grin = np.vstack([spline([body + [-7, 2.5], body + [0, 6.5], body + [7, 2.2]], 10), spline([body + [7, 2.2], body + [0, 4.4], body + [-7, 2.5]], 10)])
    p.fill(grin, value=.95)
    for gx in np.linspace(-5, 5, 6):
        p.line([body + [gx, 2.6 + (1 - abs(gx) / 7) * 1.2], body + [gx, 4.4 + (1 - abs(gx) / 7) * 1.8]], op.w(.6), cut=True, taper=(0, 0))
    small = q(23, 12)
    p.line(ellipse(small + [0, -8], 2.6, 2.6, 0, 2 * math.pi, 16), cw * .6, cut=True, taper=(0, 0))
    p.line([small + [0, -5.4], small + [0, 4], small + [-3, 11]], cw * .6, cut=True, taper=(0, 0))
    p.line([small + [0, 4], small + [3, 11]], cw * .6, cut=True, taper=(0, 0))
    p.line([small + [-4, -2], small + [4, -2]], cw * .6, cut=True, taper=(0, 0))
    p.contour(np.array(sheet + [sheet[0]]), op.w(1.2), lost=.35, minpx=.8)


# ------------------------------------------------------- the house's signs ---
def breath_mark(p, o):
    """Breath in cold air: three wisps let go, curling as they thin."""
    op = Optic(o)
    rows = [(38, 1.0), (52, .86), (66, .72)]
    for k, (y, f) in enumerate(rows):
        x0 = 14 + k * 5
        path = spline([[x0, y + 2], [x0 + 20 * f, y - 3], [x0 + 42 * f, y + 3], [x0 + 62 * f, y - 2], [x0 + 70 * f, y - 7], [x0 + 66 * f, y - 10]], 12)
        w = op.w(3.8 if op.o > 70 else 5.0, 1.3) * np.linspace(1, .12, len(path)) ** .9
        p.line(path, w, taper=(.10, .0), floor=.35, tremor=.25, value=.95 - .1 * k)
        if op.o > 70:
            m = int(len(path) * .5)
            p.line(path[:m] + [1.0, 3.2], op.w(.8) * np.linspace(1, .1, m), taper=(.2, .3), floor=0, value=.65, tremor=.2)


def eye_mark(p, o):
    """An eye open in the dark: heavy upper lid, a ringed iris, the pupil wide, one point of light."""
    op = Optic(o)
    c = np.array([50.0, 52.0])
    up = spline([[8, 54], [26, 36], [50, 29], [74, 35], [92, 51]], 14)
    lo = spline([[92, 51], [72, 65], [50, 70], [27, 66], [8, 54]], 14)
    eye = np.vstack([up, lo[1:]])
    p.cut(np.vstack([up - [0, 4], lo[1:] + [0, 2]]))
    # the white: a few curved cuts round the ball, bright upper left
    p.clip(eye)
    for k, off in enumerate(np.linspace(-14, 16, 7 if op.o > 70 else 4)):
        arc = spline([[10, 55 + off * .2], [30, 50 + off], [50, 48 + off * 1.05], [70, 50 + off], [90, 53 + off * .2]], 12)
        lit = np.clip(.95 - (arc[:, 0] - 20) / 110 - (off + 14) / 60, .05, 1)
        p.line(arc, op.pitch(2.4) * .55 * lit, taper=(.2, .2), floor=0, tremor=.1, value=.85)
    # iris and pupil
    ic, ir = c + [-1, -1], 15.5
    p.cut(ellipse(ic, ir + 1.2, ir + 1.2, 0, 2 * math.pi, 64))
    n = 30 if op.o > 70 else 14
    for k in range(n):
        a = 2 * math.pi * k / n
        dvec = np.array([math.cos(a), math.sin(a)])
        lit = .45 + .55 * max(0, math.cos(a - math.radians(140)))
        p.line([ic + dvec * 6.6, ic + dvec * (ir - .6)], op.w(1.5 * lit, .7), taper=(.25, .1), floor=.3, tremor=.04, value=.92)
    p.contour(ellipse(ic, ir, ir, 0, 2 * math.pi, 80), op.w(1.3), closed=True, lost=.5, minpx=.9)
    p.cut(ellipse(ic, 6.4, 6.4, 0, 2 * math.pi, 40))
    p.fill(ellipse(ic + [-3.0, -3.2], 1.8, 1.6, 0, 2 * math.pi, 16), value=1.0)
    p.unclip()
    # lids: the upper one heavy and dark-lashed, the lower one a hair
    p.line(up, op.w(3.0, 1.8) * np.r_[np.linspace(.4, 1, 7), np.ones(len(up) - 14), np.linspace(1, .5, 7)], taper=(.05, .05), floor=.5, tremor=.15)
    p.line(lo, op.w(1.0, .8), taper=(.15, .15), floor=.2, tremor=.15, value=.85)
    crease = spline([[18, 44], [34, 30], [52, 25], [72, 29], [86, 41]], 12)
    p.line(crease, op.w(.9, .7), taper=(.3, .3), floor=0, tremor=.2, value=.75)
    for k in range(7 if op.o > 70 else 5):
        t = .18 + .64 * k / (6 if op.o > 70 else 4)
        i = int(t * (len(up) - 1))
        nrm, tan = normals(up)
        base_pt = up[i]
        outward = -nrm[i] if nrm[i][1] > 0 else nrm[i]
        tip = base_pt + outward * 7 + tan[i] * (t - .5) * 7
        p.line([base_pt, tip], op.w(1.4, .9), taper=(0, .8), floor=.1, tremor=.1)


def ear_mark(p, o):
    """A listening ear, the rim lit, the bowl in shadow."""
    op = Optic(o)
    helix = spline([[44, 86], [36, 80], [34, 70], [28, 58], [24, 40], [32, 21], [52, 12], [70, 18], [78, 34], [74, 50], [64, 60], [62, 70], [60, 80], [52, 90], [44, 86]], 12, closed=False)
    p.cut(helix + (helix - helix.mean(0)) * .06)
    # the rim: parallel cuts following its curl, bright where it faces up-left
    rim_in = spline([[37, 66], [33, 52], [33, 38], [40, 26], [54, 21], [66, 26], [70, 37], [67, 47], [59, 54]], 12)
    for k, f in enumerate(np.linspace(.15, .85, 4 if op.o > 70 else 2)):
        n = min(len(helix), len(rim_in))
        H = resample(helix[3:-3], 1.0)
        R = resample(rim_in, 1.0)
        m = min(len(H), len(R))
        H = H[np.linspace(0, len(H) - 1, m).astype(int)]
        R = R[np.linspace(0, len(R) - 1, m).astype(int)]
        path = H * (1 - f) + R * f
        nrm, _ = normals(path)
        L2 = LIGHT[:2] / np.linalg.norm(LIGHT[:2])
        lit = np.clip(.25 + (nrm @ L2) * -.9, 0, 1)
        p.line(path, op.pitch(2.4) * .8 * lit, taper=(.15, .15), floor=0, tremor=.1)
    p.contour(helix, op.w(1.9), lost=.35, minpx=1.0, taper=(.1, .1))
    p.line(rim_in, op.w(1.3, .9), taper=(.2, .3), floor=.2, tremor=.12, cut=False, value=.8)
    # antihelix and the bowl
    anti = spline([[44, 70], [47, 56], [46, 43], [52, 34], [61, 33]], 10)
    p.line(anti, op.w(1.6, 1.0), taper=(.3, .3), floor=.2, tremor=.12)
    p.line(spline([[46, 44], [55, 42], [62, 45]], 8), op.w(1.0, .8), taper=(.3, .4), floor=.1, tremor=.1)
    bowl = spline([[48, 72], [56, 66], [56, 56], [50, 50]], 10)
    p.line(bowl, op.w(1.1, .8), taper=(.3, .3), floor=.1, tremor=.1, value=.7)
    # tragus and lobe
    p.line(spline([[40, 74], [44, 70], [46, 76]], 8), op.w(1.4, .9), taper=(.2, .2), tremor=.08)
    p.line(spline([[46, 82], [51, 84], [55, 80]], 8), op.w(1.0, .8), taper=(.3, .3), tremor=.08, value=.75)


def heart_mark(p, o):
    """A heart that knows it is close: the left lobe lit, cuts running round the form."""
    op = Optic(o)
    def shape(s, cx=50.0, cy=52.0):
        pts = []
        for t in np.linspace(0, 2 * math.pi, 160):
            x = 16 * math.sin(t) ** 3
            y = -(13 * math.cos(t) - 5 * math.cos(2 * t) - 2 * math.cos(3 * t) - math.cos(4 * t))
            pts.append([cx + x * s, cy + y * s])
        return np.array(pts)
    outer = shape(2.35)
    p.cut(outer + (outer - outer.mean(0)) * .07)
    # cross-contour cuts, bowed over the form, wide on the lit lobe
    p.clip(outer)
    pitch = op.pitch(2.5)
    y = outer[:, 1].min() + pitch * .5
    top, bot = outer[:, 1].min(), outer[:, 1].max()
    while y < bot:
        xs = np.linspace(10, 90, 60)
        f = (y - top) / (bot - top)
        bow = np.sin((xs - 10) / 80 * math.pi) * 3.5
        ys = y + bow
        nx = (xs - 50) / 42
        ny = (ys - 50) / 40
        lit = shade(np.c_[nx * .9, ny * .7 - .1, np.sqrt(np.clip(1 - nx ** 2 * .7, .05, 1))], 1.7)
        lobe = np.exp(-(((xs - 34) / 22) ** 2 + ((ys - 36) / 20) ** 2))
        w = pitch * np.clip(lit * (.55 + .6 * lobe), 0, 1)
        p.line(np.c_[xs, ys], w, taper=(0, 0), floor=0, tremor=.1)
        y += pitch
    p.unclip()
    p.contour(outer, op.w(1.8), closed=True, lost=.3, minpx=1.0)


# ------------------------------------------------------------ furniture ---
def key_mark(p, o):
    """The key cap for prompts: a small engraved frame, lit along its top and left edges. Drawn on a
    plate the size it is nine-sliced from; the letter stays native type."""
    op = Optic(o)
    x0, y0, x1, y1, rr = 8, 8, 92, 92, 15
    def rrect(inset):
        a, b, c, d, r = x0 + inset, y0 + inset, x1 - inset, y1 - inset, max(2, rr - inset)
        pts = []
        for (cx, cy, s0) in ((c - r, b + r, -math.pi / 2), (c - r, d - r, 0), (a + r, d - r, math.pi / 2), (a + r, b + r, math.pi)):
            for t in np.linspace(s0, s0 + math.pi / 2, 10):
                pts.append([cx + math.cos(t) * r, cy + math.sin(t) * r])
        return np.array(pts)
    outer = rrect(0)
    p.contour(outer, op.w(4.2), closed=True, lost=.42, minpx=1.0, tremor=.25)
    inner = rrect(7)
    p.contour(inner, op.w(1.6), closed=True, lost=.05, minpx=.6, value=.7, tremor=.2, flip=True)


def wax_mark(stage):
    heights = {'Full': (1.0, 'lit'), 'Medium': (.66, 'lit'), 'Low': (.38, 'lit'), 'Gutter': (.2, 'gutter'), 'Out': (.62, 'out')}
    h, fl = heights[stage]
    def draw(p, o):
        candle_mark(p, o, h, fl, cx=47.0, base=146.0, radius=21.0, max_h=96.0, dish_w=1.85)
    return draw
