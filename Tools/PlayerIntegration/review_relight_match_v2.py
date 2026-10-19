import bpy,json,math,os
import numpy as np
from pathlib import Path
from mathutils import Vector,Matrix
# Open RelightMatchRevision02/08_relight-match-v2.blend. Writes the match contract (review_relight_sleeve.py schema/formula), attaches the match to hand_l at 19/30 s like the game, renders review stills.
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'Tallow/SourceAssets/Player/InteractionV05/RelightMatchRevision02';s=bpy.context.scene;r=bpy.data.objects['RIG_Tallow_Generated'];c=bpy.data.objects['SM_CandleV02_Crooked'];part=bpy.data.objects['SK_Tallow_Generated_Partial'];full=bpy.data.objects['SK_Tallow_Generated_Full'];full.hide_render=True;part.hide_render=False
PCT=int(os.environ.get('REVIEW_PCT','100'));FRAMES=[int(f) for f in os.environ.get('REVIEW_FRAMES','1,8,14,20,26,32').split(',')];DIR=Path(os.environ.get('REVIEW_DIR',str(OUT)))
with bpy.data.libraries.load(str(ROOT/'Tallow/SourceAssets/Production/ToolProps/ToolProps_Production.blend'),link=False) as(src,dst):dst.objects=['SM_ToolV02_Crowbar','SM_ToolV02_Fuse','SM_ToolV02_Match']
for o in dst.objects:s.collection.objects.link(o);o.hide_render=True
report={'tools':{}}
for ob in dst.objects:
 report['tools'][ob.name]={'bounds_cm':[[round(v*100,5) for v in xyz] for xyz in [tuple(min(v.co[i] for v in ob.data.vertices) for i in range(3)),tuple(max(v.co[i] for v in ob.data.vertices) for i in range(3))]]}
def pose(n,f):
 a=bpy.data.actions['AN_PlayerV05_'+n];r.animation_data.action=a;r.animation_data.action_slot=a.slots[0];s.frame_set(f);bpy.context.view_layer.update()
pose('RelightMatch',20);index=r.pose.bones['index_03_l'].tail.copy();thumb=r.pose.bones['thumb_03_l'].tail.copy();pinch=(index+thumb)/2;wick=c.matrix_world@Vector((.004,0,.255));match=dst.objects[2];match.location=pinch;match.rotation_euler=(wick-pinch).to_track_quat('Z','Y').to_euler();bpy.context.view_layer.update()
conv=Matrix(((0,1,0),(1,0,0),(0,0,1)));asset=Matrix.Diagonal((1,-1,1));rotation=conv@match.matrix_world.to_3x3()@asset;q=rotation.to_quaternion();report['match']={'calibrate_clip':'RelightMatch','time_seconds':19/30,'camera_location_cm':[(pinch.y+.2)*100,pinch.x*100,(pinch.z-1.1)*100],'camera_quaternion_xyzw':[q.x,q.y,q.z,q.w],'pinch_tip_gap_cm':(index-thumb).length*100,'match_tip_to_wick_cm':(match.matrix_world@Vector((0,0,.041))-wick).length*100}
report['match']['accepted_contact']=report['match']['pinch_tip_gap_cm']<.35 and report['match']['match_tip_to_wick_cm']<.6
assert report['match']['accepted_contact'],report
if DIR==OUT:(OUT/'tool-match-contract.json').write_text(json.dumps(report,indent=2))
# KeepWorld attach to hand_l at the calibration pose, as the game does.
con=match.constraints.new('CHILD_OF');con.target=r;con.subtarget='hand_l';con.inverse_matrix=(r.matrix_world@r.pose.bones['hand_l'].matrix).inverted();match.hide_render=False
# Review-only flame on the lit match (shaken out by the flick).
bpy.ops.mesh.primitive_cone_add(vertices=16,radius1=.0042,radius2=0,depth=.017);flame=bpy.context.object;flame.name='REVIEW_MatchFlame';bpy.ops.object.shade_smooth()
mat=bpy.data.materials.new('REVIEW_Flame');mat.use_nodes=True;nt=mat.node_tree;nt.nodes.clear();em=nt.nodes.new('ShaderNodeEmission');em.inputs['Color'].default_value=(1,.55,.15,1);em.inputs['Strength'].default_value=25;o=nt.nodes.new('ShaderNodeOutputMaterial');nt.links.new(em.outputs[0],o.inputs[0]);flame.data.materials.append(mat)
def place_flame(f):
 bpy.context.view_layer.update();tip=match.matrix_world@Vector((0,0,.041));flame.location=tip+Vector((0,0,.0065));flame.scale=(1,1,1);flame.hide_render=f>27
s.render.engine='CYCLES';s.cycles.device='CPU';s.cycles.samples=12;s.render.threads_mode='FIXED';s.render.threads=4;s.render.resolution_x=1280;s.render.resolution_y=720;s.render.resolution_percentage=PCT
cam=s.camera;cam.data.lens=16;cam.location=(0,-.20,1.10);cam.rotation_euler=Vector((0,1,0)).to_track_quat('-Z','Y').to_euler();shots=[]
for f in FRAMES:
 pose('RelightMatch',f);place_flame(f);s.render.filepath=str(DIR/f'FINAL-FP-RelightMatch-f{f:02d}.png');bpy.ops.render.render(write_still=True);shots.append(s.render.filepath)
# 3/4 grip view at the calibration frame: in front of the hand, left and above, looking back at the pinch.
pose('RelightMatch',20);place_flame(20);tgt=(pinch+r.pose.bones['hand_l'].tail)/2;eye=tgt+Vector((-.25,1,.35)).normalized()*.3;fw=(tgt-eye).normalized();x=fw.cross(Vector((0,0,1))).normalized();cam.matrix_world=Matrix.Translation(eye)@Matrix((x,x.cross(fw),-fw)).transposed().to_4x4();cam.data.lens=45
s.render.filepath=str(DIR/'FINAL-34-RelightMatch-f20.png');bpy.ops.render.render(write_still=True)
# contact sheet of the in-game frames (3 per row, half size)
ims=[bpy.data.images.load(p) for p in shots];w,h=ims[0].size;px=[np.array(im.pixels[:],dtype=np.float32).reshape(h,w,4)[::2,::2] for im in ims];cols=3;rows=(len(px)+cols-1)//cols;sheet=np.zeros((rows*(h//2),cols*(w//2),4),np.float32);sheet[...,3]=1
for i,p in enumerate(px):R0=(rows-1-i//cols)*(h//2);sheet[R0:R0+p.shape[0],(i%cols)*(w//2):(i%cols)*(w//2)+p.shape[1]]=p
out=bpy.data.images.new('sheet',cols*(w//2),rows*(h//2),alpha=True);out.pixels.foreach_set(sheet.ravel());out.filepath_raw=str(DIR/'FINAL-FP-RelightMatch-sheet.png');out.file_format='PNG';out.save()
print('RESULT',json.dumps(report['match']))
