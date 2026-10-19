import bpy,json,math
from pathlib import Path
from collections import defaultdict
R=Path(__file__).resolve().parents[2];O=R/'Tallow/SourceAssets/Production/HouseIntegrationV01';O.mkdir(exist_ok=True)
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
bpy.ops.import_scene.fbx(filepath=str(R/'Tallow/SourceAssets/TallowHouse/SM_TallowHouse_Greybox.fbx'));bpy.context.view_layer.update();rows=[]
for o in bpy.context.scene.objects:
 if o.type!='MESH':continue
 m=o.data;m.calc_loop_triangles();pts=[o.matrix_world@v.co for v in m.vertices];row={'name':o.name,'triangles':len(m.loop_triangles),'bounds_min_m':[min(v[i] for v in pts) for i in range(3)],'bounds_max_m':[max(v[i] for v in pts) for i in range(3)]}
 if 'stair' in o.name.lower():
  levels=defaultdict(list)
  for p in m.polygons:
   vs=[o.matrix_world@m.vertices[i].co for i in p.vertices]
   if max(v.z for v in vs)-min(v.z for v in vs)<.0001 and p.normal.z>.8:levels[round(sum(v.z for v in vs)/len(vs),4)].extend(vs)
  row['horizontal_treads']=[{'z':z,'min_xy':[min(v[i] for v in vs) for i in range(2)],'max_xy':[max(v[i] for v in vs) for i in range(2)]} for z,vs in sorted(levels.items())]
 rows.append(row)
(O/'source-house-audit.json').write_text(json.dumps({'blender_version':list(bpy.app.version),'units':'metres after FBX import','objects':rows},indent=2));bpy.ops.wm.save_as_mainfile(filepath=str(O/'ReadOnlyHouseReference.blend'));print('RESULT '+json.dumps([r for r in rows if 'stair' in r['name'].lower()]))
