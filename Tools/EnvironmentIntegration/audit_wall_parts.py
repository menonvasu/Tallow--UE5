import bpy,json
from pathlib import Path
R=Path(__file__).resolve().parents[2];O=R/'Tallow/SourceAssets/Production/HouseIntegrationV01';bpy.ops.wm.open_mainfile(filepath=str(O/'ReadOnlyHouseReference.blend'));out={}
for o in bpy.context.scene.objects:
 if o.type!='MESH' or '_Wall_' not in o.name or o.name.startswith('UCX'):continue
 adj={i:set() for i in range(len(o.data.vertices))}
 for e in o.data.edges:a,b=e.vertices;adj[a].add(b);adj[b].add(a)
 unseen=set(adj);parts=[]
 while unseen:
  todo=[unseen.pop()];ids=[]
  while todo:
   i=todo.pop();ids.append(i)
   for n in adj[i]&unseen:unseen.remove(n);todo.append(n)
  vs=[o.matrix_world@o.data.vertices[i].co for i in ids];parts.append({'min':[round(min(v[k] for v in vs),4) for k in range(3)],'max':[round(max(v[k] for v in vs),4) for k in range(3)]})
 out[o.name]=parts
(O/'wall-parts.json').write_text(json.dumps(out,indent=2));print('RESULT wall parts',len(out))
