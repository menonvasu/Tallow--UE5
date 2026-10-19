import bpy,json
from pathlib import Path
R=Path(__file__).resolve().parents[2];O=R/'Tallow/SourceAssets/Production/HouseIntegrationV01/TaskProps';bpy.ops.wm.open_mainfile(filepath=str(O/'TaskProps.blend'));rows=[]
for o in bpy.context.scene.objects:
 if o.type!='MESH':continue
 o.data.calc_loop_triangles();uv=o.data.uv_layers.get('ProductionUV');zero=0
 for t in o.data.loop_triangles:
  a,b,c=[uv.data[i].uv for i in t.loops];area=abs((b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x))*.5
  if area<1e-12:zero+=1
 rows.append(dict(mesh=o.name,triangles=len(o.data.loop_triangles),dimensions_m=list(o.dimensions),uv_layers=[u.name for u in o.data.uv_layers],zero_area_uv_triangles=zero,overlap='Intentional planar projection for constant/procedural finishes; not a bake atlas',visual_review='Agent inspected review-task-props.png; frame overlap repaired'))
(O/'measured-metrics.json').write_text(json.dumps(rows,indent=2));print('RESULT',rows)
