import bpy,math,json,hashlib,os
from pathlib import Path
from mathutils import Vector,Matrix,Quaternion,Euler
from mathutils.bvhtree import BVHTree
# Tool clips revision 01: ToolHold (2 s loop), CrowbarPry (2 s loop, two lever strokes), FuseInsert (one-shot, ends seated), Pickup (candle pickup, ends at Hold).
# Open RelightMatchRevision01/07_relight-sleeve-revision.blend. One oblique power grip (axis from the hypothenar to the thumb-index web) carries both tools;
# the game attaches the tool rigidly to hand_l from ToolHold t=0, so this script also writes the camera-relative tool transforms for that pose.
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'Tallow/SourceAssets/Player/InteractionV05/ToolRevision01';OUT.mkdir(exist_ok=True);CLIPS=os.environ.get('TOOL_CLIPS','ToolHold,CrowbarPry,FuseInsert,Pickup').split(',')
s=bpy.context.scene;r=bpy.data.objects['RIG_Tallow_Generated'];c=bpy.data.objects['SM_CandleV02_Crooked'];part=bpy.data.objects['SK_Tallow_Generated_Partial']
R={b.name:b.matrix_local.copy() for b in r.data.bones};LEN={b.name:b.length for b in r.data.bones};PAR={b.name:(b.parent.name if b.parent else None) for b in r.data.bones};rest={b.name:[list(row) for row in b.matrix_local] for b in r.data.bones}
ARM=('clavicle','upperarm','lowerarm','hand','thumb','index','middle','ring','pinky');SIDE={sd:[n for n in R if n.endswith('_'+sd) and n.split('_')[0] in ARM] for sd in 'lr'};FING={sd:[n for n in SIDE[sd] if n.split('_')[0] in ARM[4:]] for sd in 'lr'}
CAM=Vector((0,-.2,1.1))
def update():bpy.context.view_layer.update()
def qe(e):return Euler([math.radians(v) for v in e],'XYZ').to_quaternion()
with bpy.data.libraries.load(str(ROOT/'Tallow/SourceAssets/Production/ToolProps/ToolProps_Production.blend'),link=False) as(src,dst):dst.objects=['SM_ToolV02_Crowbar','SM_ToolV02_Fuse']
TOOLS=dict(zip(('crowbar','fuse'),dst.objects))
# --- grip: fingers solved to wrap a 2.56 cm cylinder (fuse) at 7.5 mm pad thickness, joints coupled DIP = .7 PIP; thumb lies along the shaft.
GRIP={n:qe(e) for n,e in {'index_01_l':(-43.36,0,0),'index_02_l':(-44.77,0,0),'index_03_l':(-31.34,0,0),'middle_01_l':(-73.52,0,0),'middle_02_l':(-46.25,0,0),'middle_03_l':(-32.38,0,0),'ring_01_l':(-89.69,0,0),'ring_02_l':(-37.58,0,0),'ring_03_l':(-26.3,0,0),'pinky_01_l':(-110,0,0),'pinky_02_l':(-81.72,0,0),'pinky_03_l':(-57.2,0,0),'thumb_01_l':(9.45,-33.91,5.08),'thumb_02_l':(.94,0,23.13),'thumb_03_l':(0,0,0)}.items()}
GA=Vector((.36698,.63122,-.68329)).normalized();GC=Vector((-.28083,.03988,.57333))  # grip axis (toward the thumb-index web) and centre, rest armature space
TOH=R['hand_l'].inverted()  # rest armature -> hand_l bone space
def tool_rel(name,roll_ref):
 """tool matrix in hand_l bone space: local +Z on the grip axis (hook / fuse top toward the web), grip centre 12.5 cm up the crowbar (fist over its rubber wrap, 10-17.5 cm, near the straight end) or 6 cm up the fuse; roll so local +X is as close to roll_ref (rest space) as possible."""
 z=GA;x=(roll_ref-z*roll_ref.dot(z)).normalized();y=z.cross(x);M=Matrix((x,y,z)).transposed().to_4x4();M.translation=GC-z*(.125 if name=='crowbar' else .06);return TOH@M
