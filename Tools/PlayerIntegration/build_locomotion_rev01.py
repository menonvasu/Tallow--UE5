import bpy,math,json,hashlib,os
import numpy as np
from pathlib import Path
from mathutils import Vector,Matrix,Quaternion,Euler
from mathutils.bvhtree import BVHTree
# Locomotion revision 01: Walk / Sprint / Crouch rebuilt as camera-relative candle-arm cycles for a scared child.
# Open RelightMatchRevision02/08_relight-match-v2.blend. Hold frame 1 is the base; the right fingers never leave Hold (grip is fixed), the arms move.
# Body motion (bob, sway, surge, shoulder swing) is written as periodic signals; the hand follows them through a damped spring (lag + overshoot)
# and the candle tilts as a wrist-held pendulum driven by the hand's acceleration, so the flame swings with the steps. Everything is a Fourier
# series over the clip period, so every loop seam is exact in position and velocity. LOCO_PREVIEW=1 renders quick Workbench strips, no export.
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'Tallow/SourceAssets/Player/InteractionV05/LocomotionRevision01';OUT.mkdir(exist_ok=True)
PREVIEW=os.environ.get('LOCO_PREVIEW');PDIR=Path(os.environ.get('LOCO_PREVIEW_DIR',str(OUT)))
s=bpy.context.scene;r=bpy.data.objects['RIG_Tallow_Generated'];c=bpy.data.objects['SM_CandleV02_Crooked'];part=bpy.data.objects['SK_Tallow_Generated_Partial'];full=bpy.data.objects['SK_Tallow_Generated_Full']
R={b.name:b.matrix_local.copy() for b in r.data.bones};LEN={b.name:b.length for b in r.data.bones};PAR={b.name:(b.parent.name if b.parent else None) for b in r.data.bones};rest={b.name:[list(row) for row in b.matrix_local] for b in r.data.bones}
DIG=('thumb','index','middle','ring','pinky');FING={sd:[n for n in R if n.endswith('_'+sd) and n.split('_')[0] in DIG] for sd in 'lr'};ARM={sd:['clavicle_'+sd,'upperarm_'+sd,'lowerarm_'+sd,'hand_'+sd]+FING[sd] for sd in 'lr'}
CAM=Vector((0,-.20,1.10));TANH,TANV=1.0,.5625  # in-game view: lens 16 on the 32 mm review sensor = the game's 90 deg FP FOV, 16:9
def update():bpy.context.view_layer.update()
def sample(name,f=1):
 a=bpy.data.actions['AN_PlayerV05_'+name];r.animation_data.action=a;r.animation_data.action_slot=a.slots[0];s.frame_set(f);ps={p.name:p.matrix_basis.copy() for p in r.pose.bones};pm={p.name:p.matrix.copy() for p in r.pose.bones};r.animation_data.action=None;return ps,pm
def apply(ps):
 r.animation_data.action=None
 for p in r.pose.bones:p.matrix_basis=ps[p.name]
 update()
def rotw(n,q,h=None):
 p=r.pose.bones[n];h=p.head.copy() if h is None else h;p.matrix=Matrix.Translation(h)@q.to_matrix().to_4x4()@Matrix.Translation(-h)@p.matrix;update()
def aim(n,target):
 p=r.pose.bones[n];h=p.head.copy();rotw(n,(p.tail-h).rotation_difference(Vector(target)-h))
def twist(q):q=q if q.w>=0 else -q;return 2*math.atan2(q.y,q.w)
def local(child,cm,pm):return ((R[PAR[child]].to_3x3().inverted()@R[child].to_3x3()).inverted()@pm.to_3x3().inverted()@cm.to_3x3()).to_quaternion()
def secant(f,goal):
 a,b,fa,fb=0.,.2,f(0.)-goal,f(.2)-goal
 for i in range(40):
  if abs(fb)<1e-9 or fb==fa:break
  a,b,fa=b,b-fb*(b-a)/(fb-fa),fb;fb=f(b)-goal
 return b
base,basem=sample('Hold');apply(base)
TW0={sd:(twist(local('hand_'+sd,basem['hand_'+sd],basem['lowerarm_'+sd])),twist(local('lowerarm_'+sd,basem['lowerarm_'+sd],basem['upperarm_'+sd]))) for sd in 'lr'}  # Hold's own twist split is the baseline
def clav(sd,elev,prot):
 # world-axis clavicle lift / protraction in degrees (shoulder rides with the bob instead of the forearm doing it all)
 k=1 if sd=='r' else -1;rotw('clavicle_'+sd,Quaternion((0,0,1),k*math.radians(prot))@Quaternion((0,1,0),-k*math.radians(elev)))
