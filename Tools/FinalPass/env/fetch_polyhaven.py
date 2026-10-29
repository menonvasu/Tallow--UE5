"""Download the parlour's Poly Haven texture sets (CC0 1.0) and stamp provenance into every raster.

python3 -I fetch_polyhaven.py <project>/Tallow/SourceAssets/FinalPass/Env/PolyHaven
Each set lands in <out>/<id>/ as <id>_<map>.png (diff, nor_dx, arm, disp); every PNG carries a 'provenance' text
chunk (source, authors, licence, URL). sources.json lists them all. Re-runnable: finished sets are skipped.
"""
import io
import json
import os
import sys
import urllib.request
from PIL import Image, PngImagePlugin

SETS = {  # id: (resolution, use in the parlour)
    'old_wooden_floor_02': ('2k', 'floorboards'),
    'decrepit_wallpaper': ('2k', 'aged paper ground under the wallpaper pattern (stains, tears, roughness, relief)'),
    'white_plaster_02': ('2k', 'ceiling lime plaster'),
    'painted_plaster_wall': ('2k', 'brushed relief of the painted joinery and cornice'),
    'wood_table_worn': ('2k', 'stained softwood joinery: skirting, dado, door and window casings'),
    'velour_velvet': ('2k', 'upholstery velvet'),
    'quatrefoil_jacquard_fabric': ('2k', 'curtain damask'),
    'rusty_metal_04': ('2k', 'black-leaded cast iron of the grate and fender'),
    'factory_brick': ('2k', 'sooted firebrick of the fireplace opening'),
    'terracotta_floor_tiles': ('2k', 'glazed hearth tiles'),
    'brown_leather': ('2k', 'leather upholstery and cases'),
    'rough_linen': ('2k', 'dust sheets, linen, dress forms'),
    'floral_jacquard': ('2k', 'upholstery and hangings'),
    'dark_wood': ('2k', 'dark furniture timber'),
    'worn_planks': ('2k', 'loose boards, crates, splintered planks'),
    'white_plaster_rough_01': ('2k', 'limewashed plaster (kitchen, pantry, cellar)'),
    'worn_tile_floor': ('2k', 'kitchen and scullery floor tiles'),
    'checkered_pavement_tiles': ('2k', 'front hall encaustic floor'),
    'brick_wall_13': ('2k', 'whitewashed cellar brick'),
    'slate_floor_03': ('2k', 'cellar and pantry flags'),
}
# Every set the library (and each floor's env_rooms.py) names is fetched too, so any checkout can build every room.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import env_library  # noqa: E402
for key, sid in env_library.SETS.items():
    SETS.setdefault(sid, ('2k', f'library set {key}'))
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
json.dump(rows, open(os.path.join(out, 'sources.json'), 'w'), indent=1)
print('sets', len(rows))
