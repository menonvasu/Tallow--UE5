"""Rebake ORIGINAL generated PBR from TripoSourceUV to ProductionUV; no invented replacement materials.
Usage: blender -b --factory-startup --python-exit-code 1 -P rebake_tripo_pbr.py -- --dir /abs/DirectV04
"""
import argparse,bpy,json,sys
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--dir',required=True);a=p.parse_args(sys.argv[sys.argv.index('--')+1:]);out=Path(a.dir);tex=out/'Textures';tex.mkdir(exist_ok=True)
bpy.ops.wm.open_mainfile(filepath=str(out/'keeper-tripo-clean-uv.blend'))
obj=bpy.data.objects['SK_Keeper_Tripo_Direct'];scene=bpy.context.scene
bpy.ops.object.select_all(action='DESELECT');obj.select_set(True);bpy.context.view_layer.objects.active=obj
scene.render.engine='CYCLES';scene.cycles.device='CPU';scene.cycles.samples=4;scene.render.threads_mode='FIXED';scene.render.threads=4
scene.render.bake.margin=12;scene.render.bake.use_clear=True;scene.render.bake.use_selected_to_active=False;scene.render.bake.use_pass_direct=False;scene.render.bake.use_pass_indirect=False;scene.render.bake.use_pass_color=True
images={};source_mats=list(obj.data.materials)
for mat in source_mats:
    for node in mat.node_tree.nodes:
        if node.type=='NORMAL_MAP':node.uv_map='TripoSourceUV'
for suffix,kind in [('BaseColor','DIFFUSE'),('Roughness','ROUGHNESS'),('Normal','NORMAL'),('Metallic','EMIT')]:
    image=bpy.data.images.new('T_KeeperTripo_'+suffix,width=4096,height=4096,alpha=False);image.colorspace_settings.name='sRGB' if suffix=='BaseColor' else 'Non-Color'
    restores=[]
    for mat in source_mats:
        node=mat.node_tree.nodes.new('ShaderNodeTexImage');node.image=image;node.label='ProductionUV rebake target';mat.node_tree.nodes.active=node
        if suffix=='Metallic':
            output=next(n for n in mat.node_tree.nodes if n.type=='OUTPUT_MATERIAL' and n.is_active_output)
            old_link=output.inputs['Surface'].links[0] if output.inputs['Surface'].links else None
            bsdf=next((n for n in mat.node_tree.nodes if n.type=='BSDF_PRINCIPLED'),None)
            emission=mat.node_tree.nodes.new('ShaderNodeEmission')
            if bsdf and bsdf.inputs['Metallic'].is_linked:mat.node_tree.links.new(bsdf.inputs['Metallic'].links[0].from_socket,emission.inputs['Color'])
            else:
                value=bsdf.inputs['Metallic'].default_value if bsdf else 0
                emission.inputs['Color'].default_value=(value,value,value,1)
            restores.append((mat,output,old_link.from_socket if old_link else None,emission));mat.node_tree.links.new(emission.outputs[0],output.inputs['Surface'])
    bpy.ops.object.bake(type=kind)
    for mat,output,socket,emission in restores:
        if socket:mat.node_tree.links.new(socket,output.inputs['Surface'])
        mat.node_tree.nodes.remove(emission)
    image.filepath_raw=str(tex/(image.name+'.png'));image.file_format='PNG';image.save();images[suffix]=image
    print('TRIPO_ORIGINAL_PBR_REBAKED '+suffix,flush=True)
# Preserve material provenance in source blend; deliver only the newly packed UV to engines.
for mat in source_mats:mat.use_fake_user=True
m=bpy.data.materials.new('M_KeeperTripo_Production');m.use_nodes=True;nodes=m.node_tree.nodes;links=m.node_tree.links;bsdf=nodes.get('Principled BSDF')
for index,(suffix,img) in enumerate(images.items()):
    node=nodes.new('ShaderNodeTexImage');node.image=img;node.location=(-700,index*-220);img.pack()
    if suffix=='Normal':
        normal=nodes.new('ShaderNodeNormalMap');links.new(node.outputs['Color'],normal.inputs['Color']);links.new(normal.outputs['Normal'],bsdf.inputs['Normal'])
    else:links.new(node.outputs['Color'],bsdf.inputs[{'BaseColor':'Base Color','Roughness':'Roughness','Metallic':'Metallic'}[suffix]])
for i in range(len(obj.data.materials)):obj.data.materials[i]=m
obj.data.uv_layers.remove(obj.data.uv_layers['TripoSourceUV']);obj.data.uv_layers.active=obj.data.uv_layers['ProductionUV'];obj.data.uv_layers.active.active_render=True
bpy.ops.wm.save_as_mainfile(filepath=str(out/'keeper-tripo-production-pbr.blend'))
report={'source_materials':[x.name for x in source_mats],'source_uv':'TripoSourceUV preserved in keeper-tripo-source-normalized.blend and keeper-tripo-clean-uv.blend','target_uv':'ProductionUV (export channel0)','texture_size':4096,'channels':list(images),'method':'bake original generated shader values; no procedural replacement shading','rigged':False}
(out/'pbr-rebake-audit.json').write_text(json.dumps(report,indent=2));print('TRIPO_PBR '+json.dumps(report))
