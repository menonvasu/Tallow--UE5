import bpy,json
from pathlib import Path
from collections import defaultdict
R=Path(__file__).resolve().parents[3];O=R/'Tallow/SourceAssets/Keeper/V07RevealRevision01'
bpy.ops.wm.open_mainfile(filepath=str(R/'Tallow/SourceAssets/Keeper/RiggedV07/keeper-v07-export.blend'))
o=next(o for o in bpy.data.objects if o.type=='MESH');ks=o.data.shape_keys.key_blocks;groups=defaultdict(list)
for i,v in enumerate(ks['Basis'].data):groups[tuple(round(c,7) for c in v.co)].append(i)
bad=[]
for co,ids in groups.items():
 if len(ids)>1:
  spread=max((ks['HOOD_OPEN'].data[a].co-ks['HOOD_OPEN'].data[b].co).length for a in ids for b in ids)
  if spread>.001:bad.append({'co':co,'ids':ids,'open_spread_m':spread})
print('RESULT '+json.dumps({'divergent_coincident_groups':len(bad),'vertices':sum(len(x['ids']) for x in bad),'max_spread_m':max(x['open_spread_m'] for x in bad)}))
(O/'coincident-seam-audit.json').write_text(json.dumps(bad,indent=2))
