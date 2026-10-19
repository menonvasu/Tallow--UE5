import bpy,math,json,hashlib,sys
from pathlib import Path
from mathutils import Vector,Matrix,Quaternion,Euler
from mathutils.bvhtree import BVHTree
# CupShield revision 02: a real shielding cup, 2 s loop held while RMB. Open CupShieldRevision01/06_cup-sleeve-revision.blend.
# drawn (default): the candle hand draws the candle 13.9 cm in toward the chest (hand_r rotation and grip unchanged, so the candle stays upright)
#   and the left palm cups directly behind the flame, facing it and the player, fingers together and curved, thumb tucked to the index.
# fixed (-- fixed): right side exactly the Revision01 clip; the left hand can only reach the screen-left side of the flame (wall beside it).
VARIANT=(sys.argv[sys.argv.index('--')+1:] or ['drawn'])[0] if '--' in sys.argv else 'drawn'
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'Tallow/SourceAssets/Player/InteractionV05/CupShieldRevision02'/('' if VARIANT=='drawn' else 'FixedCandleVariant');OUT.mkdir(parents=True,exist_ok=True)
s=bpy.context.scene;r=bpy.data.objects['RIG_Tallow_Generated'];c=bpy.data.objects['SM_CandleV02_Crooked'];part=bpy.data.objects['SK_Tallow_Generated_Partial']
R={b.name:b.matrix_local.copy() for b in r.data.bones};LEN={b.name:b.length for b in r.data.bones};PAR={b.name:(b.parent.name if b.parent else None) for b in r.data.bones};rest={b.name:[list(row) for row in b.matrix_local] for b in r.data.bones}
FLAME=3.0/100;CAM=Vector((0,-.2,1.1));N=61  # flame: TallowFlameVisualComponent HeightCm 3 (WidthCm .9) on the wick
LEFT=[n for n in R if n.endswith('_l') and n.split('_')[0] in('clavicle','upperarm','lowerarm','hand','thumb','index','middle','ring','pinky')];FINGERS=[n for n in LEFT if n.split('_')[0] in('thumb','index','middle','ring','pinky')];RIGHT=[n[:-2]+'_r' for n in LEFT if n!='clavicle_l'];RARM=['upperarm_r','lowerarm_r','hand_r']
def update():bpy.context.view_layer.update()
def qe(e):return Euler([math.radians(v) for v in e],'XYZ').to_quaternion()
# C-wall hand (local XYZ deg): fingers drawn together and gently curved, index and pinky a touch more so the palm side is concave; thumb solved flat along the index side.
CUP={n:qe(e) for n,e in {'index_01_l':(-20,0,12),'index_02_l':(-15,0,0),'index_03_l':(-9,0,0),'middle_01_l':(-15,0,2),'middle_02_l':(-18,0,0),'middle_03_l':(-10,0,0),'ring_01_l':(-17,0,-3),'ring_02_l':(-20,0,0),'ring_03_l':(-11,0,0),'pinky_01_l':(-24,0,-12),'pinky_02_l':(-21,0,0),'pinky_03_l':(-12,0,0),'thumb_01_l':(-10.13,44.63,26.31),'thumb_02_l':(-5.25,0,-9.31),'thumb_03_l':(11.19,-0,0)}.items()}
# Placement (from a reach scan): palm centre at azimuth PHI around the flame (90 = behind it, 180 = screen-left), RHO off its axis, height HZ; palm normal tilted TILT; fingers rolled PSI from vertical.
PULL,(PRO,ELE,PHI,RHO,HZ,TILT,PSI)={'drawn':(Vector((-.12,-.06,-.05)),(25,12,90,.065,1.005,0,-30)),'fixed':(Vector(),(30,15,180,.085,1.06,-20,-30))}[VARIANT];POLE=Vector((-.4,-.1,-1))
hb=r.data.bones['hand_l'];RD=(hb.tail_local-hb.head_local).normalized();RB=r.data.bones['pinky_01_l'].head_local-r.data.bones['index_01_l'].head_local;RB=(RB-RD*RB.dot(RD)).normalized();RN=RB.cross(RD).normalized()
def aim(n,target):
 p=r.pose.bones[n];h=p.head.copy();q=(p.tail-h).rotation_difference(Vector(target)-h);p.matrix=Matrix.Translation(h)@q.to_matrix().to_4x4()@Matrix.Translation(-h)@p.matrix;update()
