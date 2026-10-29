"""Children's crayon drawings for the TALLOW plates: how a ten year old draws Mim.

Strokes are placed by hand: the circle she goes round twice, the eyes she presses into, the
grin she cuts teeth into, the arms she pulls out to the edge of the page. The hand's wobble
and overshoot come from smooth noise, so the same drawing comes back every build.
"""
import math

import numpy as np

from paper import Wobble, spline, wander, resample


def circle(c, r, seed, turns=1.08, start=-1.9, wobble=.035, squash=1.0, n=60):
    """A circle a child draws: one push round, a little more than closed, never quite round."""
    w = Wobble(seed)
    t = np.linspace(start, start + turns * 2 * math.pi, n)
    rr = r * (1 + wobble * (w(t * 1.3) + .6 * w(t * 3.1 + 7)))
    return np.stack([c[0] + np.cos(t) * rr, c[1] + np.sin(t) * rr * squash], 1)


def line(points, seed, amp=3.0, scale=70.0, n_per=14):
    return wander(spline(points, n_per), seed, amp, scale)


def scribble_fill(centre, rx, ry, angle, pitch, seed, keep=None, overshoot=1.08, amp=.18):
    """Colouring in: one back-and-forth stroke across an ellipse, `pitch` px between passes.
    keep(x, y) -> bool drops the parts of the stroke that should stay unfilled (a face, a tooth)."""
    w = Wobble(seed)
    ca, sa = math.cos(angle), math.sin(angle)
    pts = []
    k = 0
    v = -ry
    while v <= ry:
        half = rx * math.sqrt(max(0.0, 1 - (v / ry) ** 2)) * overshoot * (1 + amp * w(k * .7))
        for u in ((-half, half) if k % 2 == 0 else (half, -half)):
            x = centre[0] + u * ca - (v + pitch * .35 * w(k * 1.3 + 4)) * sa
            y = centre[1] + u * sa + (v + pitch * .35 * w(k * 1.3 + 4)) * ca
            pts.append((x, y))
        v += pitch * (1 + .25 * w(k * 2.1 + 11))
        k += 1
    P = resample(np.array(pts), 3.0)
    return split_keep(P, keep)


def split_keep(P, keep):
    if keep is None:
        return [P]
    ok = np.array([keep(x, y) for x, y in P])
    out, cur = [], []
    for p, o in zip(P, ok):
        if o:
            cur.append(p)
        elif cur:
            if len(cur) > 2:
                out.append(np.array(cur))
            cur = []
    if len(cur) > 2:
        out.append(np.array(cur))
    return out


def zigzag(a, b, teeth, depth, seed, side=1.0):
    """Teeth: a zigzag from a to b, `teeth` points, each `depth` px off the line, a little uneven."""
    a, b = np.asarray(a, float), np.asarray(b, float)
    d = b - a
    n = np.array([-d[1], d[0]]) / np.linalg.norm(d)
    w = Wobble(seed)
    pts = []
    for i in range(teeth * 2 + 1):
        t = i / (teeth * 2)
        off = (depth * (1 + .3 * w(i * .9))) if i % 2 else 0.0
        pts.append(a + d * t + n * off * side + d * .01 * w(i * 1.7 + 3))
    return np.array(pts)


def arm(root, joints, hand_angle, fingers, finger_len, seed, spread=1.6):
    """One of Mim's arms: from the body out through its bends to a hand of splayed fingers.
    Returns (arm path, finger paths)."""
    P = line([root] + joints, seed, amp=4.0, scale=90.0)
    tip = P[-1]
    fs = []
    for i in range(fingers):
        a = hand_angle + (i / max(1, fingers - 1) - .5) * spread
        L = finger_len * (.8 + .4 * ((i * 37 + seed) % 7) / 6)
        bend = .25 * (1 if i % 2 else -1)
        q1 = tip + np.array([math.cos(a), math.sin(a)]) * L * .55
        q2 = tip + np.array([math.cos(a + bend), math.sin(a + bend)]) * L
        fs.append(line([tip, q1, q2], seed * 7 + i, amp=1.2, scale=30.0, n_per=6))
    return P, fs


def fur(outline, length, every, seed, outward=1.0, lean=.5):
    """Short strokes standing out of an outline like matted hair, one every `every` px along it."""
    P = resample(outline, every)
    T = np.gradient(P, axis=0)
    T /= np.maximum(1e-9, np.linalg.norm(T, axis=1))[:, None]
    N = np.stack([T[:, 1], -T[:, 0]], 1) * outward
    w = Wobble(seed)
    out = []
    for i, (p, nrm, tg) in enumerate(zip(P, N, T)):
        L = length * (.45 + 1.1 * (.5 + .5 * w(i * .8)) * (.7 + .3 * (.5 + .5 * w(i * 3.7 + 2))))
        d = nrm * math.cos(lean * w(i * 1.9 + 5)) + tg * math.sin(lean * w(i * 1.9 + 5))
        q = p + d * L
        mid = (p + q) / 2 + tg * L * .12 * w(i * 2.3)
        out.append(spline([p - d * 6, mid, q], 6))
    return out


def stick_girl(c, h, seed, dress_w=None):
    """A small girl as a child draws one: round head, hair lines, triangle dress, stick arms and legs.
    c is the top of the head. Returns (outline paths, dress fill centre, dress rx/ry)."""
    hr = h * .14
    head = np.array([c[0], c[1] + hr])
    dw = dress_w or h * .42
    neck = head + (0, hr)
    hem = head + (0, h * .70)
    paths = [circle(head, hr, seed, 1.12, n=40)]
    for i, dx in enumerate((-.9, -.55, .55, .9)):
        paths.append(line([head + (dx * hr, -hr * .2), head + (dx * hr * 1.25, hr * 1.6 + i % 2 * 4)], seed + i, 1.0, 20))
    dress = [neck + (-hr * .3, 0), hem + (-dw / 2, 0), hem + (dw / 2, 0), neck + (hr * .3, 0)]
    paths.append(line(dress + [dress[0]], seed + 9, 2.0, 50, n_per=6))
    sh = neck + (0, h * .08)
    paths.append(line([sh + (-hr * .4, 0), sh + (-h * .26, h * .14)], seed + 11, 1.2, 20))
    paths.append(line([sh + (hr * .4, 0), sh + (h * .26, h * .14)], seed + 12, 1.2, 20))
    paths.append(line([hem + (-dw * .18, 0), hem + (-dw * .2, h * .28)], seed + 13, 1.0, 20))
    paths.append(line([hem + (dw * .18, 0), hem + (dw * .22, h * .28)], seed + 14, 1.0, 20))
    # eyes, two dots; mouth, one flat line (drawn finer by the caller)
    face = [line([head + (-hr * .4, -hr * .12), head + (-hr * .3, -hr * .1)], seed + 15, .3, 10),
            line([head + (hr * .3, -hr * .12), head + (hr * .4, -hr * .1)], seed + 16, .3, 10),
            line([head + (-hr * .28, hr * .42), head + (hr * .28, hr * .4)], seed + 17, .4, 10)]
    return paths, face, (hem + neck) / 2 + (0, h * .05), (dw * .35, h * .26)
