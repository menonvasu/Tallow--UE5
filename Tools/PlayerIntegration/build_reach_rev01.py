import bpy,math,json,hashlib
from pathlib import Path
from mathutils import Vector,Matrix,Quaternion,Euler
# Reach revision 01: reach-to-grab for hold-E interactions. Left hand rises into view from lower-left, palm turning down/inward toward a target in front
# at chest height, fingers relaxed, opening a little just before contact, then a close (grab), a small tug while held, release and settle into Hold.
# Open RelightMatchRevision01/07_relight-sleeve-revision.blend. 31 frames at 30 fps; frames 1 and 31 are Hold (the current Reach endpoints); everything but the left arm is the current Reach.
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'Tallow/SourceAssets/Player/InteractionV05/ReachRevision01';OUT.mkdir(exist_ok=True)
s=bpy.context.scene;r=bpy.data.objects['RIG_Tallow_Generated'];part=bpy.data.objects['SK_Tallow_Generated_Partial']
R={b.name:b.matrix_local.copy() for b in r.data.bones};LEN={b.name:b.length for b in r.data.bones};PAR={b.name:(b.parent.name if b.parent else None) for b in r.data.bones};rest={b.name:[list(row) for row in b.matrix_local] for b in r.data.bones}
N=31;CAM=Vector((0,-.2,1.1))
LEFT=[n for n in R if n.endswith('_l') and n.split('_')[0] in('clavicle','upperarm','lowerarm','hand','thumb','index','middle','ring','pinky')];FINGERS=[n for n in LEFT if n.split('_')[0] in('thumb','index','middle','ring','pinky')]
def update():bpy.context.view_layer.update()
def qe(e):return Euler([math.radians(v) for v in e],'XYZ').to_quaternion()
def shape(curl,thumb,spread=(4,0,0,-3)):
 """finger shape from (MCP,PIP,DIP) flexion per finger (index,middle,ring,pinky) plus a thumb pose"""
 out={}
 for (d,c),z in zip(curl.items(),spread):out.update({f'{d}_01_l':qe((-c[0],0,z)),f'{d}_02_l':qe((-c[1],0,0)),f'{d}_03_l':qe((-c[2],0,0))})
 out.update({k:qe(v) for k,v in thumb.items()});return out
RELAX=shape({'index':(12,18,10),'middle':(15,22,12),'ring':(18,26,14),'pinky':(22,28,16)},{'thumb_01_l':(-10,0,10),'thumb_02_l':(0,0,0),'thumb_03_l':(8,0,0)})
OPEN=shape({'index':(3,5,3),'middle':(4,6,3),'ring':(5,7,4),'pinky':(6,8,4)},{'thumb_01_l':(-18,0,16),'thumb_02_l':(0,0,0),'thumb_03_l':(2,0,0)},(7,1,-3,-8))
# grab round a ~3 cm handle; thumb solved to wrap over the middle finger's middle phalanx
GRAB=shape({'index':(50,62,35),'middle':(55,66,38),'ring':(58,68,40),'pinky':(60,68,40)},{'thumb_01_l':(-50,25.44,32.31),'thumb_02_l':(-8.13,0,-11.25),'thumb_03_l':(11.75,0,0)})
REST={n:Quaternion() for n in FINGERS}
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
# --- target hand: palm down and turned in toward the midline, fingers forward, grab centre in front at chest height
GRABC,P,D=Vector((.04,.37,.95)),Vector((.7,.1,-.7)).normalized(),Vector((.35,.92,.15));D=(D-P*D.dot(P)).normalized()
Rq=Matrix((D.cross(P),D,P)).transposed()@Matrix((RB,RD,RN));RHT=Rq@R['hand_l'].to_3x3()
GC0=R['middle_02_l'].translation+RN*.022;WT=GRABC+Rq@(R['hand_l'].translation-GC0)  # grab centre: under the middle phalanges, in hand space
CLAV,POLE=(6,14),Vector((-.6,-.2,-1))
def sample(name,f=1):
 a=bpy.data.actions['AN_PlayerV05_'+name];r.animation_data.action=a;r.animation_data.action_slot=a.slots[0];s.frame_set(f);update();ps={p.name:p.matrix_basis.copy() for p in r.pose.bones};pm={p.name:p.matrix.copy() for p in r.pose.bones};r.animation_data.action=None;return ps,pm