def tool_world(rel,hand):return r.matrix_world@hand@rel
# --- arm solver (either side): clavicle (elevation,protraction); hand head at `wrist` with world rotation Rh; IK toward the pole; axial twist split forearm/wrist, lifted off the elbow into the upper arm
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
def pose_arm(sd,clav,wrist,Rh,pole,fing=None,FL=.6,FU=.85):
 sg=1 if sd=='l' else -1;r.pose.bones['clavicle_'+sd].rotation_euler=Euler((math.radians(clav[0])*sg,0,math.radians(-clav[1])*sg),'XYZ');update()
 S=r.pose.bones['upperarm_'+sd].head.copy();L1,L2=LEN['upperarm_'+sd],LEN['lowerarm_'+sd];d=wrist-S;dl=min(d.length,L1+L2-1e-4)
 ax=d.normalized();a=(L1*L1-L2*L2+dl*dl)/(2*dl);pv=(pole-ax*pole.dot(ax)).normalized();aim('upperarm_'+sd,S+ax*a+pv*math.sqrt(max(0,L1*L1-a*a)));aim('lowerarm_'+sd,S+ax*dl)
 U,L=r.pose.bones['upperarm_'+sd],r.pose.bones['lowerarm_'+sd];Um,Lm=U.matrix.copy(),L.matrix.copy();Hm=Matrix.Translation(L.tail)@Rh.to_4x4();la=(L.tail-L.head).normalized();ua=(U.tail-U.head).normalized()
 hand_tw=lambda phi:twist(local('hand_'+sd,Hm,Matrix.Rotation(phi,4,la)@Lm));phi=secant(hand_tw,(1-FL)*hand_tw(0))
 L.matrix=Matrix.Translation(L.head)@Matrix.Rotation(phi,4,la)@Matrix.Translation(-L.head)@Lm;update();Lm=L.matrix.copy()
 elbow_tw=lambda th:twist(local('lowerarm_'+sd,Lm,Matrix.Translation(U.head)@Matrix.Rotation(th,4,ua)@Matrix.Translation(-U.head)@Um));th=secant(elbow_tw,(1-FU)*elbow_tw(0))
 U.matrix=Matrix.Translation(U.head)@Matrix.Rotation(th,4,ua)@Matrix.Translation(-U.head)@Um;update();L.matrix=Lm;update()
 H=r.pose.bones['hand_'+sd];H.matrix=Matrix.Translation(H.head)@Rh.to_4x4();update()
 for n,q in (fing or {}).items():r.pose.bones[n].rotation_euler=q.to_euler('XYZ')
 update();return d.length
def hand_from_tool(Tw,rel):M=Tw@rel.inverted();return M.translation.copy(),M.to_3x3().normalized()  # hand_l world head + rotation that puts the tool at Tw
def place(Gw,Aw,Xw,name):
 """tool world matrix with grip centre at Gw, tool +Z along Aw, +X toward Xw"""
 z=Aw.normalized();x=(Xw-z*Xw.dot(z)).normalized();M=Matrix((x,z.cross(x),z)).transposed().to_4x4();M.translation=Gw-z*(.125 if name=='crowbar' else .06);return M
# --- splines and timing
def cr(us,vs,u):
 i=max(j for j in range(len(us)-1) if us[j]<=u) if u<us[-1] else len(us)-2;u0,u1=us[i],us[i+1];t=(u-u0)/(u1-u0)
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
 fs,us=[k[0] for k in keys],[k[1] for k in keys];n=len(fs);sec=[(us[i+1]-us[i])/(fs[i+1]-fs[i]) for i in range(n-1)];m=[0]+[0 if sec[i-1]*sec[i]<=0 else min((us[i+1]-us[i-1])/(fs[i+1]-fs[i-1]),3*sec[i-1],3*sec[i]) for i in range(1,n-1)]+[0]
 i=max(j for j in range(n-1) if fs[j]<=f) if f<fs[-1] else n-2;h=fs[i+1]-fs[i];t=(f-fs[i])/h
 return us[i]*(2*t**3-3*t*t+1)+m[i]*h*(t**3-2*t*t+t)+us[i+1]*(-2*t**3+3*t*t)+m[i+1]*h*(t**3-t*t)
