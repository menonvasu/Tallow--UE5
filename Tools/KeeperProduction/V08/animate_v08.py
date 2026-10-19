"""V08 Keeper performance library: 20 game clips + variant library, authored from key poses and gait events.
Every animator control is keyed per frame (LINEAR) from the evaluated performance; planted hands are solved analytically.
blender -b --factory-startup --threads 4 --python-exit-code 1 --python animate_v08.py -- [Clip,Clip|all] [out.blend]"""
import bpy,sys,math,json
from pathlib import Path
from mathutils import Vector,Matrix,Quaternion
sys.path.insert(0,str(Path(__file__).parent));import keeper_v08_lib as K
from keeper_v08_lib import CX,sg,smooth,clamp,lerp,track,ease_in,ease_out
argv=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else []
ONLY=None if not argv or argv[0]=='all' else argv[0].split(',');DEST=Path(argv[1]) if len(argv)>1 else K.OUT/'keeper-v08-animated.blend'
bpy.ops.wm.open_mainfile(filepath=str(K.OUT/'keeper-v08-control-rig.blend'))
k=K.Keeper();K._k=k;rig=k.rig;FPS=24;CRAWLK=['2L','2R','3L','3R']
# ================================================================== stances
# Crawl: pelvis pivot ~0.30 m, torso pitched 45 deg, hands planted wide; elbows UP (the mesh's untwisted bend) and
# hands in line with the forearm on curled fingertips. Poles found by search: upper-arm twist <= ~30 deg vs rest.
CRAWL={'drop':1.06,'pitch':34,'home':{'2L':(.33,-.36),'2R':(.33,-.36),'3L':(.44,-.02),'3R':(.44,-.02)},
       'pole':{'2':(.05,-.15,1.),'3':(.12,0.,1.)},'flex':25,'claw':[.42,.5,.55,.5,.45]}
HP_CHEST=(-.5,.6,.6);HP_REACH=(-.65,.65,.35);HP_UP=(.35,-.08,.93);HP_OUT=(.6,-.3,-.7)
GUARD={'off':(.16,.14,-.14),'pole':HP_CHEST,'hand':('rel',20,0,0),'curls':[.4,.5,.55,.5,.45]}
# Host: hands clasped at the sternum, elbows out; the four crawl arms coiled under the robe (hands tucked).
HOST_BODY={'loc':(0,0,.004),'pitch':0,'roll':0,'yaw':0,'mantle':0}
# poles from a twist-limited search (host arms are near-straight, so the untwisted pole is used; elbows read down/forward)
HOST_ARM={'1L':{'pos':(.17,-.27,1.62),'pole':(-.09,-.97,.24),'hand':('rel',10,0,0),'curls':[.35,.45,.5,.5,.45]},
          '1R':{'pos':(.07,-.28,1.67),'pole':(-.16,-.84,.52),'hand':('rel',10,0,0),'curls':[.35,.45,.5,.5,.45]},
          '2L':{'pos':(.16,-.12,1.50),'pole':(-.12,-.99,.02),'hand':('rel',35,0,0),'curls':.6},'2R':{'pos':(.16,-.12,1.50),'pole':(-.12,-.99,.02),'hand':('rel',35,0,0),'curls':.6},
          '3L':{'pos':(.16,-.12,1.26),'pole':(.11,-.95,-.31),'hand':('rel',35,0,0),'curls':.6},'3R':{'pos':(.16,-.12,1.26),'pole':(.11,-.95,-.31),'hand':('rel',35,0,0),'curls':.6}}
HOST_ARM['1L']['pos']=(.13,-.27,1.62);HOST_ARM['1R']['pos']=(.11,-.28,1.67)  # asymmetric clasp, right hand higher
def crawl_body(**o):
    b={'loc':(0,0,-CRAWL['drop']),'pitch':CRAWL['pitch'],'roll':0,'yaw':0,'mantle':1.};b.update(o);return b
def cb(dz=0,dy=0,pitch=0,roll=0,yaw=0,mantle=1.,dx=0):
    return {'loc':(dx,dy,-CRAWL['drop']+dz),'pitch':CRAWL['pitch']+pitch,'roll':roll,'yaw':yaw,'mantle':mantle}
def hb(dz=0,dy=0,pitch=0,roll=0,yaw=0,mantle=0.,dx=0):
    return {'loc':(dx,dy,.004+dz),'pitch':pitch,'roll':roll,'yaw':yaw,'mantle':mantle}
CAM_D=1.25;CAM=Vector((0,-CAM_D,1.1))   # nominal capture: player eye 1.1 m, Keeper root 1.25 m away (grab sphere 85 cm + capsules)
def H(nod=0,turn=0,tilt=0,stab=.8,hood=0.,chomp=0.,fwd=0.,up=0.,look=0.,maw=0.):
    return {'nod':nod,'turn':turn,'tilt':tilt,'stab':stab,'hood':hood,'chomp':chomp,'fwd':fwd,'up':up,'look':look,'maw':maw}
HEAD_REL=Vector((0,-.02,.75))   # head bone origin relative to the body pivot at rest
def look_angles(b,at):
    """nod/turn (deg, body-local) that point the mask's forward axis at world point `at`."""
    R,_=k.body_matrix(b);d=R.transposed()@(Vector(at)-k.to_world(b,HEAD_REL))
    return math.degrees(math.atan2(-d.z,math.hypot(d.x,d.y))),math.degrees(math.atan2(d.x,-d.y))
def _maw_local():
    """Open-maw centre in the head bone's frame (from the HOOD_OPEN key of the tiny closed maw cluster)."""
    import numpy as np
    kb=k.obj.data.shape_keys.key_blocks;base=np.array([list(v.co) for v in kb['Basis'].data]);ids=np.where(np.linalg.norm(base-np.array([CX,.015,2.275]),axis=1)<.006)[0]
    c=Vector(np.array([list(kb['HOOD_OPEN'].data[i].co) for i in ids]).mean(0));R=k.rest['CTRL_head'];return R.to_3x3().inverted()@(c-R.translation)
MAW_LOCAL=_maw_local()
def head_rot_world(b,nod,turn,tilt):
    R,_=k.body_matrix(b);q=Quaternion((0,1,0),math.radians(turn))@Quaternion((1,0,0),math.radians(nod))@Quaternion((0,0,1),math.radians(-tilt))
    return R@k.rest['CTRL_head'].to_3x3()@q.to_matrix()
def maw_lock(b,nod,turn,tilt,eye,fwd=0.):
    """Extra nod/turn so the maw centre sits on the capture camera's line of sight (the game aims at the head bone base)."""
    Ho=k.to_world(b,HEAD_REL)
    def err(n,t_):
        Rw=head_rot_world(b,n,t_,tilt);m=Ho+Rw@(MAW_LOCAL+Vector((0,0,fwd)))-eye;r=(Ho+Rw@Vector((0,0,fwd)))-eye;return m.angle(r)
    best=(err(nod,turn),0,0)
    for dn in range(0,81,2):
        for dt in range(-24,25,3):
            e=err(nod+dn,turn+dt)+.0015*(abs(dn)+abs(dt))
            if e<best[0]:best=(e,dn,dt)
    _,dn,dt=best
    for dn2 in (dn-1,dn,dn+1):
        for dt2 in (dt-1,dt,dt+1):
            e=err(nod+dn2,turn+dt2)+.0015*(abs(dn2)+abs(dt2))
            if e<best[0]:best=(e,dn2,dt2)
    return best[1],best[2]
def head_final(b,h):
    a=h.get('stab',.8)
    if h.get('look',0)>0:
        n,tn=look_angles(b,h.get('at',CAM));w=h['look']
        if h.get('maw',0)>0:
            dn,dt=maw_lock(b,n+h.get('nod',0),tn+h.get('turn',0),h.get('tilt',0),h.get('at',CAM),h.get('fwd',0));n+=dn*h['maw'];tn+=dt*h['maw']
        nod0=-b.get('pitch',0)*a+h.get('nod',0);turn0=-b.get('yaw',0)*a*.6+h.get('turn',0)
        return {'nod':nod0+(n+h.get('nod',0)-nod0)*w,'turn':turn0+(tn+h.get('turn',0)-turn0)*w,'tilt':-b.get('roll',0)*a*(1-w)+h.get('tilt',0),
                'hood':clamp(h.get('hood',0)),'chomp':clamp(h.get('chomp',0)),'loc':(0,h.get('up',0),h.get('fwd',0))}
    return {'nod':-b.get('pitch',0)*a+h.get('nod',0),'turn':-b.get('yaw',0)*a*.6+h.get('turn',0),'tilt':-b.get('roll',0)*a+h.get('tilt',0),
            'hood':clamp(h.get('hood',0)),'chomp':clamp(h.get('chomp',0)),'loc':(0,h.get('up',0),h.get('fwd',0))}
