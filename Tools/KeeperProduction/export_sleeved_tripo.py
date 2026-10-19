"""Bake own authored ProductionUV PBR atlas, export staged skeletal FBX and 13 clips.
Never writes existing /Animated source or Unreal Content. CPU only.
"""
import bpy, json, hashlib, sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'Tallow/SourceAssets/Keeper/SleevedV06';TEX=OUT/'Textures';FBX=OUT/'Export'
TEX.mkdir(exist_ok=True);FBX.mkdir(exist_ok=True)
bpy.ops.wm.open_mainfile(filepath=str(OUT/'keeper-direct-animated.blend'))
obj=bpy.data.objects['SK_Keeper_Tripo_Direct'];rig=bpy.data.objects['KEEPER_PRODUCTION_RIG'];scene=bpy.context.scene
rig.animation_data.action=None;obj.data.shape_keys.animation_data.action=None;rig.data.pose_position='REST'
for key in obj.data.shape_keys.key_blocks:key.value=0
bpy.ops.object.select_all(action='DESELECT');obj.select_set(True);bpy.context.view_layer.objects.active=obj
scene.render.engine='CYCLES';scene.cycles.device='CPU';scene.cycles.samples=8;scene.render.threads_mode='FIXED';scene.render.threads=6
scene.render.bake.margin=12;scene.render.bake.use_selected_to_active=False;scene.render.bake.use_clear=True
scene.render.bake.use_pass_direct=False;scene.render.bake.use_pass_indirect=False;scene.render.bake.use_pass_color=True
images={}
for suffix in ['BaseColor','Roughness','Normal','Metallic']:
 image=bpy.data.images.load(str(TEX/('T_KeeperTripo_'+suffix+'.png')),check_existing=False)
 image.colorspace_settings.name='sRGB' if suffix=='BaseColor' else 'Non-Color';image.pack();images[suffix]=image
 for mat in obj.data.materials:
  for node in mat.node_tree.nodes:
   if node.type=='TEX_IMAGE' and node.image and suffix in node.image.name:node.image=image
# Only mesh/rig selected; review floor/cameras/lights cannot leak into export.
bpy.ops.object.select_all(action='DESELECT');obj.select_set(True);rig.select_set(True);bpy.context.view_layer.objects.active=rig
opts=dict(use_selection=True,object_types={'MESH','ARMATURE'},global_scale=100.0,apply_scale_options='FBX_SCALE_ALL',axis_forward='-Y',axis_up='Z',add_leaf_bones=False,use_armature_deform_only=True,mesh_smooth_type='FACE',use_mesh_modifiers=False,path_mode='RELATIVE')
bpy.ops.export_scene.fbx(filepath=str(FBX/'SK_Keeper_Tripo_Direct.fbx'),bake_anim=False,**opts)
rig.data.pose_position='POSE'
clips=json.loads((OUT/'animation-audit.json').read_text())['clips']
for clip in clips:
    name=clip['name'];action=bpy.data.actions['AN_Keeper_'+name];morph=bpy.data.actions['MORPH_Keeper_'+name]
    rig.animation_data.action=action;rig.animation_data.action_slot=action.slots[0];obj.data.shape_keys.animation_data.action=morph;obj.data.shape_keys.animation_data.action_slot=morph.slots[0]
    scene.frame_start=1;scene.frame_end=clip['frames'];scene.frame_set(1)
    bpy.ops.export_scene.fbx(filepath=str(FBX/('AN_Keeper_'+name+'.fbx')),bake_anim=True,bake_anim_use_all_actions=False,bake_anim_use_nla_strips=False,bake_anim_step=1,bake_anim_simplify_factor=0,**opts)
rig.animation_data.action=bpy.data.actions['AN_Keeper_Idle'];rig.animation_data.action_slot=rig.animation_data.action.slots[0]
obj.data.shape_keys.animation_data.action=bpy.data.actions['MORPH_Keeper_Idle'];obj.data.shape_keys.animation_data.action_slot=obj.data.shape_keys.animation_data.action.slots[0]
scene.frame_start=1;scene.frame_end=97;scene.frame_set(1)
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'keeper-direct-delivery.blend'))
report={'stage':'staged_export_pending_roundtrip_and_engine_validation','textures':{s:{'path':str(Path(i.filepath).relative_to(ROOT)),'resolution':[4096,4096],'colorspace':i.colorspace_settings.name} for s,i in images.items()},'fbx_files':{p.name:{'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in FBX.glob('*.fbx')},'unit_export':'Blender meters with FBX global_scale100; intended Unreal centimeters','actions':len(clips),'pose_rate':12,'frame_rate':24,'root_motion':'none','source':'keeper-direct-delivery.blend'}
(OUT/'export-audit.json').write_text(json.dumps(report,indent=2));print('KEEPER_EXPORT '+json.dumps({'files':len(report['fbx_files']),'textures':len(images),'source':report['source']}))
