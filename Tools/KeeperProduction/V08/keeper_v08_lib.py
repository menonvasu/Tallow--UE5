"""V08 Keeper performance library: body-frame poses, poles that the elbows actually point at, palm-aware hands
measured against the forearm (no candy-wrap), analytic planted contacts. Imported by animate_v08.py / pose_lab_v08.py."""
import bpy,math,json
from pathlib import Path
from mathutils import Vector,Matrix,Quaternion,Euler
ROOT=Path(__file__).resolve().parents[3];OUT=ROOT/'Tallow/SourceAssets/Keeper/V08Animation'
CX=-.04  # mesh centre line (V07 seam/mouth centre)
ARMS=[(p,s) for p in (1,2,3) for s in 'LR'];CRAWL=[(p,s) for p in (2,3) for s in 'LR'];FINGERS=['thumb','index','middle','ring','little']
def sg(side):return 1 if side=='L' else -1
def key(p,s):return f'{p}{s}'
class Keeper:
    def __init__(self):
        self.rig=bpy.data.objects['RIG_KeeperV07'];self.obj=bpy.data.objects['SK_KeeperRiggedV07'];self.vl=bpy.context.view_layer;self.scene=bpy.context.scene
        assert self.rig.matrix_world==Matrix.Identity(4)
        assert len(self.rig.animation_data.drivers)==90,'rig drivers missing'
        pb=self.rig.pose.bones;self.pb=pb
        self.rest={p.name:p.bone.matrix_local.copy() for p in pb}
        self.palm={};self.chain={};self.shoulder_body={};self.restrel={}
        for p,s in ARMS:
            a=f'arm_{p}.{s}';c=pb['CTRL_'+a+'_hand'];self.palm[key(p,s)]=Vector(c['palm_axis']).normalized()
            up=pb['MCH_'+a+'_upper'].bone;lo=pb['MCH_'+a+'_lower'].bone;self.chain[key(p,s)]=(up.length,lo.length)
            self.shoulder_body[key(p,s)]=up.head_local-Vector((0,0,1.4))
            self.restrel[key(p,s)]=(lo.matrix_local.to_3x3().inverted()@pb['DEF_'+a+'_hand'].bone.matrix_local.to_3x3())
        self.contact_cache={};self.last_pole={};self.pole_step=45.;self.mesh_live(False)
    # ---------- evaluation ----------
    def mesh_live(self,on):
        self.obj.hide_viewport=not on;self.vl.update()
    def upd(self):
        self.rig.update_tag(refresh={'OBJECT','DATA','TIME'});self.vl.update()
    def reset(self):
        for p in self.pb:p.matrix_basis=Matrix.Identity(4)
        for p,s in ARMS:
            c=self.pb[f'CTRL_arm_{p}.{s}_hand']
            for f in FINGERS:c[f+'_curl']=0.
        self.pb['CTRL_head']['hood_open']=0.;self.pb['CTRL_head']['chomp']=0.;self.pb['CTRL_body']['mantle_gather']=0.
    # ---------- body ----------
    def body_matrix(self,b):
        """b: dict loc (x,y,z offset from rest pivot 0,0,1.4), pitch (+ leans forward toward -Y), roll (+ top toward +X), yaw (+ turns face toward +X)."""
        R=Matrix.Rotation(math.radians(b.get('yaw',0)),3,'Z')@Matrix.Rotation(math.radians(b.get('pitch',0)),3,'X')@Matrix.Rotation(math.radians(b.get('roll',0)),3,'Y')
        return R,Vector((0,0,1.4))+Vector(b.get('loc',(0,0,0)))
    def to_world(self,b,local):
        R,t=self.body_matrix(b);return t+R@Vector(local)
    def set_body(self,b):
        R,t=self.body_matrix(b);m=(R@self.rest['CTRL_body'].to_3x3()).to_4x4();m.translation=t;self.pb['CTRL_body'].matrix=m
        self.pb['CTRL_body']['mantle_gather']=float(b.get('mantle',0))
    def set_head(self,h):
        """nod + = chin down, turn + = looks toward +X (Keeper's left), tilt + = crown toward +X. Local to body."""
        q=Quaternion((0,1,0),math.radians(h.get('turn',0)))@Quaternion((1,0,0),math.radians(h.get('nod',0)))@Quaternion((0,0,1),math.radians(-h.get('tilt',0)))
        hb=self.pb['CTRL_head'];hb.rotation_mode='QUATERNION';hb.rotation_quaternion=q;hb.location=Vector(h.get('loc',(0,0,0)))
        hb['hood_open']=float(h.get('hood',0));hb['chomp']=float(h.get('chomp',0))
    def set_cloak(self,angles):
        for i,(swing,side) in enumerate(angles):
            c=self.pb[f'CTRL_cloak_{i}'];c.rotation_mode='QUATERNION';c.rotation_quaternion=Quaternion((1,0,0),math.radians(swing))@Quaternion((0,0,1),math.radians(side))
    # ---------- arms ----------
    def shoulder_world(self,b,k):
        return self.to_world(b,self.shoulder_body[k])
    def set_arm_target(self,k,target,pole_world):
        p,s=k[0],k[1];a=f'arm_{p}.{s}';c=self.pb['CTRL_'+a+'_hand'];m=c.matrix.copy();m.translation=Vector(target);c.matrix=m
        pm=self.pb['CTRL_POLE_'+a+'_hand'];q=pm.matrix.copy();q.translation=Vector(pole_world);pm.matrix=q
    def pole_from_dir(self,b,k,target,d_body):
        """Elbow direction in body terms: (out, back, up). out=+lateral for that side. Always placed perpendicular to the arm."""
        s=k[1];R,_=self.body_matrix(b);d=R@Vector((sg(s)*d_body[0],d_body[1],d_body[2]))
        sh=self.shoulder_world(b,k);ax=(Vector(target)-sh).normalized();dp=d-ax*d.dot(ax)
        if dp.length<.25*max(d.length,1e-6):
            alt=ax.cross(R@Vector((0,0,1)));alt=alt if alt.length>.2 else ax.cross(R@Vector((0,-1,0)));dp=dp+alt.normalized()*.3
        return (sh+Vector(target))*.5+dp.normalized()*.7
    def upper_twist(self,k):
        """Roll of the IK upper-arm bone about its own axis relative to its rest relation to the torso (deg)."""
        a=f'arm_{k[0]}.{k[1]}';p=self.pb['MCH_'+a+'_upper'];par=p.parent
        R=par.matrix.to_3x3().inverted()@p.matrix.to_3x3();R0=par.bone.matrix_local.to_3x3().inverted()@p.bone.matrix_local.to_3x3()
        q=(R0.inverted()@R).to_quaternion();t=math.degrees(2*math.atan2(q.y,q.w));return (t+180)%360-180
    def solve_poles(self,P,targets,budget=32.):
        """Authored poles are PREFERENCES. Sample the elbow plane all the way round each arm (15 deg steps, all arms at
        once), keep the planes whose upper-arm roll vs rest stays within `budget`, and take the one closest to the authored
        direction and to last frame's plane (no flips). Refined by bisection. ('force',dir) bypasses this."""
        auto=[k for k,A in P['arms'].items() if not (isinstance(A.get('pole'),tuple) and A['pole'] and A['pole'][0]=='force')]
        if not auto:return
        Rb,_=self.body_matrix(P['body']);base={}
        for k in auto:
            sh=self.shoulder_world(P['body'],k);ax=(targets[k]-sh).normalized();v0=self.pb[f'CTRL_POLE_arm_{k[0]}.{k[1]}_hand'].matrix.translation-sh;base[k]=(sh,ax,v0)
        def setphi(phis):
            for k,ph in phis.items():
                sh,ax,v0=base[k];pm=self.pb[f'CTRL_POLE_arm_{k[0]}.{k[1]}_hand'];m=pm.matrix.copy();m.translation=sh+Matrix.Rotation(math.radians(ph),3,ax)@v0;pm.matrix=m
            self.upd()
        def prev_phi(k):
            pv=self.last_pole.get(k)
            if pv is None:return None
            sh,ax,v0=base[k];w=Rb@pv;w=w-ax*w.dot(ax);v=v0-ax*v0.dot(ax)
            if w.length<1e-6 or v.length<1e-6:return None
            a=math.degrees(v.angle(w));return a if v.cross(w).dot(ax)>0 else -a
        samples={k:[] for k in auto}
        for ph in range(-180,180,15):
            setphi({k:ph for k in auto})
            for k in auto:samples[k].append((ph,self.upper_twist(k)))
        def cost(k,ph,tw,pp):
            over=max(0,abs(tw)-budget);d=abs(ph);dp=0 if pp is None else abs((ph-pp+180)%360-180)
            return over*20+d+.8*dp
        best={}
        for k in auto:
            pp=prev_phi(k);best[k]=min(samples[k],key=lambda s:cost(k,s[0],s[1],pp))[0]
        step=7.5
        for it in range(3):
            for sgn in (-1,1):
                trial={k:best[k]+sgn*step for k in auto};setphi(trial)
                for k in auto:
                    pp=prev_phi(k);tw=self.upper_twist(k)
                    setattr(self,'_tmp',None)
                    samples[k].append((trial[k],tw))
            for k in auto:
                pp=prev_phi(k);best[k]=min(samples[k],key=lambda s:cost(k,s[0],s[1],pp))[0]
            step/=2
        setphi(best)
        for k in auto:
            sh,ax,_=base[k];cur=self.pb[f'CTRL_POLE_arm_{k[0]}.{k[1]}_hand'].matrix.translation-sh;cur=cur-ax*cur.dot(ax);self.last_pole[k]=Rb.transposed()@cur.normalized()
    def forearm(self,k):
        a=f'arm_{k[0]}.{k[1]}';return self.pb['MCH_'+a+'_lower'].matrix.to_3x3()
    def hand_frame_local(self,k):
        Y=Vector((0,1,0));P=self.palm[k];F=Y.cross(P).normalized();return Matrix((Y,P,F)).transposed()  # columns Y,P,F
    def hand_rel(self,k,flex=0,dev=0,twist=0):
        """Forearm-relative hand: neutral = rest wrist relation; flex + toward palm; dev about palm axis; twist about finger axis."""
        Y=Vector((0,1,0));P=self.palm[k];F=Y.cross(P).normalized()
        L=Matrix.Rotation(math.radians(twist),3,Y)@Matrix.Rotation(math.radians(flex),3,F)@Matrix.Rotation(math.radians(dev),3,P)
        return self.forearm(k)@self.restrel[k]@L
    def hand_world(self,k,fingers_dir,palm_dir):
        Yw=Vector(fingers_dir).normalized();Pw=Vector(palm_dir);Pw=(Pw-Yw*Pw.dot(Yw)).normalized();Fw=Yw.cross(Pw)
        W=Matrix((Yw,Pw,Fw)).transposed();return W@self.hand_frame_local(k).transposed()
    def plant_rot(self,k,yaw,pitch=0):
        """Palm toward the floor; fingers along yaw (deg from forward -Y toward the outside of that side), tipped down by pitch."""
        s=k[1];a=math.radians(yaw);d=Vector((sg(s)*math.sin(a),-math.cos(a),0));lat=d.cross(Vector((0,0,1)))
        R=Matrix.Rotation(math.radians(-pitch),3,lat);return self.hand_world(k,R@d,R@Vector((0,0,-1)))
    def hand_spec(self,k,h):
        if h[0]=='mat':return h[1]
        if h[0]=='plant':return self.plant_rot(k,*h[1:])
        if h[0]=='world':return self.hand_world(k,h[1],h[2])
        return self.hand_rel(k,*h[1:])
    def set_hand_rot(self,k,R3):
        a=f'arm_{k[0]}.{k[1]}';c=self.pb['CTRL_'+a+'_hand'];m=R3.to_4x4();m.translation=c.matrix.translation.copy();c.matrix=m
    def set_curls(self,k,curls):
        a=f'arm_{k[0]}.{k[1]}';c=self.pb['CTRL_'+a+'_hand']
        if not isinstance(curls,(list,tuple)):curls=[curls]*5
        for f,v in zip(FINGERS,curls):c[f+'_curl']=float(max(-.25,min(1,v)))
    def wrist(self,k):
        return self.pb[f'DEF_arm_{k[0]}.{k[1]}_hand'].head.copy()
    def twist_report(self,k):
        """Hand twist about the forearm axis and swing, degrees, relative to the rest wrist relation."""
        a=f'arm_{k[0]}.{k[1]}';rel=self.restrel[k].inverted()@(self.forearm(k).inverted()@self.pb['DEF_'+a+'_hand'].matrix.to_3x3())
        q=rel.to_quaternion();tw=math.degrees(2*math.atan2(q.y,q.w));tw=(tw+180)%360-180
        sw=math.degrees(math.acos(max(-1,min(1,(rel@Vector((0,1,0))).y))));return tw,sw
    def elbow_out(self,k):
        a=f'arm_{k[0]}.{k[1]}';up=self.pb['MCH_'+a+'_upper'];lo=self.pb['MCH_'+a+'_lower'];ax=(lo.tail-up.head).normalized();e=(up.tail-up.head);e=e-ax*e.dot(ax)
        R=self.pb['CTRL_body'].matrix.to_3x3()@self.rest['CTRL_body'].to_3x3().inverted();el=R.inverted()@e
        return (sg(k[1])*el.x/e.length if e.length>.01 else 0.),e.length
    # ---------- contacts ----------
    def hand_vertices(self,k):
        a=f'arm_{k[0]}.{k[1]}';names=['DEF_'+a+'_hand']+[n for n in self.obj.vertex_groups.keys() if n.startswith('DEF_'+a+'_digit')]
        ids={self.obj.vertex_groups[n].index for n in names}
        return [v.index for v in self.obj.data.vertices if v.groups and max(v.groups,key=lambda g:g.weight).group in ids]
    def hand_points(self,k,curls):
        """Hand+digit vertices in the hand bone frame (relative to the wrist) for these curls. Rigid while curls are held."""
        import numpy as np
        ck=(k,tuple(curls) if isinstance(curls,(list,tuple)) else curls)
        if ck in self.contact_cache:return self.contact_cache[ck]
        saved={p.name:p.matrix_basis.copy() for p in self.pb};props={(p.name,n):p[n] for p in self.pb for n in p.keys() if isinstance(p[n],float)}
        self.reset();self.set_curls(k,curls);self.mesh_live(True);self.upd()
        ids=self.hand_vertices(k);ev=self.obj.evaluated_get(bpy.context.evaluated_depsgraph_get());me=ev.to_mesh()
        hb=self.pb[f'DEF_arm_{k[0]}.{k[1]}_hand'];Ri=hb.matrix.to_3x3().inverted();w=hb.head.copy()
        pts=np.array([list(Ri@(me.vertices[i].co-w)) for i in ids]);ev.to_mesh_clear();self.mesh_live(False)
        for n,m in saved.items():self.pb[n].matrix_basis=m
        for (n,pn),v in props.items():self.pb[n][pn]=v
        self.upd();self.contact_cache[ck]=(pts,ids);return pts,ids
    def contact(self,k,curls,R3,floor=.0015):
        """Wrist height that puts the lowest hand vertex `floor` above z=0 for world hand rotation R3; returns (wrist_z, vertex_id, local_point)."""
        import numpy as np
        pts,ids=self.hand_points(k,curls);M=np.array([list(r) for r in R3]);z=pts@M[2];i=int(z.argmin());return floor-float(z[i]),ids[i],pts[i]
    # ---------- full pose ----------
    def apply(self,P,iterations=3):
        """P: body{}, head{}, cloak[], arms{k:{target(world) | target_body, pole(out,back,up), hand:('plant',yaw,pitch)|('rel',flex,dev,twist)|('world',fingers,palm), blend, curls}}"""
        b=P['body'];self.set_body(b);self.set_head(P.get('head',{}));self.set_cloak(P.get('cloak',[(0,0)]*3))
        targets={}
        for k,A in P['arms'].items():
            t=Vector(A['target']) if 'target' in A else self.to_world(b,A['target_body'])
            pd=A.get('pole',(1,.4,.2));pd=pd[1] if (isinstance(pd,tuple) and pd and pd[0]=='force') else pd
            targets[k]=t;self.set_arm_target(k,t,self.pole_from_dir(b,k,t,pd));self.set_curls(k,A.get('curls',.15))
        self.upd();self.solve_poles(P,targets)
        def orient():
            for k,A in P['arms'].items():
                R=self.hand_spec(k,A.get('hand',('rel',0,0,0)))
                if 'hand2' in A and A.get('blend',0)>0:R=R.to_quaternion().slerp(self.hand_spec(k,A['hand2']).to_quaternion(),A['blend']).to_matrix()
                self.set_hand_rot(k,R)
            self.upd()
        orient()
        # IK residual correction so a planted wrist lands exactly where asked.
        for it in range(iterations):
            worst=0
            for k,A in P['arms'].items():
                if not A.get('exact'):continue
                e=targets[k]-self.wrist(k);worst=max(worst,e.length)
                c=self.pb[f'CTRL_arm_{k[0]}.{k[1]}_hand'];m=c.matrix.copy();m.translation+=e;c.matrix=m
            if worst<2e-6:break
            self.upd();orient()
        return targets
    def capture(self):
        """Snapshot of every animator control channel (no action is assigned while authoring, so nothing overwrites the pose)."""
        row={}
        for p in self.pb:
            if not p.name.startswith('CTRL_'):continue
            b=f'pose.bones["{p.name}"]'
            for i,v in enumerate(p.location):row[(b+'.location',i)]=v
            for i,v in enumerate(p.rotation_quaternion):row[(b+'.rotation_quaternion',i)]=v
            for i,v in enumerate(p.scale):row[(b+'.scale',i)]=v
            for n in p.keys():
                if isinstance(p[n],float):row[(b+f'["{n}"]',0)]=p[n]
        return row