def arm(sd,wrist,Rh,pole,FL=.55,FU=.6):
 """two-bone IK of side sd to `wrist` toward the pole; Rh = hand world rotation (None keeps Hold's wrist). Extra axial twist vs Hold is shared forearm/wrist (FL) and lifted off the elbow into the upper arm (FU)."""
 U,L,H=r.pose.bones['upperarm_'+sd],r.pose.bones['lowerarm_'+sd],r.pose.bones['hand_'+sd];S=U.head.copy();L1,L2=LEN['upperarm_'+sd],LEN['lowerarm_'+sd];d=wrist-S;dl=min(d.length,L1+L2-1e-4)
 ax=d.normalized();a=(L1*L1-L2*L2+dl*dl)/(2*dl);pv=(pole-ax*pole.dot(ax)).normalized();aim('upperarm_'+sd,S+ax*a+pv*math.sqrt(max(0,L1*L1-a*a)));aim('lowerarm_'+sd,S+ax*dl)
 if Rh is None:return dl
 Um,Lm=U.matrix.copy(),L.matrix.copy();Hm=Matrix.Translation(L.tail)@Rh.to_4x4();la=(L.tail-L.head).normalized();ua=(U.tail-U.head).normalized();h0,e0=TW0[sd]
 hand_tw=lambda phi:twist(local('hand_'+sd,Hm,Matrix.Rotation(phi,4,la)@Lm));phi=secant(hand_tw,h0+(1-FL)*(hand_tw(0)-h0))
 L.matrix=Matrix.Translation(L.head)@Matrix.Rotation(phi,4,la)@Matrix.Translation(-L.head)@Lm;update();Lm=L.matrix.copy()
 elbow_tw=lambda th:twist(local('lowerarm_'+sd,Lm,Matrix.Rotation(th,4,ua)@Um));th=secant(elbow_tw,e0+(1-FU)*(elbow_tw(0)-e0))
 U.matrix=Matrix.Translation(U.head)@Matrix.Rotation(th,4,ua)@Matrix.Translation(-U.head)@Um;update();L.matrix=Lm;update()
 H.matrix=Matrix.Translation(H.head)@Rh.to_4x4();update();return dl
# --- Hold references: right wrist, grip point on the candle axis at the fist, wick, Hold elbow pole
W0=basem['hand_r'].translation.copy();RH0=basem['hand_r'].to_3x3().normalized();CM0=c.matrix_world.copy();G0=CM0@Vector((.004,0,.064));WICK0=CM0@Vector((.004,0,.255))
def pole_of(sd):
 S,E,W=basem['upperarm_'+sd].translation,basem['lowerarm_'+sd].translation,basem['hand_'+sd].translation;ax=(W-S).normalized();return ((E-S)-ax*(E-S).dot(ax)).normalized()
POLE0={sd:pole_of(sd) for sd in 'lr'};WL0=basem['hand_l'].translation.copy()
# left running fist (local XYZ degrees on top of the relaxed rest curl; -X curls on this rig), thumb wrapped over the index
FIST={**{f'{d}_{j:02d}_l':(-v,0,0) for d in('index','middle','ring','pinky') for j,v in zip((1,2,3),(82,90,55))},'thumb_01_l':(-22,18,32),'thumb_02_l':(-38,0,0),'thumb_03_l':(-34,0,0)}
FIST={n:Euler([math.radians(v) for v in e],'XYZ').to_quaternion() for n,e in FIST.items()}
# --- periodic signal kit: Fourier series over the clip period T (seconds); t is clip phase 0..1
NS=720;TT=np.arange(NS)/NS
class Sig:
 def __init__(s_,c,T):s_.c,s_.T=c,T
 @staticmethod
 def of(fn,T):return Sig(np.fft.rfft(np.array([fn(t) for t in TT]))/NS,T)
 def at(s_,t,der=0):
  k=np.arange(len(s_.c));w=2j*np.pi*k/s_.T;z=s_.c*(w**der)*np.exp(2j*np.pi*k*t);return float(z[0].real+2*z[1:].real.sum())
 def spring(s_,fn,zeta):
  # hand on the arm: base-excited damped spring (follows slow motion, lags and overshoots near fn, filters jitter)
  wn=2*np.pi*fn;w=2*np.pi*np.arange(len(s_.c))/s_.T;return Sig(s_.c*(wn*wn+2j*zeta*wn*w)/(wn*wn-w*w+2j*zeta*wn*w),s_.T)
 def pend(s_,fn,zeta):
  # candle tilt: wrist-held pendulum driven by hand acceleration (response to -a, normalised later)
  wn=2*np.pi*fn;w=2*np.pi*np.arange(len(s_.c))/s_.T;return Sig(-s_.c*(-w*w)/(wn*wn-w*w+2j*zeta*wn*w),s_.T)
 def __add__(s_,o):return Sig(s_.c+o.c,s_.T)
 def scale(s_,k):return Sig(s_.c*k,s_.T)
 def pp(s_):v=[s_.at(t) for t in TT[::4]];return max(v)-min(v)
 def unit(s_):p=s_.pp();return s_.scale(1/p if p>1e-9 else 0)
def loop_keys(keys):
 """periodic Catmull-Rom through (phase,value) keys on 0..1: an animator's step curve"""
 us,vs,n=[k[0] for k in keys],[k[1] for k in keys],len(keys)
 def f(t):
  t%=1;i=max([j for j in range(n) if us[j]<=t] or [n-1]);U=lambda j:us[j%n]+math.floor(j/n);u0,u1,um,u2=U(i),U(i+1),U(i-1),U(i+2);tt=t if t>=u0 else t+1
  v0,v1,vm,v2=vs[i],vs[(i+1)%n],vs[(i-1)%n],vs[(i+2)%n];h=u1-u0;m0,m1=(v1-vm)/(u1-um)*h,(v2-v0)/(u2-u0)*h;x=(tt-u0)/h
  return v0*(2*x**3-3*x*x+1)+m0*(x**3-2*x*x+x)+v1*(-2*x**3+3*x*x)+m1*(x**3-x*x)
 return f
