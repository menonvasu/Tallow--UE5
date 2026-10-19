import bpy,math,json,hashlib
from pathlib import Path
from mathutils import Vector,Matrix,Quaternion,Euler
from mathutils.bvhtree import BVHTree
# Relight with match, revision 02: pencil-stub pinch from the side, eased arc in, scared tremble at the wick, flick out.
# Open RelightMatchRevision01/07_relight-sleeve-revision.blend. Only the left arm chain animates; everything else is Hold frame 1.
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'Tallow/SourceAssets/Player/InteractionV05/RelightMatchRevision02';OUT.mkdir(exist_ok=True)
s=bpy.context.scene;r=bpy.data.objects['RIG_Tallow_Generated'];c=bpy.data.objects['SM_CandleV02_Crooked'];part=bpy.data.objects['SK_Tallow_Generated_Partial']
R={b.name:b.matrix_local.copy() for b in r.data.bones};LEN={b.name:b.length for b in r.data.bones};PAR={b.name:(b.parent.name if b.parent else None) for b in r.data.bones};rest={b.name:[list(row) for row in b.matrix_local] for b in r.data.bones}
WICK=c.matrix_world@Vector((.004,0,.255));LEFT=[n for n in R if n.endswith('_l') and n.split('_')[0] in('clavicle','upperarm','lowerarm','hand','thumb','index','middle','ring','pinky')];FINGERS=[n for n in LEFT if n.split('_')[0] in('thumb','index','middle','ring','pinky')]
def update():bpy.context.view_layer.update()
def qe(e):return Euler([math.radians(v) for v in e],'XYZ').to_quaternion()
# Pinch grip (local XYZ degrees): index+thumb solved so the fingertip bone tails meet (<2.5 mm); middle tucks under the match, ring/pinky curl into the palm.
GRIP={n:qe(e) for n,e in {'index_01_l':(-36,0,0),'index_02_l':(-57.8,0,0),'index_03_l':(-27.5,0,0),'middle_01_l':(-47,0,3),'middle_02_l':(-74,0,0),'middle_03_l':(-42,0,0),'ring_01_l':(-74,0,5),'ring_02_l':(-90,0,0),'ring_03_l':(-55,0,0),'pinky_01_l':(-80,0,8),'pinky_02_l':(-90,0,0),'pinky_03_l':(-55,0,0),'thumb_01_l':(-42.56,6.39,15.06),'thumb_02_l':(-14.52,0,1.46),'thumb_03_l':(-.69,0,0)}.items()}
# Contact: match leaves the pinch out of the palm side (.45 finger axis + .85 palm normal); points at the wick from screen-left (az 5, el 5), thumb on top.
hb=r.data.bones['hand_l'];RD=(hb.tail_local-hb.head_local).normalized();RB=r.data.bones['pinky_01_l'].head_local-r.data.bones['index_01_l'].head_local;RB=(RB-RD*RB.dot(RD)).normalized();RN=RB.cross(RD).normalized();MAXIS=(.45*RD+.85*RN).normalized()
AZ,EL,PSI,CLAV,POLE,TIPGAP=5,5,110,(10,20),Vector((-.4,-.1,-1)),.001
def sample(name,f=1):
 a=bpy.data.actions['AN_PlayerV05_'+name];r.animation_data.action=a;r.animation_data.action_slot=a.slots[0];s.frame_set(f);ps={p.name:p.matrix_basis.copy() for p in r.pose.bones};pm={p.name:p.matrix.copy() for p in r.pose.bones};r.animation_data.action=None;return ps,pm
def apply(ps):
 r.animation_data.action=None
 for p in r.pose.bones:p.matrix_basis=ps[p.name]
 update()
def fk(hand,basis):
 M={'hand_l':hand}
 for d in ['thumb','index','middle','ring','pinky']:
  for j in (1,2,3):n=f'{d}_{j:02d}_l';p=PAR[n];M[n]=M[p]@R[p].inverted()@R[n]@basis.get(n,Quaternion()).to_matrix().to_4x4()
 return M
