"""Original compact garden silhouette set; camera contract preserved. CPU preview only."""
import bpy,math,json,random
from pathlib import Path
from mathutils import Vector
R=Path(__file__).resolve().parents[2];O=R/'Tallow/SourceAssets/Production/HouseOutroV01';O.mkdir(exist_ok=True)
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
rng=random.Random(25218);parts=[];mats={}
for name,col,metal in [('CharredWood',(.045,.033,.024),0),('Iron',(.027,.035,.041),.45),('WetStone',(.075,.086,.09),0),('Earth',(.024,.03,.026),0),('PaleEdge',(.12,.11,.08),0)]:
 m=bpy.data.materials.new('M_Outro_'+name);m.diffuse_color=(*col,1);m.use_nodes=True;p=m.node_tree.nodes.get('Principled BSDF');p.inputs['Base Color'].default_value=(*col,1);p.inputs['Roughness'].default_value=.84;p.inputs['Metallic'].default_value=metal;mats[name]=m

def beam(a,b,r,mat='CharredWood',r2=None,verts=6):
 a,b=Vector(a),Vector(b);d=b-a;bpy.ops.mesh.primitive_cone_add(vertices=verts,radius1=r,radius2=r if r2 is None else r2,depth=d.length,location=(a+b)/2);o=bpy.context.object;o.rotation_euler=d.to_track_quat('Z','Y').to_euler();o.data.materials.append(mats[mat]);parts.append(o);return o

def box(loc,scale,mat,rot=0):
 bpy.ops.mesh.primitive_cube_add(size=1,location=loc);o=bpy.context.object;o.scale=scale;o.rotation_euler.z=rot;bpy.ops.object.transform_apply(location=False,rotation=False,scale=True);o.data.materials.append(mats[mat]);parts.append(o);return o
box((1,7,-.09),(7,14,.14),'Earth')
# Broken paired stepping stones retain a coherent narrow escape line.
for j in range(20):
 for side in [-1,1]:
  x=1+side*.29+.09*math.sin(j*.8);y=2+j*.47
  box((x,y,-.005+rng.uniform(-.015,.015)),(.5+rng.uniform(-.08,.08),.39+rng.uniform(-.05,.05),.07),'WetStone',rng.uniform(-.12,.12))
# Fence posts lean towards the path, while rails give strong receding perspective.
for side in [-1,1]:
 x=1+side*1.15
 for j in range(8):
  y=2.35+j*.9;h=1.55+rng.uniform(-.18,.24);lean=-side*rng.uniform(.03,.14)
  beam((x,y,0),(x+lean,y+.04,h),.07,r2=.045)
  if j<7:
   for z in [.52,1.12]:beam((x,y,z),(x-side*.04,y+.9,z+rng.uniform(-.09,.09)),.035)
  for k in range(2):
   yy=y+.28+k*.3;hh=1.4+rng.uniform(-.2,.2)
   beam((x,yy,.12),(x-side*.07,yy+.04,hh),.023,'Iron',.014,5)
# Far narrow gate, deliberately closed and unreadable beyond the candle pool.
for side in [-1,1]:beam((1+side*.89,9.65,0),(1+side*.83,9.65,2.6),.1,r2=.055)
for z in [.4,1.65]:beam((.15,9.65,z),(1.85,9.65,z+.035),.033,'Iron')
for i in range(12):
 x=.2+i*.145;h=1.9+.32*math.sin(i/11*math.pi);beam((x,9.65,.1),(x,9.65,h),.016,'Iron',.008,5)
# Angular, individually designed branch chains frame the first frame at both sides.
for side,y in [(-1,3.1),(1,4.0),(-1,7.3),(1,8.3)]:
 x=1+side*1.65;p=[(x,y,0),(x-side*.2,y+.08,1.25),(x-side*.3,y-.08,2.35),(x-side*.65,y+.05,3.35),(x-side*.85,y+.2,3.9)]
 for j in range(4):beam(p[j],p[j+1],.15*(1-j*.2),r2=.12*(1-j*.24))
 for j in [1,2,3]:
  a=Vector(p[j]);b=a+Vector((-side*(.6 if j==2 else -.45),-.14,.45));c=b+Vector((-side*.4,.1,.43));beam(a,b,.055,r2=.029);beam(b,c,.029,r2=.007);beam(b,b+Vector((side*.18,.2,.42)),.023,r2=.004)
