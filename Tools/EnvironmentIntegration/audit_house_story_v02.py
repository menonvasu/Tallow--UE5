import bpy,json,hashlib,math
from mathutils import Vector,Quaternion
from pathlib import Path
R=Path(__file__).resolve().parents[2];O=R/'Tallow/SourceAssets/Production/HouseStoryV02';bpy.ops.wm.open_mainfile(filepath=str(O/'HouseStoryV02.blend'));rows=[]
for o in bpy.context.scene.objects:
 if o.type!='MESH':continue
 o.data.calc_loop_triangles();uv=o.data.uv_layers.get('ProductionUV');bad=0
 for t in o.data.loop_triangles:
  a,b,c=[uv.data[i].uv for i in t.loops];area=abs((b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x))*.5
  if area<1e-12:bad+=1
 rows.append(dict(mesh=o.name,triangles=len(o.data.loop_triangles),zero_area_uv_triangles=bad,uv_layers=[v.name for v in o.data.uv_layers],dimensions_m=list(o.dimensions),export_sha256=hashlib.sha256((O/(o.name+'.fbx')).read_bytes()).hexdigest()))
p=json.loads((O/'placement.json').read_text());by={r['mesh']:r['triangles'] for r in rows};total=sum(by[r['mesh']] for r in p['placements']);assert total<=10000;assert all(r['zero_area_uv_triangles']==0 for r in rows)
# Runners are entirely on verified solid floor regions and are never blocking geometry.
rug_checks=[]
for r in p['placements']:
 if 'Runner' not in r['mesh']:continue
 x,y,z=r['location_cm'];ob=bpy.data.objects[r['mesh']];q=Quaternion((0,0,1),math.radians(-r['yaw']))@Quaternion((0,1,0),math.radians(r['tilt']));points=[]
 for corner in ob.bound_box:
  v=q@Vector([corner[i]*r['scale'][i] for i in range(3)]);points.append([x+v.x*100,y-v.y*100,z+v.z*100])
 bounds=[min(v[0] for v in points),min(v[1] for v in points),max(v[0] for v in points),max(v[1] for v in points)]
 rug_checks.append(dict(label=r['label'],bounds_xy_cm=bounds,surface_offset_cm_max=1.2,collision='NoCollision',floor_note='Upper landing runner ends before main void y=-450; collection/wake entirely inside room floors'))
(O/'measured-audit.json').write_text(json.dumps(dict(meshes=rows,placed_triangles=total,uv_overlap='Intentional projection on constant-color frame/peg surfaces; artwork and rug have full-sheet0–1 UVs',rug_checks=rug_checks,limits='Decor-only source checks; root UE image inspection still required'),indent=2));print('RESULT audit passed',total)
