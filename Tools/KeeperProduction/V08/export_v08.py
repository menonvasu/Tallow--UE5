"""Bake the V08 control rig onto the unchanged 109-bone export skeleton and the game mesh (V07 Reveal Revision02), then
write one FBX per clip with exactly the V07 export call. Morph curves: FULL_HORROR, HOOD_OPEN, CHOMP (+ Blink_L/R held 0).
blender -b --factory-startup --threads 4 --python-exit-code 1 --python export_v08.py"""
import bpy,json,hashlib
import numpy as np
from pathlib import Path
from mathutils import Matrix
ROOT=Path(__file__).resolve().parents[3];OUT=ROOT/'Tallow/SourceAssets/Keeper/V08Animation';FBX=OUT/'Export';FBX.mkdir(exist_ok=True)
REV2=ROOT/'Tallow/SourceAssets/Keeper/V07RevealRevision02/keeper-reveal-revision02-export.blend'
bpy.ops.wm.open_mainfile(filepath=str(OUT/'keeper-v08-animated.blend'));scene=bpy.context.scene
src=bpy.data.objects['RIG_KeeperV07'];src_mesh=bpy.data.objects['SK_KeeperRiggedV07'];src_mesh.hide_viewport=False
contract=json.loads((OUT/'animation-contract.json').read_text());deform=[b for b in src.data.bones if b.use_deform]
# game mesh + its skeleton (reference only: rest matrices must match ours exactly)
with bpy.data.libraries.load(str(REV2),link=False) as (src_lib,dst):dst.objects=['SK_KeeperRiggedV07_Export','KeeperV07']
gmesh=bpy.data.objects['SK_KeeperRiggedV07_Export'];gref=bpy.data.objects['KeeperV07'];gref.name='REF_Revision02_Skeleton';gref.data.name='REF_Revision02_SkeletonData'
rest_err=max(max(abs(a-b) for ra,rb in zip(src.data.bones[b.name].matrix_local,gref.data.bones[b.name].matrix_local) for a,b in zip(ra,rb)) for b in deform)
assert len(gref.data.bones)==109 and rest_err<1e-5,'game skeleton differs from the source deform skeleton'
arm=bpy.data.armatures.new('KeeperV07_ExportSkeleton');rig=bpy.data.objects.new('KeeperV07',arm);scene.collection.objects.link(rig)
bpy.ops.object.select_all(action='DESELECT');rig.select_set(True);bpy.context.view_layer.objects.active=rig;bpy.ops.object.mode_set(mode='EDIT')
for s in deform:
    b=arm.edit_bones.new(s.name);b.head=s.head_local;b.tail=s.tail_local;b.matrix=s.matrix_local;b.length=s.length;b.use_deform=True
    if s.parent:b.parent=arm.edit_bones[s.parent.name]
bpy.ops.object.mode_set(mode='OBJECT')
scene.collection.objects.link(gmesh);gmesh.parent=rig;gmesh.matrix_parent_inverse=Matrix.Identity(4)
for mod in gmesh.modifiers:
    if mod.type=='ARMATURE':mod.object=rig
if gmesh.data.shape_keys.animation_data:gmesh.data.shape_keys.animation_data_clear()
for p in rig.pose.bones:p.rotation_mode='QUATERNION'
def new_action(idb,name):
    old=bpy.data.actions.get(name)
    if old:bpy.data.actions.remove(old)
    idb.animation_data_create();a=bpy.data.actions.new(name);a.use_fake_user=True;sl=a.slots.new(id_type=idb.id_type,name=idb.name);idb.animation_data.action=a;idb.animation_data.action_slot=sl;return a