# per-footstep vertical shapes (phase within one step)
WALK_STEP=loop_keys([(0,-.25),(.13,-1),(.58,1),(.82,.55)])  # foot lands, hand sinks after it (cautious weight transfer), slow rise over the passing pose
RUN_STEP=loop_keys([(0,-1),(.22,-.1),(.5,1),(.75,.55),(.9,-.4)])  # compressed at mid-stance, thrown up at push-off, floats, lands
CREEP_STEP=loop_keys([(0,-.2),(.2,-1),(.62,1),(.85,.4)])
# --- clip specs. Phase 0 = right foot contact (Walk/Crouch) or right foot mid-stance (Sprint). Offsets in metres (x right, y away from camera, z up).
# G = grip point base offset vs Hold, lean = candle top toward screen centre (deg), back = candle top toward the face (deg).
CLIPS={
 'Walk':dict(frames=31,G=(-.035,-.015,.022),lean=4,back=3,shrug=(2,2),ulnar=30,yawk=.6,
  bob=lambda t:WALK_STEP(2*t)*(1+.1*math.cos(2*math.pi*t)),  # two steps a loop, the right (candle-side) step a little heavier
  sway=lambda t:math.sin(2*math.pi*t-.35),surge=lambda t:-math.sin(4*math.pi*(t-.25)),swing=lambda t:-math.cos(2*math.pi*t),
  amp=dict(bob=.04,sway=.02,surge=.008,swing=.012),spring=(4.5,.42),tilt=(2.4,.45),roll=9,pitch=4,clav=(.25,.15),tremor=(.0024,1.0),
  left=dict(swing=.055,lift=.012,phase=0)),
 'Sprint':dict(frames=19,G=(-.06,-.05,.045),lean=6,back=9,shrug=(7,6),ulnar=32,yawk=.6,
  bob=lambda t:RUN_STEP(2*t)*(1+.08*math.cos(2*math.pi*t)),
  sway=lambda t:math.cos(2*math.pi*t),surge=lambda t:-math.sin(4*math.pi*(t-.2)),swing=lambda t:-math.cos(2*math.pi*(t-.03)),  # weight over the right foot at t=0; right shoulder forward on the left step
  amp=dict(bob=.105,sway=.03,surge=.02,swing=.028),spring=(5.5,.3),tilt=(3.0,.32),roll=12,pitch=8,clav=(.35,.25),
  left=dict(pump=True,phase=.45)),  # left fist peaks forward just before right mid-stance (opposite arm and leg)
 'Crouch':dict(frames=61,G=(-.08,-.02,.018),lean=9,back=5,shrug=(5,8),ulnar=30,yawk=.6,
  bob=lambda t:CREEP_STEP(2*t)-.7*math.cos(2*math.pi*t-1.2),  # soft creeping steps riding a slow breath (one breath per loop)
  sway=lambda t:math.sin(2*math.pi*t-.5),surge=lambda t:-math.sin(4*math.pi*(t-.25))+.8*math.cos(2*math.pi*t-1.2),swing=lambda t:-math.cos(2*math.pi*t),
  amp=dict(bob=.018,sway=.028,surge=.012,swing=.006),spring=(2.0,.55),tilt=(1.4,.6),roll=5,pitch=3,clav=(.3,.2),tremor=(.0026,1.1),
  left=dict(swing=.02,lift=.0,phase=0)),
}
for k,v in json.loads(os.environ.get('LOCO_OVERRIDE','{}')).items():CLIPS[k].update(v)
ONLY=[n for n in os.environ.get('LOCO_ONLY',','.join(CLIPS)).split(',')]
def build_signals(spec):
 T=(spec['frames']-1)/30;A=spec['amp'];fn,z=spec['spring'];tf,tz=spec['tilt']
 bz=Sig.of(spec['bob'],T).unit().scale(A['bob']);bx=Sig.of(spec['sway'],T).unit().scale(A['sway']);by=Sig.of(spec['surge'],T).unit().scale(A['surge'])+Sig.of(spec['swing'],T).unit().scale(A['swing'])
 hx,hy,hz=bx.spring(fn,z),by.spring(fn,z),bz.spring(fn,z)
 # re-normalise so the delivered hand bob/sway are the authored amplitudes after the spring's overshoot
 hz=hz.scale(A['bob']/hz.pp());hx=hx.scale(A['sway']/hx.pp())
 roll=hx.pend(tf,tz).unit().scale(-spec['roll']);pitch=hy.pend(tf,tz).unit().scale(-spec['pitch'])+hz.pend(tf,tz).unit().scale(-spec['pitch']*.5)  # top lags the hand's acceleration
 tr=spec.get('tremor')
 if tr:  # fear tremor: two near-7-9 Hz partials per axis, whole harmonics of the loop so it closes exactly; adds to the arm, not the grip
  K=[round(f*T) for f in(6.8,8.6)];sh=lambda ph:(lambda t:sum(math.sin(2*math.pi*k*t+ph*(i+1)) for i,k in enumerate(K)))
  hx=hx+Sig.of(sh(.3),T).unit().scale(tr[0]);hy=hy+Sig.of(sh(1.9),T).unit().scale(tr[0]*.6);hz=hz+Sig.of(sh(4.1),T).unit().scale(tr[0]);roll=roll+Sig.of(sh(2.7),T).unit().scale(tr[1])
 sig=dict(x=hx,y=hy,z=hz,roll=roll,pitch=pitch,T=T)
 def wick_at(t):q=cand_q(spec,sig,t);return G0+Vector(spec['G'])+Vector((hx.at(t),hy.at(t),hz.at(t)))+q@(WICK0-G0)
 L=spec['left'];fwd=lambda t:(.5-.5*math.cos(2*math.pi*(t-L['phase']))) if L.get('pump') else 0
 sig['t0']=min((i/240 for i in range(240)),key=lambda t:(wick_at(t)-WICK0).length+.1*fwd(t))  # loop starts where the candle is nearest Hold (and the left fist is down): clean 0.18 s crossfade
 return sig
