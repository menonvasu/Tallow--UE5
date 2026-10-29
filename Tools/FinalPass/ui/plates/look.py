"""Shared look for the TALLOW story plates: the soot ground, the candle, the grade, the grain.

Every plate is one object on a dark table under one warm candle. These helpers keep the
ground, light, grain and black level the same on every plate, photo or document.
All procedural (numpy noise); no sampled imagery.
"""
import math

import numpy as np
from PIL import Image, PngImagePlugin

SOOT = np.array([13, 10, 8], np.float32) / 255.0          # #0D0A08
TALLOW = np.array([233, 223, 199], np.float32) / 255.0    # #E9DFC7
THREAD = np.array([168, 32, 23], np.float32) / 255.0      # #A82017
FLAME = np.array([1.0, .87, .70], np.float32)             # candle light on paper, before the grade
W, H = 1920, 1080

# Caption zones (the game sets the headline and sub-line here) and the box that must stay dark.
CAPTION = {'L': (0, 820), 'R': (1100, 1920)}
TEXT_BOX = {'L': (150, 640, 800, 980), 'R': (1140, 640, 1780, 980)}


# ------------------------------------------------------------------ noise ---
def periodic_noise(n, seed, beta=2.0, aniso=(1.0, 1.0), angle=0.0):
    """Seamless 1/f^beta noise on an n x n tile, optionally stretched (brush passes), -1..1."""
    rng = np.random.default_rng(seed)
    white = rng.normal(0, 1, (n, n))
    fy = np.fft.fftfreq(n)[:, None]
    fx = np.fft.fftfreq(n)[None, :]
    ca, sa = math.cos(angle), math.sin(angle)
    u = (fx * ca + fy * sa) * aniso[0]
    v = (-fx * sa + fy * ca) * aniso[1]
    f = np.sqrt(u * u + v * v)
    f[0, 0] = 1
    spec = np.fft.fft2(white) / f ** (beta / 2)
    spec[0, 0] = 0
    out = np.real(np.fft.ifft2(spec))
    out -= out.mean()
    return (out / max(1e-6, np.abs(out).max())).astype(np.float32)


def noise(w, h, seed, beta=2.0, aniso=(1.0, 1.0), angle=0.0, res=512):
    """Low-frequency noise for a w x h field, made on a small tile and enlarged (it has no fine detail)."""
    n = min(res, 1 << int(math.ceil(math.log2(max(w, h)))))
    k = max(w, h) / n
    tile = periodic_noise(n, seed, beta, aniso, angle)[:max(2, int(math.ceil(h / k)) + 1), :max(2, int(math.ceil(w / k)) + 1)]
    im = Image.fromarray(tile.astype(np.float32), 'F').resize((int(round(tile.shape[1] * k)), int(round(tile.shape[0] * k))), Image.BICUBIC)
    return np.asarray(im, np.float32)[:h, :w]


