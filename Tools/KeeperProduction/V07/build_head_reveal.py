"""V07-only centre split through whole hood/mask; original V06 source/UV remain immutable."""
import bpy,math,json
from pathlib import Path
from mathutils import Vector,Matrix
ROOT=Path(__file__).resolve().parents[3];OUT=ROOT/'Tallow/SourceAssets/Keeper/RiggedV07'
bpy.ops.wm.open_mainfile(filepath=str(OUT/'keeper-v07-control-rig.blend'))
o=bpy.data.objects['SK_KeeperRiggedV07'];rig=bpy.data.objects['RIG_KeeperV07'];old=o.data
keys={k.name:[v.co.copy() for v in k.data] for k in old.shape_keys.key_blocks};coords=list(keys['Basis']);weights=[{g.group:g.weight for g in v.groups} for v in old.vertices];faces=[];uvs=[];matids=[];cutfaces=0;cx=-.04
# Clip only head triangles across the vertical central seam; interpolate atlas coordinates and weights.
def append_point(a,b,t):
 idx=len(coords);coords.append(coords[a].lerp(coords[b],t));weights.append({g:weights[a].get(g,0)*(1-t)+weights[b].get(g,0)*t for g in set(weights[a])|set(weights[b])})
 for name in keys:keys[name].append(keys[name][a].lerp(keys[name][b],t))
 return idx
for poly in old.polygons:
 data=[(i,old.uv_layers.active.data[li].uv.copy()) for i,li in zip(poly.vertices,poly.loop_indices)]
 crosses=min(coords[i].z for i,_ in data)>1.96 and min(coords[i].x for i,_ in data)<cx<max(coords[i].x for i,_ in data)
 if not crosses:
  faces.append([i for i,_ in data]);uvs.append([uv for _,uv in data]);matids.append(poly.material_index);continue
 cutfaces+=1
 for sign in [-1,1]:
  clipped=[]
  for j,(a,uva) in enumerate(data):
   b,uvb=data[(j+1)%len(data)];inside=(coords[a].x-cx)*sign>=0;nextinside=(coords[b].x-cx)*sign>=0
   if inside:clipped.append((a,uva))
   if inside!=nextinside:
    t=(cx-coords[a].x)/(coords[b].x-coords[a].x);idx=append_point(a,b,t);clipped.append((idx,uva.lerp(uvb,t)))
  for k in range(1,len(clipped)-1):
   tri=[clipped[0],clipped[k],clipped[k+1]];faces.append([i for i,_ in tri]);uvs.append([uv for _,uv in tri]);matids.append(poly.material_index)
new=bpy.data.meshes.new('KeeperV07_SeamedSurface');new.from_pydata(coords,[],faces);new.update()
for mat in old.materials:new.materials.append(mat)
uv=new.uv_layers.new(name='ProductionUV')
for poly,faceuv,mi in zip(new.polygons,uvs,matids):
 poly.material_index=mi;poly.use_smooth=True
 for li,v in zip(poly.loop_indices,faceuv):uv.data[li].uv=v
# IDs of added seam vertices retain side from their adjacent clipped polygon.
seamside={}
for face in faces:
 sign=1 if sum(coords[i].x-cx for i in face)>=0 else -1
 for i in face:
  if i>=len(old.vertices):seamside[i]=sign
oldgroups=[g.name for g in o.vertex_groups];o.data=new
for group in list(o.vertex_groups):o.vertex_groups.remove(group)
for name in oldgroups:o.vertex_groups.new(name=name)
for i,ws in enumerate(weights):
 largest=sorted(ws.items(),key=lambda x:x[1],reverse=True)[:4];total=sum(w for _,w in largest)
 for g,w in largest:
  if w>0:o.vertex_groups[g].add([i],w/total,'REPLACE')
for name,positions in keys.items():
 key=o.shape_key_add(name=name);key.value=0
 for vertex,co in zip(key.data,positions):vertex.co=co
# Extra hem clearance for actual tilted crawl, tapered into unchanged upper torso.
for i,co in enumerate(coords):
 if co.z<1.7 and weights[i].get(oldgroups.index('DEF_body'),0)>.5:
  o.data.shape_keys.key_blocks['FULL_HORROR'].data[i].co.z+=.12*(1-co.z/1.7)
openkey=o.shape_key_add(name='HOOD_OPEN');openkey.value=0
for i,co in enumerate(coords):
 weight=max(0,min(1,(co.z-1.96)/.20));weight=weight*weight*(3-2*weight)
 if weight:
  sign=seamside.get(i,1 if co.x>=cx else -1);pivot=Vector((cx,0,1.96));v=co-pivot;v=Matrix.Rotation(math.radians(sign*22*weight),3,'Y')@v;v+=pivot;v.x+=sign*.025*weight;openkey.data[i].co=v
