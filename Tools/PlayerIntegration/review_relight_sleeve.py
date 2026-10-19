import bpy,json,math
from pathlib import Path
from mathutils import Vector,Matrix
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'Tallow/SourceAssets/Player/InteractionV05/RelightMatchRevision01';s=bpy.context.scene;r=bpy.data.objects['RIG_Tallow_Generated'];c=bpy.data.objects['SM_CandleV02_Crooked'];part=bpy.data.objects['SK_Tallow_Generated_Partial'];full=bpy.data.objects['SK_Tallow_Generated_Full'];full.hide_render=True;part.hide_render=False
with bpy.data.libraries.load(str(ROOT/'Tallow/SourceAssets/Production/ToolProps/ToolProps_Production.blend'),link=False) as(src,dst):dst.objects=['SM_ToolV02_Crowbar','SM_ToolV02_Fuse','SM_ToolV02_Match']
for o in dst.objects:s.collection.objects.link(o);o.hide_render=True
report={'tools':{}}
for ob in dst.objects:
 report['tools'][ob.name]={'bounds_cm':[[round(v*100,5) for v in xyz] for xyz in [tuple(min(v.co[i] for v in ob.data.vertices) for i in range(3)),tuple(max(v.co[i] for v in ob.data.vertices) for i in range(3))]]}
def pose(n,f):
 a=bpy.data.actions['AN_PlayerV05_'+n];r.animation_data.action=a;r.animation_data.action_slot=a.slots[0];s.frame_set(f);bpy.context.view_layer.update()
# Calibrate fixed hand sockets in the agreed ToolHold frame.
pose('ToolHold',1)
for ob,z in zip(dst.objects[:2],[.75,.83]):
 ob.location=(-.225,.245,z);con=ob.constraints.new('CHILD_OF');con.target=r;con.subtarget='hand_l';con.inverse_matrix=(r.matrix_world@r.pose.bones['hand_l'].matrix).inverted()
pose('RelightMatch',20);index=r.pose.bones['index_03_l'].tail.copy();thumb=r.pose.bones['thumb_03_l'].tail.copy();pinch=(index+thumb)/2;wick=c.matrix_world@Vector((.004,0,.255));match=dst.objects[2];match.location=pinch;match.rotation_euler=(wick-pinch).to_track_quat('Z','Y').to_euler();bpy.context.view_layer.update()
conv=Matrix(((0,1,0),(1,0,0),(0,0,1)));asset=Matrix.Diagonal((1,-1,1));rotation=conv@match.matrix_world.to_3x3()@asset;q=rotation.to_quaternion();report['match']={'calibrate_clip':'RelightMatch','time_seconds':19/30,'camera_location_cm':[(pinch.y+.2)*100,pinch.x*100,(pinch.z-1.1)*100],'camera_quaternion_xyzw':[q.x,q.y,q.z,q.w],'pinch_tip_gap_cm':(index-thumb).length*100,'match_tip_to_wick_cm':(match.matrix_world@Vector((0,0,.041))-wick).length*100}
report['match']['accepted_contact']=report['match']['pinch_tip_gap_cm']<.35 and report['match']['match_tip_to_wick_cm']<.6
(OUT/'tool-match-contract.json').write_text(json.dumps(report,indent=2))
s.render.engine='CYCLES';s.cycles.device='CPU';s.cycles.samples=12;s.render.threads_mode='FIXED';s.render.threads=4;s.render.resolution_x=1280;s.render.resolution_y=720;s.camera.location=(0,-.20,1.10);s.camera.rotation_euler=Vector((0,1,0)).to_track_quat('-Z','Y').to_euler();s.camera.data.lens=16
for name,frame,ob in [('RelightMatch',20,match)]:
 pose(name,frame)
 if ob:ob.hide_render=False
 s.render.filepath=str(OUT/('FINAL-FP-'+name+'-supplement.png'));bpy.ops.render.render(write_still=True)
 if ob:ob.hide_render=True
print('RESULT',json.dumps(report))
