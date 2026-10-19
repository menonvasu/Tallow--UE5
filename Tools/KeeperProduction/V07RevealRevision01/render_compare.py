import bpy,json
from pathlib import Path
from mathutils import Vector
R=Path(__file__).resolve().parents[3];O=R/'Tallow/SourceAssets/Keeper/V07RevealRevision01';report=[]
for mode in ['fbx']:
 if mode=='source':
  bpy.ops.wm.open_mainfile(filepath=str(O/'keeper-reveal-revision01-animated.blend'));r=bpy.data.objects['RIG_KeeperV07'];o=bpy.data.objects['SK_KeeperRiggedV07'];act=bpy.data.actions['AN_Keeper_JumpscareDeath'];r.animation_data.action=act;r.animation_data.action_slot=act.slots[0]
 else:
  bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=str(O/'Export/AN_Keeper_JumpscareDeath_Revision01.fbx'));r=next(o for o in bpy.data.objects if o.type=='ARMATURE');o=next(o for o in bpy.data.objects if o.type=='MESH');unit=bpy.data.objects.new('FBX_REVIEW_UNIT_ONLY',None);bpy.context.scene.collection.objects.link(unit);r.parent=unit;unit.scale=(.01,.01,.01)
  names=[m.name for m in o.data.materials]
  with bpy.data.libraries.load(str(O/'keeper-reveal-revision01-export.blend'),link=False) as (src,dst):dst.materials=[n for n in src.materials if n in names]
  loaded={m.name.split('.00')[0]:m for m in dst.materials}
  for i,name in enumerate(names):
   if name in loaded:o.data.materials[i]=loaded[name]
 s=bpy.context.scene;s.frame_set(34,subframe=.6);bpy.context.view_layer.update()
 s.render.engine='CYCLES';s.cycles.samples=16;s.cycles.device='CPU';s.render.threads_mode='FIXED';s.render.threads=4;s.render.resolution_x=760;s.render.resolution_y=760;s.render.resolution_percentage=100
 if not s.world:s.world=bpy.data.worlds.new('World')
 s.world.color=(.055,.055,.055)
 for name,pos,power,size in [('Key',(-2,-3,4),400,3),('Fill',(2,-2,2),160,2)]:
  d=bpy.data.lights.new(name,'AREA');d.energy=power;d.size=size;ob=bpy.data.objects.new(name,d);s.collection.objects.link(ob);ob.location=pos;ob.rotation_euler=(Vector((0,0,1.1))-ob.location).to_track_quat('-Z','Y').to_euler()
 camd=bpy.data.cameras.new('ReviewCamera');cam=bpy.data.objects.new('ReviewCamera',camd);s.collection.objects.link(cam);s.camera=cam;camd.type='ORTHO';camd.ortho_scale=.8
 target=r.matrix_world@r.pose.bones['head'].matrix.translation+Vector((0,-.05,.08));cam.location=target+Vector((.12,-2,.1));cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler();s.render.filepath=str(O/f'{mode}-capture-t1p4.png');bpy.ops.render.render(write_still=True)
 report.append({'mode':mode,'time_seconds':1.4,'frame':34.6,'bones':len(r.data.bones),'morphs':{k.name:k.value for k in o.data.shape_keys.key_blocks}})
(O/'render-comparison.json').write_text(json.dumps(report,indent=2));print('RESULT '+json.dumps(report))
