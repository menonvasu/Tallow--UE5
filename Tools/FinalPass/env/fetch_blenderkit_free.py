"""Download the free BlenderKit models used in the parlour (no login: these are free assets) and log their licence.

python3 -I fetch_blenderkit_free.py <project>/Tallow/SourceAssets/FinalPass/Env/BlenderKit
Each lands as <out>/<id8>_<slug>.blend; sources.json records title, author, licence and URL.
"""
import json
import os
import sys
import urllib.parse
import urllib.request

ASSETS = {  # asset base id prefix: (search words, use)
    '2bc2b5b6': ('Fireplace tools set', 'companion set beside the hearth'),
    '24107bdf': ('Old rusty stove', 'kitchen cooker (replaces the generated range; a 1950s cooker is right for the 1974 house)'),
    'c5134ad0': ('Victorian Double Student Lamp', 'study desk oil lamp (a practical light)'),
}
HEAD = {'User-Agent': 'Blender/5.2 blenderkit'}
out = sys.argv[1]; os.makedirs(out, exist_ok=True)
rows = []
for bid, (words, use) in ASSETS.items():
    q = urllib.parse.urlencode({'query': f'{words} asset_type:model is_free:true', 'page_size': 40})
    hits = json.load(urllib.request.urlopen(urllib.request.Request('https://www.blenderkit.com/api/v1/search/?' + q, headers=HEAD), timeout=60))['results']
    a = [h for h in hits if h['assetBaseId'].startswith(bid)][0]
    files = {f['fileType']: f for f in a['files']}
    f = files.get('resolution_2K') or files['blend']
    slug = ''.join(c if c.isalnum() else '_' for c in a['name'])[:32]
    dst = os.path.join(out, f'{bid}_{slug}.blend')
    if not os.path.exists(dst):
        meta = json.load(urllib.request.urlopen(urllib.request.Request(f['downloadUrl'] + '?scene_uuid=7a3e1b52-0e1d-4c2b-9b8e-5e0e7c0a0001', headers=HEAD), timeout=60))
        open(dst, 'wb').write(urllib.request.urlopen(urllib.request.Request(meta['filePath'], headers=HEAD), timeout=600).read())
    au = a.get('author') or {}
    rows.append({'id': a['assetBaseId'], 'title': a['name'], 'author': f"{au.get('firstName', '')} {au.get('lastName', '')}".strip(),
                 'licence': a.get('license'), 'free': a.get('isFree'), 'file': os.path.basename(dst), 'variant': f['fileType'],
                 'url': f"https://www.blenderkit.com/asset-gallery-detail/{a['assetBaseId']}/", 'use': use})
    print('ok', dst, os.path.getsize(dst))
json.dump(rows, open(os.path.join(out, 'sources.json'), 'w'), indent=1)
