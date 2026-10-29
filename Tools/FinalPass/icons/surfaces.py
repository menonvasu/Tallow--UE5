"""Soot grounds for the interface: the dark the marks are cut into.

A ground is painted, not flat: a broad brush laid it on in long horizontal passes,
the sheet underneath has a tooth and a few rag fibres. The variation is kept to a few
levels of value so type on top stays sharp. Tiles wrap seamlessly (FFT-filtered noise).
"""
import numpy as np
from PIL import Image

from engrave import save_png

SOOT = np.array([22, 20, 17], np.float32)      # warm soot, a hair above the panel black it replaces
SHEET = np.array([35, 32, 28], np.float32)     # the reading sheet, one step lighter


def periodic_noise(n, seed, beta=2.0, aniso=(1.0, 1.0), angle=0.0):
    """Seamless 1/f^beta noise, optionally stretched (brush passes), normalised to -1..1."""
    rng = np.random.default_rng(seed)
    white = rng.normal(0, 1, (n, n))
    fy = np.fft.fftfreq(n)[:, None]
    fx = np.fft.fftfreq(n)[None, :]
    ca, sa = np.cos(angle), np.sin(angle)
    u = (fx * ca + fy * sa) * aniso[0]
    v = (-fx * sa + fy * ca) * aniso[1]
    f = np.sqrt(u * u + v * v)
    f[0, 0] = 1
    spec = np.fft.fft2(white) / f ** (beta / 2)
    spec[0, 0] = 0
    out = np.real(np.fft.ifft2(spec))
    out -= out.mean()
    return out / max(1e-6, np.abs(out).max())


def fibre_layer(n, seed, count):
    """Pale rag fibres, wrapped at the tile edge."""
    from engrave import fibres
    big = fibres(n * 2, n * 2, seed, count=count * 4)
    return np.maximum.reduce([big[:n, :n], big[n:, :n], big[:n, n:], big[n:, n:]])


def ground(n, base, seed, amount=1.0):
    brush = periodic_noise(n, seed, 1.6, aniso=(1.6, .18), angle=.04)        # long horizontal passes
    mottle = periodic_noise(n, seed + 1, 2.6)                                # where the brush loaded up
    tooth = periodic_noise(n, seed + 2, .6)                                  # the sheet's grain
    fib = fibre_layer(n, seed + 3, n * n // 2600)
    delta = amount * (2.6 * brush + 1.8 * mottle + 1.6 * tooth + 3.5 * fib)  # value levels (0..255)
    rgb = base[None, None, :] + delta[..., None] * np.array([1.0, .95, .86])[None, None, :]
    return np.clip(rgb, 0, 255)


def card(w, h, base, seed):
    """A painted soot card for HUD notes: dry-brush edges, opaque body, nine-sliced in Slate."""
    n = max(w, h)
    body = ground(n, base, seed, .9)[:h, :w]
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    edge = np.minimum.reduce([xx, w - 1 - xx, yy, h - 1 - yy])
    rough = periodic_noise(n, seed + 9, 1.4, aniso=(1.4, .25))[:h, :w]
    streak = periodic_noise(n, seed + 10, 1.0, aniso=(2.2, .08))[:h, :w]
    margin = 22.0
    d = edge - margin * .45 + rough * 7 + streak * 4
    alpha = np.clip(d / 9.0, 0, 1)
    alpha = alpha * alpha * (3 - 2 * alpha)
    return np.dstack([body, alpha * 255])


def build_all():
    out = []
    soot = ground(512, SOOT, 41)
    out.append(('Soot', Image.fromarray(soot.astype(np.uint8), 'RGB'),
                'A seamless soot ground: brush passes, a mottled load, paper tooth and rag fibres, procedural noise.'))
    sheet = ground(512, SHEET, 57, 1.1)
    out.append(('Sheet', Image.fromarray(sheet.astype(np.uint8), 'RGB'),
                'A seamless reading sheet one step lighter than the soot ground, procedural noise.'))
    c = card(512, 256, SOOT, 73)
    out.append(('Card', Image.fromarray(c.astype(np.uint8), 'RGBA'),
                'A soot card with dry-brush edges for HUD notes, nine-sliced; procedural noise.'))
    return out


def save(img, path, provenance):
    save_png(img, path, provenance)
