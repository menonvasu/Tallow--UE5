import bpy,json,os
import numpy as np
from pathlib import Path
from mathutils import Vector,Matrix
# Open ReachRevision01/10_reach-rev01.blend. In-game camera stills through the reach, a 3/4 view of the grab, and a contact sheet.
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'Tallow/SourceAssets/Player/InteractionV05/ReachRevision01';s=bpy.context.scene;r=bpy.data.objects['RIG_Tallow_Generated'];part=bpy.data.objects['SK_Tallow_Generated_Partial'];bpy.data.objects['SK_Tallow_Generated_Full'].hide_render=True;part.hide_render=False
PCT=int(os.environ.get('REVIEW_PCT','100'));FRAMES=[int(f) for f in os.environ.get('REVIEW_FRAMES','1,8,11,13,15,20,25').split(',')];DIR=Path(os.environ.get('REVIEW_DIR',str(OUT)));DIR.mkdir(parents=True,exist_ok=True)
a=bpy.data.actions['AN_PlayerV05_Reach'];r.animation_data.action=a;r.animation_data.action_slot=a.slots[0]
s.render.engine='CYCLES';s.cycles.device='CPU';s.cycles.samples=12;s.render.threads_mode='FIXED';s.render.threads=4;s.render.resolution_x=1280;s.render.resolution_y=720;s.render.resolution_percentage=PCT
cam=s.camera;shots=[]
for f in FRAMES:
 s.frame_set(f);cam.data.lens=16;cam.location=(0,-.20,1.10);cam.rotation_euler=Vector((0,1,0)).to_track_quat('-Z','Y').to_euler();s.render.filepath=str(DIR/f'FINAL-FP-Reach-f{f:02d}.png');bpy.ops.render.render(write_still=True);shots.append(s.render.filepath)
# 3/4 of the grab from in front, left and a little above, looking back at the closed hand.
s.frame_set(15);bpy.context.view_layer.update();tgt=r.pose.bones['middle_02_l'].head.copy();eye=tgt+Vector((-.25,.3,.18));f=(tgt-eye).normalized();x=f.cross(Vector((0,0,1))).normalized();cam.matrix_world=Matrix.Translation(eye)@Matrix((x,x.cross(f),-f)).transposed().to_4x4();cam.data.lens=40
s.render.filepath=str(DIR/'FINAL-34-Reach-f15.png');bpy.ops.render.render(write_still=True)
ims=[bpy.data.images.load(p) for p in shots];w,h=ims[0].size;px=[np.array(im.pixels[:],dtype=np.float32).reshape(h,w,4)[::2,::2] for im in ims];cols=4;rows=(len(px)+cols-1)//cols;sheet=np.zeros((rows*(h//2),cols*(w//2),4),np.float32);sheet[...,3]=1
for i,p in enumerate(px):R0=(rows-1-i//cols)*(h//2);sheet[R0:R0+p.shape[0],(i%cols)*(w//2):(i%cols)*(w//2)+p.shape[1]]=p
out=bpy.data.images.new('sheet',cols*(w//2),rows*(h//2),alpha=True);out.pixels.foreach_set(sheet.ravel());out.filepath_raw=str(DIR/'FINAL-FP-Reach-sheet.png');out.file_format='PNG';out.save()
print('RESULT',json.dumps({'frames':FRAMES,'dir':str(DIR)}))
