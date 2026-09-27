"""Visual review of the exported mesh against its original authored poses."""
import bpy,json,os
from mathutils import Matrix,Vector
s=bpy.context.scene;rig=bpy.data.objects['KeeperSkeleton'];mesh=bpy.data.objects['SK_Keeper'];rig.data.pose_position='POSE';rest={b.name:b.matrix_local.copy() for b in rig.data.bones}
for o in s.objects:o.hide_render=o not in [mesh,rig]
cam=bpy.data.objects['ExportReviewCamera'];s.camera=cam;cam.hide_render=False
s.render.resolution_x=900;s.render.resolution_y=900;s.render.resolution_percentage=100
report=[]
for name,f in [('Reveal',20),('Chase',7),('WallCrawl',48),('JumpscareDeath',50)]:
 a=bpy.data.actions['KEEPER_'+name];rig.animation_data_create();rig.animation_data.action=a;rig.animation_data.action_slot=a.slots[0];s.frame_set(f);bpy.context.view_layer.update()
 inv=(rig.pose.bones['root'].matrix@rest['root'].inverted()).inverted();poses={b.name:inv@b.matrix.copy() for b in rig.pose.bones};values={k:rig.get(k,0) for k in ['rage','bite','blink_L','blink_R']};rig.animation_data.action=None
 for b in rig.pose.bones:
  b.matrix_basis=(rest[b.name].inverted()@rest[b.parent.name]@poses[b.parent.name].inverted()@poses[b.name]) if b.parent else rest[b.name].inverted()@poses[b.name]
 for key,control in {'FULL_HORROR':'rage','CHOMP':'bite','Blink_L':'blink_L','Blink_R':'blink_R'}.items():mesh.data.shape_keys.key_blocks[key].value=values[control]
 bpy.context.view_layer.update();dg=bpy.context.evaluated_depsgraph_get();eo=mesh.evaluated_get(dg);points=[v.co for v in eo.data.vertices]
 lo=[min(p[i] for p in points) for i in range(3)];hi=[max(p[i] for p in points) for i in range(3)]
 cam.data.ortho_scale=8.5 if name in ['Chase','WallCrawl','JumpscareDeath'] else 6.6;cam.location=(7,-13,6);cam.rotation_euler=(Vector((0,0,2.5))-cam.location).to_track_quat('-Z','Y').to_euler()
 path='/home/tyron/Projects/cgra252-team9/Tools/previews/keeper/'+name+'.png';s.render.filepath=path;bpy.ops.render.render(write_still=True)
 report.append({'clip':name,'frame':f,'min_cm':[x*.4896857534347332*100 for x in lo],'max_cm':[x*.4896857534347332*100 for x in hi],'face':values,'preview':path})
open('/home/tyron/Projects/cgra252-team9/Tools/keeper-pose-review.json','w').write(json.dumps(report,indent=2));result={"poses":report}