# ================================================================== floor safety
_FL={}
def floor_lift(b,margin=.006):
    """Lift the body just enough that the gathered robe (rigid with DEF_body + FULL_HORROR) clears the floor."""
    import numpy as np
    if not _FL:
        o=k.obj;kb=o.data.shape_keys.key_blocks;gi=o.vertex_groups['DEF_body'].index
        ids=[v.index for v in o.data.vertices if v.groups and max(v.groups,key=lambda g:g.weight).group==gi]
        _FL['B']=np.array([list(kb['Basis'].data[i].co) for i in ids])-np.array([0,0,1.4]);_FL['F']=np.array([list(kb['FULL_HORROR'].data[i].co) for i in ids])-np.array([0,0,1.4])
    R,tt=k.body_matrix(b);m=b.get('mantle',0);P=_FL['B']+(_FL['F']-_FL['B'])*m;z=P@np.array(list(R[2]))+tt.z;zmin=float(z.min())
    if zmin<margin:
        b=dict(b);l=list(b['loc']);l[2]+=margin-zmin;b['loc']=tuple(l)
    return b
# ================================================================== helpers
def noise(f,seed,freqs=(.31,.53,.97)):
    return sum(math.sin(f*w+seed*(i+1.7))/(i+1) for i,w in enumerate(freqs))/1.83
def pulse(f,at,rise=1.,fall=4.):
    """0 -> 1 in `rise` frames at `at`, decays over `fall` frames."""
    d=f-at
    if d<0:return 0.
    return clamp(d/max(rise,1e-6)) if d<rise else math.exp(-(d-rise)/fall)
def curls(base,amt=0,seed=0,f=0,spread=.12):
    b=base if isinstance(base,(list,tuple)) else [base]*5
    return [b[j]+amt*spread*math.sin(f*.37+seed+j*1.31) for j in range(5)]
def pole_lerp(a,b,u):
    """Spherical blend of elbow directions: the elbow swings round the arm instead of collapsing through the axis (IK flip)."""
    va=Vector(a).normalized();vb=Vector(b).normalized()
    if va.dot(vb)<-.98:vb=(vb+Vector((0,0,.3))).normalized()
    q=va.rotation_difference(vb);return tuple(Quaternion().slerp(q,clamp(u))@va)
def T(keys,loop_period=None):
    if loop_period and keys[-1][0]>=loop_period:keys=keys[:-1]   # last key duplicates the first in a loop
    return lambda f:track(keys,f,loop_period)
# ================================================================== performance engine
class Perf:
    """One clip. body(f)/head(f) return dicts; arms[kk] = time-ordered segments:
       ('plant',f_land,f_lift,(x,y),style)  hand planted on the floor (x = lateral distance from the centre line)
       ('free',[(f,spec,ease),...])          spec: off=(out,fwd,up) from shoulder | pos=(x,y,z) | body=(out,fwd,up), pole, hand, curls
       Gaps between segments are transitions (swing / lift / reach-down)."""
    def __init__(s,name,frames,loop=False,v=0.,yaw=None,events=None,note=''):
        s.name=name;s.N=frames;s.loop=loop;s.P=frames-1;s.v=v;s.yaw=yaw or (lambda f:0.);s.arms={};s.extra=None;s.events=events or {};s.note=note;s.plants={}
    # ---- time
    def wrapf(s,f):return f%s.P if s.loop else f
    def seg_at(s,kk,f):
        segs=s.arms[kk];cands=[f]+([f+s.P,f-s.P,f+2*s.P] if s.loop else [])
        for ff in cands:
            for i,sgm in enumerate(segs):
                a,b=s.span(sgm)
                if a<=ff<=b:return ('seg',i,ff)
        for ff in cands:
            for i in range(len(segs)):
                j=i+1
                if j>=len(segs):
                    if not s.loop:continue
                    nb=s.span(segs[0]);nb=(nb[0]+s.P,nb[1]+s.P)
                else:nb=s.span(segs[j])
                a,b=s.span(segs[i])
                if b<ff<nb[0]:return ('gap',i,ff,(ff-b)/(nb[0]-b),j%len(segs),b,nb[0])
        if not s.loop and segs[-1][0]=='free' and f>s.span(segs[-1])[1]:return ('seg',len(segs)-1,f)
        if not s.loop and segs[0][0]=='free' and f<s.span(segs[0])[0]:return ('seg',0,f)
        raise RuntimeError(f'{s.name} {kk} frame {f} not covered')
    def span(s,sg_):return (sg_[1],sg_[2]) if sg_[0]=='plant' else (sg_[1][0][0],sg_[1][-1][0])
    # ---- free arm spec -> world target
    def spec_target(s,kk,spec,b):
        if 'off' in spec:
            R=Matrix.Rotation(math.radians(b.get('yaw',0)),3,'Z');return k.shoulder_world(b,kk)+R@Vector((sg(kk[1])*spec['off'][0],-spec['off'][1],spec['off'][2]))
        if 'body' in spec:o=spec['body'];return k.to_world(b,Vector((sg(kk[1])*o[0],-o[1],o[2])))
        x,y,z=spec['pos'];return Vector((CX+sg(kk[1])*x,y,z))
    def free_eval(s,kk,keys,f,b):
        if f<=keys[0][0]:A=B=keys[0];u=0.
        elif f>=keys[-1][0]:A=B=keys[-1];u=0.
        else:
            for A,B in zip(keys,keys[1:]):
                if A[0]<=f<=B[0]:break
            u=K.EASE[B[2] if len(B)>2 else 'smooth']((f-A[0])/max(1e-9,B[0]-A[0]))
        a,bb=A[1],B[1];t=s.spec_target(kk,a,b).lerp(s.spec_target(kk,bb,b),u) if u else s.spec_target(kk,a,b)
        ca=a.get('curls',.3);cb_=bb.get('curls',.3);ca=ca if isinstance(ca,(list,tuple)) else [ca]*5;cb_=cb_ if isinstance(cb_,(list,tuple)) else [cb_]*5
        arm={'target':t,'pole':pole_lerp(a.get('pole',HP_CHEST),bb.get('pole',HP_CHEST),u),'curls':lerp(list(ca),list(cb_),u)}
        ha=a.get('hand',('rel',0,0,0));hb_=bb.get('hand',('rel',0,0,0))
        if ha[0]=='rel' and hb_[0]=='rel':arm['hand']=('rel',)+tuple(lerp(ha[1:],hb_[1:],u))
        else:arm['hand']=ha;arm['hand2']=hb_;arm['blend']=u
        return arm
    # ---- plants
    def plant_point(s,kk,P_,f):
        dy=s.v*(f-P_['land'])/FPS;dyaw=s.yaw(f)-s.yaw(P_['land']);Rz=Matrix.Rotation(math.radians(-dyaw),3,'Z')
        return Rz@(P_['p']+Vector((0,dy,0))),Rz@P_['R']
    def prepass(s,body):
        for kk,segs in s.arms.items():
            for i,sgm in enumerate(segs):
                if sgm[0]!='plant':continue
                land=sgm[1];x,y=sgm[3];style=sgm[4] if len(sgm)>4 else {};claw=style.get('claw',CRAWL['claw']);flex=style.get('flex_plant',CRAWL['flex'])
                b=body(s.wrapf(land));P={'body':b,'arms':{}};z=.15
                for it in range(3):
                    P['arms'][kk]={'target':Vector((CX+sg(kk[1])*x,y,z)),'pole':CRAWL['pole'][kk[0]],'hand':('rel',flex,0,0),'curls':claw,'exact':True};k.reset();k.apply(P)
                    R=k.pb[f'DEF_arm_{kk[0]}.{kk[1]}_hand'].matrix.to_3x3();z,vid,loc=k.contact(kk,claw,R)
                p=Vector((CX+sg(kk[1])*x,y,z));sh=k.shoulder_world(b,kk);L=sum(k.chain[kk])
                ext=[(p-sh).length/L]
                lift=sgm[2];bl=body(s.wrapf(lift));pl,_=s.plant_point(kk,{'p':p,'R':R,'land':land},lift);ext.append((pl-k.shoulder_world(bl,kk)).length/L)
                s.plants[(kk,i)]={'p':p,'R':R,'vid':vid,'local':loc,'land':land,'claw':claw}
                if max(ext)>.95:print('REACH',s.name,kk,i,'ext %.2f %.2f'%tuple(ext),flush=True)
    # ---- one arm at frame f
    def arm_at(s,kk,f,b):
        w=s.seg_at(kk,f);segs=s.arms[kk];pole=CRAWL['pole'].get(kk[0],HP_CHEST)
        if w[0]=='seg':
            i,ff=w[1],w[2];sgm=segs[i]
            if sgm[0]=='plant':
                P_=s.plants[(kk,i)];p,R=s.plant_point(kk,P_,ff)
                return {'target':p,'pole':pole,'hand':('mat',R),'curls':P_['claw'],'exact':True},{'stance':True,'vertex':P_['vid'],'local':P_['local'],'R':R}
            return s.free_eval(kk,sgm[1],ff,b),{'stance':False}
        _,i,ff,u,j,f0,f1=w;A=segs[i];B=segs[j];style=(B[4] if B[0]=='plant' and len(B)>4 else {})|(A[4].get('out',{}) if A[0]=='plant' and len(A)>4 else {})
        if A[0]=='plant':pa,Ra=s.plant_point(kk,s.plants[(kk,i)],f0);ha=('mat',Ra);ca=s.plants[(kk,i)]['claw'];pola=pole
        else:fa=s.free_eval(kk,A[1],A[1][-1][0],b);pa=fa['target'];ha=fa['hand'];ca=fa['curls'];pola=fa['pole']
        if B[0]=='plant':
            P_=s.plants[(kk,j)];pb,Rb=s.plant_point(kk,P_,P_['land']);hb_=('mat',Rb);cbb=P_['claw'];polb=pole
        else:fb=s.free_eval(kk,B[1],B[1][0][0],b);pb=fb['target'];hb_=fb['hand'];cbb=fb['curls'];polb=fb['pole']
        lift=style.get('lift',.12 if (A[0]=='plant' and B[0]=='plant') else .05);bul=style.get('bulge',.07);reach=style.get('reach',.05)
        if style.get('slap'):
            a=style.get('peak',.42);pr=ease_out(u,2.6)+reach*math.sin(math.pi*clamp(u/.8));hgt=lift*ease_out(u/a,2) if u<a else lift*(1-ease_in((u-a)/(1-a),2.6))
        else:
            a=style.get('peak',.45);pr=smooth(u)+reach*math.sin(math.pi*u)**2*(1-u)*2.2;hgt=lift*ease_out(u/a,1.8) if u<a else lift*(1-smooth((u-a)/(1-a)))
            hgt+=.012*math.sin(math.pi*u)**.5
        if B[0]!='plant':pr=smooth(u);hgt=lift*math.sin(math.pi*u)
        side=Vector((sg(kk[1]),0,0));t=pa.lerp(pb,pr)+side*bul*math.sin(math.pi*u)**1.2+Vector((0,0,hgt))
        if A[0]=='plant' and B[0]=='plant':
            t.x=pa.x+(pb.x-pa.x)*smooth(u)+sg(kk[1])*bul*math.sin(math.pi*u)**1.2;t.z=pa.z+(pb.z-pa.z)*smooth(u)+hgt
        air=('rel',style.get('flex',-15),style.get('dev',0),style.get('twist',0))
        if A[0]=='plant' and B[0]=='plant':
            arm={'target':t,'pole':pole,'hand':ha,'hand2':air,'blend':smooth(u/.45)} if u<.5 else {'target':t,'pole':pole,'hand':air,'hand2':hb_,'blend':smooth((u-.55)/.45)}
            o=style.get('open',-.05);rip=[smooth((u-.04-.04*j)/.3)*(1-smooth((u-.42-.04*j)/.3)) for j in range(5)]
            arm['curls']=[ca[j]+(o-ca[j])*rip[j] for j in range(5)]
        else:
            arm={'target':t,'pole':pole_lerp(pola,polb,smooth(u)),'hand':ha,'hand2':hb_,'blend':smooth(u),'curls':lerp(list(ca),list(cbb),smooth(u))}
        return arm,{'stance':False}
    # ---- full clip
    def frames_iter(s,body0,head):
        body=lambda f:floor_lift(body0(f));s.prepass(body);k.last_pole={};bodies=[body(s.wrapf(f)) for f in range(s.N)];clo=cloak_sim(bodies,s.loop)
        for f in range(s.N):
            ff=s.wrapf(f);b=bodies[f];arms={};recs={}
            for kk in s.arms:arms[kk],recs[kk]=s.arm_at(kk,ff,b)
            P={'body':b,'head':head_final(b,head(ff)),'cloak':clo[f],'arms':arms}
            if s.extra:s.extra(ff,P)
            yield P,recs