_,holdm=sample('Hold');W0=holdm['hand_l'].translation.copy();RH0=holdm['hand_l'].to_3x3().normalized();S0,E0=holdm['upperarm_l'].translation,holdm['lowerarm_l'].translation;ax=(W0-S0).normalized();POLE0=((E0-S0)-ax*(E0-S0).dot(ax)).normalized()
def rot(axis,deg,M):return Matrix.Rotation(math.radians(deg),3,axis)@M
# wrist waypoints (u, hand_l head, rotation): swing forward below the view, rise in from lower-left turning the palm down/in, settle on the target; return drops in front
IN=[(0,W0,RH0),(.5,Vector((-.15,.22,.76)),RH0.to_quaternion().slerp(RHT.to_quaternion(),.45).to_matrix()),(.82,WT+Vector((-.035,-.03,-.035)),rot(D.cross(P).normalized(),10,RHT)),(1,WT,RHT)]
TUG,TUGR=Vector((0,-.012,-.004)),rot(D.cross(P).normalized(),-4,RHT)
BACK=[(0,WT+TUG,TUGR),(.35,WT+Vector((-.05,-.05,-.07)),RHT.to_quaternion().slerp(RH0.to_quaternion(),.3).to_matrix()),(.7,Vector((-.17,.2,.72)),RHT.to_quaternion().slerp(RH0.to_quaternion(),.7).to_matrix()),(1,W0,RH0)]
def cr(us,vs,u):
 """non-uniform Catmull-Rom through values vs at knots us (vectors, or sign-aligned quaternion components)"""
 i=max(j for j in range(len(us)-1) if us[j]<=u) if u<1 else len(us)-2;u0,u1=us[i],us[i+1];t=(u-u0)/(u1-u0)
 def tan(j):
  if j==0:return (vs[1]-vs[0])/(us[1]-us[0])
  if j==len(us)-1:return (vs[j]-vs[j-1])/(us[j]-us[j-1])
  return (vs[j+1]-vs[j-1])/(us[j+1]-us[j-1])
 m0,m1=tan(i)*(u1-u0),tan(i+1)*(u1-u0);h=(2*t**3-3*t*t+1,t**3-2*t*t+t,-2*t**3+3*t*t,t**3-t*t);return vs[i]*h[0]+m0*h[1]+vs[i+1]*h[2]+m1*h[3]
def path(keys,u):
 us=[k[0] for k in keys];qs=[k[2].to_quaternion() for k in keys]
 for j in range(1,len(qs)):
  if qs[j].dot(qs[j-1])<0:qs[j].negate()
 return cr(us,[Vector(k[1]) for k in keys],u),Quaternion(cr(us,[Vector(q) for q in qs],u)).normalized().to_matrix()
def warp(keys,f):
 """monotone cubic (frame->path u) through keys, flat at both ends: timing/spacing chart for a phase"""
 fs,us=[k[0] for k in keys],[k[1] for k in keys];n=len(fs);sec=[(us[i+1]-us[i])/(fs[i+1]-fs[i]) for i in range(n-1)];m=[0]+[0 if sec[i-1]*sec[i]<=0 else min((us[i+1]-us[i-1])/(fs[i+1]-fs[i-1]),3*sec[i-1],3*sec[i]) for i in range(1,n-1)]+[0]
 i=max(j for j in range(n-1) if fs[j]<=f) if f<fs[-1] else n-2;h=fs[i+1]-fs[i];t=(f-fs[i])/h
 return us[i]*(2*t**3-3*t*t+1)+m[i]*h*(t**3-2*t*t+t)+us[i+1]*(-2*t**3+3*t*t)+m[i+1]*h*(t**3-t*t)
ss=lambda t:(lambda x:x*x*(3-2*x))(max(0,min(1,t)))
INT=[(1,0),(4,.2),(7,.58),(10,.9),(12,1)];OUTT=[(23,0),(25,.25),(28,.75),(31,1)]
def fingers(f):
 # rest -> relaxed (rise) -> open a little (f10-12) -> grab (f12-15) -> hold -> release (f21-24) -> relaxed -> rest (Hold)
 for a,b,f0,f1 in [(REST,RELAX,1,6),(RELAX,OPEN,9,12),(OPEN,GRAB,12,15),(GRAB,RELAX,21,24),(RELAX,REST,26,31)]:
  if f<=f1 or (a is REST and f<f0):break
 w=ss((f-f0)/(f1-f0));return {n:a[n].slerp(b[n],w) for n in FINGERS},a,b,w
frames=[]
for f in range(1,N+1):
 if f<=12:wr,Rh=path(IN,warp(INT,f));k=ss(warp(INT,f)/.8)
 elif f<23:t=ss((f-15)/5);wr,Rh=WT+TUG*t,rot(D.cross(P).normalized(),-4*t,RHT);k=1  # small tug while held, wrist settles back a touch
 else:u=warp(OUTT,f);wr,Rh=path(BACK,u);k=1-ss(u/.9)
 fg,_,_,_=fingers(f);frames.append((f,wr,Rh,(CLAV[0]*k,CLAV[1]*k),POLE0.lerp(POLE,ss(k)).normalized(),fg))
