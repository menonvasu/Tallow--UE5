#!/usr/bin/env python3
"""Build every DualSense glyph the prompts name, in the engraved hand of the interface marks.

  python3 pad_extra.py   -> Raster/T_Pad*.png (the 11 in icons/pad.py plus Options, Create and the whole d-pad)

pad.py is not edited; the extra marks are drawn here with its helpers. Every pixel is vector paths cut by
engrave.py; nothing is sampled. Each PNG carries the provenance chunk.
"""
import math
import sys
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
ICONS = HERE.parent / 'icons'
sys.path.insert(0, str(ICONS))

import pad                              # noqa: E402
from engrave import Plate, spline       # noqa: E402
from marks import Optic                 # noqa: E402
import build_icons                      # noqa: E402

RASTER = HERE / 'Raster'


def _pill(p, op):
    pts = spline([[10, 36], [18, 28], [82, 28], [90, 36], [90, 64], [82, 72], [18, 72], [10, 64], [10, 36]], 10, tension=.4)
    p.contour(pts, op.w(4.0), closed=False, lost=.4, minpx=1.0, tremor=.2)
    p.contour(spline([[18, 36], [24, 34], [76, 34]], 8), op.w(1.3), lost=.05, minpx=.6, value=.65, flip=True)


def options_mark(p, o):
    """Options: the small pill with three lines."""
    op = Optic(o)
    _pill(p, op)
    for y in (43, 50, 57):
        p.line([np.array([32.0, y]), np.array([68.0, y])], op.w(3.6, 1.4), taper=(.1, .1), floor=.5, tremor=.2)


def create_mark(p, o):
    """Create: the small pill with a tray and an arrow lifting out of it."""
    op = Optic(o)
    _pill(p, op)
    w = op.w(3.4, 1.4)
    p.line([np.array([50.0, 58.0]), np.array([50.0, 40.0])], w, taper=(.1, .1), floor=.5, tremor=.2)
    p.line([np.array([43.0, 47.0]), np.array([50.0, 39.0]), np.array([57.0, 47.0])], w, taper=(.1, .1), floor=.5, tremor=.2)
    p.line([np.array([36.0, 52.0]), np.array([36.0, 61.0]), np.array([64.0, 61.0]), np.array([64.0, 52.0])], w, taper=(.1, .1), floor=.5, tremor=.2)


def dpad_whole(p, o):
    """The whole d-pad: the cross outline with all four arms tipped."""
    op = Optic(o)
    a, b = 14.0, 42.0
    outline = np.array([[-a, -b], [a, -b], [a, -a], [b, -a], [b, a], [a, a], [a, b], [-a, b], [-a, a], [-b, a], [-b, -a], [-a, -a], [-a, -b]]) + 50
    p.contour(outline, op.w(2.6), lost=.4, minpx=.9, tremor=.2)
    for rot in (np.array([[1, 0], [0, 1]]), np.array([[0, -1], [1, 0]]), np.array([[-1, 0], [0, -1]]), np.array([[0, 1], [-1, 0]])):
        tip = np.array([[0, -b + 9], [6, -b + 16], [-6, -b + 16]]) @ rot.T + 50
        p.cut(tip)


EXTRA = {
    'PadOptions': (options_mark, 96, 96, 34),
    'PadCreate': (create_mark, 96, 96, 34),
    'PadDPad': (dpad_whole, 96, 96, 34),
}


def build():
    RASTER.mkdir(exist_ok=True)
    table = dict(pad.GLYPHS)
    table.update(EXTRA)
    for i, (name, (fn, w, h, o)) in enumerate(table.items()):
        p = Plate(w, h, design_w=100.0 * w / max(w, h) if w > h else 100.0, seed=91 + i)
        fn(p, o)
        p.save(RASTER / f'T_{name}.png', build_icons.provenance(name, 'A DualSense button glyph cut in the interface hand.'), grain_amt=.10)
        print(name, w, h)


if __name__ == '__main__':
    build()