def cloak_sim(bodies,loop):
    """Three cloak controls lag the body: swing from vertical/forward velocity and pitch change, sway from roll/yaw."""
    n=len(bodies);sig=[]
    for i in range(n):
        a=bodies[i];p=bodies[(i-1)%(n-1)] if loop else bodies[max(0,i-1)]
        vz=(a['loc'][2]-p['loc'][2])*FPS;vy=(a['loc'][1]-p['loc'][1])*FPS;dp=(a.get('pitch',0)-p.get('pitch',0));dr=a.get('roll',0)-p.get('roll',0);dyw=a.get('yaw',0)-p.get('yaw',0)
        sig.append((clamp(-vz*14+vy*10-dp*.8,-14,14),clamp(dr*1.5+dyw*1.2,-10,10)))
    out=[[(0.,0.)]*3 for _ in range(n)];st=[[0.,0.] for _ in range(3)];lags=(.45,.3,.2)
    for rep in range(3 if loop else 1):
        for i in range(n):
            for c in range(3):
                tgt=sig[i] if c==0 else out[i][c-1] if False else sig[i];g=1.+.35*c
                st[c][0]+=(tgt[0]*g-st[c][0])*lags[c];st[c][1]+=(tgt[1]*g-st[c][1])*lags[c]
            out[i]=[(st[c][0],st[c][1]) for c in range(3)]
    if loop:out[-1]=out[0]
    return out
def gait_plants(steps,home,v,P,style_all=None):
    """steps[kk]=[(lift,land,style)] -> plant segments for a periodic gait (positions centred on the home stroke)."""
    arms={}
    for kk,st in steps.items():
        segs=[];n=len(st)
        for i in range(n):
            land=st[i][1];lift=st[(i+1)%n][0]+(P if i==n-1 else 0);stroke=v*(lift-land)/FPS;hx,hy=home[kk];sty=dict(style_all or {});sty.update(st[i][2] if len(st[i])>2 else {})
            segs.append(('plant',land,lift,(hx+sty.get('dx',0),hy-stroke/2+sty.get('dy',0)),sty))
        segs.sort(key=lambda x:x[1]);arms[kk]=segs
    return arms
def swing_w(steps,kk,f,P):
    for a,b,*_ in steps[kk]:
        for off in (0,P,-P):
            if a+off<=f<b+off:return math.sin(math.pi*(f-a-off)/(b-a))
    return 0.
def landing_dip(steps,f,P,dep2=.016,dep3=.011,width=3.):
    z=0
    for kk,st in steps.items():
        for a,land,*_ in st:
            for off in (-P,0,P):
                d=f-(land+off)
                if 0<=d<width*2.5:z-=(dep2 if kk[0]=='2' else dep3)*(d/width)*math.exp(1-d/width)
    return z
# ================================================================== CLIPS
CLIPS={}
def clip(fn):CLIPS[fn.__name__]=fn;return fn
def host_free(kk,f=0,**o):
    d=dict(HOST_ARM[kk]);d.update(o);return ('free',[(f,d)])
def crawl_guard(**o):d=dict(GUARD);d.update(o);return d
# ---------------------------------------------------------------- locomotion: crawl gaits
def gait_clip(name,frames,v,steps,bodyfn,headfn,hostfn,style=None,home=None,note=''):
    p=Perf(name,frames,loop=True,v=v,note=note);p.arms.update(gait_plants(steps,home or CRAWL['home'],v,p.P,style))
    for s_ in 'LR':p.arms['1'+s_]=('free_fn',hostfn)
    return p,bodyfn,headfn
@clip
def Walk():
    # Investigating stalk, 2 s loop (two steps per hand, the second hesitates/varies so it never reads as a 1 s tick).
    # Same-side hands never leave together; diagonal pairs overlap. Front hands reach OUT and PAST, hover, feel down.
    S={'2L':[(0,8,{}),(24,33,{'lift':.16,'reach':.09})],'3R':[(4,12,{}),(28,36,{'lift':.10})],'2R':[(12,20,{'lift':.14}),(36,44,{'bulge':.12})],'3L':[(16,24,{}),(40,48,{'lift':.10})]}
    P=48
    def body(f):
        roll=yaw=pitch=0
        for kk in S:
            w=swing_w(S,kk,f,P);roll+=-sg(kk[1])*2.6*w;yaw+=(sg(kk[1])*3.5 if kk[0]=='2' else -sg(kk[1])*1.5)*w;pitch+=(-2.2 if kk[0]=='2' else 1.4)*w
        return cb(dz=.02+landing_dip(S,f,P),dy=-.012*math.sin(2*math.tau*f/P+.6),pitch=pitch,roll=roll,yaw=yaw)
    turn=T([(0,-9),(14,-9),(20,7,'snap'),(34,10),(40,-9)],P);tilt=T([(0,0),(22,0),(25,16,'snap'),(36,13),(46,0)],P)
    def head(f):return H(turn=turn(f),tilt=tilt(f),nod=2*math.sin(f/P*2*math.tau),stab=.85)
    def host(kk,f,b):
        ph=0 if kk[1]=='R' else .47;u=(f/P+ph)%1;reach=smooth(clamp((u-.04)/.42))*(1-ease_in(clamp((u-.62)/.26),1.6));feel=reach*(1-reach)*4
        return {'off':lerp((.16,.12,-.10),(.26,.38,.04),reach),'pole':lerp(HP_CHEST,HP_REACH,reach),'hand':('rel',22-40*reach,8*reach,0),
                'curls':[.55-.6*reach+.18*feel*math.sin(f*.5+j*1.1+(0 if kk[1]=='L' else 2.2)) for j in range(5)]}
    return gait_clip('Walk',49,.3667,S,body,head,host,{'lift':.13,'bulge':.09,'reach':.06},note='2 s loop (was 1 s): two steps per hand with a varied second cycle')
