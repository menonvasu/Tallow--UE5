import bpy,math,json,hashlib
from pathlib import Path
from mathutils import Matrix,Vector
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'Tallow/SourceAssets/Player/InteractionV05';s=bpy.context.scene;r=bpy.data.objects['RIG_Tallow_Generated'];full=bpy.data.objects['SK_Tallow_Generated_Full'];part=bpy.data.objects['SK_Tallow_Generated_Partial'];c=bpy.data.objects['SM_CandleV02_Crooked']
poses={n:{b:Matrix(m) for b,m in data.items()} for n,data in json.loads((OUT/'pose-bases.json').read_text()).items()}
def apply(ps):
 r.animation_data.action=None
 for p in r.pose.bones:p.matrix_basis=ps[p.name]
 bpy.context.view_layer.update()
def snapshot():return {p.name:p.matrix_basis.copy() for p in r.pose.bones}
def lerp(a,b,t):return {n:a[n].lerp(b[n],t) for n in a}
def smooth(t):return t*t*(3-2*t)
# Pinched relight gesture, distinct from the broad draft shield.
def aim(name,target):
 bpy.context.view_layer.update();pb=r.pose.bones[name];head=pb.head.copy();rot=(pb.tail-head).rotation_difference(Vector(target)-head).to_matrix().to_4x4();pb.matrix=Matrix.Translation(head)@rot@Matrix.Translation(-head)@pb.matrix;bpy.context.view_layer.update()
apply(poses['Cup'])
for j,target in enumerate([(.175,.225,1.015),(.195,.235,1.035),(.205,.237,1.044)],1):aim(f'index_{j:02d}_l',target)
for j,target in enumerate([(.173,.222,1.013),(.190,.230,1.031),(.204,.236,1.043)],1):aim(f'thumb_{j:02d}_l',target)
for digit,target in [('index',Vector((.202,.236,1.049))),('thumb',Vector((.202,.234,1.047)))]:
 for iteration in range(20):
  for joint in [3,2,1]:
   bpy.context.view_layer.update();pb=r.pose.bones[f'{digit}_{joint:02d}_l'];head=pb.head.copy();tip=r.pose.bones[f'{digit}_03_l'].tail.copy();q=(tip-head).rotation_difference(target-head);pb.matrix=Matrix.Translation(head)@q.to_matrix().to_4x4()@Matrix.Translation(-head)@pb.matrix
  bpy.context.view_layer.update()