def cand_q(spec,sig,t):return Quaternion((0,1,0),-math.radians(sig['roll'].at(t)))@Quaternion((1,0,0),math.radians(sig['pitch'].at(t)))@candle_rot(spec['lean'],spec['back'])
def candle_rot(lean,back):return Quaternion((0,-1,0),math.radians(lean))@Quaternion((1,0,0),math.radians(back))
def left_pose(spec,t):
 L=spec['left'];ph=2*math.pi*(t-L['phase'])
 if L.get('pump'):
  # running pump: forward-up into the bottom-left of frame, back-down behind the hip
  u=.5-.5*math.cos(ph)  # 0 back .. 1 forward, pendulum timing
  back,mid,fwd=Vector((-.2,-.11,.70)),Vector((-.22,.03,.72)),Vector((-.15,.17,.925))
  w=(1-u)**2*back+2*u*(1-u)*mid+u*u*fwd  # quadratic arc past the hip
  elbow=Vector((-.75,-.45,-.45)).lerp(Vector((-.6,-.25,-.75)),u)
  return w,elbow,u
 w=WL0+Vector((0,L['swing']*math.cos(ph),L['lift']*(.5+.5*math.cos(ph))));return w,POLE0['l'],0
def wrist_angles(Rh):
 # forearm direction in hand axes (X pinky side / down, Y knuckles, Z back of hand): ulnar deviation and extension of the thumb-up candle fist
 Lb=r.pose.bones['lowerarm_r'];f=Rh.transposed()@(Lb.tail-Lb.head).normalized();return math.degrees(math.atan2(-f.x,f.y)),math.degrees(math.atan2(-f.z,f.y))
def right_arm(spec,G,q,off):
 """candle arm: hand pose from the candle, elbow placed on the IK circle at the height that keeps ulnar deviation near Hold's,
 the fist turned about the candle's own axis (candle stays where it is) to take up the wrist extension"""
 yaw=0.;ut=math.radians(spec['ulnar']);sh=spec['shrug'];ce,cp=spec['clav']
 for it in range(5):
  for n in ARM['r'][:4]:r.pose.bones[n].matrix_basis=base[n]
  update();clav('r',sh[0]+ce*off.z*100,sh[1]+cp*off.y*100)
  qy=q@Quaternion((0,0,1),yaw);wrist=G+qy@(W0-G0);Rh=qy.to_matrix()@RH0
  S=r.pose.bones['upperarm_r'].head.copy();L1,L2=LEN['upperarm_r'],LEN['lowerarm_r'];d=wrist-S;dl=min(d.length,L1+L2-1e-4);a=d.normalized();A=(L1*L1-L2*L2+dl*dl)/(2*dl);rho=math.sqrt(max(0,L1*L1-A*A));C=S+a*A
  u=(Vector((1,0,0))-a*a.x).normalized();v=a.cross(u);hp=math.asin(max(-1,min(1,(Rh@Vector((0,1,0))).z)));zt=wrist.z-L2*math.sin(hp+ut)
  Rz=math.hypot(u.z,v.z);ph0=math.atan2(v.z,u.z);kk=max(-1,min(1,(zt-C.z)/(rho*Rz)))
  E=max((C+rho*(math.cos(ph0+sg*math.acos(kk))*u+math.sin(ph0+sg*math.acos(kk))*v) for sg in(1,-1)),key=lambda e:e.x)  # elbow out to the side, never across the chest
  reach=arm('r',wrist,Rh,(E-C).normalized());uln,ext=wrist_angles(Rh);yaw+=spec['yawk']*math.radians(ext)
 return reach
def pose_frame(name,spec,sig,t):
 apply(base);t=t+sig['t0']
 off=Vector(spec['G'])+Vector((sig['x'].at(t),sig['y'].at(t),sig['z'].at(t)));q=cand_q(spec,sig,t);G=G0+off;reach=right_arm(spec,G,q,off)
 wl,pl,u=left_pose(spec,t)
 if spec['left'].get('pump'):
  clav('l',3+4*u,4+6*u)  # scared hunch, more on the forward pump
  arm('l',wl,None,pl.normalized())
  for n in FING['l']:r.pose.bones[n].rotation_euler=(base[n].to_quaternion()@FIST[n].slerp(Quaternion(),.2*(1-u))).to_euler('XYZ')  # fist loosens a touch on the back swing
 else:arm('l',wl,None,pl)
 update();return reach,G,q
