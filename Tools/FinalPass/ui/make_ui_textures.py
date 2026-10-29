"""Draw the interface's soft soot smudge: the darkness laid under words that sit over the game.

A feathered pool of soot (no edge, no card), wider than tall, with a faint dry-brush grain in its
alpha so it reads as a smear of the same soot the menus are laid on rather than a flat shadow.
Drawn by this script from numpy noise; no sampled or traced pixels.

  python3 make_ui_textures.py   ->  source/textures/T_Smudge.png
"""
from pathlib import Path

import numpy as np
from PIL import Image, PngImagePlugin

HERE = Path(__file__).resolve().parent
OUT = HERE / "source" / "textures"
SOOT = (13, 10, 8)


def smooth_noise(h, w, cell, rng, cell_x=None):
    cell_x = cell_x or cell
    gy, gx = h // cell + 2, w // cell_x + 2
    grid = rng.random((gy, gx))
    ys = np.linspace(0, gy - 2, h, endpoint=False)
    xs = np.linspace(0, gx - 2, w, endpoint=False)
    y0, x0 = ys.astype(int), xs.astype(int)
    fy, fx = (ys - y0)[:, None], (xs - x0)[None, :]
    fy, fx = fy * fy * (3 - 2 * fy), fx * fx * (3 - 2 * fx)
    a = grid[y0][:, x0]
    b = grid[y0][:, x0 + 1]
    c = grid[y0 + 1][:, x0]
    d = grid[y0 + 1][:, x0 + 1]
    return (a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy


def smudge(w=512, h=256, seed=1974):
    rng = np.random.default_rng(seed)
    y, x = np.mgrid[0:h, 0:w].astype(np.float64)
    u = (x + .5) / w * 2 - 1
    v = (y + .5) / h * 2 - 1
    # A rounded pool: flat through the middle, falling away over the outer third, so text of any
    # length sits on even darkness and the edge is never seen.
    r = np.sqrt((np.abs(u) ** 2.6 + np.abs(v) ** 2.6)) ** (1 / 1.3)
    core = np.clip(1 - (r - .38) / .62, 0, 1)
    core = core * core * (3 - 2 * core)
    # Dry brush: horizontal passes thin the soot a little, never holes.
    streak = smooth_noise(h, w, 9, rng, cell_x=170)
    grain = .9 + .1 * streak
    alpha = np.clip(core * grain, 0, 1)
    rgba = np.zeros((h, w, 4), np.uint8)
    rgba[..., 0], rgba[..., 1], rgba[..., 2] = SOOT
    rgba[..., 3] = np.round(alpha * 255).astype(np.uint8)
    return Image.fromarray(rgba, "RGBA")


def save(img, path, note):
    info = PngImagePlugin.PngInfo()
    info.add_text("provenance", note)
    img.save(path, pnginfo=info)


if __name__ == "__main__":
    OUT.mkdir(parents=True, exist_ok=True)
    save(smudge(), OUT / "T_Smudge.png",
         "Tools/FinalPass/ui/make_ui_textures.py: procedural soot smudge (numpy value noise), no sampled or traced pixels.")
    print("wrote", OUT / "T_Smudge.png")