poses['Relight']=snapshot()
# New skeletal animation retains every original rest matrix and bone name.
seated=Path('/home/tyron/Projects/tallow-workstreams/player/Tallow/SourceAssets/Player/SeatedV04/04_seated-bound.blend')
with bpy.data.libraries.load(str(seated),link=False) as(src,dst):dst.actions=['AN_Generated_SeatedBoundIdle']
a=dst.actions[0];r.animation_data.action=a;r.animation_data.action_slot=a.slots[0];s.frame_set(1);poses['Bound']=snapshot();r.animation_data.action=None
# Pickup starts below the view with a closed grip already acquired by gameplay.
apply(poses['Hold']);r.pose.bones['upperarm_r'].rotation_euler.x-=.45;r.pose.bones['lowerarm_r'].rotation_euler.x-=.28;bpy.context.view_layer.update();poses['PickupLow']=snapshot()
# Rest and non-candle intro poses retain the generated original surface.
clips={'Hold':(61,True),'Cup':(61,True),'Rest':(61,True),'Walk':(31,True),'Sprint':(19,True),'Crouch':(61,True),'ToolHold':(61,True),'Reach':(31,False),'Relight':(37,False),'Pickup':(46,False),'IntroBound':(121,True),'ChairEscape':(46,False)}
report={'fps':30,'clips':{},'mesh_weights':{},'engine_verified':False,'surface':'Original generated character with longitudinal finger weight refinement; no primitive substitution'}
for name,(end,loop) in clips.items():
 action=bpy.data.actions.new('AN_PlayerV05_'+name);action.use_fake_user=True;first=None;last=None
 for frame in range(1,end+1):
  t=(frame-1)/(end-1);phase=t*math.tau
  if name=='Pickup':ps=lerp(poses['PickupLow'],poses['Hold'],smooth(t))
  elif name=='ChairEscape':
   # Hands release first, hips rise after a short brace. Gameplay owns actor movement.
   ps=lerp(poses['Bound'],poses['Rest'],smooth(max(0,min(1,(t-.15)/.85))))
   for n in ps:
    if 'arm_' in n or 'hand_' in n or n.startswith(('thumb','index','middle','ring','pinky')):ps[n]=poses['Bound'][n].lerp(poses['Rest'][n],smooth(min(1,t/.55)))
  elif name in ['Relight','Reach']:
   target=poses['Relight'] if name=='Relight' else poses['Reach'];weight=smooth(min(1,t/.33)) if t<.33 else (1 if t<.75 else smooth((1-t)/.25));ps=lerp(poses['Hold'],target,weight)
  elif name=='IntroBound':ps={n:m.copy() for n,m in poses['Bound'].items()}
  else:ps={n:m.copy() for n,m in poses['Cup' if name=='Cup' else 'Rest' if name=='Rest' else 'ToolHold' if name=='ToolHold' else 'Hold'].items()}
  apply(ps)
  if loop:
   amount=.007 if name=='Walk' else .012 if name=='Sprint' else .0015
   # Whole upper-body sway preserves the constant hand-to-holder contact.
   r.pose.bones['chest'].rotation_euler.y+=amount*math.sin(phase)
   r.pose.bones['chest'].rotation_euler.x+=amount*.55*math.sin(phase*2)
   if name=='Crouch':r.pose.bones['root'].location+=r.data.bones['root'].matrix_local.to_3x3().inverted()@Vector((0,0,-.025))
  bpy.context.view_layer.update();r.animation_data.action=action
  for p in r.pose.bones:
   p.keyframe_insert('location',frame=frame);p.keyframe_insert('rotation_euler',frame=frame);p.keyframe_insert('scale',frame=frame)
  matrices={p.name:p.matrix.copy() for p in r.pose.bones}
  if first is None:first=matrices
  last=matrices
 for fc in action.layers[0].strips[0].channelbags[0].fcurves:
  previous=None
  for k in fc.keyframe_points:
   k.interpolation='LINEAR'
   if 'rotation_euler' in fc.data_path and previous is not None:
    while k.co.y-previous>math.pi:k.co.y-=math.tau
    while k.co.y-previous< -math.pi:k.co.y+=math.tau
   previous=k.co.y
 seam=max(abs(first[n][i][j]-last[n][i][j]) for n in first for i in range(4) for j in range(4))
 if loop:assert seam<1e-5,(name,seam)
 report['clips'][name]={'samples':end,'seconds':(end-1)/30,'loop':loop,'loop_matrix_error':seam if loop else None}
# Animator-facing optional IK controls; FK actions remain the default and exported result.
apply(poses['Hold']);r['Use_IK_Controls']=0.0;r.id_properties_ui('Use_IK_Controls').update(min=0,max=1,description='Set 1 for authoring wrist IK; bake back to FK before export.')
for side in ['r','l']:
 ctrl=bpy.data.objects.new('CTRL_Wrist_'+side,None);s.collection.objects.link(ctrl);ctrl.empty_display_type='CIRCLE';ctrl.empty_display_size=.035;ctrl.matrix_world=r.matrix_world@r.pose.bones['hand_'+side].matrix
 ik=r.pose.bones['lowerarm_'+side].constraints.new('IK');ik.name='AUTHORING_WristIK';ik.target=ctrl;ik.chain_count=2;ik.use_stretch=False;ik.influence=0
 dr=ik.driver_add('influence').driver;dr.expression='enabled';v=dr.variables.new();v.name='enabled';v.targets[0].id=r;v.targets[0].data_path='["Use_IK_Controls"]'
 rot=r.pose.bones['hand_'+side].constraints.new('COPY_ROTATION');rot.name='AUTHORING_HandOrientation';rot.target=ctrl;rot.influence=0
 dr=rot.driver_add('influence').driver;dr.expression='enabled';v=dr.variables.new();v.name='enabled';v.targets[0].id=r;v.targets[0].data_path='["Use_IK_Controls"]'
