import bpy,json,hashlib
from pathlib import Path
from mathutils import Vector,Matrix
# Re-import the ToolRevision01 FBXs: 52 bones, frame counts, loop seams, FuseInsert starts on ToolHold, Pickup ends on Hold, every clip matches the source blend,
# and the contract's camera-relative tool transforms reproduce from the re-imported ToolHold frame 1 (as the game calibrates).
ROOT=Path(__file__).resolve().parents[2];BASE=ROOT/'Tallow/SourceAssets/Player/InteractionV05';OUT=BASE/'ToolRevision01';s=bpy.context.scene
CLIPS={'ToolHold':61,'CrowbarPry':61,'FuseInsert':34,'Pickup':46}
with bpy.data.libraries.load(str(OUT/'11_tools-rev01.blend'),link=False) as(src,dst):dst.objects=['RIG_Tallow_Generated'];dst.actions=['AN_PlayerV05_'+n for n in CLIPS]+['AN_PlayerV05_Hold']
rig=dst.objects[0];s.collection.objects.link(rig);source={}
for a in dst.actions:
 rig.animation_data.action=a;rig.animation_data.action_slot=a.slots[0];n=a.name.split('_')[-1];source[n]={}
 for f in sorted({1,CLIPS.get(n,1)//2,CLIPS.get(n,1)}):s.frame_set(f);bpy.context.view_layer.update();source[n][f]={p.name:rig.matrix_world@p.matrix for p in rig.pose.bones}
def read(path,frames):
 bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
 for a in list(bpy.data.actions):
  if a.name.startswith('AN_PlayerV05_') and a.users==0 and a not in dst.actions:bpy.data.actions.remove(a)
 bpy.ops.import_scene.fbx(filepath=str(path));r=next(o for o in s.objects if o.type=='ARMATURE');a=r.animation_data.action;start=int(a.frame_range[0]);res={}  # importer keys clip frame 1 at its own start frame
 for f in frames:s.frame_set(start+f-1);bpy.context.view_layer.update();res[f]={p.name:r.matrix_world@p.matrix for p in r.pose.bones}
 return len(r.data.bones),int(a.frame_range[1]-a.frame_range[0])+1,res
delta=lambda A,B,names:max(abs(A[n][i][j]-B[n][i][j]) for n in names for i in range(4) for j in range(4))
report={'clips':{}};imp={}
for n,N in CLIPS.items():
 bones,samples,res=read(OUT/f'AN_PlayerV05_{n}.fbx',sorted({1,N//2,N}));imp[n]=res;names=sorted(res[1]);core=[k for k in names if '_03_' not in k]  # importer rebuilds fingertip leaf orientation
 rep={'bones':bones,'samples':samples,'seconds':round((samples-1)/30,4),'max_vs_source_blend':max(delta(res[f],source[n][f],core) for f in res),'fbx_sha256':hashlib.sha256((OUT/f'AN_PlayerV05_{n}.fbx').read_bytes()).hexdigest()}
 if n in('ToolHold','CrowbarPry'):rep['loop_seam_frame1_vs_last']=delta(res[1],res[N],names)
 if n=='Pickup':rep['last_frame_vs_hold_frame1']=delta(res[N],source['Hold'][1],core)
 report['clips'][n]=rep;assert bones==52 and samples==N and rep['max_vs_source_blend']<1e-4,(n,rep)
names=[k for k in sorted(imp['ToolHold'][1]) if '_03_' not in k];report['clips']['FuseInsert']['frame1_vs_toolhold_frame1']=delta(imp['FuseInsert'][1],imp['ToolHold'][1],names)
C=json.loads((OUT/'tool-hold-contract.json').read_text())['tools'];report['contract_reproduced_from_fbx_toolhold_frame1_cm']={}
for k,v in C.items():
 p=(imp['ToolHold'][1]['hand_l']@Matrix(v['hand_l_relative_matrix_blender'])).translation;loc=[(p.y+.2)*100,p.x*100,(p.z-1.1)*100]
 report['contract_reproduced_from_fbx_toolhold_frame1_cm'][k]=max(abs(a-b) for a,b in zip(loc,v['camera_location_cm']))
assert report['clips']['ToolHold']['loop_seam_frame1_vs_last']<1e-5 and report['clips']['CrowbarPry']['loop_seam_frame1_vs_last']<1e-5 and report['clips']['FuseInsert']['frame1_vs_toolhold_frame1']<1e-4 and report['clips']['Pickup']['last_frame_vs_hold_frame1']<1e-4,report
assert all(x<.05 for x in report['contract_reproduced_from_fbx_toolhold_frame1_cm'].values()),report
(OUT/'fbx-roundtrip-validation.json').write_text(json.dumps(report,indent=2));print('RESULT',json.dumps(report))