def bake(name,spec):
 sig=build_signals(spec);N=spec['frames'];old=bpy.data.actions.get('AN_PlayerV05_'+name)
 if old:old.name='SOURCE_'+name+'_InteractionV05'
 action=bpy.data.actions.new('AN_PlayerV05_'+name);action.use_fake_user=True;prev={};info=[]
 for f in range(1,N+1):
  t=((f-1)%(N-1))/(N-1);reach,G,q=pose_frame(name,spec,sig,t)
  r.animation_data.action=action
  if action.slots:r.animation_data.action_slot=action.slots[0]
  for p in r.pose.bones:
   e=p.matrix_basis.to_quaternion().to_euler('XYZ',prev[p.name]) if p.name in prev else p.matrix_basis.to_euler('XYZ')
   if f==N:e=first[p.name].copy()  # exact seam: last sample is frame 1
   p.rotation_euler=e;prev[p.name]=e.copy();p.keyframe_insert('location',frame=f);p.keyframe_insert('rotation_euler',frame=f);p.keyframe_insert('scale',frame=f)
  if f==1:first={k:v.copy() for k,v in prev.items()}
  r.animation_data.action=None;info.append({'frame':f,'shoulder_wrist_m':round(reach,4)})
 for fc in action.layers[0].strips[0].channelbags[0].fcurves:
  for k in fc.keyframe_points:k.interpolation='LINEAR'  # every frame is a sample of the eased Fourier curves
 return action,info
built={n:bake(n,sp) for n,sp in CLIPS.items() if n in ONLY}
def play(name,f):
 a=bpy.data.actions['AN_PlayerV05_'+name];r.animation_data.action=a;r.animation_data.action_slot=a.slots[0];s.frame_set(f);update()
def proj(p):
 v=p-CAM;return (v.x/v.y/TANH,v.z/v.y/TANV,v.y)  # screen x,y in -1..1, depth
full.hide_render=True;part.hide_render=False
if PREVIEW:
 # --- review flame: emissive cone on the wick, rigid with the candle like the game's flame visual
 fl=bpy.data.objects.get('REVIEW_CandleFlame')
 if not fl:
  bpy.ops.mesh.primitive_cone_add(vertices=20,radius1=.0055,radius2=0,depth=.026);fl=bpy.context.object;fl.name='REVIEW_CandleFlame';bpy.ops.object.shade_smooth()
  mat=bpy.data.materials.new('REVIEW_CandleFlame');mat.use_nodes=True;nt=mat.node_tree;nt.nodes.clear();em=nt.nodes.new('ShaderNodeEmission');em.inputs['Color'].default_value=(1,.55,.16,1);em.inputs['Strength'].default_value=30;o=nt.nodes.new('ShaderNodeOutputMaterial');nt.links.new(em.outputs[0],o.inputs[0]);mat.diffuse_color=(1,.55,.1,1);fl.data.materials.append(mat)
  fl.parent=c;fl.matrix_parent_inverse=Matrix();fl.location=(.004,0,.255+.013+.002);fl.rotation_euler=(0,0,0)
 PDIR.mkdir(parents=True,exist_ok=True);s.render.engine='BLENDER_WORKBENCH';s.display.shading.light='STUDIO';s.display.shading.color_type='MATERIAL';s.render.resolution_x=640;s.render.resolution_y=360;s.render.resolution_percentage=100
 cam=s.camera;cam.data.lens=16;cam.location=CAM;cam.rotation_euler=Vector((0,1,0)).to_track_quat('-Z','Y').to_euler()
 for name,sp in [(n,CLIPS[n]) for n in ONLY]:
  N=sp['frames'];fr=[1+round(i*(N-1)/6) for i in range(6)];ims=[]
  for f in fr:
   play(name,f);s.render.filepath=str(PDIR/f'p-{name}-{f:02d}.png');bpy.ops.render.render(write_still=True);ims.append(s.render.filepath)
  # strip + wick trail (every frame, red) on the first frame
  px=[np.array(bpy.data.images.load(p).pixels[:],dtype=np.float32).reshape(360,640,4) for p in ims];tr=px[0].copy()
  for f in range(1,N):
   play(name,f);wx,wy,_=proj(c.matrix_world@Vector((.004,0,.255)));X=int(320+wx*320);Y=int(180+wy*180)
   gx,gy,_=proj(r.pose.bones['hand_r'].head);GX=int(320+gx*320);GY=int(180+gy*180)
   for (a,b,col) in((X,Y,(1,0,0,1)),(GX,GY,(0,.4,1,1))):
    if 1<=a<639 and 1<=b<359:tr[b-1:b+2,a-1:a+2]=col
  td=[p[::-1] for p in px]+[tr[::-1]];bl=np.zeros((360,1280,4),np.float32)+[.2,.2,.2,1];sheet=np.concatenate([np.concatenate(td[:3],1),np.concatenate(td[3:6],1),np.concatenate([td[6],bl],1)],0)[::-1]
  out=bpy.data.images.new('sheet',1920,1080,alpha=True);out.pixels.foreach_set(sheet.astype(np.float32).ravel());out.filepath_raw=str(PDIR/f'PREVIEW-{name}.png');out.file_format='PNG';out.save()
  st=[]
  for f in range(1,N):
   play(name,f);wk=c.matrix_world@Vector((.004,0,.255));st.append((proj(wk),wk.copy(),r.pose.bones['hand_r'].head.copy(),(r.pose.bones['upperarm_r'].tail-r.pose.bones['upperarm_r'].head).angle(r.pose.bones['lowerarm_r'].tail-r.pose.bones['lowerarm_r'].head),(c.matrix_world.to_3x3()@Vector((0,0,1))),wrist_angles(r.pose.bones['hand_r'].matrix.to_3x3())))
  json.dump([[a[0][0],a[0][1],a[0][2],a[1].x,a[1].y,a[1].z,a[2].x,a[2].y,a[2].z,math.degrees(math.atan2(-a[4].x,a[4].z)),math.degrees(math.atan2(-a[4].y,a[4].z))] for a in st],open(PDIR/f'curves-{name}.json','w'))
  rng=lambda vals:(round(min(vals),3),round(max(vals),3))
  print('STAT',name,'wick sx',rng([a[0][0] for a in st]),'sy',rng([a[0][1] for a in st]),'depth',rng([a[0][2] for a in st]),'wick travel cm',[round((max(a[1][i] for a in st)-min(a[1][i] for a in st))*100,1) for i in range(3)],'hand travel cm',[round((max(a[2][i] for a in st)-min(a[2][i] for a in st))*100,1) for i in range(3)],'hand sy',rng([proj(a[2])[1] for a in st]),'elbow bend deg',rng([math.degrees(a[3]) for a in st]),'uln/ext',rng([a[5][0] for a in st]),rng([a[5][1] for a in st]),'roll deg',rng([math.degrees(math.atan2(-a[4].x,a[4].z)) for a in st]),'pitch deg',rng([math.degrees(math.atan2(-a[4].y,a[4].z)) for a in st]),flush=True)
 for spec_ in filter(None,os.environ.get('LOCO_CLOSE','').split(';')):
  nm,f,bn,dx,dy,dz=spec_.split(':');play(nm,int(f));tgt=r.pose.bones[bn].head.copy();eye=tgt+Vector((float(dx),float(dy),float(dz))).normalized()*.3;fw=(tgt-eye).normalized();x=fw.cross(Vector((0,0,1))).normalized();cam.matrix_world=Matrix.Translation(eye)@Matrix((x,x.cross(fw),-fw)).transposed().to_4x4();cam.data.lens=35
  s.render.filepath=str(PDIR/f'close-{nm}-{f}-{bn}-{dx}_{dy}_{dz}.png');bpy.ops.render.render(write_still=True);cam.data.lens=16;cam.location=CAM;cam.rotation_euler=Vector((0,1,0)).to_track_quat('-Z','Y').to_euler()
 print('PREVIEW done',flush=True)