for group,prefixes in [('Hands and fingers',('hand','thumb','index','middle','ring','pinky')),('Arms',('clavicle','upperarm','lowerarm')),('Body',('root','pelvis','spine','chest','neck','head','skirt','thigh','calf','foot'))]:
 col=r.data.collections.get(group) or r.data.collections.new(group)
 for b in r.data.bones:
  if b.name.startswith(prefixes):col.assign(b)
# Prop follows the hand in every Blender animation exactly as runtime attachment will.
apply(poses['Hold']);c['calibrated_camera_offset_UE_cm']=[44.5,22.5,-27.5];c['calibrated_camera_yaw_deg']=90
con=c.constraints.new('CHILD_OF');con.name='HoldContact';con.target=r;con.subtarget='hand_r';con.inverse_matrix=(r.matrix_world@r.pose.bones['hand_r'].matrix).inverted();bpy.context.view_layer.update()
# Export meshes in original rest pose, same 52-bone rig, no controls included.
common=dict(use_selection=True,add_leaf_bones=False,axis_forward='X',axis_up='Z',apply_unit_scale=True,mesh_smooth_type='FACE',path_mode='STRIP',use_armature_deform_only=True)
apply(poses['Rest'])
for ob,label in [(full,'Full'),(part,'Partial')]:
 ob.data.calc_loop_triangles();sums=[sum(g.weight for g in v.groups) for v in ob.data.vertices];assert all(abs(x-1)<1e-4 for x in sums)
 report['mesh_weights'][label]={'triangles':len(ob.data.loop_triangles),'unweighted':sum(x==0 for x in sums),'max_influences':max(len(v.groups) for v in ob.data.vertices)}
 bpy.ops.object.select_all(action='DESELECT');ob.hide_set(False);ob.select_set(True);r.select_set(True);bpy.context.view_layer.objects.active=r
 bpy.ops.export_scene.fbx(filepath=str(OUT/('SK_PlayerV05_'+label+'.fbx')),object_types={'ARMATURE','MESH'},bake_anim=False,**common)
for name,(end,loop) in clips.items():
 action=bpy.data.actions['AN_PlayerV05_'+name];r.animation_data.action=action;r.animation_data.action_slot=action.slots[0];s.frame_start=1;s.frame_end=end;s.frame_set(1);s.render.fps=30
 bpy.ops.object.select_all(action='DESELECT');r.select_set(True);bpy.context.view_layer.objects.active=r
 bpy.ops.export_scene.fbx(filepath=str(OUT/('AN_PlayerV05_'+name+'.fbx')),object_types={'ARMATURE'},bake_anim=True,bake_anim_use_all_actions=False,bake_anim_use_nla_strips=False,bake_anim_simplify_factor=0,**common)
 for f in range(1,end+1):
  s.frame_set(f);e=part.evaluated_get(bpy.context.evaluated_depsgraph_get());me=e.to_mesh();assert all(math.isfinite(x) for v in me.vertices for x in v.co);e.to_mesh_clear()
r.animation_data.action=bpy.data.actions['AN_PlayerV05_Hold'];r.animation_data.action_slot=r.animation_data.action.slots[0];s.frame_set(1);s.frame_end=61
report['files']={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in OUT.glob('*.fbx')};report['bones']=len(r.data.bones)
(OUT/'animation-validation.json').write_text(json.dumps(report,indent=2));bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'05_interaction-production.blend'));print('RESULT',json.dumps(report))