tail=lambda M,n:M[n]@Vector((0,LEN[n],0))
M0=fk(R['hand_l'],GRIP);ANCHOR=(tail(M0,'index_03_l')+tail(M0,'thumb_03_l'))/2;ANCHOR_H=R['hand_l'].inverted()@ANCHOR  # pinch point in hand_l space
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
def pose_arm(clav,wrist,Rh,pole,w,FL=.6,FU=.85):
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
 for n in FINGERS:r.pose.bones[n].rotation_euler=base[n].to_quaternion().slerp(GRIP[n],w).to_euler('XYZ')
 update();return d.length
# --- key poses
base,basem=sample('Hold');apply(base);Hh=basem['hand_l'];RH0=Hh.to_3x3().normalized()
U0,L0=basem['upperarm_l'],basem['lowerarm_l'];S0,E0,W0=U0.translation,L0.translation,Hh.translation;ax=(W0-S0).normalized();POLE0=((E0-S0)-ax*(E0-S0).dot(ax)).normalized()
MV=Vector((math.cos(math.radians(EL))*math.cos(math.radians(AZ)),math.cos(math.radians(EL))*math.sin(math.radians(AZ)),math.sin(math.radians(EL))))
RHC=(Quaternion(MV,math.radians(PSI))@MAXIS.rotation_difference(MV)).to_matrix()@R['hand_l'].to_3x3();AC=WICK-MV*(.041+TIPGAP)
D,P=RHC@(R['hand_l'].to_3x3().inverted()@RD),RHC@(R['hand_l'].to_3x3().inverted()@RN)
def rot(axis,deg,M=RHC):return Matrix.Rotation(math.radians(deg),3,axis)@M
WC=AC-RHC@ANCHOR_H;T=P.cross(D)
# wrist waypoints (path parameter u, hand_l head, hand rotation): swing forward below the view, rise in from lower-left, settle at the wick; exit pulls back left, drops in front, hangs
IN=[(0,W0,RH0),(.5,Vector((-.045,.24,.86)),RH0.to_quaternion().slerp(RHC.to_quaternion(),.5).to_matrix()),(.8,WC+Vector((-.03,0,-.035)),RHC),(1,WC,RHC)]
OUTW=[(0,WC,RHC),(.15,WC+Vector((-.035,-.008,-.012)),RHC),(.35,WC+Vector((-.075,-.02,-.045)),rot(T,8)),(.65,Vector((-.12,.24,.72)),RHC.to_quaternion().slerp(RH0.to_quaternion(),.55).to_matrix()),(1,W0,RH0)]
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
INT=[(1,0),(4,.2),(7,.55),(10,.88),(13,1)];OUTT=[(26,0),(28,.15),(31,.33),(33,.55),(37,1)]  # approach in view from ~frame 6, exit shakes in view to ~frame 31, leaves view ~frame 33
ss=lambda t:t*t*(3-2*t)
def tremble(f):
 # 1-2 mm smooth nervous tremble while the match is at the wick (frames 13..26), zero at both ends
 w=math.sin(math.pi*(f-13)/13)**2 if 13<f<26 else 0;t=f/30
 return Vector((math.sin(2*math.pi*4.3*t+.4)+.5*math.sin(2*math.pi*7.1*t+1.9),.6*math.sin(2*math.pi*3.7*t+2.6),math.sin(2*math.pi*5.3*t+.9)+.4*math.sin(2*math.pi*8.2*t+.3)))*.0011*w,math.sin(2*math.pi*4.9*t+1.2)*.6*w
def flick(f):
 # wrist shake-out: up/down half-cycles from frame 28, damped out by frame 35
 if f<=27.3 or f>=36:return 0
 t=f-27.3;return 40*math.sin(math.pi*t/2.4)*math.exp(-t/4.2)*min(1,t/1.2)
