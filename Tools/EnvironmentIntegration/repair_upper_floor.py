"""Explicit cell slabs replace unreliable boolean on imported disconnected floor."""
import bpy,json
from pathlib import Path
R=Path(__file__).resolve().parents[2];O=R/'Tallow/SourceAssets/Production/HouseIntegrationV01';bpy.ops.wm.open_mainfile(filepath=str(O/'HouseArchitecture.blend'));old=bpy.data.objects['SM_HouseV01_Upper_Floor'];mat=old.data.materials[0];name=old.name;label=old['original_actor_label'];bpy.data.objects.remove(old,do_unlink=True)
xs=[0,5.94,9.66,14.005,15.505,15.6];ys=[0,4.50,6.9,8.8,12.32,13];verts=[];faces=[];covered=0
for x0,x1 in zip(xs,xs[1:]):
 for y0,y1 in zip(ys,ys[1:]):
  x=(x0+x1)/2;y=(y0+y1)/2
  if (5.94<x<9.66 and 4.5<y<8.8) or (14.005<x<15.505 and 6.9<y<12.32):continue
  k=len(verts);verts.extend([(a,b,z) for z in [3.6,3.9] for b in [y0,y1] for a in [x0,x1]]);faces.extend([tuple(k+i for i in f) for f in [(0,2,3,1),(4,5,7,6),(0,1,5,4),(2,6,7,3),(0,4,6,2),(1,3,7,5)]]);covered+=(x1-x0)*(y1-y0)
me=bpy.data.meshes.new(name);me.from_pydata(verts,[],faces);me.update();o=bpy.data.objects.new(name,me);bpy.context.scene.collection.objects.link(o);o['original_actor_label']=label;me.materials.append(mat);uv=me.uv_layers.new(name='ProductionUV')
for f in me.polygons:
 axis=max(range(3),key=lambda i:abs(f.normal[i]));a,b=([1,2] if axis==0 else [0,2] if axis==1 else [0,1])
 for li in f.loop_indices:v=me.vertices[me.loops[li].vertex_index].co;uv.data[li].uv=(v[a]*.65,v[b]*.65)
bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o;bpy.ops.export_scene.fbx(filepath=str(O/'Architecture'/(name+'.fbx')),use_selection=True,object_types={'MESH'},axis_forward='-Z',axis_up='Y',bake_anim=False);bpy.ops.wm.save_as_mainfile(filepath=str(O/'HouseArchitecture.blend'));print('RESULT upper floor area',covered,'triangles',len(faces)*2)
p=O/'Architecture/manifest.json';rows=json.loads(p.read_text())
for r in rows:
 if r['mesh']==name:r['triangles']=len(faces)*2;r['geometry']='Explicit measured slab cells around main atrium and back stairs; collection floor restored'
p.write_text(json.dumps(rows,indent=2))
