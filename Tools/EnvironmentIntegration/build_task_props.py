import bpy,json,math
from pathlib import Path
from mathutils import Vector
R=Path(__file__).resolve().parents[2];O=R/'Tallow/SourceAssets/Production/HouseIntegrationV01';F=O/'TaskProps';F.mkdir(exist_ok=True);bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False);materials={};groups={}
for name,color,metal,rough in [('Enamel',(.045,.085,.075),.35,.5),('Brass',(.38,.23,.07),.8,.36),('Ceramic',(.59,.54,.40),.0,.34),('Dark',(.018,.017,.014),.3,.7),('Wood',(.12,.075,.04),0,.8)]:
 m=bpy.data.materials.new('M_HouseTask_'+name);m.diffuse_color=(*color,1);m.use_nodes=True;p=m.node_tree.nodes.get('Principled BSDF');p.inputs['Base Color'].default_value=(*color,1);p.inputs['Metallic'].default_value=metal;p.inputs['Roughness'].default_value=rough;materials[name]=m

def tag(o,g,mat):o.data.materials.append(materials[mat]);groups.setdefault(g,[]).append(o);return o
def box(g,loc,size,mat,bev=.003):
 bpy.ops.mesh.primitive_cube_add(size=1,location=loc);o=bpy.context.object;o.scale=size;bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
 if bev:m=o.modifiers.new('Rounded hand-finished edge','BEVEL');m.width=bev;m.segments=3;bpy.ops.object.modifier_apply(modifier=m.name)
 return tag(o,g,mat)
def cyl(g,loc,r,depth,mat):
 bpy.ops.mesh.primitive_cylinder_add(vertices=20,radius=r,depth=depth,location=loc,rotation=(math.pi/2,0,0));return tag(bpy.context.object,g,mat)
g='Switchboard';box(g,(0,-.035,0),(.49,.07,.62),'Wood',.009);box(g,(0,-.078,0),(.43,.045,.55),'Enamel',.009)
for x in [-.13,0,.13]:
 cyl(g,(x,-.109,.06),.049,.018,'Brass');cyl(g,(x,-.12,.06),.040,.013,'Ceramic');cyl(g,(x,-.130,.06),.024,.014,'Dark');box(g,(x,-.119,-.017),(.065,.008,.012),'Brass',.001)
for x in [-.185,.185]:
 for z in [-.235,.235]:cyl(g,(x,-.104,z),.009,.008,'Brass');box(g,(x,-.110,z),(.010,.003,.002),'Dark',0)
box(g,(0,-.11,-.17),(.11,.035,.13),'Brass');cyl(g,(0,-.146,-.16),.027,.03,'Dark');box(g,(0,-.17,-.12),(.025,.035,.13),'Brass');box(g,(0,-.178,-.056),(.08,.04,.035),'Dark',.009)
# Raised warning badge: geometry evokes original old switchboard without illegible generated writing.
box(g,(0,-.105,.205),(.19,.009,.055),'Brass');box(g,(0,-.111,.205),(.15,.005,.025),'Dark',.001)
g='EscapeLatch';box(g,(0,-.012,0),(.085,.024,.29),'Brass',.006)
for z in [-.115,.115]:cyl(g,(0,-.029,z),.007,.012,'Dark')
cyl(g,(0,-.041,0),.026,.055,'Brass');box(g,(.048,-.062,0),(.13,.026,.026),'Brass',.012);box(g,(.11,-.055,.024),(.026,.035,.06),'Brass',.009)
g='EscapeWindowFrame'
for x in [-.66,.66]:box(g,(x,0,.94),(.09,.16,1.78),'Wood',.01)
for z in [0,1.88]:box(g,(0,0,z),(1.40,.16,.10),'Wood',.012)
box(g,(0,.02,.94),(.045,.08,1.78),'Wood')
for x in [-.31875,.31875]:box(g,(x,.02,.98),(.5925,.08,.045),'Wood')
rows=[]
for g,obs in groups.items():
 bpy.ops.object.select_all(action='DESELECT')
 for o in obs:o.select_set(True)
 bpy.context.view_layer.objects.active=obs[0];bpy.ops.object.join();o=bpy.context.object;o.name='SM_HouseTask_'+g;bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
 for layer in list(o.data.uv_layers):o.data.uv_layers.remove(layer)
 uv=o.data.uv_layers.new(name='ProductionUV')
 for p in o.data.polygons:
  ax=max(range(3),key=lambda i:abs(p.normal[i]));a,b=([1,2] if ax==0 else [0,2] if ax==1 else [0,1])
  for li in p.loop_indices:v=o.data.vertices[o.data.loops[li].vertex_index].co;uv.data[li].uv=(v[a],v[b])
 bpy.ops.export_scene.fbx(filepath=str(F/(o.name+'.fbx')),use_selection=True,object_types={'MESH'},axis_forward='-Z',axis_up='Y',bake_anim=False);o.data.calc_loop_triangles();rows.append(dict(mesh=o.name,triangles=len(o.data.loop_triangles),materials=[m.name for m in o.data.materials]))
(F/'manifest.json').write_text(json.dumps(dict(meshes=rows,materials={m.name:dict(color=list(m.diffuse_color[:3]),metallic=m.node_tree.nodes.get('Principled BSDF').inputs['Metallic'].default_value,roughness=m.node_tree.nodes.get('Principled BSDF').inputs['Roughness'].default_value) for m in materials.values()},switchboard=dict(pivot='backplate center',dimensions_cm=[49,19.8,62],front_local_UE=[0,1,0],socket_centers_local_UE_cm=[[-13,13.7,6],[0,13.7,6],[13,13.7,6]],lever_center_local_UE_cm=[0,17,-12]),escape_latch=dict(pivot='backplate center',front_local_UE=[0,1,0]),status='Authored own UV; root assigns existing task components; visual gate pending'),indent=2));bpy.ops.wm.save_as_mainfile(filepath=str(F/'TaskProps.blend'));print('RESULT',rows)
