import bpy,json,hashlib
from pathlib import Path
from mathutils import Vector
R=Path(__file__).resolve().parents[2];O=R/'Tallow/SourceAssets/Production/HouseIntegrationV01';F=O/'Furniture';F.mkdir(exist_ok=True);rows=[]
for name in ['GothicTable','PaintedChair','Sofa']:
 bpy.ops.wm.open_mainfile(filepath=str(O/'BlenderKitRaw'/name/'source.blend'));objects=[o for o in bpy.context.scene.objects if o.type=='MESH'];bpy.ops.object.select_all(action='DESELECT')
 for o in objects:o.select_set(True)
 bpy.context.view_layer.objects.active=objects[0];bpy.ops.object.join();o=bpy.context.object;o.name='SM_House_'+name;bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
 for mod in list(o.modifiers):
  try:bpy.ops.object.modifier_apply(modifier=mod.name)
  except RuntimeError:o.modifiers.remove(mod)
 o.data.calc_loop_triangles();rawtris=len(o.data.loop_triangles)
 if rawtris>20000:
  m=o.modifiers.new('Game budget preserving source UV','DECIMATE');m.ratio=19000/rawtris;bpy.ops.object.modifier_apply(modifier=m.name)
 lo=Vector([min(v.co[i] for v in o.data.vertices) for i in range(3)]);hi=Vector([max(v.co[i] for v in o.data.vertices) for i in range(3)]);offset=Vector(((lo.x+hi.x)/2,(lo.y+hi.y)/2,lo.z))
 for v in o.data.vertices:v.co-=offset
 mats=[]
 for mat in o.data.materials:
  textures={}
  for n in mat.node_tree.nodes:
   if n.type!='TEX_IMAGE' or not n.image:continue
   im=n.image;role='BaseColor' if '_diff' in im.name else 'Roughness' if '_rough' in im.name else 'Normal' if '_nor' in im.name else 'Opacity' if 'opacity' in im.name else None
   if role:
    fn='T_House_'+name+'_'+role+'.png';im.filepath_raw=str(F/fn);im.file_format='PNG';im.save();im.pack();textures[role]=fn
  mats.append({'name':mat.name,'textures':textures})
 o.data.calc_loop_triangles();row={'mesh':o.name,'triangles':len(o.data.loop_triangles),'raw_triangles':rawtris,'dimensions_m':list(o.dimensions),'materials':mats,'uv':'Original licensed author UV retained; not AI-generated','source_provenance':'BlenderKitRaw/'+name+'/provenance.json'}
 bpy.ops.export_scene.fbx(filepath=str(F/(o.name+'.fbx')),use_selection=True,object_types={'MESH'},axis_forward='-Z',axis_up='Y',bake_anim=False,path_mode='COPY');bpy.ops.wm.save_as_mainfile(filepath=str(F/(name+'.blend')));rows.append(row)
(F/'manifest.json').write_text(json.dumps(rows,indent=2));print('RESULT '+json.dumps(rows))