def value_noise(w, h, seed, cell):
    """Smooth value noise with a given cell size in px, 0..1."""
    rng = np.random.default_rng(seed)
    gw, gh = max(2, w // cell + 3), max(2, h // cell + 3)
    g = Image.fromarray((rng.uniform(0, 1, (gh, gw)) * 65535).astype(np.uint16).astype(np.float32), 'F')
    g = g.resize((gw * cell, gh * cell), Image.BICUBIC).crop((0, 0, w, h))
    return np.clip(np.asarray(g, np.float32) / 65535.0, 0, 1)


def blur(a, r):
    """Approximate Gaussian blur (three box passes), any 2D or 3D float array; r in px.
    Wide blurs are taken on a reduced copy and enlarged again."""
    a = np.asarray(a, np.float32)
    if r >= 8 and a.ndim == 2:
        k = int(max(2, r // 4))
        h, w = a.shape
        ph, pw = (-h) % k, (-w) % k
        s = np.pad(a, ((0, ph), (0, pw)), mode='edge').reshape((h + ph) // k, k, (w + pw) // k, k).mean((1, 3))
        s = _box3(s, r / k)
        return np.asarray(Image.fromarray(s, 'F').resize((w + pw, h + ph), Image.BILINEAR), np.float32)[:h, :w]
    if a.ndim == 3 and r >= 8:
        return np.stack([blur(a[..., c], r) for c in range(a.shape[2])], -1)
    return _box3(a, r)


def _box3(a, r):
    r = int(round(r))
    if r < 1:
        return a
    out = np.asarray(a, np.float32)
    for axis in (0, 1):
        n = out.shape[axis]
        for _ in range(3):
            pad = [(0, 0)] * out.ndim
            pad[axis] = (r, r)
            p = np.pad(out, pad, mode='edge')
            acc = np.zeros_like(out)
            for k in range(2 * r + 1):
                acc += p[:, k:k + n] if axis else p[k:k + n]
            out = acc / (2 * r + 1)
    return out


def smoothstep(a, b, x):
    t = np.clip((x - a) / (b - a), 0, 1)
    return t * t * (3 - 2 * t)


def lum(rgb):
    return rgb[..., 0] * .2126 + rgb[..., 1] * .7152 + rgb[..., 2] * .0722


# ------------------------------------------------------------- the ground ---
def soot_ground(w=W, h=H, seed=7):
    """The dark table every plate lies on: #0D0A08, long horizontal brush passes, tooth, a little grain.
    Returns float RGB 0..1. A few levels of value only."""
    brush = noise(w, h, seed, 1.6, aniso=(1.6, .16), angle=.035)
    mottle = noise(w, h, seed + 1, 2.7)
    tooth = noise(w, h, seed + 2, .7)
    delta = (2.0 * brush + 1.6 * mottle + 1.1 * tooth) / 255.0
    rgb = SOOT[None, None, :] + delta[..., None] * np.array([1.0, .93, .82], np.float32)
    return np.clip(rgb, 0, 1)


def candle(w, h, pos, reach=1150.0, peak=1.0, ambient=.05):
    """One candle above the table. Irradiance falls off with the distance to `pos` (px, may be off frame).
    Returns a float map ~ambient..peak; multiply it into lit surfaces."""
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    d = np.hypot(xx - pos[0], yy - pos[1]) / reach
    return ambient + (peak - ambient) * np.exp(-d ** 1.7)


def grain(w, h, seed, amount=.014):
    """Fine film grain, a little coarser than a pixel, stronger in the mids."""
    rng = np.random.default_rng(seed)
    g = rng.normal(0, 1, (h // 2 + 1, w // 2 + 1)).astype(np.float32)
    g = np.asarray(Image.fromarray(g, 'F').resize((w + 2, h + 2), Image.BILINEAR), np.float32)[1:h + 1, 1:w + 1]
    fine = rng.normal(0, .5, (h, w)).astype(np.float32)
    return (g * .8 + fine) * amount


def caption_ramp(w, h, side, strength=.92):
    """A linear darkness ramp over the caption side only: full at the frame edge and the bottom of the
    caption column, gone at the art side. Not radial: no vignette."""
    x0, x1 = CAPTION[side]
    xs = np.arange(w, dtype=np.float32)
    if side == 'L':
        hr = np.clip((x1 + 280 - xs) / (x1 + 280 - 0.0), 0, 1)
    else:
        hr = np.clip((xs - (x0 - 280)) / (w - (x0 - 280)), 0, 1)
    hr = np.clip(hr * 1.35, 0, 1)
    vr = np.clip(.35 + .65 * np.arange(h, dtype=np.float32) / (h * .62), 0, 1)
    return 1 - strength * hr[None, :] * vr[:, None]


def clamp_text_box(rgb, side, limit=20.0 / 255.0, feather=60):
    """Safety: wherever the caption's text box is brighter than `limit`, scale it down smoothly."""
    h, w = rgb.shape[:2]
    x0, y0, x1, y1 = TEXT_BOX[side]
    m = np.zeros((h, w), np.float32)
    m[max(0, y0 - 20):min(h, y1 + 40), max(0, x0 - 40):min(w, x1 + 40)] = 1
    m = blur(m, feather)
    L = lum(rgb)
    scale = np.where(L > limit, limit / np.maximum(L, 1e-6), 1.0)
    scale = 1 - m * (1 - scale)
    return rgb * scale[..., None]


def finish(rgb, side, seed, grain_amt=.012):
    """The plate grade shared by every plate: crush, warm the mids a hair, grain, keep the caption dark."""
    rgb = np.clip(rgb, 0, 1)
    rgb = clamp_text_box(rgb, side)
    rgb = rgb + grain(rgb.shape[1], rgb.shape[0], seed, grain_amt)[..., None] * (.35 + np.sqrt(np.clip(rgb, 0, 1)))
    return np.clip(rgb, 0, 1)


# -------------------------------------------------------------- the grade ---
def grade_photo(img, side, seed=11, desat=.6, target_median=22.0 / 255.0, ramp=.92):
    """Grade a real game still into the soot-and-tallow world.

    Desaturate (about 60%), split-tone shadows to soot and highlights to tallow, crush the blacks
    so most of the frame sits under 30/255 (the median is pulled to `target_median`), fine film
    grain, and a linear darkness ramp on the caption side only. Never a vignette.
    img: PIL image or float/uint8 array. side: 'L' or 'R' (the caption side). Returns PIL RGB.
    """
    a = np.asarray(img.convert('RGB') if isinstance(img, Image.Image) else img)
    rgb = a.astype(np.float32) / (255.0 if a.dtype == np.uint8 else 1.0)
    if rgb.shape[:2] != (H, W):
        rgb = np.asarray(Image.fromarray((np.clip(rgb, 0, 1) * 255).astype(np.uint8)).resize((W, H), Image.LANCZOS), np.float32) / 255.0
    L = lum(rgb)[..., None]
    rgb = L + (1 - desat) * (rgb - L)
    # crush: lift the black point, then a gamma that brings the median down to the target
    rgb = np.clip((rgb - .018) / (1 - .018), 0, 1)
    med = float(np.median(lum(rgb)))
    g = 1.0 if med <= target_median or med <= 1e-4 else float(np.clip(math.log(target_median) / math.log(med), 1.0, 3.2))
    rgb = rgb ** g
    # split tone: shadows toward soot, highlights toward tallow
    L = lum(rgb)[..., None]
    shadow = SOOT / max(1e-6, float(lum(SOOT[None, None, :])[0, 0]))
    light = TALLOW / float(lum(TALLOW[None, None, :])[0, 0])
    t = smoothstep(.02, .6, L)
    tint = shadow * (1 - t) + light * t
    rgb = L * (.55 * tint + .45 * (rgb / np.maximum(L, 1e-5)))
    rgb = rgb * caption_ramp(W, H, side, ramp)[..., None]
    rgb = finish(rgb, side, seed)
    return Image.fromarray((rgb * 255 + .5).astype(np.uint8), 'RGB')


# ---------------------------------------------------------------- output ---
def to_image(rgb):
    return Image.fromarray((np.clip(rgb, 0, 1) * 255 + .5).astype(np.uint8), 'RGB')


def save_png(img, path, provenance):
    """Write a PNG with a `provenance` text chunk (generator script, fonts and licences, sources)."""
    info = PngImagePlugin.PngInfo()
    info.add_text('provenance', provenance)
    img.save(path, pnginfo=info)
