"""Bake animator control rig into a clean109-bone export skeleton and20 animation FBXs."""
import bpy,json,hashlib,shutil,numpy as np
from pathlib import Path
from mathutils import Matrix
from bpy_extras.anim_utils import action_get_channelbag_for_slot
ROOT=Path(__file__).resolve().parents[3];OUT=ROOT/'Tallow/SourceAssets/Keeper/RiggedV07';FBX=OUT/'Export';TEX=OUT/'Textures';FBX.mkdir(exist_ok=True);TEX.mkdir(exist_ok=True)
bpy.ops.wm.open_mainfile(filepath=str(OUT/'keeper-v07-animated.blend'));source_rig=bpy.data.objects['RIG_KeeperV07'];source_mesh=bpy.data.objects['SK_KeeperRiggedV07'];scene=bpy.context.scene
contract=json.loads((OUT/'animation-contract.json').read_text());deform=[b for b in source_rig.data.bones if b.use_deform];arm=bpy.data.armatures.new('KeeperV07_ExportSkeleton');rig=bpy.data.objects.new('KeeperV07',arm);scene.collection.objects.link(rig)
bpy.ops.object.select_all(action='DESELECT');rig.select_set(True);bpy.context.view_layer.objects.active=rig;bpy.ops.object.mode_set(mode='EDIT')
for source in deform:
 b=arm.edit_bones.new(source.name);b.head=source.head_local;b.tail=source.tail_local;b.matrix=source.matrix_local;b.length=source.length;b.use_deform=True
 if source.parent:b.parent=arm.edit_bones[source.parent.name]
bpy.ops.object.mode_set(mode='OBJECT')
mesh=source_mesh.copy();mesh.data=source_mesh.data.copy();mesh.name='SK_KeeperRiggedV07_Export';scene.collection.objects.link(mesh);mesh.parent=rig
for mod in mesh.modifiers:
 if mod.type=='ARMATURE':mod.object=rig
mesh.data.shape_keys.animation_data_clear()
for p in rig.pose.bones:p.rotation_mode='QUATERNION'
for texture in (ROOT/'Tallow/SourceAssets/Keeper/SleevedV06/Textures').glob('*.png'):shutil.copy2(texture,TEX/texture.name)
for image in bpy.data.images:
 if image.packed_file:continue
 if Path(image.filepath).name.startswith('T_KeeperTripo_') and (TEX/Path(image.filepath).name).is_file():image.filepath=str(TEX/Path(image.filepath).name);image.pack()

def new_action(idblock,name):
 idblock.animation_data_create();act=bpy.data.actions.new(name);act.use_fake_user=True;slot=act.slots.new(id_type=idblock.id_type,name=idblock.name);idblock.animation_data.action=act;idblock.animation_data.action_slot=slot;return act