def twist(q):q=q if q.w>=0 else -q;return 2*math.atan2(q.y,q.w)
def local(child,cm,pm):return ((R[PAR[child]].to_3x3().inverted()@R[child].to_3x3()).inverted()@pm.to_3x3().inverted()@cm.to_3x3()).to_quaternion()
def secant(f,goal):
 a,b,fa,fb=0.,.2,f(0.)-goal,f(.2)-goal
 for i in range(40):
  if abs(fb)<1e-9 or fb==fa:break
  a,b,fa=b,b-fb*(b-a)/(fb-fa),fb;fb=f(b)-goal
 return b
def pose_arm(clav,wrist,Rh,pole,fing,FL=.6,FU=.85):
 """Clavicle (elevation,protraction) deg; hand_l head at `wrist` with world rotation Rh; two-bone IK toward the pole; axial wrist twist split forearm/wrist and lifted off the elbow into the upper arm."""
 r.pose.bones['clavicle_l'].rotation_euler=Euler((math.radians(clav[0]),0,math.radians(-clav[1])),'XYZ');update()
 S=r.pose.bones['upperarm_l'].head.copy();L1,L2=LEN['upperarm_l'],LEN['lowerarm_l'];d=wrist-S;dl=min(d.length,L1+L2-1e-4)
 ax=d.normalized();a=(L1*L1-L2*L2+dl*dl)/(2*dl);pv=(pole-ax*pole.dot(ax)).normalized();aim('upperarm_l',S+ax*a+pv*math.sqrt(max(0,L1*L1-a*a)));aim('lowerarm_l',S+ax*dl)
 U,L=r.pose.bones['upperarm_l'],r.pose.bones['lowerarm_l'];Um,Lm=U.matrix.copy(),L.matrix.copy();Hm=Matrix.Translation(L.tail)@Rh.to_4x4();la=(L.tail-L.head).normalized();ua=(U.tail-U.head).normalized()
 hand_tw=lambda phi:twist(local('hand_l',Hm,Matrix.Rotation(phi,4,la)@Lm));phi=secant(hand_tw,(1-FL)*hand_tw(0))
 L.matrix=Matrix.Translation(L.head)@Matrix.Rotation(phi,4,la)@Matrix.Translation(-L.head)@Lm;update();Lm=L.matrix.copy()
 elbow_tw=lambda th:twist(local('lowerarm_l',Lm,Matrix.Translation(U.head)@Matrix.Rotation(th,4,ua)@Matrix.Translation(-U.head)@Um));th=secant(elbow_tw,(1-FU)*elbow_tw(0))
 U.matrix=Matrix.Translation(U.head)@Matrix.Rotation(th,4,ua)@Matrix.Translation(-U.head)@Um;update();L.matrix=Lm;update()
 H=r.pose.bones['hand_l'];H.matrix=Matrix.Translation(H.head)@Rh.to_4x4();update()
 for n in FINGERS:r.pose.bones[n].rotation_euler=fing[n].to_euler('XYZ')
 update();return d.length