# Original designed inner maw: narrow recessed plum cavity, asymmetrical ivory peg teeth.
def material(name,color,roughness):
 m=bpy.data.materials.new(name);m.diffuse_color=(*color,1);m.use_nodes=True;p=m.node_tree.nodes.get('Principled BSDF');p.inputs['Base Color'].default_value=(*color,1);p.inputs['Roughness'].default_value=roughness;return m
mouthmat=material('M_KeeperV07_Mouth',(.007,.001,.002),.88);teethmat=material('M_KeeperV07_Teeth',(.30,.23,.13),.85)
parts=[];centre=Vector((cx,-.04,2.275));verts=[];faces=[];rings=[(1.,-.11),(.91,-.122),(.7,-.075),(.38,-.035),(.04,-.005)]
for radius,y in rings:
 for i in range(24):
  angle=math.tau*i/24;verts.append((cx+(.10+.002*math.sin(i*2.7))*radius*math.cos(angle),y+.0025*math.sin(i*2.1)*radius,2.275+(.135+.002*math.cos(i*3.1))*radius*math.sin(angle)))
for ring in range(len(rings)-1):
 for i in range(24):faces.append((ring*24+i,ring*24+(i+1)%24,(ring+1)*24+(i+1)%24,(ring+1)*24+i))
faces.append(tuple(range(96,120)));me=bpy.data.meshes.new('RecessedMouthBowl');me.from_pydata(verts,[],faces);me.update();cavity=bpy.data.objects.new('Recessed_Maw',me);bpy.context.scene.collection.objects.link(cavity);cavity.data.materials.append(mouthmat);parts.append(cavity)
uv=me.uv_layers.new(name='ProductionUV')
for poly in me.polygons:
 for li in poly.loop_indices:
  co=me.vertices[me.loops[li].vertex_index].co;uv.data[li].uv=((co.x-cx)/.22+.5,(co.z-2.275)/.29+.5)
for row in [-1,1]:
 for j in range(9):
  i=j+(9 if row>0 else 0);x=(j-4)*.018;z=row*.101*math.sqrt(max(.1,1-(x/.10)**2));pos=Vector((cx+x,-.112,2.275+z));direction=Vector((-.10*x,.10,-row));direction.normalize();length=.024+.014*(.5+.5*math.sin(i*3.7))
  bpy.ops.mesh.primitive_cone_add(vertices=7,radius1=.005+.0015*math.sin(i),radius2=.0007,depth=length,location=pos+direction*length*.5)
  tooth=bpy.context.object;tooth.name='Maw_Peg_'+str(i);tooth.rotation_euler=direction.to_track_quat('Z','Y').to_euler();tooth.data.materials.append(teethmat);parts.append(tooth)

for part in parts:
 bpy.ops.object.select_all(action='DESELECT');part.select_set(True);bpy.context.view_layer.objects.active=part;bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
 vg=part.vertex_groups.new(name='head');vg.add(list(range(len(part.data.vertices))),1,'REPLACE')
 if part.data.uv_layers:part.data.uv_layers.active.name='ProductionUV'
 for poly in part.data.polygons:poly.use_smooth=True
bpy.ops.object.select_all(action='DESELECT');o.select_set(True)
for part in parts:part.select_set(True)
mouth_start=len(o.data.vertices)
bpy.context.view_layer.objects.active=o;bpy.ops.object.join()
# Recess the closed maw within the original mask; opening unfolds its own interior.
for i in range(mouth_start,len(o.data.vertices)):
 full=o.data.shape_keys.key_blocks['Basis'].data[i].co.copy();closed=Vector((cx,.015,2.275))+(full-Vector((cx,.015,2.275)))*.015
 for key in o.data.shape_keys.key_blocks:key.data[i].co=full if key.name=='HOOD_OPEN' else closed
 o.data.vertices[i].co=closed
# Reattach drivers after topology rebuild; facial key is direct animator control.
for prop,key,owner in [('mantle_gather','FULL_HORROR','CTRL_body'),('hood_open','HOOD_OPEN','CTRL_head')]:
 p=rig.pose.bones[owner];p[prop]=0.;p.id_properties_ui(prop).update(min=0,max=1,description='Whole hood/top and mask centre opening' if key=='HOOD_OPEN' else 'Gather lower mantle')
 fc=o.data.shape_keys.key_blocks[key].driver_add('value');drv=fc.driver;drv.expression='value';v=drv.variables.new();v.name='value';v.targets[0].id=rig;v.targets[0].data_path=p.path_from_id()+f'["{prop}"]'
o.data.calc_loop_triangles();result={'head_faces_split':cutfaces,'triangles':len(o.data.loop_triangles),'vertices':len(o.data.vertices),'shape_keys':[k.name for k in o.data.shape_keys.key_blocks],'source_unchanged':True,'new_geometry':'interpolated centre seam, recessed concave recessed mouth and18 irregular upper/lower teeth; no bone arms'}
(OUT/'head-construction-audit.json').write_text(json.dumps(result,indent=2));bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'keeper-v07-face-rig.blend'));print('RESULT '+json.dumps(result))
