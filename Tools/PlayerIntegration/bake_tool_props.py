import bpy,json,math
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'Tallow/SourceAssets/Production/ToolProps';s=bpy.context.scene;s.render.engine='CYCLES';s.cycles.device='CPU';s.cycles.samples=4;s.render.threads_mode='FIXED';s.render.threads=4;s.render.bake.margin=8;s.render.bake.use_selected_to_active=False;s.render.bake.use_pass_direct=False;s.render.bake.use_pass_indirect=False;s.render.bake.use_pass_color=True
report={}
for c in [o for o in bpy.data.objects if o.type=='MESH']:
 bpy.ops.object.select_all(action='DESELECT');c.select_set(True);bpy.context.view_layer.objects.active=c;source=list(c.data.materials);images={};label=c.name.removeprefix('SM_')
 for name,kind in [('BaseColor','DIFFUSE'),('Roughness','ROUGHNESS'),('NormalGL','NORMAL'),('Metallic','EMIT')]:
  image=bpy.data.images.new('T_'+label+'_'+name,width=1024,height=1024,alpha=False);image.colorspace_settings.name='sRGB' if name=='BaseColor' else 'Non-Color';restore=[]
  for m in source:
   nodes=m.node_tree.nodes;node=nodes.new('ShaderNodeTexImage');node.image=image;nodes.active=node
   if kind=='EMIT':
    bs=nodes.get('Principled BSDF');out=nodes.get('Material Output');em=nodes.new('ShaderNodeEmission');v=bs.inputs['Metallic'].default_value;em.inputs['Color'].default_value=(v,v,v,1);m.node_tree.links.new(em.outputs[0],out.inputs['Surface']);restore.append((m,bs,out,em))
  bpy.ops.object.bake(type=kind)
  for m,bs,out,em in restore:m.node_tree.links.new(bs.outputs[0],out.inputs['Surface']);m.node_tree.nodes.remove(em)
  image.filepath_raw=str(OUT/(image.name+'.png'));image.file_format='PNG';image.save();images[name]=image
 mat=bpy.data.materials.new('M_'+label+'_Baked');mat.use_nodes=True;n=mat.node_tree.nodes;l=mat.node_tree.links;bs=n.get('Principled BSDF')
 for name,image in images.items():
  tex=n.new('ShaderNodeTexImage');tex.image=image
  if name=='NormalGL':
   normal=n.new('ShaderNodeNormalMap');l.new(tex.outputs['Color'],normal.inputs['Color']);l.new(normal.outputs['Normal'],bs.inputs['Normal'])
  else:l.new(tex.outputs['Color'],bs.inputs[{'BaseColor':'Base Color','Roughness':'Roughness','Metallic':'Metallic'}[name]])
 c.data.materials.clear();c.data.materials.append(mat)
 for face in c.data.polygons:face.material_index=0
 bpy.ops.export_scene.fbx(filepath=str(OUT/(c.name+'.fbx')),use_selection=True,object_types={'MESH'},axis_forward='X',axis_up='Z',apply_unit_scale=True,bake_anim=False,path_mode='STRIP',mesh_smooth_type='FACE')
 c.data.calc_loop_triangles();uv=c.data.uv_layers.active;zero=0
 for t in c.data.loop_triangles:
  a,b,d=[uv.data[i].uv for i in t.loops];zero+=abs((b-a).cross(d-a))<1e-12
 assert zero==0
 report[c.name]={'triangles':len(c.data.loop_triangles),'height_cm':c.dimensions.z*100,'zero_area_uv':zero,'texture_size':1024}
(OUT/'validation.json').write_text(json.dumps(report,indent=2));bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'ToolProps_Production.blend'));print('RESULT',json.dumps(report))
