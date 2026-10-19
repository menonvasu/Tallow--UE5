"""Check source surface retention, UV finiteness, weights, animated triangle stretch and fixed root."""
import bpy,json,math,numpy as np
from pathlib import Path
OUT=Path(__file__).resolve().parents[2]/'Tallow/SourceAssets/Keeper/SleevedV06'
bpy.ops.wm.open_mainfile(filepath=str(OUT/'keeper-direct-animated.blend'));o=bpy.data.objects['SK_Keeper_Tripo_Direct'];r=bpy.data.objects['KEEPER_PRODUCTION_RIG'];m=o.data
source=np.load(OUT/'semantic-source.npz')['positions'];source_set={tuple(np.round(p,6)) for p in source};retained=all(tuple(np.round(v.co[:],6)) in source_set for v in m.vertices)
assert retained,'Vertex moved away from source surface at bind'
assert len(m.polygons)==78554
weights=[sum(g.weight for g in v.groups) for v in m.vertices];assert min(weights)>.9998 and max(weights)<1.0002
uv=np.array([u.uv[:] for u in m.uv_layers.active.data]);assert np.isfinite(uv).all();assert uv.min()>-.0001 and uv.max()<1.0001
areas=[]
for poly in m.polygons:
 a,b,c=[uv[i] for i in poly.loop_indices];areas.append(abs(float((b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])))/2)
assert min(areas)>1e-14, 'Collapsed production UV triangle'
rest=np.array([v.co[:] for v in m.vertices]);edges=np.array([e.vertices[:] for e in m.edges]);length=np.linalg.norm(rest[edges[:,0]]-rest[edges[:,1]],axis=1)
rows=[]
for name,frame in [('Idle',1),('Reveal',73),('Chase',7),('JumpscareDeath',19),('Squeeze',25)]:
 a=bpy.data.actions['AN_Keeper_'+name];k=bpy.data.actions['MORPH_Keeper_'+name];r.animation_data.action=a;r.animation_data.action_slot=a.slots[0];m.shape_keys.animation_data.action=k;m.shape_keys.animation_data.action_slot=k.slots[0];bpy.context.scene.frame_set(frame)
 ev=o.evaluated_get(bpy.context.evaluated_depsgraph_get());me=ev.to_mesh();v=np.array([p.co[:] for p in me.vertices]);assert np.isfinite(v).all();ratios=np.linalg.norm(v[edges[:,0]]-v[edges[:,1]],axis=1)/np.maximum(length,1e-8)
 rows.append(dict(clip=name,frame=frame,edge_stretch_p99=float(np.percentile(ratios,99)),edge_stretch_max=float(ratios.max()),max_edge_length_increase_m=float((np.linalg.norm(v[edges[:,0]]-v[edges[:,1]],axis=1)-length).max()),root_displacement=float(r.pose.bones['root'].matrix.translation.length)));ev.to_mesh_clear()
finger_mass={}
for pair in range(1,4):
 for side in ['L','R']:
  for digit in range(1,6):
   prefix=f'arm_{pair}.{side}_digit{digit}_';indices={g.index for g in o.vertex_groups if g.name.startswith(prefix)}
   finger_mass[prefix]=sum(g.weight for v in m.vertices for g in v.groups if g.group in indices)
assert min(finger_mass.values())>0,'A finger chain has no weighted surface'
report=dict(finger_weight_mass=finger_mass,original_triangles_retained=78554,source_vertex_positions_retained=retained,source_vertices=len(source),delivery_vertices=len(m.vertices),seam_duplicates=len(m.vertices)-len(source),weight_sum_range=[min(weights),max(weights)],max_influences=max(len(v.groups) for v in m.vertices),uv_finite_in_unit_square=True,minimum_uv_triangle_area=min(areas),collapsed_uv_triangles=sum(a<1e-14 for a in areas),stress=rows,remaining=['Unreal skeletal import and render validation','Wall/Ceiling actual traversal route review','Pose seam openings require in-engine viewing; no raw surface faces deleted'])
(OUT/'delivery-validation.json').write_text(json.dumps(report,indent=2));print('DIRECT_VALIDATION '+json.dumps(report))
