"""Write a 'provenance' PNG text chunk into the maps prepare_props_blender.py baked from BlenderKit models.

python3 -I stamp_provenance.py <SourceAssets/FinalPass/Env>
Reads BlenderKit/sources.json (title, author, licence, URL) and each Export/<SM>.json (its source .blend and maps).
"""
import json
import os
import sys
from PIL import Image, PngImagePlugin

root = sys.argv[1]
src = {r['file']: r for r in json.load(open(os.path.join(root, 'BlenderKit', 'sources.json')))}
exp = os.path.join(root, 'Export')
n = 0
for name in os.listdir(exp):
    if not name.endswith('.json') or name in ('hearth.json', 'curtains.json'):
        continue
    info = json.load(open(os.path.join(exp, name)))
    row = src.get(info.get('source'))
    if not row or not info.get('maps'):
        continue
    prov = (f"BlenderKit model '{row['title']}' by {row['author']} ({row['url']}), licence {row['licence']}, free asset; "
            f"its own maps, re-packed for Unreal (colour; AO 1 / roughness / metal; DirectX normal)")
    for m in info['maps'].values():
        for key in ('D', 'ARM', 'N'):
            if m.get(key):
                p = os.path.join(exp, m[key]); im = Image.open(p)
                meta = PngImagePlugin.PngInfo(); meta.add_text('provenance', prov)
                im.save(p, pnginfo=meta); n += 1
print('stamped', n)