frames=[]
for f in range(1,38):
 if f<=13:u=warp(INT,f);a,Rh=path(IN,u);clav=(CLAV[0]*ss(min(1,u/.85)),CLAV[1]*ss(min(1,u/.85)));pole=POLE0.lerp(POLE,ss(u));w=ss(min(1,(f-1)/6))
 elif f<=26:a,Rh=WC.copy(),RHC.copy();clav,pole,w=CLAV,POLE,1
 else:u=warp(OUTT,f);a,Rh=path(OUTW,u);clav=(CLAV[0]*(1-ss(u)),CLAV[1]*(1-ss(u)));pole=POLE.lerp(POLE0,ss(u));w=1-ss(max(0,(f-32)/5))
 dt,ang=tremble(f);wrist=a+dt;fl=flick(f);Rh=rot((T+.4*D).normalized(),fl,rot(D,ang,Rh))  # tremble/flick pivot at the wrist; flick waggles the match up/down on screen
 frames.append((f,wrist,Rh,clav,pole.normalized(),w,fl))
# --- bake
old=bpy.data.actions.get('AN_PlayerV05_RelightMatch')
if old:old.name='SOURCE_RelightMatchRevision01'
action=bpy.data.actions.new('AN_PlayerV05_RelightMatch');action.use_fake_user=True;report={'bones':len(rest),'frames':37,'fps':30,'max_right_bone_matrix_delta':0,'max_nonleft_bone_matrix_delta':0,'per_frame':[]};prev={}
for f,wrist,Rh,clav,pole,w,fl in frames:
 apply(base)
 reach=pose_arm(clav,wrist,Rh,pole,w) if f not in(1,37) else (W0-S0).length
 r.animation_data.action=action
 if action.slots:r.animation_data.action_slot=action.slots[0]
 for p in r.pose.bones:
  e=p.matrix_basis.to_quaternion().to_euler('XYZ',prev[p.name]) if p.name in prev else p.matrix_basis.to_euler('XYZ');p.rotation_euler=e;prev[p.name]=e.copy()
  p.keyframe_insert('location',frame=f);p.keyframe_insert('rotation_euler',frame=f);p.keyframe_insert('scale',frame=f)
 r.animation_data.action=None
 for p in r.pose.bones:
  dm=max(abs(p.matrix[i][j]-basem[p.name][i][j]) for i in range(4) for j in range(4))
  if p.name not in LEFT:report['max_nonleft_bone_matrix_delta']=max(report['max_nonleft_bone_matrix_delta'],dm)
  if p.name.endswith('_r'):report['max_right_bone_matrix_delta']=max(report['max_right_bone_matrix_delta'],dm)
 tw=lambda n:round(math.degrees(twist(r.pose.bones[n].matrix_basis.to_quaternion())),2)
 report['per_frame'].append({'frame':f,'shoulder_wrist_m':round(reach,4),'elbow_deg':round(math.degrees(math.pi-(r.pose.bones['upperarm_l'].tail-r.pose.bones['upperarm_l'].head).angle(r.pose.bones['lowerarm_l'].tail-r.pose.bones['lowerarm_l'].head)),1),'twist_deg':{'upperarm_l':tw('upperarm_l'),'lowerarm_l':tw('lowerarm_l'),'hand_l':tw('hand_l')},'grip':round(w,3),'flick_deg':round(fl,1)})
for fc in action.layers[0].strips[0].channelbags[0].fcurves:
 for k in fc.keyframe_points:k.interpolation='LINEAR'
# --- validate
r.animation_data.action=action;r.animation_data.action_slot=action.slots[0];bm={}
for f in (1,37):
 s.frame_set(f);update();bm[f]=max(abs(p.matrix[i][j]-basem[p.name][i][j]) for p in r.pose.bones for i in range(4) for j in range(4))
