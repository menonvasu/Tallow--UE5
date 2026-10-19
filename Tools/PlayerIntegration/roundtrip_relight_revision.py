import bpy,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];BASE=ROOT/'Tallow/SourceAssets/Player/InteractionV05';OUT=BASE/'RelightMatchRevision01'
def read(path):
 bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
 for a in list(bpy.data.actions):bpy.data.actions.remove(a)
 bpy.ops.import_scene.fbx(filepath=str(path));r=next(o for o in bpy.context.scene.objects if o.type=='ARMATURE');a=r.animation_data.action;assert len(r.data.bones)==52;assert int(a.frame_range[1]-a.frame_range[0])+1==37
 result=[]
 for frame in [1,20,37]:
  bpy.context.scene.frame_set(frame);bpy.context.view_layer.update();result.append({n:r.matrix_world@r.pose.bones[n].matrix for n in ['hand_l','hand_r','index_03_l','thumb_03_l','index_03_r','thumb_03_r']})
 return result
original=read(BASE/'AN_PlayerV05_RelightMatch.fbx');revised=read(OUT/'AN_PlayerV05_RelightMatch.fbx');delta=max(abs(original[k][n][i][j]-revised[k][n][i][j]) for k in range(3) for n in original[k] for i in range(4) for j in range(4));seam=max(abs(revised[0][n][i][j]-revised[2][n][i][j]) for n in revised[0] for i in range(4) for j in range(4));original_seam=max(abs(original[0][n][i][j]-original[2][n][i][j]) for n in original[0] for i in range(4) for j in range(4));assert delta<1e-4 and abs(seam-original_seam)<1e-4,(delta,seam,original_seam);report={'bones':52,'samples':37,'seconds':1.2,'max_hand_fingertip_matrix_change':delta,'one_shot':True,'original_endpoint_difference':original_seam,'revised_endpoint_difference':seam};(OUT/'fbx-roundtrip-validation.json').write_text(json.dumps(report,indent=2));print('RESULT',json.dumps(report))