@clip
def Sneak():
    # Slowest investigate: one step per hand per 2 s, long hover with fingers feeling the air before each plant.
    S={'2L':[(0,16,{'lift':.15,'reach':.08,'peak':.35})],'3R':[(8,24,{'lift':.09})],'2R':[(24,40,{'lift':.13,'reach':.07,'peak':.35})],'3L':[(32,48,{'lift':.09})]}
    P=48
    def body(f):
        roll=yaw=pitch=0
        for kk in S:
            w=swing_w(S,kk,f,P);roll+=-sg(kk[1])*3.2*w;yaw+=(sg(kk[1])*4 if kk[0]=='2' else 0)*w;pitch+=(-1.5 if kk[0]=='2' else 1)*w
        return cb(dz=-.04+landing_dip(S,f,P,.010,.007,4),dy=-.01*math.sin(math.tau*f/P),pitch=5+pitch,roll=roll,yaw=yaw)
    turn=T([(0,-14),(18,-14),(26,12,'in'),(30,16,'snap'),(44,-14,'smooth')],P);tilt=T([(0,-6),(24,8),(30,24,'snap'),(42,-6)],P)
    def head(f):return H(turn=turn(f),tilt=tilt(f),nod=4,stab=.9)
    def host(kk,f,b):
        u=(f/P+(0 if kk[1]=='L' else .5))%1;reach=smooth(clamp(u/.5))*(1-smooth(clamp((u-.6)/.4)))
        wrong=smooth(clamp((u-.30)/.12))*(1-smooth(clamp((u-.46)/.04)))   # wrist bends back too far, then snaps
        return {'off':lerp((.18,.20,-.12),(.24,.40,-.16),reach),'pole':lerp(HP_CHEST,HP_REACH,reach),'hand':('rel',15-35*reach-45*wrong,-6*reach,0),
                'curls':[.45-.5*reach+.22*reach*math.sin(f*.33+j*1.5+(0 if kk[1]=='L' else 1.7)) for j in range(5)]}
    return gait_clip('Sneak',49,.1333,S,body,head,host,{'lift':.12,'bulge':.10,'reach':.07,'open':-.12})
@clip
def Stalk():
    # Menacing locomotion between Sneak and Walk: heavy deliberate plants, body rolling over each hand, head locked dead on.
    S={'2L':[(0,9,{'lift':.18}),(24,34,{'lift':.16})],'3R':[(4,12,{}),(28,36,{})],'2R':[(12,21,{'lift':.17}),(36,45,{'lift':.18})],'3L':[(16,24,{}),(40,48,{})]}
    P=48
    def body(f):
        roll=yaw=0
        for kk in S:w=swing_w(S,kk,f,P);roll+=-sg(kk[1])*4.5*w;yaw+=(sg(kk[1])*5 if kk[0]=='2' else -sg(kk[1])*2)*w
        return cb(dz=.03+landing_dip(S,f,P,.03,.018,3.5),pitch=-4,roll=roll,yaw=yaw)
    def head(f):return H(turn=0,tilt=4*math.sin(f/P*math.tau),nod=-6,stab=1.)
    def host(kk,f,b):
        u=(f/P+(0 if kk[1]=='L' else .5))%1;r=smooth(clamp((u-.1)/.5))*(1-smooth(clamp((u-.7)/.3)))
        return {'off':lerp((.30,.20,.06),(.32,.30,.16),r),'pole':lerp(HP_REACH,HP_UP,.4),'hand':('rel',-25,10,0),'curls':[.15,.05,.1,.15,.2]}
    return gait_clip('Stalk',49,.24,S,body,head,host,{'lift':.16,'bulge':.10,'reach':.03},note='new; heavier than Walk, slower than it')
@clip
def Chase():
    # Scrabbling trot: diagonal pairs land together, hands whip out wide and slap.
    S={'2L':[(0,6,{})],'3R':[(1,7,{})],'2R':[(8,14,{})],'3L':[(9,15,{})]};P=16
    def body(f):return cb(dz=.01+landing_dip(S,f,P,.03,.02,1.8),pitch=6+3*math.sin(f/P*2*math.tau),roll=4*math.sin(f/P*math.tau),yaw=-5*math.sin(f/P*math.tau))
    def head(f):return H(stab=1.,nod=-4,turn=2*math.sin(f/P*math.tau+1))
    def host(kk,f,b):
        u=(f/P+(0 if kk[1]=='L' else .5))%1;r=.5+.5*math.sin(u*math.tau)
        return {'off':lerp((.22,.18,-.08),(.30,.40,-.02),r),'pole':HP_REACH,'hand':('rel',-10,0,0),'curls':lerp([.5]*5,[.0,-.1,-.05,0,.05],r)}
    return gait_clip('Chase',17,.60,S,body,head,host,{'slap':True,'lift':.14,'bulge':.14,'reach':.04,'peak':.4,'open':-.15})
@clip
def RunChase():
    # Bounding pursuit (0.5 s): the front pair slaps down almost together, body drops onto them, rear pair drives it on.
    S={'2L':[(3,12,{})],'2R':[(4,13,{})],'3L':[(9,18,{})],'3R':[(10,19,{})]};P=12
    home={'2L':(.34,-.38),'2R':(.34,-.38),'3L':(.46,-.02),'3R':(.46,-.02)}
    def body(f):
        ph=f/P;dz=.05*math.sin(math.tau*(ph-.15))-.02;pitch=8+7*math.sin(math.tau*(ph-.05))
        return cb(dz=dz-.03,dy=-.03*math.sin(math.tau*ph),pitch=pitch,roll=3*math.sin(math.tau*ph+.5),yaw=-3*math.sin(math.tau*ph))
    def head(f):return H(stab=1.,nod=-6+3*math.sin(math.tau*f/P))
    def host(kk,f,b):
        r=.5+.5*math.sin(math.tau*(f/P+(0 if kk[1]=='L' else .12)))
        return {'off':lerp((.26,.14,-.04),(.36,.36,.06),r),'pole':HP_REACH,'hand':('rel',-20,8,0),'curls':lerp([.55]*5,[-.1,-.15,-.1,-.05,0],r)}
    return gait_clip('RunChase',13,2.24,S,body,head,host,{'slap':True,'lift':.22,'bulge':.20,'reach':.03,'peak':.38,'open':-.2},home=home)
@clip
def WallCrawl():
    # Clinging climb (surface = local floor): one hand at a time, body pressed flat, host arms reach on ahead.
    S={'2L':[(0,12,{'lift':.10})],'3R':[(12,24,{'lift':.07})],'2R':[(24,36,{'lift':.10})],'3L':[(36,48,{'lift':.07})]};P=48
    def body(f):
        roll=0
        for kk in S:roll+=-sg(kk[1])*2.5*swing_w(S,kk,f,P)
        return cb(dz=-.07+landing_dip(S,f,P,.01,.008,4),pitch=10,roll=roll,yaw=3*math.sin(f/P*math.tau))
    def head(f):return H(stab=.6,nod=-20,turn=8*math.sin(f/P*math.tau+.5),tilt=-5)
    def host(kk,f,b):
        u=(f/P+(0 if kk[1]=='L' else .5))%1;r=smooth(clamp(u/.45))*(1-smooth(clamp((u-.55)/.45)))
        return {'off':lerp((.20,.30,-.30),(.24,.46,-.26),r),'pole':HP_REACH,'hand':('rel',lerp(30,-10,r),0,0),'curls':lerp([.6]*5,[.05]*5,r)}
    return gait_clip('WallCrawl',49,.13235,S,body,head,host,{'bulge':.06,'reach':.02})
@clip
def CeilingCrawl():
    # Hanging inverted (ceiling = local floor): slow sequential re-grips, 80% stance; head cranes toward the room below.
    S={'2L':[(0,14,{})],'3L':[(18,32,{})],'2R':[(36,50,{})],'3R':[(54,68,{})]};P=72
    def body(f):
        roll=0
        for kk in S:roll+=-sg(kk[1])*2*swing_w(S,kk,f,P)
        return cb(dz=-.05+.012*math.sin(f/P*2*math.tau),pitch=0,roll=roll+2*math.sin(f/P*math.tau),yaw=4*math.sin(f/P*math.tau+1))
    tilt=T([(0,0),(30,0),(34,-22,'snap'),(52,-18),(70,0)],P)
    def head(f):return H(stab=.5,nod=-38,turn=12*math.sin(f/P*math.tau),tilt=tilt(f))
    def host(kk,f,b):
        return {'off':(.22,.22,-.22),'pole':HP_CHEST,'hand':('rel',25,0,0),'curls':curls([.6]*5,.8,1 if kk[1]=='L' else 3,f)}
    return gait_clip('CeilingCrawl',73,.05833,S,body,head,host,{'lift':.08,'bulge':.05,'reach':.02})
