"""Render actual evaluated source poses with floor/contact shadows; CPU four threads."""
import bpy
from pathlib import Path
from mathutils import Vector
OUT=Path(__file__).resolve().parents[2]/'Tallow/SourceAssets/Keeper/SleevedV06';bpy.ops.wm.open_mainfile(filepath=str(OUT/'keeper-direct-animated.blend'));s=bpy.context.scene;r=bpy.data.objects['KEEPER_PRODUCTION_RIG'];o=bpy.data.objects['SK_Keeper_Tripo_Direct']
s.render.engine='CYCLES';s.cycles.samples=20;s.cycles.device='CPU';s.render.threads_mode='FIXED';s.render.threads=4;s.render.resolution_x=850;s.render.resolution_y=900;s.render.resolution_percentage=100
for ob in list(s.objects):
 if ob.type=='LIGHT':bpy.data.objects.remove(ob,do_unlink=True)
s.world.color=(.08,.08,.08)
bpy.ops.mesh.primitive_plane_add(size=200,location=(0,0,-.012));floor=bpy.context.object;floor.name='REVIEW_ONLY_FLOOR';m=bpy.data.materials.new('Review floor');m.diffuse_color=(.08,.075,.07,1);floor.data.materials.append(m)
lights=[]
for name,pos,power,size in [('Key',(-2,-3,4),400,3),('Fill',(2,-2,2),160,2),('Rim',(0,2,3),300,2)]:
 d=bpy.data.lights.new(name,'AREA');d.energy=power;d.size=size;ob=bpy.data.objects.new(name,d);s.collection.objects.link(ob);ob.location=pos;ob.rotation_euler=(Vector((0,0,1.1))-ob.location).to_track_quat('-Z','Y').to_euler();lights.append(ob)
cam=s.camera
for name,frame in [('Idle',1),('Reveal',73),('Chase',7),('JumpscareDeath',19)]:
 a=bpy.data.actions['AN_Keeper_'+name];k=bpy.data.actions['MORPH_Keeper_'+name];r.animation_data.action=a;r.animation_data.action_slot=a.slots[0];o.data.shape_keys.animation_data.action=k;o.data.shape_keys.animation_data.action_slot=k.slots[0];s.frame_set(frame)
 height=1.17 if name=='Idle' else .52;cam.data.ortho_scale=2.75 if name=='Idle' else 1.85;cam.location=(1.8,-4,1.6 if name=='Idle' else 1.1);cam.rotation_euler=(Vector((-.04,0,height))-cam.location).to_track_quat('-Z','Y').to_euler()
 s.render.filepath=str(OUT/('review-'+name+'.png'));bpy.ops.render.render(write_still=True)
# Actual capture pose in warm candlelight; no gameplay lights modified.
for light in lights:light.data.energy=0
s.world.color=(.005,.007,.012);lights[0].location=(-.6,-1,.7);lights[0].data.energy=70;lights[0].data.color=(1,.40,.12);lights[0].data.size=.16;lights[0].rotation_euler=(Vector((0,0,.55))-lights[0].location).to_track_quat('-Z','Y').to_euler();lights[2].data.energy=65;lights[2].data.color=(.20,.30,.55)
s.render.filepath=str(OUT/'review-capture-candlelight.png');bpy.ops.render.render(write_still=True)
