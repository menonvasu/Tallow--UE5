"""Seamless wallpaper tiles from public-domain museum scans (The Metropolitan Museum of Art, Open Access, CC0):
William Morris 'Willow Bough' (1887, object 384022) and 'Daisy' (1864, object 384017), block-printed wallpapers.

python3 -I make_wallpaper.py <out_dir> [key ...]      (keys: WillowBough, Daisy; default all)
Measures the print's repeat on the scan (autocorrelation, refined at full resolution), crops exactly one repeat away
from the sheet's border and selvedge text, closes the seams by cross-fading into the same motif one repeat further on
(the print repeats, so nothing is invented), and resamples to 1024 x 2048 for mips. The material undoes the
resample with separate world sizes for the two axes (written to <out>/willow_bough.json).
"""
import io
import json
import os
import sys
import urllib.request
import numpy as np
from PIL import Image, PngImagePlugin

PAPERS = {  # key: (Met object, coarse repeat across px, coarse repeat down px, file stem); coarse values from a
            # small-scale autocorrelation of each scan, refined below at full resolution.
    'WillowBough': (384022, 1444, 2456, 'willow_bough'),
    'Daisy': (384017, 1468, 1956, 'daisy'),
}
out = sys.argv[1]; os.makedirs(out, exist_ok=True)


def make(KEY):
    OBJ, CX, CY, STEM = PAPERS[KEY]
    HEAD = {'User-Agent': 'Mozilla/5.0 tallow-env-pass'}
    meta = json.load(urllib.request.urlopen(urllib.request.Request(
        f'https://collectionapi.metmuseum.org/public/collection/v1/objects/{OBJ}', headers=HEAD), timeout=60))
    assert meta['isPublicDomain'], 'not open access'
    raw = os.path.join(out, STEM + '_scan.jpg')
    if not os.path.exists(raw):
        open(raw, 'wb').write(urllib.request.urlopen(urllib.request.Request(meta['primaryImage'], headers=HEAD), timeout=300).read())
    img = np.asarray(Image.open(raw).convert('RGB'), dtype=np.float32)
    H, W, _ = img.shape
    lum = img.mean(2)

    def score(dy, dx, box=(400, 400, 1400, 1300)):
        """Mean absolute difference between a central window and the window shifted by (dy, dx)."""
        y0, x0, h, w = box
        a = lum[y0:y0 + h, x0:x0 + w]; b = lum[y0 + dy:y0 + dy + h, x0 + dx:x0 + dx + w]
        return float(np.abs(a - b).mean())


    # Refine the coarse periods (and the scan's skew) at full resolution.
    px = min(range(CX - 20, CX + 20), key=lambda dx: score(0, dx, (300, 60, H - CY - 400 if H - CY > 700 else 300, W - CX - 120)))
    py, pdx = min(((dy, dx) for dy in range(CY - 16, CY + 16) for dx in range(-6, 22)), key=lambda t: score(t[0], t[1], (80, 300, H - CY - 160, W - 600)))
    print('repeat', px, 'across;', py, 'down with', pdx, 'px drift')
    # Undo the scan's skew so one repeat down lands straight below.
    shear = pdx / py
    rows = np.arange(H)[:, None]
    cols = np.arange(W)[None, :] + rows * shear
    x_i = np.clip(cols, 0, W - 1)
    x0f = np.floor(x_i).astype(int); fr = (x_i - x0f)[..., None]; x1f = np.clip(x0f + 1, 0, W - 1)
    img = img[rows, x0f] * (1 - fr) + img[rows, x1f] * fr
    X0, Y0, B = 60, 140, 220            # crop corner (clear of border and selvedge), blend width
    assert X0 + px + B < W - 100 and Y0 + py + B < H - 40, (px, py, W, H)
    block = img[Y0:Y0 + py + B, X0:X0 + px + B]
    ramp = np.clip(np.arange(B) / (B - 1), 0, 1); ramp = ramp * ramp * (3 - 2 * ramp)
    # across: the first B columns fade from the motif one repeat on (continuous with the tile's right edge)
    t = block[:, :px].copy()
    t[:, :B] = block[:, :B] * ramp[None, :, None] + block[:, px:px + B] * (1 - ramp[None, :, None])
    # down: same with rows; the extra rows below were blended across too
    extra = block[py:py + B, :px].copy()
    extra[:, :B] = block[py:py + B, :B] * ramp[None, :, None] + block[py:py + B, px:px + B] * (1 - ramp[None, :, None])
    tile = t[:py].copy()
    tile[:B] = t[:B] * ramp[:, None, None] + extra * (1 - ramp[:, None, None])
    dpcm = W / 54.6                      # sheet 21 1/2 in across the scan
    size_cm = [round(px / dpcm, 2), round(py / dpcm, 2)]
    im = Image.fromarray(np.clip(tile, 0, 255).astype(np.uint8)).resize((1024, 2048), Image.LANCZOS)
    prov = (f"William Morris, '{meta.get('title')}' wallpaper ({meta.get('objectDate')}), Morris & Co., block print; The Metropolitan Museum of Art, "
            f"accession {meta.get('accessionNumber')}, {meta.get('objectURL')}; Met Open Access, public domain (CC0). "
            f"One measured repeat ({px} x {py} px of the scan, {size_cm[0]} x {size_cm[1]} cm), seams closed by cross-fading "
            f"into the next printed repeat; resampled to 1024 x 2048.")
    info = PngImagePlugin.PngInfo(); info.add_text('provenance', prov)
    im.save(os.path.join(out, STEM + '_tile.png'), pnginfo=info)
    json.dump({'object': OBJ, 'title': meta.get('title'), 'artist': meta.get('artistDisplayName'), 'date': meta.get('objectDate'),
               'accession': meta.get('accessionNumber'), 'url': meta.get('objectURL'), 'licence': 'CC0 (Met Open Access, public domain)',
               'repeat_px': [px, py], 'skew_px': pdx, 'repeat_cm': size_cm, 'tile': STEM + '_tile.png', 'provenance': prov},
              open(os.path.join(out, STEM + '.json'), 'w'), indent=1)
    print(KEY, 'tile cm', size_cm)


for key in (sys.argv[2:] or list(PAPERS)):
    make(key)
