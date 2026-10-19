import bpy,json,hashlib
from pathlib import Path
from mathutils import Vector
# Re-import the revision 02 FBX: 52 bones, 37 frames, frames 1/37 equal Hold, right side still Hold at the contact, and the frame-20 pinch still matches the match contract.
ROOT=Path(__file__).resolve().parents[2];BASE=ROOT/'Tallow/SourceAssets/Player/InteractionV05';OUT=BASE/'RelightMatchRevision02';s=bpy.context.scene
def read(path,frames):
 bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
 for a in list(bpy.data.actions):bpy.data.actions.remove(a)
 bpy.ops.import_scene.fbx(filepath=str(path));r=next(o for o in s.objects if o.type=='ARMATURE');a=r.animation_data.action;result=[];start=int(a.frame_range[0])  # importer keys clip frame 1 at its own start frame
 for frame in frames:
  s.frame_set(start+frame-1);bpy.context.view_layer.update();result.append({p.name:r.matrix_world@p.matrix for p in r.pose.bones})
 return len(r.data.bones),int(a.frame_range[1]-a.frame_range[0])+1,result
with bpy.data.libraries.load(str(OUT/'08_relight-match-v2.blend'),link=False) as(src,dst):dst.objects=['RIG_Tallow_Generated'];dst.actions=['AN_PlayerV05_Hold']
rig=dst.objects[0];s.collection.objects.link(rig);rig.animation_data.action=dst.actions[0];rig.animation_data.action_slot=dst.actions[0].slots[0];s.frame_set(1);bpy.context.view_layer.update()
source_hold={p.name:rig.matrix_world@p.matrix for p in rig.pose.bones};length={b.name:b.length for b in rig.data.bones}  # FBX import guesses leaf lengths; rebuild fingertip tails from the source rig
delta=lambda A,B,names:max(abs(A[n][i][j]-B[n][i][j]) for n in names for i in range(4) for j in range(4))
worst=lambda A,B,names:max((max(abs(A[n][i][j]-B[n][i][j]) for i in range(4) for j in range(4)),n) for n in names)[1]
_,_,hold=read(BASE/'AN_PlayerV05_Hold.fbx',[1]);bones,samples,new=read(OUT/'AN_PlayerV05_RelightMatch.fbx',[1,20,37]);names=sorted(hold[0]);tips=[n for n in names if '_03_' in n];core=[n for n in names if n not in tips]  # importer rebuilds fingertip leaf orientation; check those separately
contract=json.loads((OUT/'tool-match-contract.json').read_text())['match'];cl=contract['camera_location_cm'];pinch_contract=Vector((cl[1]/100,cl[0]/100-.2,cl[2]/100+1.1))
tip=lambda n:new[1][n]@Vector((0,length[n]/new[1][n].to_scale()[1],0))
pinch=(tip('index_03_l')+tip('thumb_03_l'))/2
report={'bones':bones,'samples':samples,'seconds':round((samples-1)/30,4),'one_shot':True,'frame1_vs_source_hold':delta(new[0],source_hold,core),'frame37_vs_source_hold':delta(new[2],source_hold,core),'frame1_fingertip_leaves_vs_source_hold':delta(new[0],source_hold,tips),'fingertip_leaf_worst_bone':worst(new[0],source_hold,tips),'endpoint_difference':delta(new[0],new[2],names),
 'frame20_right_side_vs_source_hold':delta(new[1],source_hold,[n for n in core if n.endswith('_r')]),'frame20_right_fingertips_vs_frame1':delta(new[1],new[0],[n for n in tips if n.endswith('_r')]),'frame20_left_hand_moved_m':(new[1]['hand_l'].translation-source_hold['hand_l'].translation).length,
 'frame1_vs_InteractionV05_Hold_fbx':delta(new[0],hold[0],names),'InteractionV05_Hold_fbx_worst_bone':worst(new[0],hold[0],names),'note':'fingertip leaf bones (*_03_*) get their rest orientation rebuilt by the FBX importer (ring_03_r ~3e-3 in every import, Hold FBX included); all other bones are checked strictly','frame20_pinch_vs_contract_cm':(pinch-pinch_contract).length*100,'frame20_pinch_tip_gap_cm':(tip('index_03_l')-tip('thumb_03_l')).length*100,'fbx_sha256':hashlib.sha256((OUT/'AN_PlayerV05_RelightMatch.fbx').read_bytes()).hexdigest()}
assert bones==52 and samples==37 and report['frame1_vs_source_hold']<1e-4 and report['frame37_vs_source_hold']<1e-4 and report['frame20_right_side_vs_source_hold']<1e-4 and report['frame20_right_fingertips_vs_frame1']<1e-4 and report['frame1_fingertip_leaves_vs_source_hold']<5e-3 and report['frame20_left_hand_moved_m']>.2 and report['frame20_pinch_vs_contract_cm']<.05 and report['frame1_vs_InteractionV05_Hold_fbx']<5e-3,report
(OUT/'fbx-roundtrip-validation.json').write_text(json.dumps(report,indent=2));print('RESULT',json.dumps(report))
