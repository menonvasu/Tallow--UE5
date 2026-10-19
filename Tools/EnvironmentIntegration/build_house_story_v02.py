"""Small original own-UV story kit. No game-map access; no borrowed geometry."""
import bpy,json,math,random
from pathlib import Path
from mathutils import Vector
R=Path(__file__).resolve().parents[2];O=R/'Tallow/SourceAssets/Production/HouseStoryV02';O.mkdir(exist_ok=True);bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False);print('BLENDER_VERSION',bpy.app.version_string);rng=random.Random(252);groups={};materials={}
def mat(name,color,rough=.8,metal=0,image=None):
 m=bpy.data.materials.new('M_Story_'+name);m.use_nodes=True;p=m.node_tree.nodes.get('Principled BSDF');p.inputs['Base Color'].default_value=(*color,1);p.inputs['Roughness'].default_value=rough;p.inputs['Metallic'].default_value=metal
 if image:
  t=m.node_tree.nodes.new('ShaderNodeTexImage');t.image=bpy.data.images.load(str(O/image));t.image.pack();m.node_tree.links.new(t.outputs['Color'],p.inputs['Base Color'])
 materials[name]=m;return m
mat('DarkOak',(.065,.042,.026));mat('WornGold',(.24,.15,.045),.6,.45);mat('Canvas',(.4,.3,.15),.88,image='T_Story_AbsentFamily.png');mat('Backing',(.09,.085,.065));mat('Ribbon',(.23,.12,.08));mat('Runner',(.3,.12,.08),.95,image='T_Story_Runner.png')
def mesh(group,v,f,material,uvs=None):
 me=bpy.data.meshes.new(group);me.from_pydata(v,[],f);me.validate();me.update();o=bpy.data.objects.new(group,me);bpy.context.scene.collection.objects.link(o);me.materials.append(materials[material]);uv=me.uv_layers.new(name='ProductionUV')
 for p in me.polygons:
  ax=max(range(3),key=lambda i:abs(p.normal[i]));a,b=([1,2] if ax==0 else [0,2] if ax==1 else [0,1])
  for li in p.loop_indices:
   i=me.loops[li].vertex_index;co=me.vertices[i].co;uv.data[li].uv=uvs[i] if uvs else (co[a],co[b])
 groups.setdefault(group,[]).append(o);return o
def box(g,lo,hi,material,bevel=.003):
 o=mesh(g,[(x,y,z) for z in [lo[2],hi[2]] for y in [lo[1],hi[1]] for x in [lo[0],hi[0]]],[(0,2,3,1),(4,5,7,6),(0,1,5,4),(2,6,7,3),(0,4,6,2),(1,3,7,5)],material)
 if bevel:bpy.context.view_layer.objects.active=o;m=o.modifiers.new('Soft carved edge','BEVEL');m.width=bevel;m.segments=2;bpy.ops.object.modifier_apply(modifier=m.name)
 return o
def frame(g,w,h,cx=0,cz=0):
 # Butt joints avoid coplanar doubled corners. Slight asymmetric inner rails retain handmade character.
 for x in [-w/2-.035,w/2+.035]:box(g,(cx+x-.035,-.05,cz-h/2),(cx+x+.035,.015,cz+h/2),'DarkOak',.007)
 for z in [-h/2-.035,h/2+.035]:box(g,(cx-w/2-.07,-.05,cz+z-.035),(cx+w/2+.07,.015,cz+z+.035),'DarkOak',.007)
 for x in [-w/2,w/2]:box(g,(cx+x-.007,-.059,cz-h/2),(cx+x+.007,-.048,cz+h/2),'WornGold',.003)
 for z in [-h/2,h/2]:box(g,(cx-w/2,-.059,cz+z-.007),(cx+w/2,-.048,cz+z+.007),'WornGold',.003)
frame('FamilyPortrait',.75,1.0)
mesh('FamilyPortrait',[(-.375,-.025,-.5),(.375,-.025,-.5),(.375,-.025,.5),(-.375,-.025,.5)],[(0,1,2,3)],'Canvas',[(0,0),(1,0),(1,1),(0,1)])
# Three empty frames deliberately contain dark old backing, not another repeated portrait.
for cx,cz,w,h in [(-.38,.15,.28,.39),(.12,.05,.34,.49),(.61,-.17,.20,.28)]:
 frame('EmptyFrameCluster',w,h,cx,cz);box('EmptyFrameCluster',(cx-w/2,-.024,cz-h/2),(cx+w/2,-.018,cz+h/2),'Backing',0)
