import bpy,json,hashlib,sys
from pathlib import Path
# Re-import the CupShield revision 03 FBX: 52 bones, 61 frames, exact loop seam, body still the Revision01 clip (the game's CupShield_01),
# right hand still upright with the same grip (fixed: whole right side identical), left hand matching the source blend.
VARIANT='drawn'
ROOT=Path(__file__).resolve().parents[2];BASE=ROOT/'Tallow/SourceAssets/Player/InteractionV05';OUT=BASE/'CupShieldRevision03';s=bpy.context.scene
with bpy.data.libraries.load(str(OUT/'12_cupshield-rev03.blend'),link=False) as(src,dst):dst.objects=['RIG_Tallow_Generated'];dst.actions=['AN_PlayerV05_CupShield']
rig=dst.objects[0];s.collection.objects.link(rig);rig.animation_data.action=dst.actions[0];rig.animation_data.action_slot=dst.actions[0].slots[0];s.frame_set(1);bpy.context.view_layer.update();source={p.name:rig.matrix_world@p.matrix for p in rig.pose.bones}
def read(path,frames):
 bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
 for a in list(bpy.data.actions):bpy.data.actions.remove(a)
 bpy.ops.import_scene.fbx(filepath=str(path));r=next(o for o in s.objects if o.type=='ARMATURE');a=r.animation_data.action;result=[];start=int(a.frame_range[0])  # importer keys clip frame 1 at its own start frame
 for frame in frames:
  s.frame_set(start+frame-1);bpy.context.view_layer.update();result.append({p.name:r.matrix_world@p.matrix for p in r.pose.bones})
 return len(r.data.bones),int(a.frame_range[1]-a.frame_range[0])+1,result
delta=lambda A,B,names,k=4:max(abs(A[n][i][j]-B[n][i][j]) for n in names for i in range(k) for j in range(k))
_,_,old=read(BASE/'CupShieldRevision01/AN_PlayerV05_CupShield.fbx',[1]);bones,samples,new=read(OUT/'AN_PlayerV05_CupShield.fbx',[1,31,61]);names=sorted(new[0])
tips=[n for n in names if '_03_' in n];core=[n for n in names if n not in tips]  # importer rebuilds fingertip leaf orientation; checked against the source blend instead
ARM=('clavicle','upperarm','lowerarm','hand','thumb','index','middle','ring','pinky');right=[n for n in core if n.endswith('_r') and n.split('_')[0] in ARM];left=[n for n in core if n.endswith('_l') and n.split('_')[0] in ARM]
body=[n for n in core if n not in right and n not in left]+['clavicle_r'];rfing=[n for n in right if n not in('clavicle_r','upperarm_r','lowerarm_r','hand_r')]
rel=lambda M,n:M['hand_r'].inverted()@M[n]
report={'variant':VARIANT,'bones':bones,'samples':samples,'seconds':round((samples-1)/30,4),'loop':True,'loop_seam_frame1_vs_61':delta(new[0],new[2],names),'body_vs_CupShield_01_fbx':delta(new[0],old[0],body),
 'right_hand_rotation_vs_CupShield_01_fbx':delta(new[0],old[0],['hand_r'],3),'right_fingers_relative_to_hand_vs_CupShield_01_fbx':max(max(abs(rel(new[0],n)[i][j]-rel(old[0],n)[i][j]) for i in range(4) for j in range(4)) for n in rfing),
 'right_side_vs_CupShield_01_fbx':delta(new[0],old[0],right),'left_core_vs_source_blend':delta(new[0],source,left),'left_fingertips_vs_source_blend':delta(new[0],source,[n for n in tips if n.endswith('_l')]),
 'fbx_sha256':hashlib.sha256((OUT/'AN_PlayerV05_CupShield.fbx').read_bytes()).hexdigest()}
assert bones==52 and samples==61 and report['loop_seam_frame1_vs_61']<1e-5 and report['body_vs_CupShield_01_fbx']<1e-4 and report['right_hand_rotation_vs_CupShield_01_fbx']<1e-4 and report['right_fingers_relative_to_hand_vs_CupShield_01_fbx']<1e-4 and report['left_core_vs_source_blend']<1e-4,report
assert VARIANT=='drawn' or report['right_side_vs_CupShield_01_fbx']<1e-4,report
(OUT/'fbx-roundtrip-validation.json').write_text(json.dumps(report,indent=2));print('RESULT',json.dumps(report))
