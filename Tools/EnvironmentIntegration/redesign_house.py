"""Own-UV structural alterations. Original source and original map stay untouched."""
import bpy,bmesh,json,math
from pathlib import Path
from mathutils import Vector
R=Path(__file__).resolve().parents[2];O=R/'Tallow/SourceAssets/Production/HouseIntegrationV01';E=O/'Architecture'
bpy.ops.wm.open_mainfile(filepath=str(O/'HouseArchitecture.blend'))
mats={m.name.removeprefix('M_HeroRoomV02_'):m for m in bpy.data.materials if m.name.startswith('M_HeroRoomV02_')};new=[]
def box(name,lo,hi,material='BlackenedTimber'):
 v=[(x,y,z) for z in [lo[2],hi[2]] for y in [lo[1],hi[1]] for x in [lo[0],hi[0]]];me=bpy.data.meshes.new(name);me.from_pydata(v,[],[(0,2,3,1),(4,5,7,6),(0,1,5,4),(2,6,7,3),(0,4,6,2),(1,3,7,5)]);me.update();o=bpy.data.objects.new('SM_HouseV01_'+name,me);bpy.context.scene.collection.objects.link(o);me.materials.append(mats[material]);uv=me.uv_layers.new(name='ProductionUV')
 for f in me.polygons:
  ax=max(range(3),key=lambda i:abs(f.normal[i]));a,b=([1,2] if ax==0 else [0,2] if ax==1 else [0,1])
  for li in f.loop_indices:p=me.vertices[me.loops[li].vertex_index].co;uv.data[li].uv=(p[a]*.65,p[b]*.65)
 new.append(o);return o
# Upper floor subtraction expands central void; all other existing floor cutouts are preserved.
floor=bpy.data.objects['SM_HouseV01_Upper_Floor'];c=box('TEMP_void',(5.94,4.50,3.40),(9.66,8.80,4.20));bpy.context.view_layer.objects.active=floor;mod=floor.modifiers.new('Expanded exposed stair void','BOOLEAN');mod.operation='DIFFERENCE';mod.object=c;mod.solver='EXACT';bpy.ops.object.modifier_apply(modifier=mod.name);new.remove(c);bpy.data.objects.remove(c,do_unlink=True)
# Sturdy upper landing rail: north lip, with an entrance notch over the top tread.
for a,b in [((7.48,4.48,3.9),(9.65,4.54,4.85)),((5.94,8.78,3.9),(9.66,8.84,4.85))]:
 x0,y0,z0=a;x1,y1,z1=b
 box('AtriumGuardRail'+str(len(new)),(x0,y0,z1-.07),(x1,y1,z1))
 n=max(2,int((x1-x0)/.16))
 for i in range(n+1):x=x0+(x1-x0)*i/n;box('AtriumSpindle'+str(len(new)),(x,y0,z0),(x+.035,y1,z1))
# Arrival alcove: dogleg screen obscures workshop until moving around its right edge.
# 1.6m clear passage to east; it does not intersect chair/candle/start or workshop arch.
box('WakeScreen',(0.55,5.75,-3.3),(2.05,5.88,-.55),'LimePlaster')
box('WakeScreenDado',(0.55,5.72,-3.3),(2.05,5.91,-2.45))
# Crooked beams and asymmetrical brackets sit overhead, outside 2.4m Keeper clearance.
for i,(x,y,z,l) in enumerate([(1.0,2.0,-.65,3.8),(10.2,3.0,3.1,4.8),(.5,9.1,6.65,4.8),(.8,11.8,6.7,4.5)]):
 ob=box('HewnCrossBeam'+str(i),(x,y,z),(x+l,y+.16,z+.20));ob.rotation_euler.z=math.radians((-1)**i*.7)
# Collection partition makes a reveal bend while keeping >1.6m exit lane.
box('CollectionRevealScreen',(3.6,7.7,3.9),(3.74,9.25,6.5),'FadedWallpaper')
# All geometry above is local authored production UV, no source/generated UV relabel.
rows=json.loads((E/'manifest.json').read_text());rows=[r for r in rows if r['mesh'] not in {o.name for o in new}]
for o in [floor]+new:
 bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o;bpy.ops.export_scene.fbx(filepath=str(E/(o.name+'.fbx')),use_selection=True,object_types={'MESH'},axis_forward='-Z',axis_up='Y',bake_anim=False)
 if o!=floor:
  o.data.calc_loop_triangles();rows.append({'mesh':o.name,'original_actor_label':None,'triangles':len(o.data.loop_triangles),'materials':[m.name for m in o.data.materials],'geometry':'New authored structural redesign','collision':'complex'})
for r in rows:r['collision']='complex' if r.get('original_actor_label') or 'Atrium' in r['mesh'] or 'Screen' in r['mesh'] else 'none'
(E/'manifest.json').write_text(json.dumps(rows,indent=2));bpy.ops.wm.save_as_mainfile(filepath=str(O/'HouseArchitecture.blend'));print('RESULT redesign exported',len(new),'new objects')