def write_action(rig,name,rows,interp='LINEAR'):
    """rows: list of capture() dicts for frames 1..N. Quaternion hemispheres kept continuous."""
    from bpy_extras.anim_utils import action_ensure_channelbag_for_slot
    act=bpy.data.actions.get(name)
    if act:bpy.data.actions.remove(act)
    act=bpy.data.actions.new(name);act.use_fake_user=True;slot=act.slots.new(id_type='OBJECT',name=rig.name);cb=action_ensure_channelbag_for_slot(act,slot)
    quats=sorted({p for (p,i) in rows[0] if p.endswith('rotation_quaternion')})
    for qp in quats:
        prev=None
        for r in rows:
            q=[r[(qp,i)] for i in range(4)]
            if prev and sum(a*b for a,b in zip(q,prev))<0:
                for i in range(4):r[(qp,i)]=-r[(qp,i)]
                q=[-x for x in q]
            prev=q
    for (path,idx) in rows[0]:
        grp=path.split('"')[1] if '"' in path else None
        fc=cb.fcurves.new(path,index=idx,group_name=grp);fc.keyframe_points.add(len(rows))
        co=[];[co.extend((f+1,r[(path,idx)])) for f,r in enumerate(rows)];fc.keyframe_points.foreach_set('co',co)
        for kp in fc.keyframe_points:kp.interpolation=interp
        fc.update()
    return act,slot