ss=lambda t:(lambda x:x*x*(3-2*x))(max(0,min(1,t)))
def sample(name,f=1):
 a=bpy.data.actions['AN_PlayerV05_'+name];r.animation_data.action=a;r.animation_data.action_slot=a.slots[0];s.frame_set(f);update();ps={p.name:p.matrix_basis.copy() for p in r.pose.bones};pm={p.name:p.matrix.copy() for p in r.pose.bones};r.animation_data.action=None;return ps,pm
HOLDB,HOLDM=sample('Hold')
hb=r.data.bones['hand_l'];RD=(hb.tail_local-hb.head_local).normalized();RB=r.data.bones['pinky_01_l'].head_local-r.data.bones['index_01_l'].head_local;RB=(RB-RD*RB.dot(RD)).normalized();RN=RB.cross(RD).normalized()
# --- ToolHold frame 1 (the calibration pose): grip low-left in view, bar up and forward-right with the hook leading; roll PSI about the bar from a reach/wrist scan
TH_G,TH_A,TH_PSI,TH_ROLL,TH_CLAV,POLE_L=Vector((-.16,.30,.90)),Vector((.287,.497,.819)).normalized(),345,Vector((0,.6,-1)),(5,8),Vector((-.6,-.2,-1))
TH_RQ=(Quaternion(TH_A,math.radians(TH_PSI))@GA.rotation_difference(TH_A)).to_matrix()  # rest -> world rotation of the hand
REL={n:tool_rel(n,TH_RQ.inverted()@TH_ROLL) for n in TOOLS}
TH_RH=TH_RQ@R['hand_l'].to_3x3();TH_W=TH_G+TH_RQ@(R['hand_l'].translation-GC)
def fuse_world(top,n,roll_deg,ref):
 """fuse world matrix with its top end at `top`, axis n, rolled roll_deg about n from ref"""
 x=(ref-n*ref.dot(n)).normalized();x=(Matrix.Rotation(math.radians(roll_deg),3,n)@x);M=Matrix((x,n.cross(x),n)).transposed().to_4x4();M.translation=top-n*.12;return M
# === CLIPS ===
def grip_fingers(open_ring=0.,open_all=0.):
 """GRIP with ring/pinky lifted off by open_ring (0..1, the three-finger fuse hold) and every finger eased open by open_all (0..1, letting go)"""
 out={}
 for n,q in GRIP.items():
  d=n.split('_')[0];k=(open_ring if d in('ring','pinky') else 0)*(.45 if n.endswith('01_l') else .15)+open_all*(.18 if d!='thumb' else .12);out[n]=q.slerp(Quaternion(),k)  # knuckle opens first so the finger swings clear of the fuse
 return out
# --- ToolHold: 2 s loop, one slow breath (exactly periodic: frame 61 == frame 1)
def toolhold(f):
 t=2*math.pi*(f-1)/60;z=lambda ph:math.sin(t+ph)-math.sin(ph)  # zero at frame 1, so frame 1 is exactly the calibration pose
 w=TH_W+Vector((.0006*z(.7),.0008*z(.3),.0014*z(0)));Rh=Matrix.Rotation(math.radians(1.2*z(.5)),3,TH_A)@TH_RH
 return {'l':(TH_CLAV,w,Rh,POLE_L,GRIP)}