@clip
def Squeeze():
    # Forced through a gap: flattened and narrow, elbows pinned high, hands placed close in. Loops (game plays it looped).
    S={'2L':[(0,16,{})],'3R':[(18,34,{})],'2R':[(36,52,{})],'3L':[(54,70,{})]};P=72
    home={'2L':(.24,-.42),'2R':(.24,-.42),'3L':(.30,-.06),'3R':(.30,-.06)}
    def body(f):return cb(dz=-.10+.01*math.sin(f/P*4*math.tau),pitch=12,roll=3*math.sin(f/P*math.tau),yaw=4*math.sin(f/P*math.tau+.4))
    def head(f):return H(stab=.7,nod=-15,tilt=18*math.sin(f/P*math.tau),turn=-6)
    def host(kk,f,b):return {'off':(.10,.24,-.22),'pole':HP_UP,'hand':('rel',30,0,0),'curls':.65}
    return gait_clip('Squeeze',73,.05556,S,body,head,host,{'lift':.07,'bulge':.02,'reach':.02},home=home,note='closed loop (game loops it)')
# ---------------------------------------------------------------- host form
def host_clip(name,frames,body,head,arms_extra=None,loop=True,v=0.,note=''):
    p=Perf(name,frames,loop=loop,v=v,note=note)
    for kk in ['1L','1R','2L','2R','3L','3R']:p.arms[kk]=('free_fn',arms_extra.get(kk) if arms_extra and kk in arms_extra else (lambda kk_,f,b:dict(HOST_ARM[kk_])))
    return p,body,head
@clip
def Idle():
    # Unaware host, near-human but off: a long slow inhale and a short exhale (wrong rhythm), the head keeps drifting a little
    # too far, and one finger of the clasped right hand ticks on its own in a broken rhythm while everything else is still.
    P=96
    def br(f):
        u=(f%48)/48;return smooth(u/.78) if u<.78 else 1-smooth((u-.78)/.22)
    def body(f):b=br(f);return hb(dz=.007*b,pitch=-.6*b,roll=.8*math.sin(f/P*math.tau))
    tilt=T([(0,0),(30,0),(60,7,'smooth'),(82,7),(96,0)],P)
    def head(f):return H(stab=0,nod=4,tilt=tilt(f),turn=2.5*math.sin(f/P*math.tau+.7))
    def hand(kk,f,b):
        d=dict(HOST_ARM[kk]);g=pulse(f,40 if kk=='1L' else 44,3,10);d['curls']=[c+.25*g*(1 if j>0 else .3) for j,c in enumerate(d['curls'])]
        if kk=='1R':
            tick=sum(pulse(f,a,1,2.2) for a in (12,15,27,70,72,74));c=list(d['curls']);c[1]-=.45*tick;d['curls']=c
        return d
    def coil(kk,f,b):
        d=dict(HOST_ARM[kk]);w=smooth(clamp((f-60)/10))*(1-smooth(clamp((f-80)/14)));x,y,z=d['pos'];d['pos']=(x+.03*w*(kk[1]=='L'),y-.02*w,z+.03*w);return d
    return host_clip('Idle',97,body,head,{'1L':hand,'1R':hand,'2L':coil})
@clip
def Idle_Twitch():
    # Stillness, then a sudden wrong twitch (head 2 frames, fingers flare), then stillness again. Loop.
    P=96
    def body(f):return hb(dz=.004*(.5-.5*math.cos(f/P*math.tau*2)),roll=1.5*pulse(f,38,1,3))
    tilt=T([(0,0),(37,0),(39,17,'snap'),(46,15),(70,0,'smooth')],P);turn=T([(0,0),(37,0),(39,-9,'snap'),(52,-6),(74,0)],P)
    def head(f):return H(stab=0,nod=3+4*pulse(f,38,1,4),tilt=tilt(f)+3*pulse(f,80,1,2),turn=turn(f))
    def hand(kk,f,b):
        d=dict(HOST_ARM[kk]);g=pulse(f,39,1,8)*(1 if kk=='1L' else .7);d['curls']=[c-.55*g for c in d['curls']];d['hand']=('rel',10-15*g,0,0);return d
    def coil(kk,f,b):
        d=dict(HOST_ARM[kk]);g=pulse(f,80,1,5);x,y,z=d['pos'];d['pos']=(x+.03*g,y-.04*g,z);return d
    return host_clip('Idle_Twitch',97,body,head,{'1L':hand,'1R':hand,'3R':coil})
@clip
def Idle_HeadTilt():
    # The head lists slowly past where a neck should stop, holds, and returns. Loop.
    P=96
    def body(f):return hb(dz=.004*(.5-.5*math.cos(f/P*math.tau*2)),roll=-2*smooth(clamp((f-10)/30))*(1-smooth(clamp((f-66)/24))))
    tilt=T([(0,0),(8,0),(40,38,'smooth'),(62,40),(90,0,'smooth')],P);nod=T([(0,4),(40,12),(62,14),(90,4)],P)
    def head(f):return H(stab=0,tilt=tilt(f),nod=nod(f),turn=-6*smooth(clamp((f-20)/30))*(1-smooth(clamp((f-62)/28))))
    return host_clip('Idle_HeadTilt',97,body,head)
@clip
def Idle_Breathe():
    # Deep slow breaths: the mantle lifts and settles (FULL_HORROR), host arms flex open/closed with it. Loop.
    P=96
    def br(f):return .5-.5*math.cos(f/P*2*math.tau)
    def body(f):return hb(dz=.016*br(f),pitch=-1.5*br(f),mantle=.22*br(f))
    def head(f):return H(stab=0,nod=4-5*br(f))
    def hand(kk,f,b):
        d=dict(HOST_ARM[kk]);x,y,z=d['pos'];o=br(f-4);d['pos']=(x+.03*o,y-.02*o,z+.01*o);d['curls']=[c-.3*o for c in d['curls']];d['hand']=('rel',10-12*o,0,0);return d
    return host_clip('Idle_Breathe',97,body,head,{'1L':hand,'1R':hand})
@clip
def HostWalk():
    # Folded patrol: a glide, not a walk. Leans into travel, hem lifted off the floor, slow sway; no contact claim.
    P=48
    def body(f):return hb(dz=.022+.008*math.sin(f/P*2*math.tau),pitch=3.5,roll=1.6*math.sin(f/P*math.tau),yaw=1.5*math.sin(f/P*math.tau+.8),dy=-.01*math.sin(f/P*2*math.tau+.5))
    def head(f):return H(stab=.5,nod=5,turn=-2*math.sin(f/P*math.tau+.4),tilt=1.5*math.sin(f/P*math.tau+1.6))
    return host_clip('HostWalk',49,body,head,v=0.,note='glide, 65 cm/s suggested')
# ---------------------------------------------------------------- aware, planted (crawl) acting
def crawl_plants(f0=0,f1=None,home=None,claw=None):
    home=home or CRAWL['home'];return {kk:[('plant',f0,f1,home[kk],{'claw':claw} if claw else {})] for kk in CRAWLK}
def acting(name,frames,body,head,arms,host=None,events=None,note=''):
    p=Perf(name,frames,loop=False,events=events,note=note);p.arms.update(arms)
    for s_ in 'LR':
        kk='1'+s_;p.arms[kk]=host(kk) if host else ('free',[(0,crawl_guard())])
    return p,body,head
@clip
def Listen():
    # Freeze. Head cocks slowly toward the sound, one host hand lifts with fingers spread (hush), a hard snap the other way, hold.
    bk=T([(0,cb()),(10,cb(dz=-.03,pitch=4),'out'),(40,cb(dz=-.03,pitch=4,yaw=-6)),(44,cb(dz=-.01,pitch=2,yaw=8),'snap'),(72,cb(dz=-.02,yaw=6))])
    hd=T([(0,H()),(8,H(nod=2)),(30,H(tilt=28,turn=-14,nod=6),'smooth'),(40,H(tilt=30,turn=-16,nod=6)),(43,H(tilt=-12,turn=22,nod=-4),'out'),(60,H(tilt=-16,turn=20,nod=-2)),(72,H(tilt=-8,turn=12))])
    def host(kk):
        if kk=='1L':return ('free',[(0,crawl_guard()),(16,crawl_guard(off=(.24,.26,.10),hand=('rel',-20,0,0),curls=[-.05,-.15,-.1,-.05,0],pole=HP_REACH),'smooth'),(42,crawl_guard(off=(.24,.26,.10),hand=('rel',-20,0,0),curls=[-.05,-.15,-.1,-.05,0],pole=HP_REACH)),(46,crawl_guard(off=(.20,.22,.08),hand=('rel',-5,0,0),curls=[.2,.1,.15,.2,.25],pole=HP_REACH),'snap'),(72,crawl_guard())])
        return ('free',[(0,crawl_guard()),(42,crawl_guard()),(45,crawl_guard(off=(.20,.18,-.10),curls=[.3,.2,.3,.4,.45]),'snap'),(72,crawl_guard())])
    return acting('Listen',73,bk,hd,crawl_plants(0,72),host)
