import bpy,json,math
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'Tallow/SourceAssets/Production/CandleV02';P=ROOT/'Tallow/SourceAssets/Player/InteractionV05';s=bpy.context.scene;c=bpy.data.objects['SM_CandleV02_Crooked'];s.render.engine='CYCLES';s.cycles.device='CPU';s.cycles.samples=8;s.render.threads_mode='FIXED';s.render.threads=4
bpy.ops.object.select_all(action='DESELECT');c.select_set(True);bpy.context.view_layer.objects.active=c
s.render.bake.margin=8;s.render.bake.use_selected_to_active=False;s.render.bake.use_pass_direct=False;s.render.bake.use_pass_indirect=False;s.render.bake.use_pass_color=True
source=list(c.data.materials)
# Handworked variation survives the texture bake, not a runtime procedural dependency.
for i,m in enumerate(source[:2]):
 n=m.node_tree.nodes;links=m.node_tree.links;bs=n.get('Principled BSDF');noise=next(x for x in n if x.type=='TEX_NOISE');noise.inputs['Scale'].default_value=95 if i==0 else 60
 ramp=n.new('ShaderNodeValToRGB');ramp.color_ramp.elements[0].position=.20;ramp.color_ramp.elements[1].position=.80
 ramp.color_ramp.elements[0].color=(.075,.095,.06,1) if i==0 else (.40,.25,.075,1)
 ramp.color_ramp.elements[1].color=(.30,.17,.048,1) if i==0 else (.72,.55,.28,1)
 links.new(noise.outputs['Fac'],ramp.inputs['Fac']);links.new(ramp.outputs['Color'],bs.inputs['Base Color'])
images={}
for name,kind in [('BaseColor','DIFFUSE'),('Roughness','ROUGHNESS'),('NormalGL','NORMAL'),('Metallic','EMIT')]:
 image=bpy.data.images.new('T_CandleV02_'+name,width=2048,height=2048,alpha=False);image.colorspace_settings.name='sRGB' if name=='BaseColor' else 'Non-Color';restore=[]
 for m in source:
  nodes=m.node_tree.nodes;node=nodes.new('ShaderNodeTexImage');node.image=image;nodes.active=node
  if kind=='EMIT':
   bs=nodes.get('Principled BSDF');out=nodes.get('Material Output');em=nodes.new('ShaderNodeEmission');v=bs.inputs['Metallic'].default_value;em.inputs['Color'].default_value=(v,v,v,1);m.node_tree.links.new(em.outputs[0],out.inputs['Surface']);restore.append((m,bs,out,em))
 bpy.ops.object.bake(type=kind)
 for m,bs,out,em in restore:m.node_tree.links.new(bs.outputs[0],out.inputs['Surface']);m.node_tree.nodes.remove(em)
 image.filepath_raw=str(OUT/('T_CandleV02_'+name+'.png'));image.file_format='PNG';image.save();images[name]=image
mat=bpy.data.materials.new('M_CandleV02_Baked');mat.use_nodes=True;n=mat.node_tree.nodes;l=mat.node_tree.links;bs=n.get('Principled BSDF')
for name,image in images.items():
 t=n.new('ShaderNodeTexImage');t.image=image;t.label=name
 if name=='NormalGL':
  nm=n.new('ShaderNodeNormalMap');l.new(t.outputs['Color'],nm.inputs['Color']);l.new(nm.outputs['Normal'],bs.inputs['Normal'])
 else:l.new(t.outputs['Color'],bs.inputs[{'BaseColor':'Base Color','Roughness':'Roughness','Metallic':'Metallic'}[name]])
c.data.materials.clear();c.data.materials.append(mat)
for f in c.data.polygons:f.material_index=0
# Static export uses mesh local origin at base, without held world placement.
original=c.location.copy();c.location=(0,0,0);bpy.context.view_layer.update();bpy.ops.export_scene.fbx(filepath=str(OUT/'SM_CandleV02_Crooked.fbx'),use_selection=True,object_types={'MESH'},axis_forward='X',axis_up='Z',apply_unit_scale=True,bake_anim=False,path_mode='STRIP',mesh_smooth_type='FACE');c.location=original
uv=c.data.uv_layers.active;areas=[]
c.data.calc_loop_triangles()
for tri in c.data.loop_triangles:
 a,b,d=[uv.data[i].uv for i in tri.loops];areas.append(abs((b-a).cross(d-a))*.5)
report={'triangles':len(c.data.loop_triangles),'zero_area_uv_triangles':sum(a<1e-12 for a in areas),'uv_outside':sum(not(0<=x<=1) for loop in uv.data for x in loop.uv),'wick_local_UE_cm':[.4,0,25.5],'maps':list(images),'size':2048,'hand_stem_diameter_cm':2.1}
(OUT/'validation.json').write_text(json.dumps(report,indent=2));assert report['zero_area_uv_triangles']==0 and report['uv_outside']==0
bpy.ops.wm.save_as_mainfile(filepath=str(P/'05_interaction-textured.blend'))
# A clean editable candle-only library.
bpy.data.libraries.write(str(OUT/'CandleV02_Production.blend'),{c},fake_user=True)
print('RESULT',json.dumps(report))
