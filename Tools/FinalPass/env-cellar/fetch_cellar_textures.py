"""Download the cellar's extra Poly Haven texture sets (CC0 1.0) beside the house library's, with provenance.

python3 -I fetch_cellar_textures.py <project>/Tallow/SourceAssets/FinalPass/Env/PolyHaven
Same layout as env/fetch_polyhaven.py: <out>/<id>/<id>_<map>.png (diff, nor_dx, arm, disp), each PNG with a
'provenance' text chunk. sources-cellar.json lists them. Re-runnable: finished files are skipped.
"""
import io
import json
import os
import sys
import urllib.request
from PIL import Image, PngImagePlugin

SETS = {  # id: (resolution, use in the cellar)
    'whitewashed_brick': ('2k', 'limewashed brick of the cellar hall and the boiler room'),
    'worn_brick_floor': ('2k', 'brick paviours of the boiler room and the laundry'),
    'damaged_plaster': ('2k', 'perished lime plaster over brick, the cellar rooms below the dado line'),
}
MAPS = {'Diffuse': 'diff', 'nor_dx': 'nor_dx', 'arm': 'arm', 'Displacement': 'disp'}
HEAD = {'User-Agent': 'tallow-env-pass'}
out = sys.argv[1]; os.makedirs(out, exist_ok=True)
index = json.load(urllib.request.urlopen(urllib.request.Request('https://api.polyhaven.com/assets?t=textures', headers=HEAD), timeout=60))
rows = []
for sid, (res, use) in SETS.items():
    meta = index[sid]; d = os.path.join(out, sid); os.makedirs(d, exist_ok=True)
    authors = ', '.join(f'{k} ({v})' for k, v in meta.get('authors', {}).items())
    prov = f"Poly Haven texture '{meta.get('name', sid)}' ({sid}) by {authors}; licence CC0 1.0; https://polyhaven.com/a/{sid}; {res}"
    files = json.load(urllib.request.urlopen(urllib.request.Request(f'https://api.polyhaven.com/files/{sid}', headers=HEAD), timeout=60))
    row = {'id': sid, 'name': meta.get('name'), 'authors': meta.get('authors'), 'licence': 'CC0 1.0', 'url': f'https://polyhaven.com/a/{sid}',
           'size_mm': meta.get('dimensions'), 'resolution': res, 'use': use, 'files': {}}
    for key, tag in MAPS.items():
        dst = os.path.join(d, f'{sid}_{tag}.png'); row['files'][tag] = os.path.relpath(dst, out)
        if os.path.exists(dst): continue
        fmt = 'png' if tag == 'nor_dx' else 'jpg'
        url = files[key][res][fmt]['url']
        img = Image.open(io.BytesIO(urllib.request.urlopen(urllib.request.Request(url, headers=HEAD), timeout=300).read()))
        info = PngImagePlugin.PngInfo(); info.add_text('provenance', prov)
        img.save(dst, pnginfo=info)
        print('saved', dst, img.size, img.mode)
    rows.append(row)
json.dump(rows, open(os.path.join(out, 'sources-cellar.json'), 'w'), indent=1)
print('sets', len(rows))