# ---------- timing helpers ----------
def clamp(x,a=0.,b=1.):return max(a,min(b,x))
def smooth(x):x=clamp(x);return x*x*(3-2*x)
def ease_in(x,p=2.):return clamp(x)**p
def ease_out(x,p=2.):return 1-(1-clamp(x))**p
def snap(x,frames_total,frames):
    """Hold then move in `frames` frames at the end of the span (horror snap)."""
    return smooth((x-1+frames/frames_total)/(frames/frames_total))
def lerp(a,b,t):
    if isinstance(a,(int,float)):return a+(b-a)*t
    if isinstance(a,(list,tuple)):return type(a)(lerp(x,y,t) for x,y in zip(a,b))
    if isinstance(a,dict):return {k:(lerp(a[k],b[k],t) if k in a and k in b else a.get(k,b.get(k))) for k in set(a)|set(b)}
    return a if t<.5 else b
def overshoot(x,amount=.12):
    """0->1 with a single overshoot and settle (back-out)."""
    x=clamp(x);s=1.70158*amount/.1;return 1+(s+1)*(x-1)**3+s*(x-1)**2
def track(keys,t,period=None):
    """keys: list of (time, value, ease) ; ease names the arrival curve of that segment. Optional periodic wrap."""
    if period:t=t%period
    if t<=keys[0][0]:
        if not period:return keys[0][1]
        a=keys[-1];b=keys[0];span=(b[0]+period)-a[0];u=(t+period-a[0])/span
        return lerp(a[1],b[1],EASE[b[2] if len(b)>2 else 'smooth'](u))
    for a,b in zip(keys,keys[1:]):
        if a[0]<=t<=b[0]:
            u=(t-a[0])/max(1e-9,b[0]-a[0]);return lerp(a[1],b[1],EASE[b[2] if len(b)>2 else 'smooth'](u))
    if not period:return keys[-1][1]
    a=keys[-1];b=keys[0];span=(b[0]+period)-a[0];u=(t-a[0])/span;return lerp(a[1],b[1],EASE[b[2] if len(b)>2 else 'smooth'](u))
EASE={'smooth':smooth,'linear':clamp,'in':lambda x:ease_in(x,2.2),'out':lambda x:ease_out(x,2.2),'in3':lambda x:ease_in(x,3),'out3':lambda x:ease_out(x,3),
      'snap':lambda x:ease_out(x,4),'hit':lambda x:ease_in(x,2.6),'over':lambda x:overshoot(x,.12),'over2':lambda x:overshoot(x,.25),'hold':lambda x:0. if x<1 else 1.,'step':lambda x:1. if x>0 else 0.}
