"""Close-ups of repaired spots at rest pose, HOOD_OPEN 0: mask forehead pinch, chin end of the hood split, torso/sleeve joins, wrists.
blender -b <blend> --python render_repair_closeups.py -- <tag>   (under flock /tmp/tallow-heavy-render.lock)"""
import bpy,sys,math
from pathlib import Path
from mathutils import Vector
R=Path(__file__).resolve().parents[3];OUT=R/'Tallow/SourceAssets/Keeper/V09Mouth/Review';tag=sys.argv[sys.argv.index('--')+1]
s=bpy.context.scene;r=bpy.data.objects['KeeperV07'];o=bpy.data.objects['SK_KeeperRiggedV07_Export'];r.data.pose_position='REST'
for k in o.data.shape_keys.key_blocks:k.value=0
s.render.engine='CYCLES';s.cycles.device='CPU';s.cycles.samples=24;s.render.threads_mode='FIXED';s.render.threads=4;s.render.resolution_x=640;s.render.resolution_y=640
if not s.world:s.world=bpy.data.worlds.new('W')
s.world.use_nodes=False;s.world.color=(.25,.25,.27);s.view_settings.view_transform='Standard'
for name,pos,power in [('Key',(-1.5,-2.5,3),300),('Fill',(2,-2,1.5),150)]:
 d=bpy.data.lights.new(name,'AREA');d.energy=power;d.size=2;ob=bpy.data.objects.new(name,d);s.collection.objects.link(ob);ob.location=pos;ob.rotation_euler=(Vector((0,0,1.6))-ob.location).to_track_quat('-Z','Y').to_euler()
cd=bpy.data.cameras.new('C');cam=bpy.data.objects.new('C',cd);s.collection.objects.link(cam);s.camera=cam;cd.lens=60
for label,tgt,dirv,dist in [('mask-pinch',(-.004,-.146,2.31),(0,-1,.1),.35),('chin-split',(-.04,-.07,1.99),(.1,-1,-.25),.35),('torso-join-R',(-.18,-.08,1.45),(-.4,-1,0),.7),('torso-join-L',(.12,-.06,1.45),(.4,-1,0),.7),('wrist',(-.557,-.312,1.661),(-.3,-1,.2),.45)]:
 d=Vector(dirv).normalized();cam.location=Vector(tgt)+d*dist;cam.rotation_euler=(-d).to_track_quat('-Z','Y').to_euler()
 s.render.filepath=str(OUT/f'repair-{tag}-closeup-{label}.png');bpy.ops.render.render(write_still=True)
print('RESULT ok')
