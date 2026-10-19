"""Before/after repair review: Cycles CPU, player-view cameras on baked game poses (armature + morph actions from the export file).
blender -b <blend> --python render_repair_review.py -- <before|after>   (run under flock /tmp/tallow-heavy-render.lock)"""
import bpy,sys,math
from pathlib import Path
from mathutils import Vector
R=Path(__file__).resolve().parents[3];OUT=R/'Tallow/SourceAssets/Keeper/V09Mouth/Review';OUT.mkdir(parents=True,exist_ok=True)
tag=sys.argv[sys.argv.index('--')+1];s=bpy.context.scene;r=bpy.data.objects['KeeperV07'];o=bpy.data.objects['SK_KeeperRiggedV07_Export']
s.render.engine='CYCLES';s.cycles.device='CPU';s.cycles.samples=24;s.render.threads_mode='FIXED';s.render.threads=4;s.render.resolution_x=900;s.render.resolution_y=900
if not s.world:s.world=bpy.data.worlds.new('W')
s.world.use_nodes=False;s.world.color=(.05,.05,.055);s.view_settings.view_transform='Standard'
bpy.ops.mesh.primitive_plane_add(size=40,location=(0,0,-.01));fl=bpy.context.object;m=bpy.data.materials.new('floor');m.use_nodes=True;m.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value=(.025,.022,.02,1);fl.data.materials.append(m)
for name,pos,power,size in [('Key',(-2,-3,4),450,3),('Fill',(2.5,-2,2),180,2),('Rim',(0,2.5,3),300,2)]:
 d=bpy.data.lights.new(name,'AREA');d.energy=power;d.size=size;ob=bpy.data.objects.new(name,d);s.collection.objects.link(ob);ob.location=pos;ob.rotation_euler=(Vector((0,0,.8))-ob.location).to_track_quat('-Z','Y').to_euler()
cd=bpy.data.cameras.new('Player');cam=bpy.data.objects.new('Player',cd);s.collection.objects.link(cam);s.camera=cam;cd.sensor_width=36;cd.lens=36/(2*math.tan(math.radians(19)))
shots=[('runchase','RunChase',7,(0.35,-2.4,1.65)),('chase','Chase',13,(-0.5,-2.3,1.65)),('wallcrawl','WallCrawl',17,(1.5,-2.0,1.2)),('ceiling-under','CeilingCrawl',25,(0.6,-1.6,0.35)),('capture','JumpscareDeath',40,(0.15,-1.75,1.15))]
for label,clip,frame,cl in shots:
 for idb,act in [(r,'EXPORT_AN_Keeper_'+clip),(o.data.shape_keys,'EXPORT_MORPH_'+clip)]:
  a=bpy.data.actions.get(act)
  if a:idb.animation_data_create();idb.animation_data.action=a;idb.animation_data.action_slot=a.slots[0]
 r.data.pose_position='POSE';s.frame_set(frame);bpy.context.view_layer.update()
 ev=o.evaluated_get(bpy.context.evaluated_depsgraph_get());pts=[o.matrix_world@ev.data.vertices[i].co for i in range(0,len(ev.data.vertices),25)]
 lo=Vector([min(p[k] for p in pts) for k in range(3)]);hi=Vector([max(p[k] for p in pts) for k in range(3)]);c=(lo+hi)/2;rad=(hi-lo).length/2
 d=(Vector(cl)-c).normalized();cam.location=c+d*rad/math.tan(math.radians(19))*1.02;cam.rotation_euler=(-d).to_track_quat('-Z','Y').to_euler()
 s.render.filepath=str(OUT/f'repair-{tag}-{label}.png');bpy.ops.render.render(write_still=True);print('RENDER '+label,flush=True)
print('RESULT ok')