else:
 # --- validate against Hold: grip, seam, travel, framing in the in-game view, camera distance, penetration, twist
 gid={g.index:g.name for g in part.vertex_groups}
 def vset(names):return [v.index for v in part.data.vertices if any(gid[g.group] in names and g.weight>.2 for g in v.groups)]
 RARM,UPR,LARM,LHAND,RHAND=vset({'upperarm_r','lowerarm_r'}),vset({'upperarm_r'}),vset(set(ARM['l'][1:])),vset({'hand_l',*FING['l']}),vset({'hand_r',*FING['r']})
 cpoly=[list(p.vertices) for p in c.data.polygons];ABOVE=[i for i,v in enumerate(c.data.vertices) if v.co.z>.07]  # cup, wax and wick: everything above the fist
 GRIP0={n:basem['hand_r'].inverted()@basem[n] for n in FING['r']};CREL0=(r.matrix_world@basem['hand_r']).inverted()@CM0
 def inframe(p):x,y,d=proj(p);return d>0 and abs(x)<=1 and abs(y)<=1
 def inside(bvh,co):hit=bvh.find_nearest(co);return hit[0] is not None and (co-hit[0]).dot(hit[1])<0
 def mdelta(A,B,names):return max(abs(A[n][i][j]-B[n][i][j]) for n in names for i in range(4) for j in range(4))
 tw=lambda m,pm,n:math.degrees(twist(local(n,m,pm)))
 hold_tw={n:tw(basem[n],basem[PAR[n]],n) for n in ('upperarm_r','lowerarm_r','hand_r')}
 bend=lambda M:math.degrees((M['lowerarm_r'].to_3x3()@Vector((0,1,0))).angle(M['hand_r'].to_3x3()@Vector((0,1,0))))
 report={'source':'InteractionV05/RelightMatchRevision02/08_relight-match-v2.blend','bones':len(rest),'fps':30,'view':{'camera_location_m':list(CAM),'looking':'+Y','lens_mm':16,'sensor_mm':32,'hfov_deg':90,'aspect':'16:9','note':'matches PresentationFieldOfView=90; UE near clip 1 cm, FP scale 0.6, so geometry must stay > 1.7 cm from the eye'},
  'hold_reference':{'wick_m':list(WICK0),'wick_screen':list(proj(WICK0)[:2]),'wrist_screen_y':proj(W0)[1],'wrist_bend_deg':bend(basem),'wrist_ulnar_extension_deg':(apply(base),wrist_angles(basem['hand_r'].to_3x3()))[1],'twist_deg':hold_tw},'clips':{}}
 for name in ONLY:
  sp=CLIPS[name];N=sp['frames'];M=[];per=[]
  for f in range(1,N+1):
   play(name,f);P={p.name:p.matrix.copy() for p in r.pose.bones};M.append(P);cw=c.matrix_world.copy();cv=[cw@v.co for v in c.data.vertices];cb=BVHTree.FromPolygons(cv,cpoly)
   dg=bpy.context.evaluated_depsgraph_get();ev=part.evaluated_get(dg);me=ev.to_mesh();pv=[part.matrix_world@v.co for v in me.vertices];ev.to_mesh_clear()
   wick=cw@Vector((.004,0,.255));tip=cw@Vector((.004,0,.255+.028));ax=cw.to_3x3()@Vector((0,0,1));vis=lambda ids,pts:sum(inframe(pts[i]) for i in ids)/len(ids)
   per.append({'frame':f,'wick_m':[round(x,5) for x in wick],'wick_screen':[round(x,4) for x in proj(wick)[:2]],'flame_tip_in_frame':inframe(tip),'wick_in_frame':inframe(wick),
    'candle_above_fist_in_frame':all(inframe(cv[i]) for i in ABOVE),'candle_verts_in_frame':round(vis(range(len(cv)),cv),4),'right_fist_verts_in_frame':round(vis(RHAND,pv),4),'left_hand_verts_in_frame':round(vis(LHAND,pv),4),'right_upperarm_verts_in_frame':sum(inframe(pv[i]) for i in UPR),
    'min_candle_to_eye_cm':round(min((v-CAM).length for v in cv)*100,2),'min_arm_to_eye_cm':round(min((pv[i]-CAM).length for i in RARM+LARM+RHAND)*100,2),
    'arm_verts_inside_candle':sum(inside(cb,pv[i]) for i in RARM+LARM+LHAND),'candle_roll_deg':round(math.degrees(math.atan2(-ax.x,ax.z)),2),'candle_pitch_deg':round(math.degrees(math.atan2(-ax.y,ax.z)),2),
    'wrist_bend_deg':round(bend(P),2),'wrist_ulnar_deg':round(wrist_angles(P['hand_r'].to_3x3())[0],2),'wrist_extension_deg':round(wrist_angles(P['hand_r'].to_3x3())[1],2),'elbow_bend_deg':round(math.degrees((P['upperarm_r'].to_3x3()@Vector((0,1,0))).angle(P['lowerarm_r'].to_3x3()@Vector((0,1,0)))),2),
    'twist_vs_hold_deg':{n:round(tw(P[n],P[PAR[n]],n)-hold_tw[n],2) for n in hold_tw},
    'grip_delta':max(mdelta({n:P['hand_r'].inverted()@P[n] for n in FING['r']},GRIP0,FING['r']),max(abs(a-b) for n in FING['r'] for a,b in zip(r.pose.bones[n].rotation_euler,base[n].to_euler('XYZ')))),
    'candle_to_hand_delta':max(abs(x-y) for x,y in zip(((r.matrix_world@P['hand_r']).inverted()@cw).col[3],CREL0.col[3]))})
  L=per[:-1];W=[Vector(x['wick_m']) for x in L];H=[M[i]['hand_r'].translation for i in range(N-1)];ext=lambda P_,i:(max(p[i] for p in P_)-min(p[i] for p in P_))*100
  sec=max(abs((M[1][n][i][j]-M[0][n][i][j])-(M[-1][n][i][j]-M[-2][n][i][j])) for n in M[0] for i in range(4) for j in range(4))
  acc=max(abs(M[f+1][n][i][j]-2*M[f][n][i][j]+M[f-1][n][i][j]) for f in range(1,N-1) for n in M[0] for i in range(4) for j in range(4))
  rep={'frames':N,'seconds':round((N-1)/30,4),'loop':True,'loop_seam_matrix_error':mdelta(M[0],M[-1],M[0]),'loop_seam_second_difference':sec,'max_in_clip_second_difference':acc,
   'grip_delta_vs_hold_max':max(x['grip_delta'] for x in per),'candle_to_hand_delta_max':max(x['candle_to_hand_delta'] for x in per),
   'frame1_vs_hold':{'wick_offset_cm':round((W[0]-WICK0).length*100,2),'max_bone_matrix_delta':mdelta(M[0],basem,M[0]),'right_arm_matrix_delta':mdelta(M[0],basem,ARM['r'][:4])},
   'candle_travel':{'wick_range_cm_xyz':[round(ext(W,i),2) for i in range(3)],'wick_max_extent_cm':round(max((a-b).length for a in W for b in W)*100,2),'wick_path_per_loop_cm':round(sum((W[(i+1)%len(W)]-W[i]).length for i in range(len(W)))*100,1),
    'hand_range_cm_xyz':[round(ext(H,i),2) for i in range(3)],'max_wick_offset_from_hold_cm':round(max((w-WICK0).length for w in W)*100,2),
    'wick_screen_x':[min(x['wick_screen'][0] for x in L),max(x['wick_screen'][0] for x in L)],'wick_screen_y':[min(x['wick_screen'][1] for x in L),max(x['wick_screen'][1] for x in L)],
    'candle_roll_deg':[min(x['candle_roll_deg'] for x in L),max(x['candle_roll_deg'] for x in L)],'candle_pitch_deg':[min(x['candle_pitch_deg'] for x in L),max(x['candle_pitch_deg'] for x in L)]},
   'framing':{'flame_and_wick_in_frame_all_frames':all(x['flame_tip_in_frame'] and x['wick_in_frame'] for x in per),'candle_above_fist_in_frame_frames':sum(x['candle_above_fist_in_frame'] for x in L),'min_candle_verts_in_frame':min(x['candle_verts_in_frame'] for x in L),
    'right_fist_verts_in_frame':[min(x['right_fist_verts_in_frame'] for x in L),max(x['right_fist_verts_in_frame'] for x in L)],'left_hand_visible_frames':sum(x['left_hand_verts_in_frame']>0 for x in L),'max_right_upperarm_verts_in_frame':max(x['right_upperarm_verts_in_frame'] for x in per)},
   'min_candle_to_eye_cm':min(x['min_candle_to_eye_cm'] for x in per),'min_arm_to_eye_cm':min(x['min_arm_to_eye_cm'] for x in per),'max_arm_verts_inside_candle':max(x['arm_verts_inside_candle'] for x in per),
   'wrist_bend_deg':[min(x['wrist_bend_deg'] for x in per),max(x['wrist_bend_deg'] for x in per)],'wrist_ulnar_deg':[min(x['wrist_ulnar_deg'] for x in per),max(x['wrist_ulnar_deg'] for x in per)],'wrist_extension_deg':[min(x['wrist_extension_deg'] for x in per),max(x['wrist_extension_deg'] for x in per)],'elbow_bend_deg':[min(x['elbow_bend_deg'] for x in per),max(x['elbow_bend_deg'] for x in per)],
   'max_abs_twist_vs_hold_deg':{n:max(abs(x['twist_vs_hold_deg'][n]) for x in per) for n in hold_tw},'per_frame':per}
  # 0.18 s crossfade from Hold as UE blends it (per-bone local rotation slerp): candle path stays in frame and clear of the arms
  cf=[];play(name,1);B1={p.name:p.matrix_basis.copy() for p in r.pose.bones}
  for w in(.25,.5,.75):
   apply({n:Matrix.LocRotScale(base[n].to_translation().lerp(B1[n].to_translation(),w),base[n].to_quaternion().slerp(B1[n].to_quaternion(),w),None) for n in base})
   cw=c.matrix_world.copy();cb=BVHTree.FromPolygons([cw@v.co for v in c.data.vertices],cpoly);dg=bpy.context.evaluated_depsgraph_get();ev=part.evaluated_get(dg);me=ev.to_mesh();pv=[part.matrix_world@v.co for v in me.vertices];ev.to_mesh_clear();wk=cw@Vector((.004,0,.255))
   cf.append({'blend':w,'wick_from_hold_cm':round((wk-WICK0).length*100,2),'wick_in_frame':inframe(wk),'arm_verts_inside_candle':sum(inside(cb,pv[i]) for i in RARM+LARM+LHAND)})
  rep['crossfade_from_hold']=cf
  report['clips'][name]=rep
  assert rep['loop_seam_matrix_error']<1e-6 and rep['grip_delta_vs_hold_max']<1e-5 and rep['candle_to_hand_delta_max']<1e-5 and rep['framing']['flame_and_wick_in_frame_all_frames'] and rep['framing']['max_right_upperarm_verts_in_frame']==0 and rep['max_arm_verts_inside_candle']==0 and rep['min_candle_to_eye_cm']>10 and all(x['wick_in_frame'] and x['arm_verts_inside_candle']==0 for x in cf),(name,{k:v for k,v in rep.items() if k!='per_frame'})
 assert len(r.data.bones)==52 and rest=={b.name:[list(row) for row in b.matrix_local] for b in r.data.bones}
 # --- export: same FBX call as the rest of InteractionV05, one clip per file
 for name in ONLY:
  N=CLIPS[name]['frames'];a=bpy.data.actions['AN_PlayerV05_'+name];r.animation_data.action=a;r.animation_data.action_slot=a.slots[0];s.frame_start=1;s.frame_end=N;s.render.fps=30;s.frame_set(1);bpy.ops.object.select_all(action='DESELECT');r.select_set(True);bpy.context.view_layer.objects.active=r
  fp=OUT/f'AN_PlayerV05_{name}.fbx';bpy.ops.export_scene.fbx(filepath=str(fp),use_selection=True,object_types={'ARMATURE'},add_leaf_bones=False,axis_forward='X',axis_up='Z',apply_unit_scale=True,use_armature_deform_only=True,bake_anim=True,bake_anim_use_all_actions=False,bake_anim_use_nla_strips=False,bake_anim_simplify_factor=0)
  report['clips'][name]['fbx']=fp.name;report['clips'][name]['fbx_sha256']=hashlib.sha256(fp.read_bytes()).hexdigest()
 a=bpy.data.actions['AN_PlayerV05_Walk'];r.animation_data.action=a;r.animation_data.action_slot=a.slots[0];s.frame_start=1;s.frame_end=31;s.frame_set(1)
 bpy.context.preferences.filepaths.save_version=0;(OUT/'validation.json').write_text(json.dumps(report,indent=1));bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'09_locomotion-rev01.blend'))
 print('RESULT',json.dumps({n:{k:v for k,v in c_.items() if k!='per_frame'} for n,c_ in report['clips'].items()}),flush=True)