# --- CrowbarPry: 2 s loop, two lever strokes about a fixed fulcrum (the hook's heel bitten behind the plank, in front and below the grip).
# bite (claw wedges 2.5 cm in, bar steepens), heave the handle down and back with the shoulder dropping, recoil, lift back to the bite.
# lever plane 40 deg right of forward so the bar reads beside the forearm; hook heel 25.5 cm up the bar from the grip; hand stays in view (from a reach/wrist/visibility scan)
PRY_F,PRY_H,PRY_L,PRY_ROLL=Vector((.08,.42,.92)),Vector((math.sin(math.radians(40)),math.cos(math.radians(40)),0)),.255,(170,160)
PRY_KEYS=[(0,46,0,0),(5,50,.02,.15),(16,-4,.02,1),(19,3,.02,.75),(29,46,0,0),(30,46,0,0)]  # stroke frame, bar angle below horizontal (deg), bite depth, effort
def pry_state(t):
 return [warp([(k[0],k[i]) for k in PRY_KEYS],t) for i in (1,2,3)]
def crowbarpry(f):
 t=(f-1)%30;phi,bite,eff=pry_state(t);ph=math.radians(phi);u=(PRY_H*math.cos(ph)+Vector((0,0,-math.sin(ph)))).normalized()
 x0=(Vector((0,0,-1))-u*(-u.z)).normalized();roll=PRY_ROLL[1]+(PRY_ROLL[0]-PRY_ROLL[1])*max(0,phi)/50
 Tw=place(PRY_F-u*(PRY_L-bite),u,Matrix.Rotation(math.radians(roll),3,u)@x0,'crowbar');w,Rh=hand_from_tool(Tw,REL['crowbar'])
 return {'l':((5-11*eff,8+6*eff),w,Rh,POLE_L,GRIP)}
# --- FuseInsert: one-shot, 34 frames. Raise from ToolHold into line with the slot, push in 7.5 cm with a 25 deg twist, click, ease the fingers open, seated.
SLOT,SLOT_N=Vector((-.03,.30,.95)),Vector((math.sin(math.radians(25)),math.cos(math.radians(25)),.05)).normalized()  # slot mouth in front at chest height, push 25 deg right of forward so it reads from the camera; seated 4.5 cm in
SLOT_REF=Matrix.Rotation(math.radians(-5),3,SLOT_N)@(Vector((0,0,1))-SLOT_N*SLOT_N.z).normalized()
FU0=tool_world(REL['fuse'],Matrix.Translation(TH_W)@TH_RH.to_4x4())  # fuse at ToolHold frame 1 (armature at origin)
def fuseinsert(f):
 push=[(1,0),(12,0),(22,1),(34,1)];tw=[(1,0),(16,0),(22,1),(34,1)];click=.003*(math.sin(math.pi*(f-22)/2) if 22<f<24 else 0)
 u=warp([(1,0),(5,.25),(9,.75),(12,1),(34,1)],f);d=warp(push,f);top_pre=SLOT-SLOT_N*.03
 if f<=12:
  Mpre=fuse_world(top_pre,SLOT_N,0,SLOT_REF);top0=FU0@Vector((0,0,.12));R0=FU0.to_3x3().normalized();z0=R0.col[2]
  # swing-twist interpolation: the fuse axis travels straight from carry to the slot line, its roll eases separately
  ax=z0.slerp(SLOT_N,u) if hasattr(z0,'slerp') else z0.lerp(SLOT_N,u).normalized();Rs=z0.rotation_difference(ax).to_matrix()@R0;Rs1=z0.rotation_difference(SLOT_N).to_matrix()@R0
  dr=math.atan2(SLOT_N.dot(Rs1.col[0].cross(Mpre.to_3x3().col[0])),Rs1.col[0].dot(Mpre.to_3x3().col[0]));rot=Matrix.Rotation(dr*u,3,ax)@Rs
  pos=cr([0,.5,1],[top0,(top0+top_pre)/2+Vector((-.02,-.02,.03)),top_pre],u);Tw=rot.to_4x4();Tw.translation=pos-rot.col[2]*.12
 else:Tw=fuse_world(top_pre+SLOT_N*(.075*d+click),SLOT_N,25*warp(tw,f),SLOT_REF)
 w,Rh=hand_from_tool(Tw,REL['fuse']);k=ss((f-1)/7);fo=ss((f-25)/8)
 return {'l':((5+3*k,8+6*k),w,Rh,POLE_L,grip_fingers(k,fo))}
