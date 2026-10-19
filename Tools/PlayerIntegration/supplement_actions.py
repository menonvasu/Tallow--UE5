import bpy,math,json
from pathlib import Path
from mathutils import Vector,Matrix
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'Tallow/SourceAssets/Player/InteractionV05';s=bpy.context.scene;r=bpy.data.objects['RIG_Tallow_Generated']
def update():bpy.context.view_layer.update()
def aim(n,target):
 update();p=r.pose.bones[n];h=p.head.copy();q=(p.tail-h).rotation_difference(Vector(target)-h);p.matrix=Matrix.Translation(h)@q.to_matrix().to_4x4()@Matrix.Translation(-h)@p.matrix;update()
def arm(side,target,D,N):
 target=Vector(target);h=r.pose.bones['upperarm_'+side].head.copy();d=target-h;L=r.data.bones['upperarm_'+side].length;M=r.data.bones['lowerarm_'+side].length
 assert d.length<L+M
 axis=d.normalized();a=(L*L-M*M+d.length_squared)/(2*d.length);pole=Vector((-1 if side=='l' else 1,0,-.7));pole=(pole-axis*pole.dot(axis)).normalized();elbow=h+axis*a+pole*math.sqrt(max(0,L*L-a*a));aim('upperarm_'+side,elbow);aim('lowerarm_'+side,target)
 b=r.data.bones['hand_'+side];rd=(b.tail_local-b.head_local).normalized();rb=r.data.bones['pinky_01_'+side].head_local-r.data.bones['index_01_'+side].head_local;rb=(rb-rd*rb.dot(rd)).normalized();rn=rb.cross(rd).normalized();D=Vector(D).normalized();N=Vector(N);N=(N-D*N.dot(D)).normalized();B=D.cross(N).normalized()
 rot=Matrix((B,D,N)).transposed()@Matrix((rb,rd,rn)).transposed().transposed()@b.matrix_local.to_3x3()
 # Distribute desired axial wrist rotation through the forearm instead of collapsing the skin seam.
 lower=r.pose.bones['lowerarm_'+side];axis=(lower.tail-lower.head).normalized();wanted=rot@b.matrix_local.to_3x3().inverted()@r.data.bones['lowerarm_'+side].matrix_local.to_3x3();cur=lower.matrix.to_3x3().col[0].copy();want=wanted.col[0].copy();cur=(cur-axis*cur.dot(axis)).normalized();want=(want-axis*want.dot(axis)).normalized();angle=math.atan2(axis.dot(cur.cross(want)),cur.dot(want));head=lower.head.copy();lower.matrix=Matrix.Translation(head)@Matrix.Rotation(angle,4,axis)@Matrix.Translation(-head)@lower.matrix;update()
 p=r.pose.bones['hand_'+side];p.matrix=Matrix.Translation(p.head)@rot.to_4x4();update()

def sample(name):
 a=bpy.data.actions['AN_PlayerV05_'+name];r.animation_data.action=a;r.animation_data.action_slot=a.slots[0];s.frame_set(1);ps={p.name:p.matrix_basis.copy() for p in r.pose.bones};r.animation_data.action=None;return ps
def apply(ps):
 r.animation_data.action=None
 for p in r.pose.bones:p.matrix_basis=ps[p.name]
 update()
base=sample('ToolHold');cup=sample('Cup');report={}
for name,end in [('CrowbarPry',61),('FuseInsert',46),('CupShield',61)]:
 action=bpy.data.actions.new('AN_PlayerV05_'+name);action.use_fake_user=True
 for frame in range(1,end+1):
  t=(frame-1)/(end-1);apply(cup if name=='CupShield' else base)
  if name=='CupShield':
   arm('l',(.095,.195,1.035),(.6,.2,.77),(-.7,-.5,.2))
  else:
   weight=math.sin(math.pi*t)**2;hand=r.pose.bones['hand_l'];target=hand.head.copy()+Vector((0,.075*weight,(.035 if name=='FuseInsert' else -.035)*weight));D=(hand.tail-hand.head).normalized();N=hand.matrix.to_3x3()@Vector((0,0,1))
   # Retain exact grip rotation while moving the elbow/wrist through a short interaction arc.
   rotation=hand.matrix.to_3x3().copy();h=r.pose.bones['upperarm_l'].head.copy();d=target-h;L=r.data.bones['upperarm_l'].length;M=r.data.bones['lowerarm_l'].length;axis=d.normalized();a=(L*L-M*M+d.length_squared)/(2*d.length);pole=Vector((-1,0,-.7));pole=(pole-axis*pole.dot(axis)).normalized();elbow=h+axis*a+pole*math.sqrt(max(0,L*L-a*a));aim('upperarm_l',elbow);aim('lowerarm_l',target);hand.matrix=Matrix.Translation(hand.head)@rotation.to_4x4();update()
  r.animation_data.action=action
  for p in r.pose.bones:
   p.keyframe_insert('location',frame=frame);p.keyframe_insert('rotation_euler',frame=frame);p.keyframe_insert('scale',frame=frame)
 for fc in action.layers[0].strips[0].channelbags[0].fcurves:
  for k in fc.keyframe_points:k.interpolation='LINEAR'
 r.animation_data.action=action;r.animation_data.action_slot=action.slots[0];s.frame_start=1;s.frame_end=end;s.render.fps=30;s.frame_set(1);bpy.ops.object.select_all(action='DESELECT');r.select_set(True);bpy.context.view_layer.objects.active=r
 bpy.ops.export_scene.fbx(filepath=str(OUT/('AN_PlayerV05_'+name+'.fbx')),use_selection=True,object_types={'ARMATURE'},add_leaf_bones=False,axis_forward='X',axis_up='Z',apply_unit_scale=True,use_armature_deform_only=True,bake_anim=True,bake_anim_use_all_actions=False,bake_anim_use_nla_strips=False,bake_anim_simplify_factor=0)
 report[name]={'seconds':(end-1)/30,'samples':end,'loop':name=='CupShield'}
(OUT/'supplement-validation.json').write_text(json.dumps(report,indent=2));bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'05_interaction-supplement.blend'))
print('RESULT',json.dumps(report))
