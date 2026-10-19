import bpy,json,math,os
import numpy as np
from pathlib import Path
from mathutils import Vector,Matrix
# Open LocomotionRevision01/09_locomotion-rev01.blend. In-game camera stills of Walk / Sprint / Crouch (plus Hold for reference), a sheet per clip
# whose last cell traces the wick (orange) and wrist (blue) over the whole loop on frame 1. Review-only flame: emissive cone rigid on the wick.
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'Tallow/SourceAssets/Player/InteractionV05/LocomotionRevision01';s=bpy.context.scene;r=bpy.data.objects['RIG_Tallow_Generated'];c=bpy.data.objects['SM_CandleV02_Crooked'];part=bpy.data.objects['SK_Tallow_Generated_Partial'];full=bpy.data.objects['SK_Tallow_Generated_Full'];full.hide_render=True;part.hide_render=False
PCT=int(os.environ.get('REVIEW_PCT','100'));DIR=Path(os.environ.get('REVIEW_DIR',str(OUT)));DIR.mkdir(parents=True,exist_ok=True);ONLY=os.environ.get('REVIEW_CLIPS','Walk,Sprint,Crouch').split(',')
FRAMES={'Walk':[1,6,11,16,21,26],'Sprint':[1,4,7,10,13,16],'Crouch':[1,11,21,31,41,51]}
for k,v in json.loads(os.environ.get('REVIEW_FRAMES','{}')).items():FRAMES[k]=v
CAM=Vector((0,-.20,1.10))
bpy.ops.mesh.primitive_cone_add(vertices=24,radius1=.0055,radius2=0,depth=.026);fl=bpy.context.object;fl.name='REVIEW_CandleFlame';bpy.ops.object.shade_smooth()
mat=bpy.data.materials.new('REVIEW_CandleFlame');mat.use_nodes=True;nt=mat.node_tree;nt.nodes.clear();em=nt.nodes.new('ShaderNodeEmission');em.inputs['Color'].default_value=(1,.55,.16,1);em.inputs['Strength'].default_value=6;o=nt.nodes.new('ShaderNodeOutputMaterial');nt.links.new(em.outputs[0],o.inputs[0]);fl.data.materials.append(mat)
fl.parent=c;fl.matrix_parent_inverse=Matrix();fl.location=(.004,0,.255+.013+.002)  # rides the wick with the candle, as the game's flame visual does
s.render.engine='CYCLES';s.cycles.device='CPU';s.cycles.samples=12;s.render.threads_mode='FIXED';s.render.threads=4;s.render.resolution_x=1280;s.render.resolution_y=720;s.render.resolution_percentage=PCT
cam=s.camera;cam.data.lens=16;cam.data.sensor_width=32;cam.data.sensor_fit='AUTO';cam.location=CAM;cam.rotation_euler=Vector((0,1,0)).to_track_quat('-Z','Y').to_euler()  # 90 deg horizontal = game FP FOV
def pose(n,f):
 a=bpy.data.actions['AN_PlayerV05_'+n];r.animation_data.action=a;r.animation_data.action_slot=a.slots[0];s.frame_set(f);bpy.context.view_layer.update()
def proj(p,w,h):v=p-CAM;return int(w/2+v.x/v.y*w/2),int(h/2+v.z/v.y/.5625*h/2)  # bottom-up pixel coords
def load(p):im=bpy.data.images.load(str(p));w,h=im.size;return np.array(im.pixels[:],dtype=np.float32).reshape(h,w,4)
shots={}
for n in ['Hold']+ONLY:
 for f in ([1] if n=='Hold' else FRAMES[n]):
  pose(n,f);s.render.filepath=str(DIR/f'FINAL-FP-{n}-f{f:02d}.png');bpy.ops.render.render(write_still=True);shots.setdefault(n,[]).append(s.render.filepath)
for n in ONLY:
 px=[load(p) for p in shots[n]];h,w=px[0].shape[:2];tr=px[0].copy();N=int(bpy.data.actions['AN_PlayerV05_'+n].frame_range[1])
 for f in range(1,N):
  pose(n,f)
  for p,col in((c.matrix_world@Vector((.004,0,.255)),(1,.55,.1,1)),(r.pose.bones['hand_r'].head,(.2,.5,1,1))):
   x,y=proj(p,w,h)
   if 3<=x<w-3 and 3<=y<h-3:tr[y-3:y+4,x-3:x+4]=col
 cells=[p[::-1][::2,::2] for p in px+[tr]];cols=3;rows=(len(cells)+cols-1)//cols;ch,cw=cells[0].shape[:2];sheet=np.zeros((rows*ch,cols*cw,4),np.float32);sheet[...,3]=1
 for i,p in enumerate(cells):sheet[(i//cols)*ch:(i//cols)*ch+ch,(i%cols)*cw:(i%cols)*cw+cw]=p
 out=bpy.data.images.new('sheet',cols*cw,rows*ch,alpha=True);out.pixels.foreach_set(sheet[::-1].ravel());out.filepath_raw=str(DIR/f'SHEET-{n}.png');out.file_format='PNG';out.save()
print('RESULT',json.dumps({n:[Path(p).name for p in v] for n,v in shots.items()}),flush=True)
