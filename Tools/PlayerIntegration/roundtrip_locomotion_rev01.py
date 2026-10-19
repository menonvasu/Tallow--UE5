import bpy,json,hashlib
from pathlib import Path
from mathutils import Vector
# Re-import the LocomotionRevision01 FBXs: 52 bones, sample counts, every frame against the source action, exact loop seam, grip still Hold's,
# and the wick path (hand_r x the game's rigid candle attach) matching the source. Fingertip leaves are checked loosely: the importer rebuilds their rest.
ROOT=Path(__file__).resolve().parents[2];BASE=ROOT/'Tallow/SourceAssets/Player/InteractionV05';OUT=BASE/'LocomotionRevision01';s=bpy.context.scene;CLIPS={'Walk':31,'Sprint':19,'Crouch':61}
with bpy.data.libraries.load(str(OUT/'09_locomotion-rev01.blend'),link=False) as(src,dst):dst.objects=['RIG_Tallow_Generated','SM_CandleV02_Crooked'];dst.actions=['AN_PlayerV05_Hold']+['AN_PlayerV05_'+n for n in CLIPS]
rig,cand=dst.objects;[s.collection.objects.link(o) for o in dst.objects];length={b.name:b.length for b in rig.data.bones};acts={a.name[13:]:a for a in dst.actions}
def src_frames(name,N):
 rig.animation_data.action=acts[name];rig.animation_data.action_slot=acts[name].slots[0];out=[]
 for f in range(1,N+1):s.frame_set(f);bpy.context.view_layer.update();out.append({p.name:rig.matrix_world@p.matrix for p in rig.pose.bones})
 return out
hold=src_frames('Hold',1)[0];bpy.context.view_layer.update();CREL=hold['hand_r'].inverted()@cand.matrix_world  # candle rigid on hand_r, calibrated at Hold like the game attach
source={n:src_frames(n,N) for n,N in CLIPS.items()}
for o in list(s.objects):bpy.data.objects.remove(o)
def read(path,N):
 bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
 for a in [a for a in bpy.data.actions if a not in dst.actions]:bpy.data.actions.remove(a)
 bpy.ops.import_scene.fbx(filepath=str(path));r=next(o for o in s.objects if o.type=='ARMATURE');a=r.animation_data.action;start=int(a.frame_range[0]);res=[]  # importer keys clip frame 1 at its own start frame
 for f in range(1,N+1):s.frame_set(start+f-1);bpy.context.view_layer.update();res.append({p.name:r.matrix_world@p.matrix for p in r.pose.bones})
 return len(r.data.bones),int(a.frame_range[1]-a.frame_range[0])+1,res
delta=lambda A,B,names:max(abs(A[n][i][j]-B[n][i][j]) for n in names for i in range(4) for j in range(4))
_,_,holdfbx=read(BASE/'AN_PlayerV05_Hold.fbx',1);names=sorted(holdfbx[0]);tips=[n for n in names if '_03_' in n];core=[n for n in names if n not in tips];rfing=[n for n in core if n.endswith('_r') and n.split('_')[0] in('thumb','index','middle','ring','pinky')]
wick=lambda M:(M['hand_r']@CREL)@Vector((.004,0,.255))
report={'note':'fingertip leaf bones (*_03_*) are checked loosely (5e-3) because the FBX importer can rebuild their rest orientation; all other bones are checked strictly (1e-4) on every frame','clips':{}}
for n,N in CLIPS.items():
 fp=OUT/f'AN_PlayerV05_{n}.fbx';bones,samples,new=read(fp,N);src=source[n];W=[wick(M) for M in new[:-1]];WS=[wick(M) for M in src[:-1]]
 rep={'fbx':fp.name,'bones':bones,'samples':samples,'seconds':round((samples-1)/30,4),'loop':True,
  'max_core_delta_vs_source_all_frames':max(delta(a,b,core) for a,b in zip(new,src)),'max_fingertip_leaf_delta_vs_source':max(delta(a,b,tips) for a,b in zip(new,src)),
  'loop_seam_delta_frame1_vs_last':delta(new[0],new[-1],names),
  'grip_right_fingers_vs_hold_in_hand_space':max(delta({k:M['hand_r'].inverted()@M[k] for k in rfing},{k:hold['hand_r'].inverted()@hold[k] for k in rfing},rfing) for M in new),
  'grip_right_fingers_vs_InteractionV05_Hold_fbx_in_hand_space':max(delta({k:M['hand_r'].inverted()@M[k] for k in rfing},{k:holdfbx[0]['hand_r'].inverted()@holdfbx[0][k] for k in rfing},rfing) for M in new),
  'wick_path_max_error_vs_source_cm':max((a-b).length for a,b in zip(W,WS))*100,
  'wick_range_cm_xyz':[round((max(w[i] for w in W)-min(w[i] for w in W))*100,2) for i in range(3)],'frame1_wick_from_hold_cm':round((W[0]-wick(hold)).length*100,2),
  'worst_core_bone':max((delta(a,b,[k]),k,f+1) for f,(a,b) in enumerate(zip(new,src)) for k in core)[1:],'fbx_sha256':hashlib.sha256(fp.read_bytes()).hexdigest()}
 report['clips'][n]=rep
 assert bones==52 and samples==N and rep['max_core_delta_vs_source_all_frames']<1e-4 and rep['max_fingertip_leaf_delta_vs_source']<5e-3 and rep['loop_seam_delta_frame1_vs_last']<1e-5 and rep['grip_right_fingers_vs_hold_in_hand_space']<1e-4 and rep['grip_right_fingers_vs_InteractionV05_Hold_fbx_in_hand_space']<1e-4 and rep['wick_path_max_error_vs_source_cm']<.01,(n,rep)
(OUT/'fbx-roundtrip-validation.json').write_text(json.dumps(report,indent=2));print('RESULT',json.dumps(report),flush=True)
