"""Parlour carpet texture from a public-domain museum photograph: 'Star Ushak' carpet, Turkey, 16th-17th century,
The Metropolitan Museum of Art object 445364, Open Access, CC0. Turkey carpets of this kind were the standard
parlour carpet of Victorian England.

python3 -I make_rug.py <out_dir>
Finds the carpet's four corners against the grey studio ground, squares it up with a perspective warp just inside
the outer edge (the frayed ends stay, the studio floor goes) and writes a 2048 x 1248 landscape texture (long axis
along u) with a provenance chunk, plus rug.json with the carpet's real size.
"""
import json
import os
import sys
import urllib.request
import numpy as np
from PIL import Image, PngImagePlugin

OBJ = 445364
out = sys.argv[1]; os.makedirs(out, exist_ok=True)
HEAD = {'User-Agent': 'Mozilla/5.0 (X11; Linux x86_64) tallow-env-pass'}
meta = json.load(urllib.request.urlopen(urllib.request.Request(
    f'https://collectionapi.metmuseum.org/public/collection/v1/objects/{OBJ}', headers=HEAD), timeout=60))
assert meta['isPublicDomain'], 'not open access'
raw = os.path.join(out, 'star_ushak_photo.jpg')
if not os.path.exists(raw):
    open(raw, 'wb').write(urllib.request.urlopen(urllib.request.Request(meta['primaryImage'], headers=HEAD), timeout=300).read())
im = Image.open(raw).convert('RGB'); W, H = im.size
s = 8
a = np.asarray(im.resize((W // s, H // s)), dtype=np.float32)
mask = ((a.max(2) - a.min(2)) > 28) | (a.mean(2) < 70)
ys, xs = np.where(mask)
sm, df = xs + ys, xs - ys
tl = np.array([xs[sm.argmin()], ys[sm.argmin()]]) * s; br = np.array([xs[sm.argmax()], ys[sm.argmax()]]) * s
tr = np.array([xs[df.argmax()], ys[df.argmax()]]) * s; bl = np.array([xs[df.argmin()], ys[df.argmin()]]) * s
c = (tl + tr + br + bl) / 4
inset = lambda p, k=.022: p + (c - p) * k            # just inside the outer edge
quad = [*inset(tl), *inset(bl), *inset(br), *inset(tr)]    # PIL order: upper-left, lower-left, lower-right, upper-right
flat = im.transform((1248, 2048), Image.QUAD, data=[float(v) for v in quad], resample=Image.BICUBIC)
flat = flat.transpose(Image.ROTATE_90)                     # long axis along u
prov = (f"'Star Ushak' carpet, Turkey, {meta.get('objectDate')}; The Metropolitan Museum of Art, accession "
        f"{meta.get('accessionNumber')}, {meta.get('objectURL')}; Met Open Access, public domain (CC0). Museum photograph "
        f"squared up by a perspective warp inside the carpet's edge; 2048 x 1248.")
info = PngImagePlugin.PngInfo(); info.add_text('provenance', prov)
flat.save(os.path.join(out, 'star_ushak_rug.png'), pnginfo=info)
json.dump({'object': OBJ, 'title': meta.get('title'), 'date': meta.get('objectDate'), 'accession': meta.get('accessionNumber'),
           'url': meta.get('objectURL'), 'licence': 'CC0 (Met Open Access, public domain)', 'dimensions': meta.get('dimensions'),
           'corners_px': [tl.tolist(), tr.tolist(), br.tolist(), bl.tolist()], 'texture': 'star_ushak_rug.png', 'provenance': prov},
          open(os.path.join(out, 'rug.json'), 'w'), indent=1)
print('corners', tl, tr, br, bl)