baked={};fidelity=[]
for clip in contract['clips']:
 name=clip['name'];act=bpy.data.actions['AN_Keeper_'+name];source_rig.animation_data.action=act;source_rig.animation_data.action_slot=act.slots[0]
 rows=[]
 for frame in range(1,clip['frames']+1):
  scene.frame_set(frame);source_rig.update_tag();bpy.context.view_layer.update();rows.append((frame,{b.name:source_rig.pose.bones[b.name].matrix.copy() for b in deform},{k.name:k.value for k in source_mesh.data.shape_keys.key_blocks if k.name in ['FULL_HORROR','HOOD_OPEN']}))
 ba=new_action(rig,'EXPORT_AN_Keeper_'+name);ma=new_action(mesh.data.shape_keys,'EXPORT_MORPH_'+name)
 for frame,matrices,morphs in rows:
  for bone in deform:
   p=rig.pose.bones[bone.name];parent=bone.parent
   p.matrix_basis=bone.convert_local_to_pose(matrices[bone.name],bone.matrix_local,parent_matrix=matrices[parent.name] if parent else Matrix.Identity(4),parent_matrix_local=parent.matrix_local if parent else Matrix.Identity(4),invert=True)
   p.keyframe_insert(data_path='location',frame=frame);p.keyframe_insert(data_path='rotation_quaternion',frame=frame);p.keyframe_insert(data_path='scale',frame=frame)
  for key,value in morphs.items():mesh.data.shape_keys.key_blocks[key].value=value;mesh.data.shape_keys.key_blocks[key].keyframe_insert(data_path='value',frame=frame)
 for action in [ba,ma]:
  for fc in action_get_channelbag_for_slot(action,action.slots[0]).fcurves:
   for k in fc.keyframe_points:k.interpolation='LINEAR'
 errors=[]
 for frame in [1,1+(clip['frames']-1)//2,clip['frames']]:
  scene.frame_set(frame);bpy.context.view_layer.update();surfaces=[]
  for ob in [source_mesh,mesh]:
   ev=ob.evaluated_get(bpy.context.evaluated_depsgraph_get());me=ev.to_mesh();co=np.empty(len(me.vertices)*3,dtype=np.float32);me.vertices.foreach_get('co',co);surfaces.append(co.reshape(-1,3));ev.to_mesh_clear()
  errors.append(float(np.linalg.norm(surfaces[0]-surfaces[1],axis=1).max()))
 fidelity.append({'clip':name,'maximum_surface_bake_error_m':max(errors)})
 assert max(errors)<.0001,'Baked rig deformation mismatch '+name+': '+str(errors)
 baked[name]=(ba,ma);print('BAKED '+name+' error='+str(max(errors)),flush=True)
# Export only baked deformation skeleton+mesh; no control dependency can enter game.
bpy.ops.object.select_all(action='DESELECT');rig.select_set(True);mesh.select_set(True);bpy.context.view_layer.objects.active=rig
opts=dict(use_selection=True,object_types={'MESH','ARMATURE'},global_scale=100.,apply_scale_options='FBX_SCALE_ALL',axis_forward='-Y',axis_up='Z',add_leaf_bones=False,use_armature_deform_only=True,mesh_smooth_type='FACE',use_mesh_modifiers=False,path_mode='RELATIVE')
rig.animation_data.action=None;mesh.data.shape_keys.animation_data.action=None;rig.data.pose_position='REST'
for key in mesh.data.shape_keys.key_blocks:key.value=0
bpy.ops.export_scene.fbx(filepath=str(FBX/'SK_KeeperRiggedV07.fbx'),bake_anim=False,**opts)
rig.data.pose_position='POSE'
for clip in contract['clips']:
 name=clip['name'];ba,ma=baked[name];rig.animation_data.action=ba;rig.animation_data.action_slot=ba.slots[0];mesh.data.shape_keys.animation_data.action=ma;mesh.data.shape_keys.animation_data.action_slot=ma.slots[0];scene.frame_start=1;scene.frame_end=clip['frames'];scene.frame_set(1)
 bpy.ops.export_scene.fbx(filepath=str(FBX/('AN_Keeper_'+name+'.fbx')),bake_anim=True,bake_anim_use_all_actions=False,bake_anim_use_nla_strips=False,bake_anim_step=1,bake_anim_simplify_factor=0,**opts)
# Source control blend remains independent. Save a clean export-only scene for forensic inspection.
for ob in list(bpy.data.objects):
 if ob not in [rig,mesh]:bpy.data.objects.remove(ob,do_unlink=True)
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'keeper-v07-export.blend'))
report={'mesh':'SK_KeeperRiggedV07.fbx','deform_bones':len(deform),'no_controls_exported':True,'animations':len(contract['clips']),'files':{p.name:{'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in FBX.glob('*.fbx')},'bake_fidelity':fidelity,'engine_import_contract':'Legacy FBX forced per task; import_uniform_scale100; import_morph_targetsTrue; measured240cm required; actor mesh scale1'}
(OUT/'export-audit.json').write_text(json.dumps(report,indent=2));print('RESULT '+json.dumps({k:v for k,v in report.items() if k!='files'}))