@clip
def Alert():
    # Startle: jerks up and back in 3 frames, head snaps onto the target, host arms flare OUT, then coils low and ready.
    bk=T([(0,cb()),(2,cb(dz=-.005,pitch=2),'in'),(5,cb(dz=.035,dy=.0,pitch=-1),'snap'),(9,cb(dz=.03,dy=.0,pitch=-1),'out'),(18,cb(dz=-.04,dy=-.01,pitch=6),'smooth'),(24,cb(dz=-.035,pitch=5))])
    hd=T([(0,H()),(2,H(nod=4)),(5,H(nod=-10,turn=0,tilt=-6),'snap'),(9,H(nod=-8,tilt=-4)),(24,H(nod=-4,tilt=6))])
    def host(kk):
        return ('free',[(0,crawl_guard()),(2,crawl_guard(off=(.15,.10,-.16)),'in'),(6,crawl_guard(off=(.38,.20,.12),pole=HP_REACH,hand=('rel',-30,0,0),curls=[-.2,-.25,-.2,-.15,-.1]),'out'),(10,crawl_guard(off=(.36,.22,.10),pole=HP_REACH,hand=('rel',-25,0,0),curls=[-.1,-.2,-.15,-.1,-.05]),'out'),(24,crawl_guard(off=(.24,.24,-.04),curls=[.3,.35,.4,.4,.4]),'smooth')])
    return acting('Alert',25,bk,hd,crawl_plants(0,24),host)
def replant(kk,f_lift,f_land,new,home=None,style=None,end=None):
    home=home or CRAWL['home'];return [('plant',0,f_lift,home[kk]),('plant',f_land,end,new,style or {})]
@clip
def SearchUnder():
    # Drops low and cranes sideways to peer under something; a host hand sweeps the floor; a second look the other way.
    N=96;bk=T([(0,cb()),(14,cb(dz=-.06,pitch=10,dy=-.04),'smooth'),(40,cb(dz=-.07,pitch=11,dy=-.05,roll=-5,yaw=-6)),(52,cb(dz=-.06,pitch=10,dy=-.04,roll=4,yaw=8),'smooth'),(78,cb(dz=-.06,pitch=10,dy=-.04,roll=4,yaw=8)),(N,cb(),'smooth')])
    hd=T([(0,H()),(16,H(nod=30,tilt=34,turn=-10),'smooth'),(38,H(nod=32,tilt=38,turn=-14)),(41,H(nod=30,tilt=36,turn=-18),'snap'),(54,H(nod=28,tilt=-36,turn=16),'smooth'),(64,H(nod=30,tilt=-40,turn=18)),(66,H(nod=26,tilt=-34,turn=12),'snap'),(80,H(nod=26,tilt=-34,turn=12)),(N,H(),'smooth')])
    arms=crawl_plants(0,N);arms['2L']=[('plant',0,44,CRAWL['home']['2L']),('plant',52,N,(.36,-.42))]
    def host(kk):
        if kk=='1R':return ('free',[(0,crawl_guard()),(18,crawl_guard(off=(.20,.40,-.40),hand=('rel',-10,0,0),curls=[.1,0,.05,.1,.15],pole=HP_REACH),'smooth'),(30,crawl_guard(off=(.02,.46,-.42),hand=('rel',-15,0,0),curls=[.2,-.05,.15,.25,.3],pole=HP_REACH),'smooth'),(40,crawl_guard(off=(.26,.40,-.40),hand=('rel',-10,0,0),curls=[.1,.25,.1,.3,.2],pole=HP_REACH),'smooth'),(60,crawl_guard()),(N,crawl_guard())])
        return ('free',[(0,crawl_guard()),(56,crawl_guard()),(70,crawl_guard(off=(.26,.40,-.38),hand=('rel',-15,0,0),curls=[.2,0,.1,.2,.3],pole=HP_REACH),'smooth'),(84,crawl_guard(off=(.26,.40,-.38),hand=('rel',-15,0,0),curls=[.35,.2,.3,.35,.4],pole=HP_REACH)),(N,crawl_guard())])
    return acting('SearchUnder',97,bk,hd,arms,host)
@clip
def SearchOver():
    # Rises onto the rear hands and the robe to feel over something high; host arms climb the air; head cranes up.
    N=96;up=cb(dz=.10,dy=.04,pitch=-26)
    bk=T([(0,cb()),(10,cb(dz=-.03,pitch=4),'in'),(26,up,'out'),(60,cb(dz=.10,dy=.04,pitch=-28,yaw=5)),(70,cb(dz=.09,dy=.04,pitch=-24,yaw=-5),'smooth'),(N,cb(),'smooth')])
    hd=T([(0,H()),(26,H(nod=-30,stab=.5),'out'),(44,H(nod=-36,tilt=14,turn=-10,stab=.5)),(48,H(nod=-30,tilt=-10,turn=12,stab=.5),'snap'),(72,H(nod=-28,tilt=-8,turn=8,stab=.5)),(N,H(),'smooth')])
    arms={'3L':[('plant',0,N,(.36,.0))],'3R':[('plant',0,N,(.36,.0))]}
    for s_ in 'LR':
        kk='2'+s_;arms[kk]=[('plant',0,12,CRAWL['home'][kk]),('free',[(22,{'off':(.30,.18,.22),'pole':(.2,-.6,.8),'hand':('rel',-20,0,0),'curls':.1}),(60,{'off':(.26,.16,.30),'pole':(.2,-.6,.8),'hand':('rel',-25,0,0),'curls':.0}),(76,{'off':(.30,.20,.10),'pole':(.2,-.6,.8),'hand':('rel',0,0,0),'curls':.3})]),('plant',88,N,CRAWL['home'][kk],{'slap':False})]
    def host(kk):
        hi=lambda t,c:crawl_guard(off=(.20,.18,.40+t),pole=(.3,.5,-.8),hand=('rel',-30,0,0),curls=c)
        ph=4 if kk=='1L' else 0
        return ('free',[(0,crawl_guard()),(24+ph,hi(0,.1),'out'),(38+ph,hi(.04,[.3,0,.2,-.1,.1])),(50+ph,hi(.02,[0,.3,0,.2,0])),(62+ph,hi(.03,[.2,-.1,.3,0,.2])),(80,crawl_guard(),'smooth'),(N,crawl_guard())])
    return acting('SearchOver',97,bk,hd,arms,host)
@clip
def Search_LookAround():
    # Upper body sweeps one way then the other; head leads by 5 frames; host arms feel OUT toward each side; one front re-plant.
    N=96;bk=T([(0,cb()),(20,cb(yaw=-17,roll=-3),'smooth'),(34,cb(yaw=-19,roll=-3)),(54,cb(yaw=16,roll=4),'smooth'),(70,cb(yaw=18,roll=4)),(N,cb(),'smooth')])
    hd=T([(0,H()),(15,H(turn=-30,tilt=8),'smooth'),(34,H(turn=-34,tilt=12)),(48,H(turn=34,tilt=-10),'smooth'),(70,H(turn=36,tilt=-14)),(90,H(),'smooth')])
    arms=crawl_plants(0,N);arms['2R']=[('plant',0,22,CRAWL['home']['2R']),('plant',30,58,(.33,-.30)),('plant',66,N,CRAWL['home']['2R'])]
    def host(kk):
        out=lambda:crawl_guard(off=(.36,.26,.02),pole=HP_REACH,hand=('rel',-25,0,0),curls=[-.1,-.2,-.1,0,.05])
        if kk=='1R':return ('free',[(0,crawl_guard()),(18,out(),'smooth'),(34,dict(out(),curls=[.2,-.1,.3,.1,.2])),(46,crawl_guard(),'smooth'),(N,crawl_guard())])
        return ('free',[(0,crawl_guard()),(44,crawl_guard()),(58,out(),'smooth'),(72,dict(out(),curls=[0,.25,-.1,.2,.1])),(86,crawl_guard(),'smooth'),(N,crawl_guard())])
    return acting('Search_LookAround',97,bk,hd,arms,host)