report['frame1_vs_hold_max_delta'],report['frame37_vs_hold_max_delta']=bm[1],bm[37]
s.frame_set(20);update();pi,pt=r.pose.bones['index_03_l'].tail.copy(),r.pose.bones['thumb_03_l'].tail.copy();pinch=(pi+pt)/2;dirn=(WICK-pinch).normalized()
report['contact_frame20']={'pinch_tip_gap_cm':(pi-pt).length*100,'match_tip_to_wick_cm':(pinch+dirn*.041-WICK).length*100,'match_direction':list(dirn),'pinch':list(pinch)}
REL=(r.matrix_world@r.pose.bones['hand_l'].matrix).inverted()@Matrix.Translation(pinch)@dirn.to_track_quat('Z','Y').to_matrix().to_4x4()  # match KeepWorld-attached to hand_l at 19/30 s, as in game
cbm=c.evaluated_get(bpy.context.evaluated_depsgraph_get()).to_mesh();cbvh=BVHTree.FromPolygons([c.matrix_world@v.co for v in cbm.vertices],[list(p.vertices) for p in cbm.polygons]);gid={g.index for g in part.vertex_groups if g.name in LEFT and g.name!='clavicle_l'}
arm=[v.index for v in part.data.vertices if any(g.group in gid and g.weight>.2 for g in v.groups)];clear=[];tips=[]
def signed(bvh,co):
 hit=bvh.find_nearest(co);return 1 if hit[0] is None else (hit[3] if (co-hit[0]).dot(hit[1])>=0 else -hit[3])
for f in range(1,38):
 s.frame_set(f);dg=bpy.context.evaluated_depsgraph_get();me=part.evaluated_get(dg).to_mesh();mw=part.matrix_world;inside=0;mn=1
 Mm=r.matrix_world@r.pose.bones['hand_l'].matrix@REL;tips.append(Mm@Vector((0,0,.041)));shaft=[Mm@Vector((0,0,z/1000)) for z in range(14,37)]  # shaft clear of the pinch (base 14 mm) and of the head touching the wick
 hb=BVHTree.FromPolygons([mw@v.co for v in me.vertices],[list(p.vertices) for p in me.polygons]);mc=min(signed(cbvh,p) for p in shaft);mh=min(signed(hb,p) for p in shaft)
 for i in arm:
  co=mw@me.vertices[i].co;hit=cbvh.find_nearest(co)
  if hit[0] is None:continue
  dd=hit[3]
  if (co-hit[0]).dot(hit[1])<0:inside+=1;dd=-dd
  mn=min(mn,dd)
 clear.append({'frame':f,'min_arm_to_candle_cm':round(mn*100,3),'arm_vertices_inside_candle':inside,'match_shaft_to_candle_cm':round(mc*100,3),'match_shaft_to_skin_cm':round(mh*100,3),'match_tip_to_wick_cm':round((tips[-1]-WICK).length*100,3)});part.evaluated_get(dg).to_mesh_clear()
report['candle_clearance']=clear;report['min_arm_to_candle_cm']=min(x['min_arm_to_candle_cm'] for x in clear);report['min_match_shaft_to_candle_cm']=min(x['match_shaft_to_candle_cm'] for x in clear)
hold=tips[12:26];mean=sum(hold,Vector())/len(hold);report['tremble_tip_max_mm_frames13_26']=max((t-mean).length for t in hold)*1000
assert len(r.data.bones)==52 and rest=={b.name:[list(row) for row in b.matrix_local] for b in r.data.bones}
assert report['max_right_bone_matrix_delta']<1e-5 and report['max_nonleft_bone_matrix_delta']<1e-5 and bm[1]<1e-5 and bm[37]<1e-5 and report['contact_frame20']['pinch_tip_gap_cm']<.35 and report['contact_frame20']['match_tip_to_wick_cm']<.6,report
s.frame_start=1;s.frame_end=37;s.render.fps=30;s.frame_set(20);bpy.ops.object.select_all(action='DESELECT');r.select_set(True);bpy.context.view_layer.objects.active=r
bpy.ops.export_scene.fbx(filepath=str(OUT/'AN_PlayerV05_RelightMatch.fbx'),use_selection=True,object_types={'ARMATURE'},add_leaf_bones=False,axis_forward='X',axis_up='Z',apply_unit_scale=True,use_armature_deform_only=True,bake_anim=True,bake_anim_use_all_actions=False,bake_anim_use_nla_strips=False,bake_anim_simplify_factor=0)
report['fbx_sha256']=hashlib.sha256((OUT/'AN_PlayerV05_RelightMatch.fbx').read_bytes()).hexdigest();(OUT/'validation.json').write_text(json.dumps(report,indent=1));bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'08_relight-match-v2.blend'))
print('RESULT',json.dumps({k:v for k,v in report.items() if k not in('per_frame','candle_clearance')}),flush=True)
