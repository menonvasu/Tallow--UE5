"""Small CPU PBR pose/face reviews. Run under shared heavy-render flock."""
import bpy,json
from pathlib import Path
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[3];OUT=ROOT/'Tallow/SourceAssets/Keeper/RiggedV07';bpy.ops.wm.open_mainfile(filepath=str(OUT/'keeper-v07-animated.blend'));s=bpy.context.scene;r=bpy.data.objects['RIG_KeeperV07'];o=bpy.data.objects['SK_KeeperRiggedV07']
s.render.engine='CYCLES';s.cycles.samples=12;s.cycles.device='CPU';s.render.threads_mode='FIXED';s.render.threads=4;s.render.resolution_x=720;s.render.resolution_y=820;s.render.resolution_percentage=100
s.world.color=(.06,.06,.06)
bpy.ops.mesh.primitive_plane_add(size=200,location=(0,0,-.012));floor=bpy.context.object;floor.name='REVIEW_ONLY_FLOOR';m=bpy.data.materials.new('Review floor');m.diffuse_color=(.075,.07,.065,1);floor.data.materials.append(m)
for name,pos,power,size in [('Key',(-2,-3,4),400,3),('Fill',(2,-2,2),160,2),('Rim',(0,2,3),300,2)]:
 d=bpy.data.lights.new(name,'AREA');d.energy=power;d.size=size;ob=bpy.data.objects.new(name,d);s.collection.objects.link(ob);ob.location=pos;ob.rotation_euler=(Vector((0,0,1.1))-ob.location).to_track_quat('-Z','Y').to_euler()
camd=bpy.data.cameras.new('ReviewCamera');cam=bpy.data.objects.new('ReviewCamera',camd);s.collection.objects.link(cam);s.camera=cam;camd.type='ORTHO'
reviews=[('Idle',1,'host'),('Reveal',73,'unfold'),('RunChase',7,'chase'),('WallCrawl',17,'wall'),('CeilingCrawl',25,'ceiling'),('JumpscareDeath',43,'capture'),('JumpscareDeath',43,'mouth-close'),('Idle',1,'head-closed')]
for name,frame,label in reviews:
 act=bpy.data.actions['AN_Keeper_'+name];r.animation_data.action=act;r.animation_data.action_slot=act.slots[0];s.frame_set(frame);r.update_tag();bpy.context.view_layer.update()
 if label in ['head-closed','mouth-close']:
  target=r.pose.bones['head'].matrix.translation+Vector((0,-.05,.08));cam.data.ortho_scale=.65;cam.location=target+Vector((.3,-2,.10))
 else:
  target=Vector((-.04,0,1.17 if label=='host' else .58));cam.data.ortho_scale=2.75 if label=='host' else 1.85;cam.location=(1.7,-4,1.65 if label=='host' else 1.1)
 cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler();s.render.filepath=str(OUT/('review-'+label+'.png'));bpy.ops.render.render(write_still=True)
 print('REVIEW '+label,flush=True)
print('RESULT '+json.dumps({'renders':len(reviews),'rendered_source':'keeper-v07-animated.blend'}))
# Actual evaluated bounding poses, not schematic arrows: all six limbs and floor shadows.
original=obj if 'obj' in globals() else o
frozen=[]
for i,frame in enumerate([1,3,5,7,9,11,13]):
 act=bpy.data.actions['AN_Keeper_RunChase'];r.animation_data.action=act;r.animation_data.action_slot=act.slots[0];s.frame_set(frame);r.update_tag();bpy.context.view_layer.update();dg=bpy.context.evaluated_depsgraph_get();me=bpy.data.meshes.new_from_object(original.evaluated_get(dg),preserve_all_data_layers=True,depsgraph=dg);ob=bpy.data.objects.new('Pose_'+str(frame),me);s.collection.objects.link(ob);ob.location.x=i*1.45;frozen.append(ob)
original.hide_render=True
for light in s.objects:
 if light.type=='LIGHT':light.location.x+=4.35;light.data.size=10;light.data.energy*=5
s.render.resolution_x=1800;s.render.resolution_y=500;cam.location=(4.35,-12,3.2);target=Vector((4.35,0,.65));cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.ortho_scale=10.6;s.render.filepath=str(OUT/'review-run-bound-strip.png');bpy.ops.render.render(write_still=True)
print('RESULT bounding-strip frames1,3,5,7,9,11,13; source0.5sec; suggested320cm/s at1.4286x')
