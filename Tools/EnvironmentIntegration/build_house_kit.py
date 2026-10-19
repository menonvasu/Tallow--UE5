"""Preserve measured shell/route geometry; author isolated own-UV art replacements."""
import bpy,bmesh,json,math
from pathlib import Path
from mathutils import Vector
R=Path(__file__).resolve().parents[2];O=R/'Tallow/SourceAssets/Production/HouseIntegrationV01';E=O/'Architecture';E.mkdir(exist_ok=True)
bpy.ops.wm.open_mainfile(filepath=str(O/'ReadOnlyHouseReference.blend'))
with bpy.data.libraries.load(str(R/'Tallow/SourceAssets/Production/HeroRoomV02/AtticV02.blend'),link=False) as(a,b):b.materials=[n for n in a.materials if n.startswith('M_HeroRoomV02_')]
mats={m.name.removeprefix('M_HeroRoomV02_'):m for m in b.materials};rows=[];originals=[];decor={}
for o in list(bpy.context.scene.objects):
 if o.type!='MESH' or o.name.startswith('UCX'):bpy.data.objects.remove(o,do_unlink=True);continue
 original=o.name;originals.append(o);o.name='SM_HouseV01_'+original;o.data.materials.clear()
 wall='_Wall_' in original;floor='Floor' in original;material='AshTimber' if floor or 'Stair' in original else 'LimePlaster' if wall else 'BlackenedTimber'
 o.data.materials.append(mats[material])
 if wall:
  o.data.materials.append(mats['BlackenedTimber']);o.data.materials.append(mats['FadedWallpaper']);base=min(v.co.z for v in o.data.vertices)
  # Coplanar segmentation changes material boundaries, not openings or volume.
  bm=bmesh.new();bm.from_mesh(o.data);bmesh.ops.bisect_plane(bm,geom=list(bm.verts)+list(bm.edges)+list(bm.faces),dist=.00001,plane_co=(0,0,base+.85),plane_no=(0,0,1));bm.to_mesh(o.data);bm.free()
  for f in o.data.polygons:
   z=sum(o.data.vertices[i].co.z for i in f.vertices)/len(f.vertices)
   if z<base+.851:f.material_index=1
   elif 'Upper' in original and abs(f.normal.z)<.5:f.material_index=2
 # All texture coordinates authored from true metre-space face planes. Tiling intentional.
 for uv in list(o.data.uv_layers):o.data.uv_layers.remove(uv)
 uv=o.data.uv_layers.new(name='ProductionUV')
 for f in o.data.polygons:
  n=f.normal;axis=max(range(3),key=lambda i:abs(n[i]));a,c=([1,2] if axis==0 else [0,2] if axis==1 else [0,1])
  for li in f.loop_indices:
   v=o.data.vertices[o.data.loops[li].vertex_index].co;uv.data[li].uv=(v[a]*.65,v[c]*.65)
 o['original_actor_label']='House_'+original

def box(name,lo,hi,mat,group):
 lo,hi=Vector(lo),Vector(hi);verts=[(x,y,z) for z in [lo.z,hi.z] for y in [lo.y,hi.y] for x in [lo.x,hi.x]];faces=[(0,2,3,1),(4,5,7,6),(0,1,5,4),(2,6,7,3),(0,4,6,2),(1,3,7,5)];me=bpy.data.meshes.new(name);me.from_pydata(verts,[],faces);me.update();ob=bpy.data.objects.new(name,me);bpy.context.scene.collection.objects.link(ob);me.materials.append(mats[mat]);uv=me.uv_layers.new(name='ProductionUV')
 for f in me.polygons:
  axis=max(range(3),key=lambda i:abs(f.normal[i]));a,c=([1,2] if axis==0 else [0,2] if axis==1 else [0,1])
  for li in f.loop_indices:v=me.vertices[me.loops[li].vertex_index].co;uv.data[li].uv=(v[a]*.8,v[c]*.8)
 decor.setdefault(group,[]).append(ob);return ob