# A frayed runner: true shallow cloth geometry, own full-sheet UVs and ragged edges.
v=[];uv=[];f=[];nx,ny=6,32
for j in range(ny+1):
 for i in range(nx+1):
  u=i/nx;t=j/ny;x=(u-.5)*.9;y=(t-.5)*2.8
  if i in [0,nx]:x+=rng.uniform(-.012,.012)
  v.append((x,y,.004+.002*math.sin(t*16)*math.sin(u*math.pi)));uv.append((u,t))
for j in range(ny):
 for i in range(nx):
  if (i==0 and j in [3,4,21]) or (i==nx-1 and j in [12,26]):continue
  k=j*(nx+1)+i;f.append((k,k+1,k+nx+2,k+nx+1))
mesh('TatteredRunner',v,f,'Runner',uv)
for side in [-1,1]:
 for i in range(29):
  x=-.42+i*.03;y=side*1.4;length=rng.uniform(.025,.07);mesh('TatteredRunner',[(x,y,.004),(x+.005,y,.004),(x+.008,y+side*length,.002),(x+.002,y+side*length,.002)],[(0,1,2,3)],'Ribbon')
# Empty coat pegs, with one small abandoned ribbon; absence repeated without more dolls.
box('EmptyPegRail',(-.50,-.02,-.045),(.5,.03,.045),'DarkOak',.008)
for x in [-.36,-.12,.12,.36]:
 box('EmptyPegRail',(x-.012,-.11,-.008),(x+.012,-.018,.018),'WornGold',.006)
v=[];f=[]
for j in range(17):
 t=j/16
 for side in [-1,1]:v.append((.12+side*.015+.012*math.sin(t*9),-.09-.03*math.sin(t*5),-.015-t*.42))
for j in range(16):k=j*2;f.append((k,k+1,k+3,k+2))
mesh('EmptyPegRail',v,f,'Ribbon')
rows=[]
for group,obs in groups.items():
 bpy.ops.object.select_all(action='DESELECT')
 for o in obs:o.select_set(True)
 bpy.context.view_layer.objects.active=obs[0];bpy.ops.object.join();o=bpy.context.object;o.name='SM_Story_'+group;bpy.ops.object.transform_apply(location=True,rotation=True,scale=True);o.data.calc_loop_triangles();assert o.data.uv_layers.get('ProductionUV')
 uv=o.data.uv_layers['ProductionUV']
 for poly in o.data.polygons:
  if o.data.materials[poly.material_index].name in ['M_Story_Canvas','M_Story_Runner']:continue
  ax=max(range(3),key=lambda i:abs(poly.normal[i]));a,b=([1,2] if ax==0 else [0,2] if ax==1 else [0,1])
  for li in poly.loop_indices:
   co=o.data.vertices[o.data.loops[li].vertex_index].co;uv.data[li].uv=(co[a],co[b])
 row=dict(mesh=o.name,triangles=len(o.data.loop_triangles),materials=[m.name for m in o.data.materials],dimensions_m=list(o.dimensions),collision='none');rows.append(row);bpy.ops.export_scene.fbx(filepath=str(O/(o.name+'.fbx')),use_selection=True,object_types={'MESH'},axis_forward='-Z',axis_up='Y',bake_anim=False)
assert sum(r['triangles'] for r in rows)<=10000
cfg=dict(meshes=rows,materials={m.name:dict(color=list(m.node_tree.nodes.get('Principled BSDF').inputs['Base Color'].default_value[:3]),roughness=m.node_tree.nodes.get('Principled BSDF').inputs['Roughness'].default_value,metallic=m.node_tree.nodes.get('Principled BSDF').inputs['Metallic'].default_value,texture='T_Story_AbsentFamily.png' if key=='Canvas' else 'T_Story_Runner.png' if key=='Runner' else None,two_sided=key in ['Canvas','Runner','Ribbon']) for key,m in materials.items()},units='metres in Blender, FBX imported as centimetres',uv='New ProductionUV; image panel0–1 full sheet, runner0–1 sheet, frame/peg face projection for constant finishes',collision='All decoration NoCollision, does not replace structural geometry')
(O/'manifest.json').write_text(json.dumps(cfg,indent=2));bpy.ops.wm.save_as_mainfile(filepath=str(O/'HouseStoryV02.blend'));print('RESULT',json.dumps(rows))