@clip
def Search_Peek():
    # Sinks low to one side, neck stretches and the head cranes round a corner at an impossible angle, holds, retracts.
    N=96;bk=T([(0,cb()),(18,cb(dz=-.05,dx=.03,roll=4,yaw=9,pitch=6),'smooth'),(66,cb(dz=-.06,dx=.03,roll=5,yaw=10,pitch=7)),(N,cb(),'smooth')])
    hd=T([(0,H()),(16,H(turn=10,nod=6),'smooth'),(30,H(turn=40,tilt=-48,nod=10,fwd=.06),'smooth'),(54,H(turn=42,tilt=-52,nod=10,fwd=.07)),(57,H(turn=36,tilt=-46,nod=6,fwd=.06),'snap'),(66,H(turn=36,tilt=-46,nod=6,fwd=.06)),(86,H(),'smooth'),(N,H())])
    arms=crawl_plants(0,N)
    def host(kk):
        if kk=='1L':return ('free',[(0,crawl_guard()),(20,crawl_guard(off=(.30,.30,-.10),pole=HP_REACH,hand=('rel',-10,0,0),curls=[.3,.1,.2,.3,.35]),'smooth'),(66,crawl_guard(off=(.30,.32,-.10),pole=HP_REACH,hand=('rel',-10,0,0),curls=[.4,.3,.35,.4,.4])),(N,crawl_guard(),'smooth')])
        return ('free',[(0,crawl_guard()),(N,crawl_guard())])
    return acting('Search_Peek',97,bk,hd,arms,host)
@clip
def Sniff():
    # Head lifts, two sharp sniffs (2-frame jerks with a body hitch and mantle quiver), then turns onto the scent.
    N=48;bk=T([(0,cb()),(10,cb(dz=.012,pitch=-2),'smooth'),(N,cb(dz=.0,yaw=8),'smooth')])
    hd=T([(0,H()),(10,H(nod=-24),'smooth'),(30,H(nod=-22)),(40,H(nod=-6,turn=18,tilt=-8),'smooth'),(N,H(nod=-4,turn=20,tilt=-8))])
    def head(f):h=hd(f);s_=pulse(f,15,1,2.5)+pulse(f,21,1,2.5);h=dict(h);h['nod']-=7*s_;h['fwd']=h.get('fwd',0)-.015*s_;return h
    def body(f):b=dict(bk(f));s_=pulse(f,15,1,2.5)+pulse(f,21,1,2.5);l=list(b['loc']);l[2]+=.004*s_;b['loc']=tuple(l);return b
    return acting('Sniff',49,body,head,crawl_plants(0,N),events={'sniff_1':15/FPS,'sniff_2':21/FPS})
@clip
def LostYou():
    # Stops dead. Head jerks one way, the other, back (snaps with holds); host arms retract to the chest; hands draw in; sinks.
    N=60;bk=T([(0,cb()),(4,cb(dz=.03,pitch=-3),'out'),(40,cb(dz=.02,pitch=-2)),(N,cb(dz=-.06,pitch=8),'smooth')])
    hd=T([(0,H()),(4,H(nod=-6)),(8,H(turn=-38,tilt=10),'out'),(16,H(turn=-36,tilt=12)),(19,H(turn=34,tilt=-14),'out'),(28,H(turn=32,tilt=-12)),(31,H(turn=-6,tilt=20),'out'),(42,H(turn=-4,tilt=22)),(N,H(nod=10,tilt=8),'smooth')])
    arms=crawl_plants(0,N);arms['2L']=[('plant',0,40,CRAWL['home']['2L']),('plant',48,N,(.32,-.36))];arms['2R']=[('plant',0,43,CRAWL['home']['2R']),('plant',51,N,(.32,-.36))]
    def host(kk):return ('free',[(0,crawl_guard(off=(.26,.30,-.02),pole=HP_REACH,curls=.2,hand=('rel',-10,0,0))),(36,crawl_guard(off=(.26,.30,-.02),pole=HP_REACH,curls=.2,hand=('rel',-10,0,0))),(46,crawl_guard(off=(.10,.10,-.12),curls=.7,hand=('rel',40,0,0)),'smooth'),(N,crawl_guard(off=(.10,.10,-.12),curls=.75,hand=('rel',40,0,0)))])
    return acting('LostYou',61,bk,hd,arms,host)
