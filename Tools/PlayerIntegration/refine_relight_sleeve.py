import bpy,math,json,hashlib
from pathlib import Path
from mathutils import Vector,Matrix
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'Tallow/SourceAssets/Player/InteractionV05/RelightMatchRevision01';OUT.mkdir(exist_ok=True)
s=bpy.context.scene;r=bpy.data.objects['RIG_Tallow_Generated'];part=bpy.data.objects['SK_Tallow_Generated_Partial'];full=bpy.data.objects['SK_Tallow_Generated_Full'];source=bpy.data.actions['AN_PlayerV05_RelightMatch'];source.name='SOURCE_RelightMatch';action=bpy.data.actions.new('AN_PlayerV05_RelightMatch');action.use_fake_user=True
rest={b.name:[list(row) for row in b.matrix_local] for b in r.data.bones};report={'bones':len(rest),'max_hand_matrix_delta':0,'max_right_bone_matrix_delta':0,'upperarm_twist_deg':[]}
for frame in range(1,38):
 r.animation_data.action=source;r.animation_data.action_slot=source.slots[0];s.frame_set(frame);bpy.context.view_layer.update();old={p.name:p.matrix.copy() for p in r.pose.bones};basis={p.name:p.matrix_basis.copy() for p in r.pose.bones};r.animation_data.action=None
 for p in r.pose.bones:p.matrix_basis=basis[p.name]
 bpy.context.view_layer.update();u=r.pose.bones['upperarm_l'];lower=r.pose.bones['lowerarm_l'];axis=(u.tail-u.head).normalized();a=u.matrix.to_3x3().col[0].copy();b=lower.matrix.to_3x3().col[0].copy();a=(a-axis*a.dot(axis)).normalized();b=(b-axis*b.dot(axis)).normalized();angle=math.atan2(axis.dot(a.cross(b)),a.dot(b));head=u.head.copy();u.matrix=Matrix.Translation(head)@Matrix.Rotation(angle,4,axis)@Matrix.Translation(-head)@u.matrix;bpy.context.view_layer.update();lower.matrix=old['lowerarm_l'];bpy.context.view_layer.update()
 report['upperarm_twist_deg'].append(math.degrees(angle))
 for name in ['hand_l','hand_r']:
  report['max_hand_matrix_delta']=max(report['max_hand_matrix_delta'],max(abs(r.pose.bones[name].matrix[i][j]-old[name][i][j]) for i in range(4) for j in range(4)))
 for name in old:
  if name.endswith('_r'):report['max_right_bone_matrix_delta']=max(report['max_right_bone_matrix_delta'],max(abs(r.pose.bones[name].matrix[i][j]-old[name][i][j]) for i in range(4) for j in range(4)))
 r.animation_data.action=action
 for p in r.pose.bones:
  p.keyframe_insert('location',frame=frame);p.keyframe_insert('rotation_euler',frame=frame);p.keyframe_insert('scale',frame=frame)
for fc in action.layers[0].strips[0].channelbags[0].fcurves:
 previous=None
 for k in fc.keyframe_points:
  k.interpolation='LINEAR'
  if 'rotation_euler' in fc.data_path and previous is not None:
   while k.co.y-previous>math.pi:k.co.y-=math.tau
   while k.co.y-previous< -math.pi:k.co.y+=math.tau
  previous=k.co.y
assert report['max_hand_matrix_delta']<1e-5 and report['max_right_bone_matrix_delta']<1e-5,report
assert rest=={b.name:[list(row) for row in b.matrix_local] for b in r.data.bones}
r.animation_data.action=action;r.animation_data.action_slot=action.slots[0];s.frame_start=1;s.frame_end=37;s.render.fps=30;s.frame_set(1);bpy.ops.object.select_all(action='DESELECT');r.select_set(True);bpy.context.view_layer.objects.active=r
bpy.ops.export_scene.fbx(filepath=str(OUT/'AN_PlayerV05_RelightMatch.fbx'),use_selection=True,object_types={'ARMATURE'},add_leaf_bones=False,axis_forward='X',axis_up='Z',apply_unit_scale=True,use_armature_deform_only=True,bake_anim=True,bake_anim_use_all_actions=False,bake_anim_use_nla_strips=False,bake_anim_simplify_factor=0)
report['fbx_sha256']=hashlib.sha256((OUT/'AN_PlayerV05_RelightMatch.fbx').read_bytes()).hexdigest();(OUT/'validation.json').write_text(json.dumps(report,indent=2));bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'07_relight-sleeve-revision.blend'))
print('RESULT',json.dumps(report),flush=True)