def pull_right():
 """Draw the candle hand in by PULL: elbow keeps its Hold swivel plane, hand_r keeps its world rotation (candle upright, grip untouched)."""
 if PULL.length==0:return
 U,H=r.pose.bones['upperarm_r'],r.pose.bones['hand_r'];Hm=H.matrix.copy();S,E=U.head.copy(),U.tail.copy();W=Hm.translation+PULL;L1,L2=LEN['upperarm_r'],LEN['lowerarm_r'];d=W-S;ax=d.normalized();a=(L1*L1-L2*L2+d.length_squared)/(2*d.length);pv=((E-S)-ax*(E-S).dot(ax)).normalized()
 aim('upperarm_r',S+ax*a+pv*math.sqrt(max(0,L1*L1-a*a)));aim('lowerarm_r',W);H.matrix=Matrix.Translation(H.head)@Hm.to_3x3().to_4x4();update()
src=bpy.data.actions['AN_PlayerV05_CupShield'];src.name='SOURCE_CupShieldRevision01'
def source(f):
 r.animation_data.action=src;r.animation_data.action_slot=src.slots[0];s.frame_set(f);update();basis={p.name:p.matrix_basis.copy() for p in r.pose.bones};m={p.name:p.matrix.copy() for p in r.pose.bones};r.animation_data.action=None
 for p in r.pose.bones:p.matrix_basis=basis[p.name]
 update();return m
source(1);pull_right();WICK=c.matrix_world@Vector((.004,0,.255))  # wick where the drawn-in candle puts it
# --- cup transform: palm frame (B,D,P) built around the flame, mapped from the rest hand frame
FC=WICK+Vector((0,0,FLAME/2));out=Vector((math.cos(math.radians(PHI)),math.sin(math.radians(PHI)),0));PCw=Vector((FC.x,FC.y,HZ))+out*RHO
P=(-out*math.cos(math.radians(TILT))+Vector((0,0,math.sin(math.radians(TILT))))).normalized();up=(Vector((0,0,1))-P*P.z).normalized();D=(up*math.cos(math.radians(PSI))+P.cross(up)*math.sin(math.radians(PSI))).normalized()
Rq=Matrix((D.cross(P),D,P)).transposed()@Matrix((RB,RD,RN));RHC=Rq@R['hand_l'].to_3x3();PC0=R['hand_l'].translation.lerp(R['middle_01_l'].translation,.6);WC=PCw+Rq@(R['hand_l'].translation-PC0)
def breath(f):
 # one slow 2 s breath per loop (exactly periodic: frame 61 == frame 1): 1.5 mm rise/settle, tiny palm rock, fingers ease 1.5 deg
 t=2*math.pi*(f-1)/(N-1);return Vector((0,.0006*math.sin(t+.8),.0015*math.sin(t))),.8*math.sin(t+.3),1.5*(.5-.5*math.cos(t))
