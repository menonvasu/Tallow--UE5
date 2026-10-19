import bpy,json,math
from pathlib import Path
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'Tallow/SourceAssets/Player/InteractionV05';report={}
def clear():
 bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
 for a in list(bpy.data.actions):bpy.data.actions.remove(a)
clear()
bpy.ops.import_scene.fbx(filepath=str(OUT/'SK_PlayerV05_Full.fbx'))
meshes=[o for o in bpy.context.scene.objects if o.type=='MESH'];z=[(o.matrix_world@v.co).z for o in meshes for v in o.data.vertices];height=max(z)-min(z);assert 1.1<height<1.3,height;report['full_height_m']=height;report['clips']={}
for path in sorted(OUT.glob('AN_PlayerV05_*.fbx')):
 clear();bpy.ops.import_scene.fbx(filepath=str(path));r=next(o for o in bpy.context.scene.objects if o.type=='ARMATURE');a=r.animation_data.action;assert a
 start,end=a.frame_range;lengths=[];s=bpy.context.scene
 for frame in [start,(start+end)/2,end]:
  s.frame_set(int(frame));bpy.context.view_layer.update();b=r.pose.bones['lowerarm_r'];length=(r.matrix_world@b.tail-r.matrix_world@b.head).length;assert .10<length<.25,(path.name,length);lengths.append(length)
 report['clips'][path.stem]={'samples':int(end-start)+1,'forearm_lengths_m':lengths,'bones':len(r.data.bones)}
(OUT/'fbx-roundtrip-validation.json').write_text(json.dumps(report,indent=2));print('RESULT',json.dumps(report))