# --- bake
src=bpy.data.actions['AN_PlayerV05_Reach'];src.name='SOURCE_Reach';action=bpy.data.actions.new('AN_PlayerV05_Reach');action.use_fake_user=True
report={'bones':len(rest),'frames':N,'fps':30,'max_nonleft_vs_source':0,'per_frame':[]};prev={};visible=0
for f,wr,Rh,clav,pole,fg in frames:
 r.animation_data.action=src;r.animation_data.action_slot=src.slots[0];s.frame_set(f);update();basis={p.name:p.matrix_basis.copy() for p in r.pose.bones};srcm={p.name:p.matrix.copy() for p in r.pose.bones};r.animation_data.action=None
 for p in r.pose.bones:p.matrix_basis=basis[p.name]
 update()
 if f not in(1,N):
  for n in LEFT:r.pose.bones[n].matrix_basis=Matrix()  # Hold's left arm is the rest pose; solve from it every frame
  pose_arm(clav,wr,Rh,pole,fg)
 r.animation_data.action=action
 if action.slots:r.animation_data.action_slot=action.slots[0]
 for p in r.pose.bones:
  e=p.matrix_basis.to_quaternion().to_euler('XYZ',prev[p.name]) if p.name in prev else p.matrix_basis.to_euler('XYZ');p.rotation_euler=e;prev[p.name]=e.copy()
  p.keyframe_insert('location',frame=f);p.keyframe_insert('rotation_euler',frame=f);p.keyframe_insert('scale',frame=f)
 r.animation_data.action=None
 for p in r.pose.bones:
  if p.name not in LEFT:report['max_nonleft_vs_source']=max(report['max_nonleft_vs_source'],max(abs(p.matrix[i][j]-srcm[p.name][i][j]) for i in range(4) for j in range(4)))
 U,L,H=r.pose.bones['upperarm_l'],r.pose.bones['lowerarm_l'],r.pose.bones['hand_l'];tw=lambda n:round(math.degrees(twist(r.pose.bones[n].matrix_basis.to_quaternion())),2)
 v=[H.head,H.tail,r.pose.bones['middle_03_l'].tail];inview=any(q.y-CAM.y>0 and abs((q.x-CAM.x)/(q.y-CAM.y))<1 and abs((q.z-CAM.z)/(q.y-CAM.y))<9/16 for q in v);visible+=inview
 Pw=(H.matrix.to_3x3()@(R['hand_l'].to_3x3().inverted()@RN)).normalized()
 report['per_frame'].append({'frame':f,'elbow_deg':round(math.degrees(math.pi-(U.tail-U.head).angle(L.tail-L.head)),1),'twist_deg':{n:tw(n) for n in ('upperarm_l','lowerarm_l','hand_l')},'hand_in_view':inview,'palm_normal':[round(x,2) for x in Pw],'hand_head':[round(x,3) for x in H.head]})
for fc in action.layers[0].strips[0].channelbags[0].fcurves:
 for k in fc.keyframe_points:k.interpolation='LINEAR'
# --- validate
r.animation_data.action=action;r.animation_data.action_slot=action.slots[0];bm={}
for f in (1,N):
 s.frame_set(f);update();bm[f]=max(abs(p.matrix[i][j]-holdm[p.name][i][j]) for p in r.pose.bones for i in range(4) for j in range(4))
report['frame1_vs_hold'],report['frame31_vs_hold']=bm[1],bm[N];report['hand_in_view_frames']=visible
report['max_twist_deg']={n:max(abs(p['twist_deg'][n]) for p in report['per_frame']) for n in ('upperarm_l','lowerarm_l','hand_l')};report['previous_reach_lowerarm_twist_deg']=171.2
s.frame_set(15);update();report['grab_frame15']={'grab_centre_target':list(GRABC),'hand_head':list(r.pose.bones['hand_l'].head),'palm_normal':report['per_frame'][14]['palm_normal']}
assert len(r.data.bones)==52 and rest=={b.name:[list(row) for row in b.matrix_local] for b in r.data.bones}
assert report['max_nonleft_vs_source']<1e-5 and bm[1]<1e-5 and bm[N]<1e-5,report
s.frame_start=1;s.frame_end=N;s.render.fps=30;s.frame_set(1);bpy.ops.object.select_all(action='DESELECT');r.select_set(True);bpy.context.view_layer.objects.active=r
bpy.ops.export_scene.fbx(filepath=str(OUT/'AN_PlayerV05_Reach.fbx'),use_selection=True,object_types={'ARMATURE'},add_leaf_bones=False,axis_forward='X',axis_up='Z',apply_unit_scale=True,use_armature_deform_only=True,bake_anim=True,bake_anim_use_all_actions=False,bake_anim_use_nla_strips=False,bake_anim_simplify_factor=0)
report['fbx_sha256']=hashlib.sha256((OUT/'AN_PlayerV05_Reach.fbx').read_bytes()).hexdigest();(OUT/'validation.json').write_text(json.dumps(report,indent=1));bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'10_reach-rev01.blend'))
print('RESULT',json.dumps({k:v for k,v in report.items() if k!='per_frame'}),flush=True)
