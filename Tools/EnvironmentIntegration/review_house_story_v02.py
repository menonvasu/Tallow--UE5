import bpy,json,math
from pathlib import Path
from mathutils import Vector,Quaternion
R=Path(__file__).resolve().parents[2];O=R/'Tallow/SourceAssets/Production/HouseStoryV02';H=R/'Tallow/SourceAssets/Production/HouseIntegrationV01';bpy.ops.wm.open_mainfile(filepath=str(H/'HouseStagedReview.blend'));cfg=json.loads((O/'placement.json').read_text())
with bpy.data.libraries.load(str(O/'HouseStoryV02.blend'),link=False) as(a,b):b.objects=[n for n in a.objects if n.startswith('SM_Story_')]
source={o.name:o for o in b.objects}
for row in cfg['placements']:
 o=source[row['mesh']].copy();o.data=source[row['mesh']].data;bpy.context.scene.collection.objects.link(o);o.name='STORY_'+row['label'];x,y,z=row['location_cm'];o.location=(x/100,-y/100,z/100);o.scale=row['scale'];o.rotation_mode='QUATERNION';o.rotation_quaternion=Quaternion((0,0,1),math.radians(-row['yaw']))@Quaternion((0,1,0),math.radians(row['tilt']))
s=bpy.context.scene;s.render.engine='CYCLES';s.cycles.device='CPU';s.cycles.samples=24;s.cycles.use_denoising=True;s.render.threads_mode='FIXED';s.render.threads=4;s.render.resolution_x=1100;s.render.resolution_y=750;s.render.resolution_percentage=100;s.camera.data.lens=24;s.camera.data.clip_start=.03
(O/'Review').mkdir(exist_ok=True);bpy.ops.wm.save_as_mainfile(filepath=str(O/'HouseStoryStaged.blend'))
for name,loc,target in [('01_WakeAbsence',(3.6,4.8,-2.22),(4.15,6.6,-1.72)),('02_StairWitness',(8.8,4.65,1.49),(9.62,5.6,2.25)),('03_CollectionAbsence',(4.9,11.1,4.98),(1.25,10.5,5.45))]:
 s.camera.location=loc;s.camera.rotation_euler=(Vector(target)-s.camera.location).to_track_quat('-Z','Y').to_euler();s.render.filepath=str(O/'Review'/(name+'.png'));bpy.ops.render.render(write_still=True)
print('RESULT story source review complete')