# --- bake
action=bpy.data.actions.new('AN_PlayerV05_CupShield');action.use_fake_user=True
report={'variant':VARIANT,'bones':len(rest),'frames':N,'fps':30,'loop':True,'right_hand_pull_cm':[round(x*100,2) for x in PULL],'max_body_vs_revision01':0,'max_right_vs_revision01':0,'max_right_hand_rotation_vs_revision01':0,'max_right_finger_local_vs_revision01':0,'per_frame':[]};prev={};first=None
for f in range(1,N+1):
 srcm=source(f);srcb={p.name:p.matrix_basis.copy() for p in r.pose.bones}
 if f<N:
  pull_right()
  for n in LEFT:r.pose.bones[n].matrix_basis=Matrix()  # Hold's left arm is the rest pose; solve from it every frame
  dw,rock,ease=breath(f);fing={n:CUP[n]@qe((-ease if n.split('_')[0]!='thumb' and n.endswith(('02_l','03_l')) else 0,0,0)) for n in FINGERS}
  pose_arm((ELE,PRO),WC+dw,Matrix.Rotation(math.radians(rock),3,(D.cross(P)).normalized())@RHC,POLE,fing)
  if f==1:first={p.name:p.matrix_basis.copy() for p in r.pose.bones}
 else:
  for p in r.pose.bones:p.matrix_basis=first[p.name]  # loop seam: last frame is frame 1
  update()
 r.animation_data.action=action
 if action.slots:r.animation_data.action_slot=action.slots[0]
 for p in r.pose.bones:
  e=p.matrix_basis.to_quaternion().to_euler('XYZ',prev[p.name]) if p.name in prev else p.matrix_basis.to_euler('XYZ');p.rotation_euler=e;prev[p.name]=e.copy()
  p.keyframe_insert('location',frame=f);p.keyframe_insert('rotation_euler',frame=f);p.keyframe_insert('scale',frame=f)
 r.animation_data.action=None
 for p in r.pose.bones:
  dm=max(abs(p.matrix[i][j]-srcm[p.name][i][j]) for i in range(4) for j in range(4))
  if p.name in RIGHT:report['max_right_vs_revision01']=max(report['max_right_vs_revision01'],dm)
  elif p.name not in LEFT:report['max_body_vs_revision01']=max(report['max_body_vs_revision01'],dm)
  if p.name in RIGHT and p.name not in RARM:report['max_right_finger_local_vs_revision01']=max(report['max_right_finger_local_vs_revision01'],max(abs(p.matrix_basis[i][j]-srcb[p.name][i][j]) for i in range(4) for j in range(4)))
 report['max_right_hand_rotation_vs_revision01']=max(report['max_right_hand_rotation_vs_revision01'],max(abs(r.pose.bones['hand_r'].matrix[i][j]-srcm['hand_r'][i][j]) for i in range(3) for j in range(3)))
 tw=lambda n:round(math.degrees(twist(r.pose.bones[n].matrix_basis.to_quaternion())),2);U,L=r.pose.bones['upperarm_l'],r.pose.bones['lowerarm_l']
 report['per_frame'].append({'frame':f,'shoulder_wrist_m':round((r.pose.bones['hand_l'].head-U.head).length,4),'elbow_deg':round(math.degrees(math.pi-(U.tail-U.head).angle(L.tail-L.head)),1),'twist_deg':{n:tw(n) for n in ('upperarm_l','lowerarm_l','hand_l','upperarm_r','lowerarm_r','hand_r')}})
for fc in action.layers[0].strips[0].channelbags[0].fcurves:
 for k in fc.keyframe_points:k.interpolation='LINEAR'