MORPH={'FULL_HORROR':('CTRL_body','mantle_gather'),'HOOD_OPEN':('CTRL_head','hood_open'),'CHOMP':('CTRL_head','chomp')}
baked={};fid=[]
for clip in contract['clips']:
    name=clip['name'];act=bpy.data.actions['AN_Keeper_'+name];src.animation_data.action=act;src.animation_data.action_slot=act.slots[0];rows=[]
    for f in range(1,clip['frames']+1):
        scene.frame_set(f);bpy.context.view_layer.update()
        rows.append((f,{b.name:src.pose.bones[b.name].matrix.copy() for b in deform},{k:float(src.pose.bones[o][p]) for k,(o,p) in MORPH.items()}))
    ba=new_action(rig,'EXPORT_AN_Keeper_'+name);ma=new_action(gmesh.data.shape_keys,'EXPORT_MORPH_'+name)
    for f,mats,morphs in rows:
        for b in deform:
            p=rig.pose.bones[b.name];par=b.parent
            p.matrix_basis=b.convert_local_to_pose(mats[b.name],b.matrix_local,parent_matrix=mats[par.name] if par else Matrix.Identity(4),parent_matrix_local=par.matrix_local if par else Matrix.Identity(4),invert=True)
            p.keyframe_insert(data_path='location',frame=f);p.keyframe_insert(data_path='rotation_quaternion',frame=f);p.keyframe_insert(data_path='scale',frame=f)
        kb=gmesh.data.shape_keys.key_blocks
        for k,v in list(morphs.items())+[('Blink_L',0.),('Blink_R',0.)]:kb[k].value=v;kb[k].keyframe_insert(data_path='value',frame=f)
    from bpy_extras.anim_utils import action_get_channelbag_for_slot
    for a in (ba,ma):
        for fc in action_get_channelbag_for_slot(a,a.slots[0]).fcurves:
            for kp in fc.keyframe_points:kp.interpolation='LINEAR'
    # fidelity: export skeleton vs control rig (bone world matrices) and game-mesh vs source surface (same topology)
    berr=0;serr=0
    for f in sorted({1,1+(clip['frames']-1)//2,clip['frames']}):
        scene.frame_set(f);bpy.context.view_layer.update()
        berr=max(berr,max((rig.pose.bones[b.name].matrix.translation-src.pose.bones[b.name].matrix.translation).length for b in deform))
        surf=[]
        for ob in (src_mesh,gmesh):
            ev=ob.evaluated_get(bpy.context.evaluated_depsgraph_get());me=ev.to_mesh();co=np.empty(len(me.vertices)*3,np.float32);me.vertices.foreach_get('co',co);surf.append(co.reshape(-1,3));ev.to_mesh_clear()
        serr=max(serr,float(np.linalg.norm(surf[0]-surf[1],axis=1).max()))
    SAMPLE=['DEF_body','head','DEF_arm_1.L_hand','DEF_arm_2.R_hand','DEF_arm_3.L_hand']
    samp={}
    for f in sorted({1,1+(clip['frames']-1)//2,clip['frames']}):
        scene.frame_set(f);bpy.context.view_layer.update();samp[f]={b_:[round(x,5) for x in rig.pose.bones[b_].head] for b_ in SAMPLE}
    fid.append({'clip':name,'max_bone_error_m':berr,'max_surface_diff_vs_source_m':serr,'samples_m':samp});assert berr<1e-4,(name,berr)
    baked[name]=(ba,ma);print('BAKED',name,'bone %.2e surface %.2e'%(berr,serr),flush=True)
bpy.ops.object.select_all(action='DESELECT');rig.select_set(True);gmesh.select_set(True);bpy.context.view_layer.objects.active=rig
opts=dict(use_selection=True,object_types={'MESH','ARMATURE'},global_scale=100.,apply_scale_options='FBX_SCALE_ALL',axis_forward='-Y',axis_up='Z',add_leaf_bones=False,use_armature_deform_only=True,mesh_smooth_type='FACE',use_mesh_modifiers=False,path_mode='RELATIVE')
rig.data.pose_position='POSE'
for clip in contract['clips']:
    name=clip['name'];ba,ma=baked[name];rig.animation_data.action=ba;rig.animation_data.action_slot=ba.slots[0];gmesh.data.shape_keys.animation_data.action=ma;gmesh.data.shape_keys.animation_data.action_slot=ma.slots[0]
    scene.frame_start=1;scene.frame_end=clip['frames'];scene.frame_set(1)
    bpy.ops.export_scene.fbx(filepath=str(FBX/('AN_Keeper_'+name+'.fbx')),bake_anim=True,bake_anim_use_all_actions=False,bake_anim_use_nla_strips=False,bake_anim_step=1,bake_anim_simplify_factor=0,**opts)
for ob in list(bpy.data.objects):
    if ob not in (rig,gmesh):bpy.data.objects.remove(ob,do_unlink=True)
for a in list(bpy.data.actions):
    if a.name.startswith('AN_Keeper_'):bpy.data.actions.remove(a)
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'keeper-v08-export.blend'))
rep={'source':'keeper-v08-animated.blend','mesh':'V07RevealRevision02 SK_KeeperRiggedV07_Export (game mesh, unchanged)','deform_bones':len(deform),'rest_matrix_max_diff_vs_game_skeleton':rest_err,
     'animations':len(contract['clips']),'morph_curves':['FULL_HORROR','HOOD_OPEN','CHOMP','Blink_L','Blink_R'],'bake_fidelity':fid,
     'fbx_call':'identical to Tools/KeeperProduction/V07/export_v07.py (global_scale 100, FBX_SCALE_ALL, -Y forward, Z up, no leaf bones, deform only, baked step 1, no simplify)',
     'files':{p.name:{'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in sorted(FBX.glob('AN_Keeper_*.fbx'))}}
(OUT/'export-audit.json').write_text(json.dumps(rep,indent=2));print('RESULT',json.dumps({k:v for k,v in rep.items() if k not in ('files','bake_fidelity')}))
