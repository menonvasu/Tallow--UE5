import bpy,json,hashlib
from pathlib import Path
# Re-import the Reach revision 01 FBX: 52 bones, 31 frames, frames 1/31 equal the current Reach endpoints (Hold) so the game's loop and 0.18 s blends stay seamless,
# right side identical to the current Reach throughout, left hand matching the source blend at the grab.
ROOT=Path(__file__).resolve().parents[2];BASE=ROOT/'Tallow/SourceAssets/Player/InteractionV05';OUT=BASE/'ReachRevision01';s=bpy.context.scene
with bpy.data.libraries.load(str(OUT/'10_reach-rev01.blend'),link=False) as(src,dst):dst.objects=['RIG_Tallow_Generated'];dst.actions=['AN_PlayerV05_Reach']
rig=dst.objects[0];s.collection.objects.link(rig);rig.animation_data.action=dst.actions[0];rig.animation_data.action_slot=dst.actions[0].slots[0];s.frame_set(15);bpy.context.view_layer.update();source={p.name:rig.matrix_world@p.matrix for p in rig.pose.bones}
def read(path,frames):
 bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
 for a in list(bpy.data.actions):bpy.data.actions.remove(a)
 bpy.ops.import_scene.fbx(filepath=str(path));r=next(o for o in s.objects if o.type=='ARMATURE');a=r.animation_data.action;result=[];start=int(a.frame_range[0])  # importer keys clip frame 1 at its own start frame
 for frame in frames:
  s.frame_set(start+frame-1);bpy.context.view_layer.update();result.append({p.name:r.matrix_world@p.matrix for p in r.pose.bones})
 return len(r.data.bones),int(a.frame_range[1]-a.frame_range[0])+1,result
delta=lambda A,B,names:max(abs(A[n][i][j]-B[n][i][j]) for n in names for i in range(4) for j in range(4))
_,_,old=read(BASE/'AN_PlayerV05_Reach.fbx',[1,15,31]);bones,samples,new=read(OUT/'AN_PlayerV05_Reach.fbx',[1,15,31]);names=sorted(new[0])
tips=[n for n in names if '_03_' in n];core=[n for n in names if n not in tips]  # importer rebuilds fingertip leaf orientation; checked against the source blend instead
ARM=('clavicle','upperarm','lowerarm','hand','thumb','index','middle','ring','pinky');left=[n for n in core if n.endswith('_l') and n.split('_')[0] in ARM];rest=[n for n in core if n not in left]
report={'bones':bones,'samples':samples,'seconds':round((samples-1)/30,4),'loop_seam_frame1_vs_31':delta(new[0],new[2],names),'frame1_vs_current_reach_fbx':delta(new[0],old[0],core),'frame31_vs_current_reach_fbx':delta(new[2],old[2],core),
 'nonleft_frame15_vs_current_reach_fbx':delta(new[1],old[1],rest),'left_core_frame15_vs_source_blend':delta(new[1],source,left),'left_fingertips_frame15_vs_source_blend':delta(new[1],source,[n for n in tips if n.endswith('_l')]),
 'left_hand_moved_from_current_reach_m':(new[1]['hand_l'].translation-old[1]['hand_l'].translation).length,'fbx_sha256':hashlib.sha256((OUT/'AN_PlayerV05_Reach.fbx').read_bytes()).hexdigest()}
assert bones==52 and samples==31 and report['loop_seam_frame1_vs_31']<1e-5 and report['frame1_vs_current_reach_fbx']<1e-4 and report['frame31_vs_current_reach_fbx']<1e-4 and report['nonleft_frame15_vs_current_reach_fbx']<1e-4 and report['left_core_frame15_vs_source_blend']<1e-4,report
(OUT/'fbx-roundtrip-validation.json').write_text(json.dumps(report,indent=2));print('RESULT',json.dumps(report))
