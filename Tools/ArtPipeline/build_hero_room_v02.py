"""Original architectural lookdev, isolated V02; no map/shared-source writes."""
import bpy,bmesh,math,random,json
from pathlib import Path
from mathutils import Vector
R=Path(__file__).resolve().parents[2];P=R/'Tallow/SourceAssets/Production';O=P/'HeroRoomV02';O.mkdir(exist_ok=True);rng=random.Random(252)
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
materials={}
for name in ['AshTimber','BlackenedTimber','WornEdge','LimePlaster','FadedWallpaper']:
 m=bpy.data.materials.new('M_HeroRoomV02_'+name);m.use_nodes=True;bs=m.node_tree.nodes.get('Principled BSDF');bs.inputs['Specular IOR Level'].default_value=.23
 for kind,socket in [('BaseColor','Base Color'),('Roughness','Roughness')]:
  im=bpy.data.images.load(str(O/('T_'+name+'_'+kind+'.png')));im.colorspace_settings.name='sRGB' if kind=='BaseColor' else 'Non-Color';im.pack();n=m.node_tree.nodes.new('ShaderNodeTexImage');n.image=im;m.node_tree.links.new(n.outputs['Color'],bs.inputs[socket])
 if name in ['AshTimber','BlackenedTimber','LimePlaster']:
  bump=m.node_tree.nodes.new('ShaderNodeBump');bump.inputs['Strength'].default_value=.18;bump.inputs['Distance'].default_value=.006;m.node_tree.links.new(n.outputs['Color'],bump.inputs['Height']);m.node_tree.links.new(bump.outputs['Normal'],bs.inputs['Normal'])
 materials[name]=m
metal=bpy.data.materials.new('M_HeroRoomV02_Iron');metal.diffuse_color=(.035,.028,.022,1);metal.use_nodes=True;metal.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value=(.035,.028,.022,1);metal.node_tree.nodes['Principled BSDF'].inputs['Metallic'].default_value=.65;metal.node_tree.nodes['Principled BSDF'].inputs['Roughness'].default_value=.7
parts={};objs=[]
def box(label,loc,dims,mat='BlackenedTimber',rot=(0,0,0),bevel=.008,group='Architecture',crook=.0):
 bpy.ops.mesh.primitive_cube_add(size=1,location=loc);o=bpy.context.object;o.name=label;o.dimensions=dims;bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
 # Authored box-face UVs, long direction follows grain; texture tiling is intentional.
 uv=o.data.uv_layers.active;uv.name='ProductionUV';uoff=rng.uniform(.01,.66);vstart=rng.uniform(.01,.18);vlen=rng.uniform(.65,.80)
 for face in o.data.polygons:
  normal=face.normal;axes=sorted([i for i in range(3) if abs(normal[i])<.5],key=lambda i:dims[i]);a,b=axes
  for li in face.loop_indices:
   v=o.data.vertices[o.data.loops[li].vertex_index].co;uv.data[li].uv=((v[a]/dims[a]+.5)*.32+uoff,(v[b]/dims[b]+.5)*vlen+vstart)
 o.data.materials.append(materials[mat]);o.data.materials.append(materials[mat if mat=='LimePlaster' else 'WornEdge'])
 if crook:
  for v in o.data.vertices:v.co.x+=crook*(v.co.z/dims[2]+.5);v.co.y+=crook*.3*(v.co.z/dims[2]+.5)
 if bevel:
  mod=o.modifiers.new('Irregular worn arris','BEVEL');mod.width=bevel;mod.segments=2;mod.material=1;bpy.ops.object.modifier_apply(modifier=mod.name)
 o.rotation_euler=tuple(math.radians(v) for v in rot);parts.setdefault(group,[]).append(o);return o
