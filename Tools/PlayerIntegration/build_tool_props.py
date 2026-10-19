import bpy,math,json
from pathlib import Path
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'Tallow/SourceAssets/Production/ToolProps';bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False);s=bpy.context.scene
materials=[]
for name,color,metal,rough in [('Iron',(.055,.068,.068,1),.75,.48),('Grip',(.105,.052,.027,1),0,.82),('Ceramic',(.58,.49,.31,1),0,.43),('Copper',(.22,.09,.035,1),.8,.38),('Glass',(.018,.055,.046,1),.25,.2)]:
 m=bpy.data.materials.new('M_Tool_'+name);m.use_nodes=True;bs=m.node_tree.nodes.get('Principled BSDF');bs.inputs['Base Color'].default_value=color;bs.inputs['Metallic'].default_value=metal;bs.inputs['Roughness'].default_value=rough
 noise=m.node_tree.nodes.new('ShaderNodeTexNoise');noise.inputs['Scale'].default_value=120;noise.inputs['Detail'].default_value=3
 bump=m.node_tree.nodes.new('ShaderNodeBump');bump.inputs['Strength'].default_value=.18;bump.inputs['Distance'].default_value=.0003;m.node_tree.links.new(noise.outputs['Fac'],bump.inputs['Height']);m.node_tree.links.new(bump.outputs['Normal'],bs.inputs['Normal']);materials.append(m)
def mesh(name,verts,faces,indices):
 me=bpy.data.meshes.new(name);me.from_pydata(verts,[],faces);me.update();o=bpy.data.objects.new(name,me);s.collection.objects.link(o)
 for m in materials:me.materials.append(m)
 for f,i in zip(me.polygons,indices):f.material_index=i;f.use_smooth=True
 bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o;bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT');bpy.ops.uv.smart_project(angle_limit=math.radians(60),island_margin=.018);bpy.ops.object.mode_set(mode='OBJECT');me.uv_layers.active.name='UV_ToolOwn';return o
v=[];f=[];mi=[]
def tube(points,radii,mat,n=12):
 start=len(v)
 for j,point in enumerate(points):
  p=Vector(point);t=Vector(points[min(j+1,len(points)-1)])-Vector(points[max(0,j-1)]);t.normalize();normal=Vector((0,1,0));side=t.cross(normal).normalized()
  for k in range(n):
   a=math.tau*k/n;r=radii[j];v.append(tuple(p+normal*math.cos(a)*r+side*math.sin(a)*r))
 for j in range(len(points)-1):
  for k in range(n):f.append((start+j*n+k,start+j*n+(k+1)%n,start+(j+1)*n+(k+1)%n,start+(j+1)*n+k));mi.append(mat)
 f.append(tuple(start+k for k in reversed(range(n))));mi.append(mat);f.append(tuple(start+(len(points)-1)*n+k for k in range(n)));mi.append(mat)
# Bent pry iron: squared low fork, long spine and swept gooseneck.
points=[(-.012,0,0),(-.008,0,.012),(0,0,.03),(0,0,.10),(0,0,.18),(0,0,.28),(.006,0,.33),(.018,0,.37),(.036,0,.395),(.057,0,.405),(.074,0,.396),(.081,0,.378),(.077,0,.365)]
tube(points,[.008,.007,.006,.006,.006,.006,.006,.006,.006,.006,.006,.005,.003],0)
tube([(0,0,z) for z in [.096,.1,.106,.116,.126,.136,.146,.156,.166,.17,.174]],[.008,.0105,.0105,.0108,.0105,.0108,.0105,.0108,.0105,.0105,.008],1,n=24)
crowbar=mesh('SM_ToolV02_Crowbar',v,f,mi)
v=[];f=[];mi=[]
# Replaceable ceramic fuse: recessed glazed inspection strip and copper contact collars.
tube([(0,0,z) for z in [0,.003,.014,.018,.102,.106,.117,.12]],[.011,.0125,.0125,.0105,.0105,.0125,.0125,.011],2,n=32)
for z in [.002,.106]:tube([(0,0,z),(0,0,z+.012)],[.0128,.0128],3,n=32)
# Curved dark glass inset replaces front ceramic faces; it is visible from the game camera.
kept=[(face,idx) for face,idx in zip(f,mi) if not(idx==2 and .018<sum(v[k][2] for k in face)/len(face)<.102 and sum(v[k][1] for k in face)/len(face)>.008)]
f=[x[0] for x in kept];mi=[x[1] for x in kept];start=len(v)
for z in [.018,.102]:
 for k in range(9):
  a=math.radians(50+10*k);v.append((.0105*math.cos(a),.0105*math.sin(a),z))
for k in range(8):f.append((start+k,start+k+1,start+10+k,start+9+k));mi.append(4)
fuse=mesh('SM_ToolV02_Fuse',v,f,mi)
v=[];f=[];mi=[]
tube([(0,0,0),(0,0,.036)],[.0012,.0012],2,n=4)
tube([(0,0,.034),(0,0,.036),(0,0,.039),(0,0,.041)],[.0014,.002,.002,.0006],1,n=12)
match=mesh('SM_ToolV02_Match',v,f,mi)
# Stored at origin separately for export. Review scene lays them out later.
for ob in [crowbar,fuse,match]:
 ob['grip_axis']='local +Z';ob['grip_radius_cm']=1.05
s.render.engine='CYCLES';s.cycles.device='CPU';s.cycles.samples=8;s.render.threads_mode='FIXED';s.render.threads=4
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'ToolProps_Production.blend'))
print('RESULT',json.dumps({o.name:{'triangles':sum(len(f.vertices)-2 for f in o.data.polygons),'height_m':o.dimensions.z} for o in [crowbar,fuse,match]}))