def turn_clip(name,sign):
    # In-place 90 deg turn as a spider scuttle. The ACTOR yaws linearly 90 deg over the clip (contract: actor_yaw_rate);
    # the body turns in three 30 deg bursts and every hand re-plants once per burst, so planted hands never slide.
    N=36;Y=lambda f:sign*90*clamp(f/N);C=[7,18,29]
    Bw=lambda f:sign*30*sum(smooth((f-c+4)/8) for c in C)
    p=Perf(name,N+1,loop=False,yaw=Y,events={'actor_yaw_deg':sign*90,'actor_yaw_rate_deg_s':sign*90/(N/FPS),'profile':'linear'})
    def landpos(kk,fl,b_after):
        h=CRAWL['home'][kk];v=Vector((CX+sg(kk[1])*h[0],h[1],0));w=Matrix.Rotation(math.radians(b_after-Y(fl)),3,'Z')@v;return (sg(kk[1])*(w.x-CX),w.y)
    order=['2L','3R','2R','3L'] if sign>0 else ['2R','3L','2L','3R']
    for i,kk in enumerate(order):
        segs=[('plant',0,C[0]-5+2*i,CRAWL['home'][kk])]
        for bi,c in enumerate(C):
            a=c-5+2*i;b=a+5;nxt=(C[bi+1]-5+2*i) if bi+1<len(C) else N
            segs.append(('plant',b,nxt,landpos(kk,b,sign*30*(bi+1)),{'lift':.09,'bulge':.03,'reach':.0}))
        p.arms[kk]=segs
    body=lambda f:cb(yaw=Bw(f)-Y(f),roll=-sign*2*sum(math.sin(math.pi*clamp((f-c+4)/8)) for c in C),dz=.012*sum(math.sin(math.pi*clamp((f-c+4)/8)) for c in C))
    head=lambda f:H(turn=sign*30*sum(smooth((f-c+7)/7) for c in C)-Bw(f)+sign*8*math.sin(math.pi*clamp(f/N)),tilt=-sign*5*math.sin(math.pi*clamp(f/N)),stab=.6)
    for s_ in 'LR':p.arms['1'+s_]=('free',[(0,crawl_guard()),(N//2,crawl_guard(off=(.20,.18,-.10))),(N,crawl_guard())])
    return p,body,head
@clip
def Turn_L90():return turn_clip('Turn_L90',1)
@clip
def Turn_R90():return turn_clip('Turn_R90',-1)
@clip
def Lunge():
    # Stalk -> chase burst: coils back and low, rears, every arm throws OUT and forward, front hands slam down far ahead.
    N=30;bk=T([(0,cb()),(8,cb(dz=-.06,dy=.08,pitch=10),'smooth'),(12,cb(dz=.07,dy=-.04,pitch=-8),'snap'),(16,cb(dz=-.02,dy=-.08,pitch=12),'hit'),(19,cb(dz=-.05,dy=-.08,pitch=12),'out'),(N,cb(dz=-.01,pitch=6),'smooth')])
    hd=T([(0,H()),(8,H(nod=10)),(12,H(nod=-14,tilt=6),'snap'),(16,H(nod=-4),'hit'),(N,H(nod=-6),'smooth')])
    arms={}
    for s_ in 'LR':
        a='2'+s_;arms[a]=[('plant',0,6,CRAWL['home'][a]),('free',[(11,{'off':(.40,.30,.06),'pole':(.4,-.6,.7),'hand':('rel',-30,0,0),'curls':-.15})]),('plant',16,N,(.33,-.45),{'slap':True,'lift':.05})]
        b='3'+s_;arms[b]=[('plant',0,N,CRAWL['home'][b])]
    def host(kk):return ('free',[(0,crawl_guard()),(8,crawl_guard(off=(.12,.06,-.10),curls=.6),'smooth'),(12,crawl_guard(off=(.36,.36,.14),pole=HP_REACH,hand=('rel',-35,0,0),curls=[-.2,-.25,-.2,-.15,-.1]),'out'),(18,crawl_guard(off=(.34,.38,.06),pole=HP_REACH,hand=('rel',-20,0,0),curls=-.1),'out'),(N,crawl_guard(off=(.30,.36,-.02),pole=HP_REACH,hand=('rel',-20,8,0),curls=[.5]*5),'smooth')])
    return acting('Lunge',31,bk,hd,arms,host,events={'slam_s':16/FPS})
# ---------------------------------------------------------------- transitions host <-> crawl
def fold_plants(f_land,order_gap=3,home=None):
    home=home or CRAWL['home'];out={}
    for i,kk in enumerate(['2L','2R','3L','3R']):out[kk]=f_land+[0,1,3,4][i]*order_gap//2
    return out
def unfold_clip(name,N,beats,reach_spec,events=None):
    """host -> crawl. beats: notice, burst(arms out), drop, plant frames."""
    b0,b1,b2,b3=beats;p=Perf(name,N+1,events=events)
    bk=T([(0,hb()),(b0,hb(dz=.01)),(b1,hb(dz=.08,pitch=-4,mantle=.5),'out'),(b1+6,hb(dz=.09,pitch=-5,mantle=.7)),(b2,cb(dz=.10,pitch=-10),'in'),(b3,cb(dz=-.03,pitch=4),'hit'),(N,cb(),'smooth')])
    hd=T([(0,H(stab=0,nod=4)),(b0-6,H(stab=0,nod=4)),(b0,H(stab=0,nod=0,turn=0,tilt=14),'snap'),(b1,H(stab=.3,nod=-6,tilt=8),'smooth'),(b2,H(stab=.8,nod=-4),'smooth'),(N,H(),'smooth')])
    for i,kk in enumerate(CRAWLK):
        land=b3+[0,1,3,4][i];burst=b1+[0,2,1,3][i];rs=reach_spec(kk)
        p.arms[kk]=[('free',[(0,dict(HOST_ARM[kk])),(b0+2+[0,2,1,3][i],dict(HOST_ARM[kk])),(burst,dict(rs,hand=('rel',-75,0,0)),'out'),(burst+4,dict(rs,hand=('rel',-28,0,0)),'out'),(b2,dict(rs,curls=-.15),'smooth')]),('plant',land,N,CRAWL['home'][kk],{'slap':True,'lift':.04})]
    for s_ in 'LR':
        kk='1'+s_;p.arms[kk]=('free',[(0,dict(HOST_ARM[kk])),(b0+3,dict(HOST_ARM[kk])),(b1+2,{'off':(.42,.12,.10),'pole':(.2,.4,-.9),'hand':('rel',-30,0,0),'curls':[-.2,-.25,-.2,-.15,-.1]},'snap'),(b2+4,{'off':(.36,.20,.02),'pole':HP_REACH,'hand':('rel',-20,0,0),'curls':-.1}),(N,crawl_guard(),'smooth')])
    return p,bk,hd
@clip
def Reveal():
    # Noticed. Head snaps a tilt; the coiled arms BURST out of the robe and spread wide (huge silhouette), a held beat, then it
    # drops onto four hands that slam down one after another, and settles coiled to run. Hood stays shut (that is the capture).
    spec=lambda kk:{'off':(.55 if kk[0]=='2' else .50,.22 if kk[0]=='2' else .10,.05 if kk[0]=='2' else -.10),'pole':(.3,.3,-.9),'hand':('rel',-30,0,0),'curls':[-.2,-.25,-.2,-.15,-.1]}
    return unfold_clip('Reveal',72,(16,30,46,56),spec,events={'notice':16/FPS,'burst':27/FPS,'drop':46/FPS,'planted':60/FPS})
@clip
def WallEnter():
    spec=lambda kk:{'off':(.30,.40,.40 if kk[0]=='2' else .10),'pole':(1,.2,-.5),'hand':('rel',-30,0,0),'curls':-.1}
    return unfold_clip('WallEnter',48,(8,18,30,38),spec)
@clip
def CeilingEnter():
    spec=lambda kk:{'off':(.20,.10,.55 if kk[0]=='2' else .35),'pole':(1,.3,-.3),'hand':('rel',-30,0,0),'curls':-.1}
    return unfold_clip('CeilingEnter',48,(8,18,30,38),spec)
def fold_clip(name,N,lift0,events=None,extra_body=None):
    """crawl -> host: hands peel off one by one (3 frames apart), the body rises, arms coil into the robe, mantle releases."""
    p=Perf(name,N+1,events=events);up=lift0+16
    bk=T([(0,cb()),(lift0,cb(dz=-.04,pitch=6),'smooth'),(lift0+6,cb(dz=-.02,pitch=4),'smooth'),(up,hb(dz=.06,pitch=-6,mantle=.6),'smooth'),(up+10,hb(dz=.03,pitch=-2,mantle=.15),'smooth'),(N,hb(),'smooth')])
    hd=T([(0,H()),(lift0,H(nod=6)),(up,H(stab=.2,nod=-8,tilt=-10),'smooth'),(up+12,H(stab=0,nod=6,tilt=6),'smooth'),(N,H(stab=0,nod=4),'smooth')])
    for i,kk in enumerate(CRAWLK):
        l=lift0+[2,4,0,1][i];p.arms[kk]=[('plant',0,l,CRAWL['home'][kk]),('free',[(l+6,{'off':(.26,.10,-.05),'pole':(.3,-.4,.8),'hand':('rel',10,0,0),'curls':.4}),(up+8,dict(HOST_ARM[kk]),'smooth'),(N,dict(HOST_ARM[kk]))])]
    for s_ in 'LR':kk='1'+s_;p.arms[kk]=('free',[(0,crawl_guard()),(up-2,crawl_guard(off=(.12,.12,-.08))),(up+10,dict(HOST_ARM[kk]),'smooth'),(N,dict(HOST_ARM[kk]))])
    return p,bk,hd
@clip
def Reset():return fold_clip('Reset',72,20,events={'fold_start':20/FPS,'host_ready':66/FPS})
@clip
def WallExit():return fold_clip('WallExit',48,8)
@clip
def CeilingExit():return fold_clip('CeilingExit',48,8)
# ---------------------------------------------------------------- jumpscares (rubric-scored, see jumpscare notes)
exec((Path(__file__).parent/'jumpscares_v08.py').read_text())
# ================================================================== driver
ORDER=['Idle','HostWalk','Walk','Sneak','Chase','RunChase','WallCrawl','CeilingCrawl','SearchUnder','SearchOver','Reveal','JumpscareDeath','Squeeze',
       'Listen','Alert','Reset','WallEnter','WallExit','CeilingEnter','CeilingExit',
       'Idle_Twitch','Idle_HeadTilt','Idle_Breathe','Search_LookAround','Search_Peek','Sniff','Turn_L90','Turn_R90','Lunge','Stalk','LostYou','Jumpscare_Ceiling','Jumpscare_Grab']
REPORT=[];names=ONLY or ORDER
for name in names:
    p,body,head=CLIPS[name]()
    for kk,v in list(p.arms.items()):
        if isinstance(v,tuple) and v[0]=='free':p.arms[kk]=[v]
    for kk,v in list(p.arms.items()):
        if isinstance(v,tuple) and v[0]=='free_fn':
            fn=v[1];p.arms[kk]=[('free',[(0,{})])];p.__dict__.setdefault('fns',{})[kk]=fn
    fns=p.__dict__.get('fns',{})
    if fns:
        orig=p.arm_at
        def arm_at(kk,f,b,orig=orig,fns=fns,p=p):
            if kk in fns:
                spec=fns[kk](kk,f,b);return p.free_eval(kk,[(0,spec)],0,b),{'stance':False}
            return orig(kk,f,b)
        p.arm_at=arm_at
    rig.animation_data.action=None;rows=[];samples=[];maxerr=0;worst=None;align=[];k.last_pole={}
    for f,(P,recs) in enumerate(p.frames_iter(body,head)):
        k.reset();targets=k.apply(P,iterations=4);rows.append(k.capture());sup=[]
        for kk,A in P['arms'].items():
            sh=k.shoulder_world(P['body'],kk);ax=(targets[kk]-sh).normalized();pw=k.pb[f'CTRL_POLE_arm_{kk[0]}.{kk[1]}_hand'].matrix.translation-sh;perp=(pw-ax*pw.dot(ax)).length/max(pw.length,1e-6)
            if perp<.35:align.append((f+1,kk,round(perp,2)))
        for kk,r in recs.items():
            if r['stance']:
                err=(targets[kk]-k.wrist(kk)).length
                if err>maxerr:maxerr=err;worst=(f+1,kk)
                sup.append({'hand':kk,'stance':True,'vertex':r['vertex'],'desired':list(targets[kk]+r['R']@Vector(r['local'])),'ik_error_m':err,'actor_yaw_deg':p.yaw(f)})
            else:sup.append({'hand':kk,'stance':False})
        samples.append({'frame':f+1,'support':sup,'hood_open':P['head']['hood'],'chomp':P['head']['chomp'],'mantle':P['body'].get('mantle',0)})
    K.write_action(rig,'AN_Keeper_'+name,rows)
    REPORT.append({'name':name,'frames':p.N,'seconds':(p.N-1)/FPS,'loop':p.loop,'reference_speed_cm_s':round(p.v*100,3),'events_seconds':p.events,'note':p.note,'max_ik_error_m':maxerr,'worst':worst,'contacts':samples})
    print('CLIP',name,p.N,'ik %.2e'%maxerr,worst,'pole-near-axis',align[:6],len(align),flush=True)
k.mesh_live(True);a=bpy.data.actions['AN_Keeper_'+names[0]];rig.animation_data.action=a;rig.animation_data.action_slot=a.slots[0]
DEST.parent.mkdir(parents=True,exist_ok=True);DEST.with_suffix('.contacts.json').write_text(json.dumps(REPORT))
bpy.ops.wm.save_as_mainfile(filepath=str(DEST));print('RESULT saved',DEST,len(REPORT),'clips')
