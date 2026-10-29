"""Write a 'provenance' PNG text chunk into the maps prepare_upper_blender.py baked from the upstairs BlenderKit models.

python3 -I stamp_upper.py <SourceAssets/FinalPass/Env>
Reads BlenderKit/sources-upper.json (fetch_upper_models.py) and Export/SM_Env_U*.json (source .blend and maps).
"""
import json
import os
import sys
from PIL import Image, PngImagePlugin

root = sys.argv[1]
src = {r['file']: r for r in json.load(open(os.path.join(root, 'BlenderKit', 'sources-upper.json')))}
exp = os.path.join(root, 'Export')
n = 0
for name in sorted(os.listdir(exp)):
    if not (name.startswith('SM_Env_U') and name.endswith('.json')):
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