# --- Pickup (candle): 46 frames. Right hand starts low and forward where it has just closed on the candle at the crate (out of view), lifts it
# up into view on an easing arc and settles exactly in Hold; the grip (fingers) and the candle's uprightness are Hold's throughout.
HR0=HOLDM['hand_r'].translation.copy();HRR=HOLDM['hand_r'].to_3x3().normalized();POLE_R=Vector((.6,-.2,-1))
def pickup(f):
 u=warp([(1,0),(4,0),(16,.3),(30,.75),(40,.95),(46,1)],f);w=cr([0,.5,1],[Vector((.12,.20,.64)),Vector((.17,.15,.80)),HR0],u)
 tilt=12*(1-ss(u/.85));Rh=Matrix.Rotation(math.radians(tilt),3,Vector((1,0,0)))@HRR;k=1-ss(u/.9)
 return {'r':((-4*k,6*k),w,Rh,POLE_R,None,.6*(1-u))}  # wrist twist split fades out so the last frames land exactly on Hold
SPEC={'ToolHold':(61,toolhold,True),'CrowbarPry':(61,crowbarpry,True),'FuseInsert':(34,fuseinsert,False),'Pickup':(46,pickup,False)}
# --- bake
report={'bones':len(rest),'fps':30,'clips':{}};prev={}
def tool_pen(name,Tw,me,mw,verts):
 o=TOOLS[name];tm=o.data;tb=BVHTree.FromPolygons([Tw@v.co for v in tm.vertices],[list(p.vertices) for p in tm.polygons]);deep=0;n=0
 for i in verts:
  co=mw@me.vertices[i].co;h=tb.find_nearest(co)
  if h[0] is not None and h[3]<.012 and (co-h[0]).dot(h[1])<0:deep=max(deep,h[3]);n+=h[3]>.001
 return deep,n
