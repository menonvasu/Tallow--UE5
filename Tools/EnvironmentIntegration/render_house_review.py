import bpy,json,math,os
from pathlib import Path
from mathutils import Vector
R=Path(__file__).resolve().parents[2];O=R/'Tallow/SourceAssets/Production/HouseIntegrationV01';bpy.ops.wm.open_mainfile(filepath=str(O/'HouseArchitecture.blend'));cfg=json.loads((O/'dressing.json').read_text());cache={}
for o in bpy.context.scene.objects:
 if 'GateDoor_' in o.name or 'GateFrame_' in o.name or 'SplinteredBoard' in o.name:o.hide_render=True
for row in cfg['props']:
 name=row['asset'].split('/')[-1]
 if name not in cache:
  if name.startswith('SM_House_'):p=O/'Furniture'/(name.removeprefix('SM_House_')+'.blend');obj=name
  elif 'Cabinet' in name:p=R/'Tallow/SourceAssets/Production/CabinetFinished/Cabinet_SageTimber.blend';obj='SM_Cabinet_Production'
  elif 'RestraintChair' in name:p=R/'Tallow/SourceAssets/Production/RestraintChair/RestraintChair_Production.blend';obj='SM_RestraintChair'
  else:p=R/'Tallow/SourceAssets/Production/CollectionDoll/CollectionDoll_Production.blend';obj='SM_CollectionDoll'
  with bpy.data.libraries.load(str(p),link=False) as(a,b):b.objects=[obj]
  source=b.objects[0];cache[name]=source
 source=cache[name];o=source.copy();o.data=source.data;bpy.context.scene.collection.objects.link(o);o.name=row['label'];x,y,z=row['location'];o.location=(x/100,-y/100,z/100);o.rotation_mode='XYZ';o.rotation_euler=(0,0,math.radians(-row['yaw']));o.scale=row['scale']
for row in cfg['lights']:
 d=bpy.data.lights.new(row['label'],'POINT');d.energy=row['lumens']*1.5;d.color=row['color'];d.shadow_soft_size=.35;o=bpy.data.objects.new(row['label'],d);bpy.context.scene.collection.objects.link(o);x,y,z=row['location'];o.location=(x/100,-y/100,z/100)
s=bpy.context.scene;s.render.engine='CYCLES';s.cycles.device='CPU';s.cycles.samples=24;s.cycles.use_denoising=True;s.render.threads_mode='FIXED';s.render.threads=4;s.render.resolution_x=1000;s.render.resolution_y=750;s.render.resolution_percentage=100;s.world.color=(.035,.035,.035);s.view_settings.view_transform='AgX';s.view_settings.exposure=1
cam=bpy.data.cameras.new('GameplayReviewCamera');ob=bpy.data.objects.new('GameplayReviewCamera',cam);s.collection.objects.link(ob);s.camera=ob;cam.lens=22;cam.clip_start=.04
views=[('01_WakeRoute',(3.8,4.4,-2.2),(1.6,5.6,-2.0)),('02_StairLandmark',(8.0,2.9,1.2),(7.7,6.5,2.8)),('03_Dining',(5.1,8.0,1.2),(2.5,10.4,1.0)),('04_CollectionReveal',(4.9,11.1,5.1),(1.7,10.0,4.9))]
(O/'Review').mkdir(exist_ok=True);bpy.ops.wm.save_as_mainfile(filepath=str(O/'HouseStagedReview.blend'))
for name,loc,target in views:
 if os.environ.get('TALLOW_REVIEW_VIEWS') and name not in os.environ['TALLOW_REVIEW_VIEWS'].split(','):continue
 ob.location=loc;ob.rotation_euler=(Vector(target)-ob.location).to_track_quat('-Z','Y').to_euler();s.render.filepath=str(O/'Review'/(name+'.png'));bpy.ops.render.render(write_still=True)
print('RESULT house source review renders finished')
