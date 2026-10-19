import bpy,json,math,os
import numpy as np
from pathlib import Path
from mathutils import Vector,Matrix
# Open CupShieldRevision02/09_cupshield-rev02.blend (or FixedCandleVariant/09_cupshield-rev02.blend with REVIEW_DIR set to that folder). Review stills with a review-only flame (3 cm tall, .9 cm wide, as TallowFlameVisualComponent) at the wick.
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'Tallow/SourceAssets/Player/InteractionV05/CupShieldRevision02';s=bpy.context.scene;r=bpy.data.objects['RIG_Tallow_Generated'];c=bpy.data.objects['SM_CandleV02_Crooked'];part=bpy.data.objects['SK_Tallow_Generated_Partial'];bpy.data.objects['SK_Tallow_Generated_Full'].hide_render=True;part.hide_render=False
PCT=int(os.environ.get('REVIEW_PCT','100'));FRAMES=[int(f) for f in os.environ.get('REVIEW_FRAMES','1,21,41').split(',')];DIR=Path(os.environ.get('REVIEW_DIR',str(OUT)));DIR.mkdir(parents=True,exist_ok=True)
a=bpy.data.actions['AN_PlayerV05_CupShield'];r.animation_data.action=a;r.animation_data.action_slot=a.slots[0];s.frame_set(1);bpy.context.view_layer.update()  # the candle (Child Of hand_r) is static through the loop
wick=c.matrix_world@Vector((.004,0,.255));bpy.ops.mesh.primitive_cone_add(vertices=20,radius1=.0045,radius2=0,depth=.03,location=wick+Vector((0,0,.015)));flame=bpy.context.object;flame.name='REVIEW_Flame';bpy.ops.object.shade_smooth()
mat=bpy.data.materials.new('REVIEW_Flame');mat.use_nodes=True;nt=mat.node_tree;nt.nodes.clear();em=nt.nodes.new('ShaderNodeEmission');em.inputs['Color'].default_value=(1,.55,.15,1);em.inputs['Strength'].default_value=25;o=nt.nodes.new('ShaderNodeOutputMaterial');nt.links.new(em.outputs[0],o.inputs[0]);flame.data.materials.append(mat)
light=bpy.data.lights.new('REVIEW_FlameLight','POINT');light.energy=.6;light.color=(1,.6,.3);light.shadow_soft_size=.005;lo=bpy.data.objects.new('REVIEW_FlameLight',light);s.collection.objects.link(lo);lo.location=wick+Vector((0,0,.012))
s.render.engine='CYCLES';s.cycles.device='CPU';s.cycles.samples=16;s.render.threads_mode='FIXED';s.render.threads=4;s.render.resolution_x=1280;s.render.resolution_y=720;s.render.resolution_percentage=PCT
cam=s.camera;shots=[]
def look(eye,tgt,up=Vector((0,0,1))):
 f=(tgt-eye).normalized();x=f.cross(up).normalized();cam.matrix_world=Matrix.Translation(eye)@Matrix((x,x.cross(f),-f)).transposed().to_4x4()
for f in FRAMES:
 s.frame_set(f);cam.data.lens=16;cam.location=(0,-.20,1.10);cam.rotation_euler=Vector((0,1,0)).to_track_quat('-Z','Y').to_euler();s.render.filepath=str(DIR/f'FINAL-FP-CupShield-f{f:02d}.png');bpy.ops.render.render(write_still=True);shots.append(s.render.filepath)
# 3/4 from the open side (front-right of the flame) looking into the cup, and straight down to show the C-wall around the flame.
s.frame_set(1);fc=wick+Vector((0,0,.015));cam.data.lens=40;look(fc+Vector((.2,-.24,.1)),fc+Vector((-.03,.01,-.01)));s.render.filepath=str(DIR/'FINAL-34-CupShield-f01.png');bpy.ops.render.render(write_still=True)
pc=r.pose.bones['hand_l'].head.lerp(r.pose.bones['middle_01_l'].head,.6);tg=(fc+pc)/2;tg.z=fc.z;look(tg+Vector((0,0,.26)),tg,Vector((0,1,0)));cam.data.lens=40;s.render.filepath=str(DIR/'FINAL-TOP-CupShield-f01.png');bpy.ops.render.render(write_still=True)
ims=[bpy.data.images.load(p) for p in shots];w,h=ims[0].size;px=[np.array(im.pixels[:],dtype=np.float32).reshape(h,w,4)[::2,::2] for im in ims];sheet=np.concatenate(px,axis=1)
out=bpy.data.images.new('sheet',sheet.shape[1],sheet.shape[0],alpha=True);out.pixels.foreach_set(sheet.ravel());out.filepath_raw=str(DIR/'FINAL-FP-CupShield-sheet.png');out.file_format='PNG';out.save()
print('RESULT',json.dumps({'frames':FRAMES,'dir':str(DIR)}))