HV={sd:[v.index for v in part.data.vertices if any(part.vertex_groups[g.group].name in FING[sd]+['hand_'+sd] and g.weight>.3 for g in v.groups)] for sd in 'lr'}
cbm=c.data;CB=lambda:BVHTree.FromPolygons([c.matrix_world@v.co for v in cbm.vertices],[list(p.vertices) for p in cbm.polygons])
for name in CLIPS:
 N,fn,loop=SPEC[name];src=bpy.data.actions['AN_PlayerV05_'+name];src.name='SOURCE_'+name;action=bpy.data.actions.new('AN_PlayerV05_'+name);action.use_fake_user=True
 rep={'frames':N,'seconds':round((N-1)/30,4),'loop':loop,'max_body_vs_source':0,'max_right_vs_source':0,'per_frame':[]};first=None;prev={}
 for f in range(1,N+1):
  r.animation_data.action=src;r.animation_data.action_slot=src.slots[0];s.frame_set(min(f,int(src.frame_range[1])));update();basis={p.name:p.matrix_basis.copy() for p in r.pose.bones};srcm={p.name:p.matrix.copy() for p in r.pose.bones};r.animation_data.action=None
  for p in r.pose.bones:p.matrix_basis=basis[p.name]
  if name=='Pickup':
   for n in SIDE['l']+SIDE['r']:r.pose.bones[n].matrix_basis=HOLDB[n].copy()  # Pickup: start from Hold for both arms (old clip moved the right arm differently)
  update()
  if loop and f==N:
   for p in r.pose.bones:p.matrix_basis=first[p.name]
   update()
  elif name=='Pickup' and f==N:pass  # last frame is exactly Hold
  else:
   for sd,(clav,w,Rh,pole,fing,*fl) in fn(f).items():
    for n in SIDE[sd]:
     if sd=='l' or n not in FING['r']:r.pose.bones[n].matrix_basis=Matrix() if sd=='l' else HOLDB[n].copy()
    update();reach=pose_arm(sd,clav,w,Rh,pole,fing,*((fl[0],0.) if sd=='r' else ()))  # right arm keeps Hold's elbow roll so Pickup lands exactly on Hold
  if f==1:first={p.name:p.matrix_basis.copy() for p in r.pose.bones}
  r.animation_data.action=action
  if action.slots:r.animation_data.action_slot=action.slots[0]
  for p in r.pose.bones:
   e=p.matrix_basis.to_quaternion().to_euler('XYZ',prev[p.name]) if p.name in prev else p.matrix_basis.to_euler('XYZ');p.rotation_euler=e;prev[p.name]=e.copy()
   p.keyframe_insert('location',frame=f);p.keyframe_insert('rotation_euler',frame=f);p.keyframe_insert('scale',frame=f)
  r.animation_data.action=None
  for p in r.pose.bones:
   if p.name not in SIDE['l'] and p.name not in SIDE['r']:rep['max_body_vs_source']=max(rep['max_body_vs_source'],max(abs(p.matrix[i][j]-srcm[p.name][i][j]) for i in range(4) for j in range(4)))
   if name!='Pickup' and p.name in SIDE['r']:rep['max_right_vs_source']=max(rep['max_right_vs_source'],max(abs(p.matrix[i][j]-srcm[p.name][i][j]) for i in range(4) for j in range(4)))
  P=r.pose.bones;tw=lambda n:round(math.degrees(twist(P[n].matrix_basis.to_quaternion())),2);sd='r' if name=='Pickup' else 'l';U,L,H=P['upperarm_'+sd],P['lowerarm_'+sd],P['hand_'+sd]
  vis=[q for q in (H.head,H.tail,P['middle_02_'+sd].tail) if (q-CAM).y>.05 and abs((q-CAM).x/(q-CAM).y)<1 and abs((q-CAM).z/(q-CAM).y)<9/16]
  rep['per_frame'].append({'frame':f,'elbow_deg':round(math.degrees(math.pi-(U.tail-U.head).angle(L.tail-L.head)),1),'twist_deg':{n:tw(n) for n in ('upperarm_'+sd,'lowerarm_'+sd,'hand_'+sd)},'hand_head':[round(x,3) for x in H.head],'hand_in_view':len(vis)>=2})
 for fc in action.layers[0].strips[0].channelbags[0].fcurves:
  for k in fc.keyframe_points:k.interpolation='LINEAR'
 rep['max_twist_deg']={n:max(abs(p['twist_deg'][n]) for p in rep['per_frame']) for n in rep['per_frame'][0]['twist_deg']};rep['hand_in_view_frames']=sum(p['hand_in_view'] for p in rep['per_frame'])
 report['clips'][name]=rep
# --- validation that needs the evaluated mesh: tool penetration, tool-candle clearance, endpoints
def clipmats(name,f):
 a=bpy.data.actions['AN_PlayerV05_'+name];r.animation_data.action=a;r.animation_data.action_slot=a.slots[0];s.frame_set(f);update();return {p.name:p.matrix.copy() for p in r.pose.bones}
