"""DualSense button glyphs in the same engraved hand as the interface marks.

Built on request (`build_icons.py --pad` -> Raster/Pad/), not imported by default: the
prompts that use them come with the controller pass. Each cap is cut like the keyboard
key cap (lit along its top and left edge); the symbol inside is one confident line.
Shoulder and trigger caps are nine-sliced like the key cap, their label stays native type.
"""
import math

import numpy as np

from engrave import spline, ellipse
from marks import Optic


def _round_cap(p, op, c=(50.0, 50.0), r=40.0):
    p.contour(ellipse(c, r, r, 0, 2 * math.pi, 120), op.w(4.0), closed=True, lost=.4, minpx=1.0, tremor=.2)
    p.contour(ellipse(c, r - 6.5, r - 6.5, 0, 2 * math.pi, 100), op.w(1.4), closed=True, lost=.05, minpx=.6, value=.65, flip=True, tremor=.15)


def face_mark(symbol):
    """cross, circle, square or triangle on a round cap."""
    def draw(p, o):
        op = Optic(o)
        _round_cap(p, op)
        w = op.w(4.6, 1.6)
        c = np.array([50.0, 51.0])
        if symbol == 'cross':
            for s in (-1, 1):
                p.line([c + [-15, -15 * s], c + [15, 15 * s]], w, taper=(.12, .12), floor=.5, tremor=.25)
        elif symbol == 'circle':
            p.line(ellipse(c, 15.5, 15.5, -math.pi * .6, math.pi * 1.45, 60), w, taper=(.1, .05), floor=.5, tremor=.2)
        elif symbol == 'square':
            sq = [c + [-14, -14], c + [14, -14.5], c + [14.5, 14], c + [-14, 14], c + [-14, -14]]
            p.line(np.array(sq), w, taper=(.05, .05), floor=.6, tremor=.2)
        elif symbol == 'triangle':
            tri = [c + [0, -17], c + [16.5, 12], c + [-16.5, 12], c + [0, -17]]
            p.line(np.array(tri), w, taper=(.05, .05), floor=.6, tremor=.2)
    return draw


def dpad_mark(direction):
    """The cross-shaped pad, the pressed arm cut in ink and the rest left as outline."""
    def draw(p, o):
        op = Optic(o)
        a, b = 14.0, 42.0
        arm = {'up': 0, 'right': 1, 'down': 2, 'left': 3}[direction]
        outline = np.array([[-a, -b], [a, -b], [a, -a], [b, -a], [b, a], [a, a], [a, b], [-a, b], [-a, a], [-b, a], [-b, -a], [-a, -a], [-a, -b]]) + 50
        p.contour(outline, op.w(2.4), lost=.4, minpx=.9, tremor=.2)
        rot = [np.array([[1, 0], [0, 1]]), np.array([[0, -1], [1, 0]]), np.array([[-1, 0], [0, -1]]), np.array([[0, 1], [-1, 0]])][arm]
        lit = np.array([[-a + 3, -b + 3], [a - 3, -b + 3], [a - 3, -a - 1], [-a + 3, -a - 1]]) @ rot.T + 50
        p.fill(lit, value=.92)
        tip = np.array([[0, -b + 9], [6, -b + 16], [-6, -b + 16]]) @ rot.T + 50
        p.cut(tip)
    return draw


def stick_mark(p, o):
    """A thumbstick from above: a dished top, its rim knurled on the lit side."""
    op = Optic(o)
    c = np.array([50.0, 50.0])
    _round_cap(p, op, c, 40)
    for k, f in enumerate((.42, .62)):
        p.contour(ellipse(c, 34 * f, 34 * f, 0, 2 * math.pi, 80), op.w(1.1), closed=True, lost=.1, minpx=.6, value=.7, flip=True)
    for t in np.linspace(math.pi * .7, math.pi * 1.8, 14):
        d = np.array([math.cos(t), math.sin(t)])
        p.line([c + d * 26, c + d * 31], op.w(1.2, .7), taper=(.2, .2), floor=.3, tremor=.03)


def bumper_mark(p, o):
    """L1/R1: a long low cap, nine-sliced; the label is native type."""
    op = Optic(o)
    pts = spline([[10, 66], [12, 40], [30, 30], [70, 30], [88, 40], [90, 66], [50, 70], [10, 66]], 10, tension=.5)
    p.contour(pts, op.w(4.0), lost=.4, minpx=1.0, tremor=.2)


def trigger_mark(p, o):
    """L2/R2: a tall cap with a rolled top, nine-sliced; the label is native type."""
    op = Optic(o)
    pts = spline([[16, 90], [14, 40], [24, 14], [50, 8], [76, 14], [86, 40], [84, 90], [50, 92], [16, 90]], 10, tension=.5)
    p.contour(pts, op.w(4.0), lost=.4, minpx=1.0, tremor=.2)
    p.contour(spline([[24, 34], [36, 18], [64, 18], [76, 34]], 10), op.w(1.3), lost=.05, minpx=.6, value=.65, flip=True)


GLYPHS = {
    'PadCross': (face_mark('cross'), 96, 96, 34),
    'PadCircle': (face_mark('circle'), 96, 96, 34),
    'PadSquare': (face_mark('square'), 96, 96, 34),
    'PadTriangle': (face_mark('triangle'), 96, 96, 34),
    'PadUp': (dpad_mark('up'), 96, 96, 34),
    'PadRight': (dpad_mark('right'), 96, 96, 34),
    'PadDown': (dpad_mark('down'), 96, 96, 34),
    'PadLeft': (dpad_mark('left'), 96, 96, 34),
    'PadStick': (stick_mark, 96, 96, 34),
    'PadBumper': (bumper_mark, 128, 96, 34),
    'PadTrigger': (trigger_mark, 96, 96, 34),
}
