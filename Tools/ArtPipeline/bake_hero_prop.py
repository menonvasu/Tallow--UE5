"""CPU selected-to-active PBR/normal bake from preserved detailed source onto independent UVs."""
import argparse,bpy,json,sys,hashlib
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--prop',choices=['RestraintChair','CollectionDoll'],required=True);a=p.parse_args(sys.argv[sys.argv.index('--')+1:]);ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'Tallow/SourceAssets/Production'/a.prop
bpy.ops.wm.open_mainfile(filepath=str(OUT/'Prepared_HighAndProduction.blend'));high=bpy.data.objects['HIGH_'+a.prop];low=bpy.data.objects['SM_'+a.prop];high.hide_set(False);high.hide_render=False;low.hide_set(False);low.hide_render=False
scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.device='CPU';scene.cycles.samples=8;scene.render.threads_mode='FIXED';scene.render.threads=2
scene.render.bake.use_selected_to_active=True;scene.render.bake.use_clear=True;scene.render.bake.margin=8;scene.render.bake.cage_extrusion=.012 if a.prop=='RestraintChair' else .006;scene.render.bake.max_ray_distance=.06 if a.prop=='RestraintChair' else .025
scene.render.bake.use_pass_direct=False;scene.render.bake.use_pass_indirect=False;scene.render.bake.use_pass_color=True
mat=bpy.data.materials.new('M_'+a.prop+'_ProductionPBR');mat.use_nodes=True;low.data.materials.clear();low.data.materials.append(mat);n=mat.node_tree.nodes;l=mat.node_tree.links;bs=n.get('Principled BSDF');baked={}
for channel,baketype in [('BaseColor','DIFFUSE'),('Roughness','ROUGHNESS'),('Normal','NORMAL')]:
 im=bpy.data.images.new('T_'+a.prop+'_'+channel,4096,4096,alpha=False);im.colorspace_settings.name='sRGB' if channel=='BaseColor' else 'Non-Color';tex=n.new('ShaderNodeTexImage');tex.image=im;tex.label=channel;n.active=tex
 bpy.ops.object.select_all(action='DESELECT');high.select_set(True);low.select_set(True);bpy.context.view_layer.objects.active=low;bpy.ops.object.bake(type=baketype)
 im.filepath_raw=str(OUT/(im.name+'.png'));im.file_format='PNG';im.save();im.pack();baked[channel]=tex
l.new(baked['BaseColor'].outputs['Color'],bs.inputs['Base Color']);l.new(baked['Roughness'].outputs['Color'],bs.inputs['Roughness']);normal=n.new('ShaderNodeNormalMap');normal.inputs['Strength'].default_value=1;l.new(baked['Normal'].outputs['Color'],normal.inputs['Color']);l.new(normal.outputs['Normal'],bs.inputs['Normal'])
high.hide_set(True);high.hide_render=True;bpy.ops.object.select_all(action='DESELECT');low.select_set(True);bpy.context.view_layer.objects.active=low
bpy.ops.export_scene.gltf(filepath=str(OUT/('SM_'+a.prop+'.glb')),export_format='GLB',use_selection=True)
bpy.ops.export_scene.fbx(filepath=str(OUT/('SM_'+a.prop+'.fbx')),use_selection=True,object_types={'MESH'},axis_forward='-Z',axis_up='Y',bake_anim=False,path_mode='COPY')
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/(a.prop+'_Production.blend')))
report={'method':'Cycles CPU selected-to-active, colour-only diffuse + roughness + tangent normal','threads':2,'resolution':[4096,4096],'source':'preserved detailed Tripo HIGH mesh and original PBR material/UVs','destination_uv':'ProductionUV independently authored','raw_unchanged':True,'maps':{c:'T_'+a.prop+'_'+c+'.png' for c in baked},'normal_convention':'OpenGL +Y; Unreal importer should flip green','export_sha256':hashlib.sha256((OUT/('SM_'+a.prop+'.glb')).read_bytes()).hexdigest()};(OUT/'bake-report.json').write_text(json.dumps(report,indent=2));print('RESULT '+json.dumps(report))