dmax=lambda A,B,names=None:max(abs(A[n][i][j]-B[n][i][j]) for n in (names or A) for i in range(4) for j in range(4))
for name in CLIPS:
 rep=report['clips'][name];N=rep['frames'];tools=['fuse'] if name=='FuseInsert' else [] if name=='Pickup' else ['crowbar','fuse'] if name=='ToolHold' else ['crowbar'];pens={t:[0,0] for t in tools};cand=1;rin=0
 for f in range(1,N+1,2 if N>40 else 1):
  m=clipmats(name,f);dg=bpy.context.evaluated_depsgraph_get();me=part.evaluated_get(dg).to_mesh();mw=part.matrix_world
  for t in tools:
   Tw=tool_world(REL[t],m['hand_l']);d,n=tool_pen(t,Tw,me,mw,HV['l']);pens[t]=[max(pens[t][0],d),max(pens[t][1],n)]
   cb=CB();cand=min(cand,min(cb.find_nearest(Tw@v.co)[3] for v in list(TOOLS[t].data.vertices)[::3]))
  if name=='Pickup':
   cb=CB();rin=max(rin,sum(1 for i in HV['r'] if (lambda co,h:h[0] is not None and h[3]<.01 and (co-h[0]).dot(h[1])<-.001)(mw@me.vertices[i].co,cb.find_nearest(mw@me.vertices[i].co))))
  part.evaluated_get(dg).to_mesh_clear()
 rep['tool_skin_penetration_mm']={t:round(v[0]*1000,2) for t,v in pens.items()};rep['tool_vertices_over_1mm_inside']={t:v[1] for t,v in pens.items()}
 if tools:rep['min_tool_to_candle_cm']=round(cand*100,2)
 if name=='Pickup':rep['right_hand_vertices_inside_candle']=rin
 m1,mN=clipmats(name,1),clipmats(name,N);rep['frame1_vs_lastframe']=dmax(m1,mN);th1=clipmats('ToolHold',1) if 'ToolHold' in CLIPS else None
 if name=='FuseInsert' and th1:rep['frame1_vs_toolhold_frame1']=dmax(m1,th1)
 if name=='Pickup':rep['frame46_vs_hold_frame1']=dmax(mN,HOLDM);rep['frame1_left_vs_hold']=dmax(m1,HOLDM,SIDE['l'])
 if name=='CrowbarPry':
  hs=[Vector(p['hand_head']) for p in rep['per_frame']];rep['pry_hand_travel_cm']=round(max((a-b).length for a in hs for b in hs)*100,1)
# --- contract: camera-relative tool transforms at ToolHold frame 1 (review_relight_sleeve.py conversion) + verified Unreal rotator
def ue_quat_from_rot(p,y,ro):
 sp,cp=math.sin(math.radians(p)/2),math.cos(math.radians(p)/2);sy,cy=math.sin(math.radians(y)/2),math.cos(math.radians(y)/2);sr,cr_=math.sin(math.radians(ro)/2),math.cos(math.radians(ro)/2)
 return (cr_*sp*sy-sr*cp*cy,-cr_*sp*cy-sr*cp*sy,cr_*cp*sy-sr*sp*cy,cr_*cp*cy+sr*sp*sy)  # FRotator::Quaternion (UE5)
def ue_rot_from_quat(x,y,z,w):
 st=z*x-w*y;yaw=math.degrees(math.atan2(2*(w*z+x*y),1-2*(y*y+z*z)))
 if st<-.4999995:return -90.,yaw,math.degrees(math.atan2(-2*(w*x+y*z),1-2*(x*x+y*y)))
 if st>.4999995:return 90.,yaw,math.degrees(math.atan2(-2*(w*x+y*z),1-2*(x*x+y*y)))
 return math.degrees(math.asin(2*st)),yaw,math.degrees(math.atan2(-2*(w*x+y*z),1-2*(x*x+y*y)))  # FQuat::Rotator (UE5)