def beam(label,a,b,width=.12,depth=.15,mat='BlackenedTimber',group='Roof'):
 a,b=Vector(a),Vector(b);length=(b-a).length;verts=[]
 for i in range(6):
  t=i/5;bow=.035*math.sin(t*math.pi);w=width*(1+.06*math.sin(t*9));dep=depth*(1+.045*math.cos(t*8))
  verts.extend([(sx*w/2+bow,sy*dep/2+.01*math.sin(t*6),length*(t-.5)) for sx,sy in [(-1,-1),(1,-1),(1,1),(-1,1)]])
 faces=[(3,2,1,0),(20,21,22,23)]+[(4*i+j,4*i+(j+1)%4,4*(i+1)+(j+1)%4,4*(i+1)+j) for i in range(5) for j in range(4)]
 me=bpy.data.meshes.new(label);me.from_pydata(verts,[],faces);me.update();o=bpy.data.objects.new(label,me);bpy.context.scene.collection.objects.link(o);o.location=(a+b)/2;o.rotation_euler=(b-a).to_track_quat('Z','Y').to_euler();me.materials.append(materials[mat]);me.materials.append(materials['WornEdge']);uv=me.uv_layers.new(name='ProductionUV')
 for face in me.polygons:
  for li in face.loop_indices:
   vi=me.loops[li].vertex_index;v=me.vertices[vi].co;uv.data[li].uv=((v.x/width+.5)*.9+.05,(v.y/depth+.5)*.9+.05) if face.index<2 else ((vi%4)/4+.01,(vi//4)/5*.96+.02)
 bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o;mod=o.modifiers.new('Carved beam arris','BEVEL');mod.width=.005;mod.segments=2;mod.material=1;bpy.ops.object.modifier_apply(modifier=mod.name);parts.setdefault(group,[]).append(o);return o
# Unequal width, staggered joints, tiny cup/yaw. No exact square grid.
x=-1.8;i=0
while x<1.8:
 width=rng.uniform(.14,.23);y=-2.2-rng.uniform(0,.8)
 while y<2.15:
  length=min(rng.uniform(.75,1.65),2.15-y);start=max(y,-2.2);length=y+length-start
  if length>.05:box('Uneven floor plank',(x+width/2,start+length/2,.02+rng.uniform(-.004,.004)),(width-.008,length-.008,.045),'AshTimber',rot=(0,rng.uniform(-.3,.3),rng.uniform(-.3,.3)),bevel=.003,group='Floor'+str(i//6))
  y=start+length
 x+=width;i+=1
# Back wall ends before a 90cm escape opening; layered plaster, torn paper above dark wainscot.
box('Back lime wall',(-.7,2.14,1.18),(2.2,.1,2.36),'LimePlaster',bevel=.015,group='Walls')
box('Left lime wall',(-1.82,0,1.18),(.1,4.3,2.36),'LimePlaster',group='Walls')
for x in [-1.62,-1.25,-.88,-.51,-.14,.20]:
 box('Wainscot board',(x,2.07,.40),(.34,.07,.78),'BlackenedTimber',rot=(0,rng.uniform(-1,1),0),group='WallJoinery')
 box('Raised panel moulding',(x,2.015,.42),(.27,.035,.57),'AshTimber',bevel=.012,group='WallJoinery')
beam('Uneven dado',(-1.8,1.99,.84),(.4,1.99,.80),.045,.055,group='WallJoinery')
# Wallpaper has irregular exposed plaster edges, restrained original flower print.
for index in range(6):
 x=-1.78+index*.355;verts=[];steps=8
 for i in range(steps+1):
  z=.91+i*.175;edge=rng.uniform(-.026,.026);verts.extend([(x+edge,2.074,z),(x+.34+rng.uniform(-.02,.02),2.074,z)])
 faces=[(2*i,2*i+1,2*i+3,2*i+2) for i in range(steps)]
 me=bpy.data.meshes.new('TornWallpaper');me.from_pydata(verts,[],faces);me.update();ob=bpy.data.objects.new('Torn wallpaper strip',me);bpy.context.scene.collection.objects.link(ob);ob.data.materials.append(materials['FadedWallpaper']);uv=me.uv_layers.new(name='ProductionUV')
 for face in me.polygons:
  for li in face.loop_indices:
   v=me.vertices[me.loops[li].vertex_index].co;uv.data[li].uv=((v.x+1.8)*.8,(v.z-.9)*.8)
 parts.setdefault('Wallpaper',[]).append(ob)
# Dark bowed frame, asymmetric post silhouettes, roof battens follow ridge.
for y in [-1.7,.05,1.94]:
 beam('Crooked left rafter',(-1.84,y,1.91),(-.08,y+.035,2.84),.14,.17)
 beam('Crooked right rafter',(-.08,y+.035,2.84),(1.83,y,1.94),.16,.17)
 beam('Wall cruck',(-1.75,y,.02),(-1.70,y+.06,1.98),.13,.17)
for side in [-1,1]:
 for j in range(8):
  t=(j+.5)/8;x=side*t*1.8;z=2.84-t*.91
  box('Roof board',(x,0,z),(.25,4.4,.045),'AshTimber',rot=(0,side*27,0),group='RoofBoards',bevel=.004)
beam('Ridge spine',(-.08,-2.2,2.82),(-.04,2.2,2.82),.14,.17)
# Exit stays >=90cm clear. Small top landing, no stair/route alteration outside staging.
beam('Left exit jamb',(.45,2.05,.03),(.39,2.04,2.15),.15,.18,group='Exit')
beam('Right exit jamb',(1.55,2.05,.03),(1.60,2.05,2.1),.16,.18,group='Exit')
beam('Leaning exit lintel',(.36,2.04,2.15),(1.66,2.04,2.10),.17,.20,group='Exit')
box('Exit threshold',(1.00,2.18,.055),(1.05,.52,.10),'AshTimber',group='Exit')
# Human-size vacant collection bay, aged black wood, no repetitive green-chip treatment.
box('Empty compartment back',(-.49,1.92,1.15),(.66,.09,1.55),'AshTimber',group='CollectionBay')
for x,z in [(-.86,1.14),(-.10,1.15)]:beam('Bay upright',(x,1.78,.33),(x+.025,1.8,1.96),.065,.11,group='CollectionBay')
for z in [.34,1.98]:beam('Bay rail',(-.91,1.78,z),(-.01,1.78,z+.025),.09,.14,group='CollectionBay')
# Sparse wood pegs give empty occupant-shaped space context.
for x in [-.73,-.23]:box('Empty bay tie peg',(x,1.70,1.68),(.045,.14,.045),'AshTimber',group='CollectionBay')
# Export independently grouped new architecture with self-authored UVs.
reports=[];placements=[]
for group,items in parts.items():
 bpy.ops.object.select_all(action='DESELECT')
 for o in items:o.select_set(True)
 bpy.context.view_layer.objects.active=items[0];bpy.ops.object.join();o=bpy.context.object;o.name='SM_HeroRoomV02_'+group;bpy.context.scene.cursor.location=(0,0,0);bpy.ops.object.origin_set(type='ORIGIN_CURSOR');bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
 bm=bmesh.new();bm.from_mesh(o.data);bmesh.ops.triangulate(bm,faces=list(bm.faces));bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(o.data);bm.free();o.data.calc_loop_triangles();assert len(o.data.loop_triangles)<8000
 bpy.ops.export_scene.fbx(filepath=str(O/(o.name+'.fbx')),use_selection=True,object_types={'MESH'},axis_forward='-Z',axis_up='Y',bake_anim=False,path_mode='COPY',embed_textures=False)
 reports.append({'mesh':o.name,'triangles':len(o.data.loop_triangles),'uv':'ProductionUV','uv_usage':'Self-authored directional material tiling; repeated charts intentional, not a unique bake atlas','materials':[m.name for m in o.data.materials]});placements.append({'asset':o.name,'location_m':[0,0,0]})
# Append existing detailed props without modifying sources.
def asset(prop,objectname):
 with bpy.data.libraries.load(str(P/prop/(prop+'_Production.blend')),link=False) as(a,b):b.objects=[objectname]
 o=b.objects[0];bpy.context.scene.collection.objects.link(o);o.hide_render=False;o.hide_set(False);o.rotation_mode='XYZ';return o
with bpy.data.libraries.load(str(P/'CabinetFinished/Cabinet_SageTimber.blend'),link=False) as(a,b):b.objects=['SM_Cabinet_Production']
cab=b.objects[0];bpy.context.scene.collection.objects.link(cab);cab.hide_render=False;cab.hide_set(False);cab.location=(-1.30,1.58,.04)
doll=asset('CollectionDoll','SM_CollectionDoll');dolls=[]
for label,loc,scale,yaw in [('ShelfBack',(-1.3,1.49,.89),.50,180),('ShelfSide',(-1.28,1.50,.49),.42,155),('SilentWitness',(.15,1.68,.05),1.04,25)]:
 o=doll.copy();o.data=doll.data;bpy.context.scene.collection.objects.link(o);o.name=label;o.location=loc;o.scale=(scale,)*3;o.rotation_euler=(0,0,math.radians(yaw));dolls.append(o)
doll.hide_render=True;doll.hide_set(True)
scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.device='CPU';scene.cycles.samples=32;scene.cycles.use_denoising=True;scene.render.threads_mode='FIXED';scene.render.threads=2;scene.render.resolution_x=1440;scene.render.resolution_y=900;scene.render.resolution_percentage=100;scene.world.use_nodes=True;scene.world.node_tree.nodes['Background'].inputs['Color'].default_value=(.025,.04,.065,1);scene.world.node_tree.nodes['Background'].inputs['Strength'].default_value=.13
lights=[]
def area(name,loc,target,power,color,size):
 d=bpy.data.lights.new(name,'AREA');d.energy=power;d.color=color;d.shape='DISK';d.size=size;o=bpy.data.objects.new(name,d);scene.collection.objects.link(o);o.location=loc;o.rotation_euler=(Vector(target)-o.location).to_track_quat('-Z','Y').to_euler();lights.append(o);return o
area('Cold exit wash',(1.1,3,1.8),(-.7,.2,.8),190,(.48,.64,1),1.2)
area('Weak amber practical',(-1.3,-1.1,.8),(-.3,1.8,1),32,(1,.57,.29),.35)
area('Soft room bounce',(.4,-1.5,2),(-.5,1,1),26,(.43,.48,.58),2.8)
bpy.ops.object.camera_add(location=(.75,-2.65,1.10));cam=bpy.context.object;cam.name='Camera_HeroRoomV02';scene.camera=cam;cam.rotation_euler=(Vector((-.35,1.6,1.25))-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.lens=28
(O/'architecture-metrics.json').write_text(json.dumps(reports,indent=2));(O/'architecture-placements.json').write_text(json.dumps(placements,indent=2));scene.render.filepath=str(O/'attic-v02.png');bpy.ops.wm.save_as_mainfile(filepath=str(O/'AtticV02.blend'))
# Dedicated menu corner: camera composition leaves dark left 40% free, chair/player on right.
for o in dolls:o.hide_render=True;o.hide_set(True)
cab.hide_render=True;cab.hide_set(True)
for name in ['SM_HeroRoomV02_CollectionBay','SM_HeroRoomV02_Exit']:bpy.data.objects[name].hide_render=True
# Backfill exit for menu study only, no architecture export overwrite.
backdrop=box('Menu plaster return',(1.0,2.14,1.18),(1.4,.1,2.36),'LimePlaster',group='MenuOnly')
bpy.ops.object.select_all(action='DESELECT');backdrop.select_set(True);bpy.context.view_layer.objects.active=backdrop;bpy.context.scene.cursor.location=(0,0,0);bpy.ops.object.origin_set(type='ORIGIN_CURSOR');bpy.ops.object.transform_apply(location=True,rotation=True,scale=True);backdrop.name='SM_HeroRoomV02_MenuReturn';backdrop.data.calc_loop_triangles();bpy.ops.export_scene.fbx(filepath=str(O/(backdrop.name+'.fbx')),use_selection=True,object_types={'MESH'},axis_forward='-Z',axis_up='Y',bake_anim=False,path_mode='COPY');reports.append({'mesh':backdrop.name,'triangles':len(backdrop.data.loop_triangles),'uv':'ProductionUV','uv_usage':'self-authored tiling','menu_only':True,'materials':[m.name for m in backdrop.data.materials]});(O/'architecture-metrics.json').write_text(json.dumps(reports,indent=2))
chair=asset('RestraintChair','SM_RestraintChair');chair.location=(.83,.85,.045);chair.scale=(.70,.70,.70)
playerrel=Path('Tallow/SourceAssets/Player/SeatedV04/04_seated-bound.blend');playerpath=R/playerrel
if not playerpath.exists():playerpath=R.parent/'player'/playerrel
assert playerpath.exists(),'Copy supplied player source into repo or keep sibling player worktree'
with bpy.data.libraries.load(str(playerpath),link=False) as(a,b):b.objects=['RIG_Tallow_Generated','SK_Tallow_Generated_Full'];b.actions=['AN_Generated_SeatedBoundIdle']
for o in b.objects:scene.collection.objects.link(o);o.hide_render=False;o.hide_set(False)
rig=bpy.data.objects['RIG_Tallow_Generated'];rig.animation_data_create();rig.animation_data.action=b.actions[0];rig.animation_data.action_slot=b.actions[0].slots[0];rig.rotation_mode='XYZ';rig.rotation_euler.z=math.pi;rig.location=(.83,.72,.12);scene.frame_set(18)
bpy.context.view_layer.update()
for o in lights:o.hide_render=True
area('Menu warm seat light',(1.6,-.9,.95),(.8,.8,1),46,(1,.58,.3),.65)
area('Menu cold edge',(1.4,1.8,2.4),(.9,.8,1),70,(.39,.52,.78),.8)
area('Menu dim fill',(-.6,-1.7,1.3),(.7,.8,1),7,(.5,.55,.68),2)
cam.location=(0,-3.45,1.0);cam.rotation_euler=(Vector((0,.7,1.05))-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.lens=36;scene.render.filepath=str(O/'menu-v02.png');bpy.ops.wm.save_as_mainfile(filepath=str(O/'MenuV02.blend'))
(O/'player-fit.json').write_text(json.dumps({'source':'Tallow/SourceAssets/Player/SeatedV04/04_seated-bound.blend read only','action':'AN_Generated_SeatedBoundIdle','frame':18,'rig_location_m':list(rig.location),'rig_yaw_degrees':180,'chair_location_m':list(chair.location),'chair_scale':.70,'wrist_fit':'Baked SeatedV04 targets x=.83±.285,y=.59,z=.67m; source action appended unchanged','status':'visual scene fit; source rig/action untouched; root integrates animation'},indent=2));print('V02 saved isolated sources and architecture exports')