# No stolen/generated UVs: a new authored smart atlas for all original geometry.
bpy.ops.object.select_all(action='DESELECT')
for o in parts:o.select_set(True)
bpy.context.view_layer.objects.active=parts[0];bpy.ops.object.join();o=bpy.context.object;o.name='SM_Outro_Garden';bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
while o.data.uv_layers:o.data.uv_layers.remove(o.data.uv_layers[0])
o.data.uv_layers.new(name='ProductionUV');bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT');bpy.ops.uv.smart_project(angle_limit=math.radians(66),island_margin=.005);bpy.ops.object.mode_set(mode='OBJECT');o.data.calc_loop_triangles();assert len(o.data.loop_triangles)<=15000
uv=o.data.uv_layers.active;zero=0
for t in o.data.loop_triangles:
 a,b,c=[uv.data[i].uv for i in t.loops];zero+=abs((b-a).cross(c-a))<1e-10
assert not zero
bpy.ops.export_scene.fbx(filepath=str(O/(o.name+'.fbx')),use_selection=True,object_types={'MESH'},axis_forward='-Z',axis_up='Y',bake_anim=False)
config={'map':'/Game/Presentation/Cinematics/Maps/Lvl_OutroPresentation','meshes':[{'mesh':o.name,'triangles':len(o.data.loop_triangles),'materials':[m.name for m in o.data.materials]}],'materials':{m.name:{'color':list(m.diffuse_color[:3]),'roughness':.84,'metallic':m.node_tree.nodes.get('Principled BSDF').inputs['Metallic'].default_value,'texture':None,'two_sided':False} for m in mats.values()},'placements':[{'label':'Garden','mesh':o.name,'location_cm':[0,0,0],'scale':[1,1,1],'yaw':0,'tilt':0}],'uv':'Original ProductionUV smart atlas; zero-area triangles 0. Overlap and padding not measured.','collision':'None, cinematic decoration only','provenance':'Original source-authored geometry, no external assets or generation spend.'}
(O/'manifest.json').write_text(json.dumps(config,indent=2))
scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.device='CPU';scene.cycles.samples=24;scene.render.resolution_x=960;scene.render.resolution_y=540;scene.render.resolution_percentage=100;scene.world.color=(.025,.035,.055);scene.view_settings.view_transform='AgX'
def light(name,loc,power,color,size):
 d=bpy.data.lights.new(name,'AREA');d.energy=power;d.color=color;d.shape='DISK';d.size=size;o=bpy.data.objects.new(name,d);scene.collection.objects.link(o);o.location=loc;o.rotation_euler=(Vector((1,5,1))-o.location).to_track_quat('-Z','Y').to_euler()
light('Candle proxy',(1,2.0,1.1),48,(1,.48,.18),.55);light('Cold rim',(-1,6,4.5),190,(.2,.37,.7),5);light('Gate mist rim',(1,11,2.2),130,(.16,.28,.54),3)
# Real volume in preview, not a textured sky photo.
bpy.ops.mesh.primitive_cube_add(size=1,location=(1,7,3));fog=bpy.context.object;fog.name='PREVIEW_MIST';fog.scale=(12,18,10);m=bpy.data.materials.new('PreviewMist');m.use_nodes=True;n=m.node_tree.nodes;n.clear();v=n.new('ShaderNodeVolumePrincipled');v.inputs['Density'].default_value=.025;v.inputs['Color'].default_value=(.16,.23,.34,1);out=n.new('ShaderNodeOutputMaterial');m.node_tree.links.new(v.outputs['Volume'],out.inputs['Volume']);fog.data.materials.append(m)
bpy.ops.object.camera_add(location=(1,1.25,1.18));cam=bpy.context.object;cam.name='LastFlameCamera_Contract';cam.rotation_euler=(Vector((1,5,1.25))-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.type='PERSP';cam.data.lens=36/(2*math.tan(math.radians(65)/2));scene.camera=cam
bpy.ops.wm.save_as_mainfile(filepath=str(O/'HouseOutroV01.blend'))
for sec,dist in [(0,0),(7,.5)]:
 cam.location=(1,1.25+dist,1.18+dist*.07/3.75);scene.render.filepath=str(O/f'Review-{sec:02d}s.png');bpy.ops.render.render(write_still=True)
print('OUTRO_AUDIT',len(o.data.loop_triangles),zero)