contract={'calibrate_clip':'ToolHold','time_seconds':0.0,'camera':'Blender (0,-.20,1.10) looking +Y; UE camera space X fwd, Y right, Z up','tools':{}}
if 'ToolHold' in CLIPS:
 m=clipmats('ToolHold',1);conv=Matrix(((0,1,0),(1,0,0),(0,0,1)));asset=Matrix.Diagonal((1,-1,1))
 for t,o in TOOLS.items():
  Tw=tool_world(REL[t],m['hand_l']);p=Tw.translation;q=(conv@Tw.to_3x3().normalized()@asset).to_quaternion();pyr=ue_rot_from_quat(q.x,q.y,q.z,q.w);back=Quaternion((lambda v:(v[3],v[0],v[1],v[2]))(ue_quat_from_rot(*pyr)))
  contract['tools'][o.name]={'camera_location_cm':[(p.y+.2)*100,p.x*100,(p.z-1.1)*100],'camera_quaternion_xyzw':[q.x,q.y,q.z,q.w],'unreal_rotator_pitch_yaw_roll':list(pyr),'rotator_roundtrip_quat_error':1-abs(back.dot(q)),
   'hand_l_relative_matrix_blender':[list(row) for row in REL[t]],'previous_default':{'SM_ToolV02_Crowbar':'FRotator(0,90,0), FVector(44.5,-22.5,-35)','SM_ToolV02_Fuse':'FRotator(0,90,0), FVector(44.5,-22.5,-27)'}[o.name]}
  assert contract['tools'][o.name]['rotator_roundtrip_quat_error']<1e-9
 # sanity: the old default (identity tool at (-.225,.245,z)) converts to FRotator(0,90,0) with this pipeline
 q0=(conv@Matrix.Identity(3)@asset).to_quaternion();contract['conversion_check_identity_tool']={'quaternion_xyzw':[q0.x,q0.y,q0.z,q0.w],'rotator':list(ue_rot_from_quat(q0.x,q0.y,q0.z,q0.w))}
 (OUT/'tool-hold-contract.json').write_text(json.dumps(contract,indent=2))
assert len(r.data.bones)==52 and rest=={b.name:[list(row) for row in b.matrix_local] for b in r.data.bones}
for name in CLIPS:
 rep=report['clips'][name];N=rep['frames'];assert rep['max_body_vs_source']<1e-5 and (name=='Pickup' or rep['max_right_vs_source']<1e-5),(name,{k:v for k,v in rep.items() if k!='per_frame'})
 if rep['loop']:assert rep['frame1_vs_lastframe']<1e-5,(name,rep['frame1_vs_lastframe'])
 if name=='Pickup':assert rep['frame46_vs_hold_frame1']<1e-5,rep['frame46_vs_hold_frame1']
 if name=='FuseInsert' and 'frame1_vs_toolhold_frame1' in rep:assert rep['frame1_vs_toolhold_frame1']<1e-4,rep['frame1_vs_toolhold_frame1']
 a=bpy.data.actions['AN_PlayerV05_'+name];r.animation_data.action=a;r.animation_data.action_slot=a.slots[0];s.frame_start=1;s.frame_end=N;s.render.fps=30;s.frame_set(1);bpy.ops.object.select_all(action='DESELECT');r.select_set(True);bpy.context.view_layer.objects.active=r
 bpy.ops.export_scene.fbx(filepath=str(OUT/f'AN_PlayerV05_{name}.fbx'),use_selection=True,object_types={'ARMATURE'},add_leaf_bones=False,axis_forward='X',axis_up='Z',apply_unit_scale=True,use_armature_deform_only=True,bake_anim=True,bake_anim_use_all_actions=False,bake_anim_use_nla_strips=False,bake_anim_simplify_factor=0)
 rep['fbx_sha256']=hashlib.sha256((OUT/f'AN_PlayerV05_{name}.fbx').read_bytes()).hexdigest()
for t,o in TOOLS.items():s.collection.objects.link(o);o.hide_render=True;o.hide_viewport=True;o.name=o.name  # keep the tool meshes in the blend for review
(OUT/'validation.json').write_text(json.dumps(report,indent=1));bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'11_tools-rev01.blend'))
print('RESULT',json.dumps({n:{k:v for k,v in rep.items() if k!='per_frame'} for n,rep in report['clips'].items()}),flush=True)
print('CONTRACT',json.dumps({k:{kk:vv for kk,vv in v.items() if kk!='hand_l_relative_matrix_blender'} for k,v in contract['tools'].items()}),flush=True)