# --- validate against the flame from the in-game camera
r.animation_data.action=action;r.animation_data.action_slot=action.slots[0];s.frame_set(1);update();m1={p.name:p.matrix.copy() for p in r.pose.bones};s.frame_set(N);update()
report['loop_seam_max_delta']=max(abs(p.matrix[i][j]-m1[p.name][i][j]) for p in r.pose.bones for i in range(4) for j in range(4))
cbm=c.evaluated_get(bpy.context.evaluated_depsgraph_get()).to_mesh();cbvh=BVHTree.FromPolygons([c.matrix_world@v.co for v in cbm.vertices],[list(p.vertices) for p in cbm.polygons])
gid={g.index for g in part.vertex_groups if g.name in LEFT and g.name.split('_')[0] in('hand','thumb','index','middle','ring','pinky')};hand=[v.index for v in part.data.vertices if any(g.group in gid and g.weight>.3 for g in v.groups)]
rid={g.index for g in part.vertex_groups if g.name.endswith('_r') and g.name.split('_')[0] in('hand','thumb','index','middle','ring','pinky')};rhand=[v.index for v in part.data.vertices if any(g.group in rid and g.weight>.3 for g in v.groups)];checks=[]
for f in (1,16,31,46):
 s.frame_set(f);dg=bpy.context.evaluated_depsgraph_get();me=part.evaluated_get(dg).to_mesh();mw=part.matrix_world;pts=[mw@me.vertices[i].co for i in hand]
 hbvh=BVHTree.FromPolygons([mw@v.co for v in me.vertices],[list(p.vertices) for p in me.polygons]);hd=[(p,math.hypot(p.x-WICK.x,p.y-WICK.y)) for p in pts];band=[(p,d) for p,d in hd if WICK.z-.01<p.z<WICK.z+FLAME+.015]
 seg=lambda p:(p-(WICK+Vector((0,0,max(0,min(FLAME,p.z-WICK.z)))))).length;az=sorted(math.degrees(math.atan2(p.y-WICK.y,p.x-WICK.x))%360 for p,d in band if d<.09);blocked=[]
 for k in (0,.5,1):
  tgt=WICK+Vector((0,0,FLAME*k));hit=hbvh.ray_cast(CAM,(tgt-CAM).normalized(),(tgt-CAM).length);blocked.append(hit[0] is not None)
 inside=lambda vs:sum(1 for p in vs if (lambda h:h[0] is not None and h[3]<.01 and (p-h[0]).dot(h[1])<-.001)(cbvh.find_nearest(p)))
 H=r.pose.bones['hand_l'];Pw=(H.matrix.to_3x3()@(R['hand_l'].to_3x3().inverted()@RN)).normalized();pc=H.head.lerp(r.pose.bones['middle_01_l'].head,.6)
 checks.append({'frame':f,'skin_to_flame_min_cm':round(min(seg(p) for p in pts)*100,2),'wall_skin_to_flame_axis_cm':round(min(d for p,d in band)*100,2) if band else None,'skin_over_flame_vertices':sum(1 for p,d in hd if d<.025 and p.z>WICK.z),
  'fingertip_bones_above_flame_tip_cm':round((max(r.pose.bones[f'{n}_03_l'].tail.z for n in ('index','middle','ring','pinky'))-(WICK.z+FLAME))*100,2),'wall_azimuth_span_deg':round(az[-1]-az[0]) if az else 0,
  'flame_line_of_sight_blocked_base_mid_tip':blocked,'left_hand_vertices_inside_candle':inside(pts),'right_hand_vertices_inside_candle_over_1mm':inside([mw@me.vertices[i].co for i in rhand]),'palm_normal':[round(x,3) for x in Pw],'palm_faces_flame_cos':round(Pw.dot((FC-pc).normalized()),3)})
 part.evaluated_get(dg).to_mesh_clear()
report['wick_world']=[round(x,4) for x in WICK];report['flame_checks']=checks
assert len(r.data.bones)==52 and rest=={b.name:[list(row) for row in b.matrix_local] for b in r.data.bones}
assert report['max_body_vs_revision01']<1e-5 and report['max_right_hand_rotation_vs_revision01']<1e-5 and report['max_right_finger_local_vs_revision01']<1e-6 and (PULL.length>0 or report['max_right_vs_revision01']<1e-5),report
assert report['loop_seam_max_delta']<1e-6 and all(x['skin_over_flame_vertices']==0 and not any(x['flame_line_of_sight_blocked_base_mid_tip']) and x['left_hand_vertices_inside_candle']==0 for x in checks),report
s.frame_start=1;s.frame_end=N;s.render.fps=30;s.frame_set(1);bpy.ops.object.select_all(action='DESELECT');r.select_set(True);bpy.context.view_layer.objects.active=r
bpy.ops.export_scene.fbx(filepath=str(OUT/'AN_PlayerV05_CupShield.fbx'),use_selection=True,object_types={'ARMATURE'},add_leaf_bones=False,axis_forward='X',axis_up='Z',apply_unit_scale=True,use_armature_deform_only=True,bake_anim=True,bake_anim_use_all_actions=False,bake_anim_use_nla_strips=False,bake_anim_simplify_factor=0)
report['fbx_sha256']=hashlib.sha256((OUT/'AN_PlayerV05_CupShield.fbx').read_bytes()).hexdigest();(OUT/'validation.json').write_text(json.dumps(report,indent=1));bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'09_cupshield-rev02.blend'))
print('RESULT',json.dumps({k:v for k,v in report.items() if k!='per_frame'}),flush=True)
