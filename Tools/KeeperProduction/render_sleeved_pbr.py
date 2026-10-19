import bpy,sys
from pathlib import Path
from mathutils import Vector
OUT=Path(__file__).resolve().parents[2]/'Tallow/SourceAssets/Keeper/SleevedV06'
for filename,label in [('keeper-tripo-source-normalized.blend','original'),('keeper-tripo-production-pbr.blend','rebaked')]:
 bpy.ops.wm.open_mainfile(filepath=str(OUT/filename));s=bpy.context.scene;s.render.engine='CYCLES';s.cycles.samples=12;s.cycles.device='CPU';s.render.threads_mode='FIXED';s.render.threads=4;s.render.resolution_x=700;s.render.resolution_y=900;s.render.resolution_percentage=100
 s.world.color=(.18,.18,.18)
 for name,pos,power,size in [('Key',(-2,-3,4),450,3),('Fill',(2,-2,2),220,2),('Rim',(0,2,3),500,2)]:
  d=bpy.data.lights.new(name,'AREA');d.energy=power;d.shape='DISK';d.size=size;o=bpy.data.objects.new(name,d);s.collection.objects.link(o);o.location=pos;o.rotation_euler=(Vector((0,0,1.2))-o.location).to_track_quat('-Z','Y').to_euler()
 d=bpy.data.cameras.new('Review');c=bpy.data.objects.new('Review',d);s.collection.objects.link(c);s.camera=c;d.type='ORTHO';d.ortho_scale=2.8;c.location=(1.3,-5,1.5);c.rotation_euler=(Vector((0,0,1.2))-c.location).to_track_quat('-Z','Y').to_euler();s.render.filepath=str(OUT/('pbr-'+label+'.png'));bpy.ops.render.render(write_still=True)