# Moulded baseboard and dado precisely follow solid wall segments; stop at actual openings.
parts=json.loads((O/'wall-parts.json').read_text())
for name,segments in parts.items():
 base=min(p['min'][2] for p in segments);group='Trim_'+name.split('_')[0]
 for p in segments:
  lo,hi=p['min'],p['max']
  if abs(lo[2]-base)>.02:continue
  long=0 if hi[0]-lo[0]>hi[1]-lo[1] else 1;short=1-long
  if hi[long]-lo[long]<.3:continue
  for side in [-1,1]:
   for z,h,d in [(base+.05,.10,.025),(base+.82,.055,.035)]:
    a=list(lo);b=list(hi);a[2]=z-h/2;b[2]=z+h/2;edge=lo[short] if side<0 else hi[short];a[short]=edge-d if side<0 else edge;b[short]=edge if side<0 else edge+d;box('Moulded rail',a,b,'BlackenedTimber',group)
# Tread nosings are true measured heights, not resized stairs or random steps.
audit=json.loads((O/'source-house-audit.json').read_text())['objects']
for stair in [a for a in audit if a['name'] in ['MainStairs','BackStairs','BasementStairs']]:
 for i,t in enumerate(stair['horizontal_treads']):
  x0,y0=t['min_xy'];x1,y1=t['max_xy'];z=t['z'];group='StairDetail_'+stair['name'];box('Carved tread',(x0,y0,z-.035),(x1,y1,z+.002),'AshTimber',group)
  # Small outer-edge uprights every third tread; no obstruction in the 1.3m clear lane.
  if i%3==0 and x1-x0<1.5:
   box('Turned newel base',(x0+.01,y0+.02,z),(x0+.065,y0+.075,z+.75),'BlackenedTimber',group)
# Curated shelf supports match randomized item point heights.
for name,loc,size in [('WorkshopShelf',(0.90,11.30,-2.54),(.90,.38,.06)),('WorkshopSideShelf',(4.40,7.80,-2.29),(.95,.42,.06)),('BoilerFuseShelf',(14.20,3.50,-2.39),(.9,.40,.06)),('BedroomFuseShelf',(12.00,5.20,4.71),(.85,.4,.06)),('ToyFuseShelf',(1.20,3.80,4.81),(.90,.42,.06))]:
 lo=Vector(loc)-Vector(size)/2;hi=Vector(loc)+Vector(size)/2;box(name,lo,hi,'AshTimber','PuzzleShelves')
# Gate leaf: narrow upper final-room gate. Hinge origin is left bottom, opens around local Z.
box('FinalGateDoor',(0,-.035,0),(1.08,.035,2.32),'BlackenedTimber','GateDoor_Narrow')
for x in [-.075,1.10]:box('FinalGateJamb',(x,-.07,0),(x+.06,.07,2.4),'AshTimber','GateFrame_Narrow')
box('FinalGateLintel',(-.075,-.07,2.34),(1.16,.07,2.4),'AshTimber','GateFrame_Narrow')
# Wide basement leaf fits below arch shoulders; physical gate blocks the child route while allowing existing arch silhouette.
box('BasementGateLeaf',(0,-.04,0),(1.58,.04,1.76),'BlackenedTimber','GateDoor_Wide')
for x in [-.06,1.60]:box('BasementGateJamb',(x,-.07,0),(x+.05,.07,1.8),'AshTimber','GateFrame_Wide')
for group,objects in decor.items():
 bpy.ops.object.select_all(action='DESELECT')
 for ob in objects:ob.select_set(True)
 bpy.context.view_layer.objects.active=objects[0];bpy.ops.object.join();ob=bpy.context.object;ob.name='SM_HouseV01_'+group;originals.append(ob)
for o in originals:
 bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o;bpy.ops.object.transform_apply(location=True,rotation=True,scale=True);o.data.calc_loop_triangles();bpy.ops.export_scene.fbx(filepath=str(E/(o.name+'.fbx')),use_selection=True,object_types={'MESH'},axis_forward='-Z',axis_up='Y',bake_anim=False,path_mode='COPY');rows.append({'mesh':o.name,'original_actor_label':o.get('original_actor_label'),'triangles':len(o.data.loop_triangles),'materials':[m.name for m in o.data.materials],'geometry':'Original shell shape and openings retained' if o.get('original_actor_label') else 'New authored trim/step/shelf/gate mesh','collision':'Original actor collision retained; new decoration nonblocking; gate leaf uses imported simple hull'})
(E/'manifest.json').write_text(json.dumps(rows,indent=2));bpy.ops.wm.save_as_mainfile(filepath=str(O/'HouseArchitecture.blend'));print('RESULT '+json.dumps({'meshes':len(rows),'tris':sum(r['triangles'] for r in rows)}))
