# Jumpscare variants (exec'd inside animate_v08.py). Authored for the CAPTURE camera: player eye 1.1 m high, Keeper root
# CAM_D (1.25 m) in front, the game snaps the view onto the `head` bone and tracks it (RInterp 16). Head targets below are
# placed in that camera space. The camera aims at the head bone's BASE, so the maw (12.5 cm up the head) is pulled onto the
# line of sight with an extra chin-down nod (MAWNOD) at the strike. Rubric: anticipation hold, 3-5 frame strike with no
# ease-in + overshoot, maw fills the view, outward-framing silhouette, HOOD_OPEN/CHOMP on the strike frames, direct address,
# living hold, short tail (the game's 0.8 s fade starts at clip_end-0.8 s, so each strike lands just before it).
EYE=CAM;MAWNOD=32
def body_for_head(Ho,pitch=0,roll=0,yaw=0,mantle=1.,ox=0):
    R=Matrix.Rotation(math.radians(yaw),3,'Z')@Matrix.Rotation(math.radians(pitch),3,'X')@Matrix.Rotation(math.radians(roll),3,'Y')
    pv=Vector(Ho)-R@HEAD_REL;return {'loc':(pv.x+ox,pv.y,pv.z-1.4),'pitch':pitch,'roll':roll,'yaw':yaw,'mantle':mantle}
def at_cam(dist,up=0,side=0):
    """Point `dist` m in front of the eye along the Keeper's facing line (+up/+side offsets)."""
    return (side,EYE.y+dist,EYE.z+up)
def fp(x,y,z,pole,hand=('rel',-25,0,0),curls=-.15):
    return {'pos':(x,y,z),'pole':pole,'hand':hand,'curls':curls}
def tremor_extra(start,amp_head=1.2,amp_curl=.10,chomp_pulse=None,seed=0,hold=None):
    def extra(f,P):
        if hold and hold[0]<=f<hold[1]:      # anticipation: near-still; single fingers tap and test, out of sync per arm
            for kk,A in P['arms'].items():
                if isinstance(A.get('curls'),list):
                    o=(ord(kk[0])*3+ord(kk[1]))%5;A['curls']=[c+.04*noise(f*3.1,j+ord(kk[0]))+.35*pulse(f,hold[0]+2+((j+o)%5)*2,1,1.5) for j,c in enumerate(A['curls'])]
        if f<start:return
        w=smooth(clamp((f-start)/3.))
        h=P['head'];h['nod']+=amp_head*w*noise(f*2.1,seed+1);h['turn']+=amp_head*w*noise(f*2.3,seed+2);h['tilt']+=amp_head*1.4*w*noise(f*1.9,seed+3)
        if chomp_pulse:h['chomp']=clamp(h['chomp']-chomp_pulse*w*(.5+.5*math.sin(f*1.7)))
        for kk,A in P['arms'].items():
            if not A.get('exact') and isinstance(A.get('curls'),list):A['curls']=[c+amp_curl*w*noise(f*2.7,seed+j+ord(kk[1])) for j,c in enumerate(A['curls'])]
    return extra
def reveal_curves(S0,hood_frames=3,chomp_delay=1,chomp_frames=3):
    return (lambda f:ease_out(clamp((f-S0)/hood_frames),2.5)),(lambda f:ease_out(clamp((f-S0-chomp_delay)/chomp_frames),2.5))
SPLAY=[-.25,-.25,-.2,-.15,-.1];GRIP=[.6,.5,.55,.6,.65]
@clip
def JumpscareDeath():
    # A. Face to face. A short pounce-lurch, it rears up and TOWERS over the child, all six arms snap OUT wide to frame the
    # view; 11 frames of held breath with a slow impossible head tilt and one micro-twitch; then the head DIVES into the lens
    # (4 frames, hard snap) as the mask splits and the maw opens, hands closing in at the frame edges; quivering hold.
    L0,T0,S0,S1=4,11,21,25;N=46
    tower=body_for_head(at_cam(1.10,1.15),pitch=-12,mantle=.3);tower2=body_for_head(at_cam(1.08,1.16),pitch=-13,mantle=.3)
    strike=body_for_head(at_cam(.52,.10),pitch=40);over=body_for_head(at_cam(.47,.08),pitch=42)
    bk=T([(0,cb()),(L0,cb(dz=-.05,dy=.06,pitch=-6),'out'),(T0,tower,'over'),(S0,tower2,'smooth'),(S1,strike,'snap'),(S1+2,over,'out'),(S1+5,strike,'smooth'),(N,strike)])
    hd=T([(0,H()),(L0,H(look=1,nod=0),'snap'),(T0,H(look=1,nod=6),'over'),(S0,H(look=1,nod=8,tilt=28),'smooth'),(S1,H(look=1,maw=1,tilt=0),'snap'),(S1+2,H(look=1,maw=1,nod=3,tilt=0),'out'),(S1+5,H(look=1,maw=1,tilt=0),'smooth'),(N,H(look=1,maw=1,tilt=0))])
    hood,chomp=reveal_curves(S0)
    def head(f):
        h=dict(hd(f));tw=pulse(f,16,1,2.2);h['tilt']+=7*tw;h['nod']-=3*tw;h['hood']=hood(f);h['chomp']=chomp(f);return h
    p=Perf('JumpscareDeath',N+1,events={'lurch':0.,'tower':T0/FPS,'hold':[T0/FPS,S0/FPS],'micro_twitch':16/FPS,'strike_start':S0/FPS,'strike':S1/FPS,
          'hood_open':[S0/FPS,(S0+3)/FPS],'chomp':[(S0+1)/FPS,(S0+4)/FPS],'fade_starts_at':round(N/FPS-.8,3),'capture_root_distance_m':CAM_D})
    for s_ in 'LR':
        kk='1'+s_;p.arms[kk]=[('free',[(0,crawl_guard()),(L0,crawl_guard(off=(.14,.04,-.18),curls=.6)),(T0-3,crawl_guard(off=(.30,.10,.10),pole=(.2,.5,-.8),curls=.2),'in'),
             (T0,fp(.60,-.18,2.02,(.2,.5,-.8),('rel',-72,0,0),SPLAY),'over'),(T0+4,fp(.61,-.19,2.03,(.2,.5,-.8),('rel',-30,0,0),SPLAY),'out'),(S0,fp(.61,-.20,2.04,(.2,.5,-.8),('rel',-35,0,0),[-.2,-.22,-.18,-.12,-.08])),
             (S1,fp(.40,-.84,1.12,HP_REACH,('rel',-10,0,0),[.2,.1,.15,.2,.25]),'snap'),(S1+3,fp(.38,-.86,1.12,HP_REACH,('rel',0,0,0),[.55,.45,.5,.55,.6]),'out'),(N,fp(.38,-.86,1.12,HP_REACH,('rel',0,0,0),GRIP))])]
        a='2'+s_;p.arms[a]=[('plant',0,2,CRAWL['home'][a]),('free',[(T0-3,fp(.44,-.30,1.05,(.3,.4,-.85),('rel',-20,0,0),.1)),(T0+1,fp(.55,-.25,1.62,(.3,.4,-.85),('rel',-35,0,0),SPLAY),'over'),
             (S0,fp(.56,-.26,1.64,(.3,.4,-.85),('rel',-35,0,0),[-.15,-.2,-.15,-.1,-.05])),(S1,fp(.34,-.84,.98,(.3,.4,-.85),('rel',0,0,0),.3),'snap'),(N,fp(.33,-.85,.98,(.3,.4,-.85),('rel',5,0,0),.55))])]
        b='3'+s_;p.arms[b]=[('plant',0,1,CRAWL['home'][b]),('free',[(T0-3,fp(.40,-.20,1.00,(.4,.3,-.85),('rel',-20,0,0),.1)),(T0+3,fp(.42,-.25,1.38,(.4,.3,-.85),('rel',-30,0,0),SPLAY),'over'),
             (S0,fp(.43,-.26,1.39,(.4,.3,-.85),('rel',-30,0,0),-.1)),(S1,fp(.40,-.55,.62,(.4,.3,-.85),('rel',0,0,0),.3),'snap'),(N,fp(.40,-.55,.62,(.4,.3,-.85),('rel',0,0,0),.5))])]
    p.extra=tremor_extra(S1+1,2.0,.16,chomp_pulse=.22,hold=(T0+1,S0))
    return p,bk,head
@clip
def Jumpscare_Ceiling():
    # B. From above. Hanging upside down from the ceiling (actor upright on the floor, body up at ~2.5 m), it lets go and
    # SWINGS down head first into the view like a spider dropping on its thread (overshoot, rebound), rear arms still up on the
    # ceiling, front and host arms hanging down past the frame edges. A still inverted beat in the child's face while the mask
    # slowly rotates; then split and a short vicious strike.
    D0,D1,R1,S0,S1=7,13,18,28,32;N=52
    hang=body_for_head(at_cam(1.00,.92),pitch=-20,roll=180);hang2=body_for_head(at_cam(.98,.94),pitch=-22,roll=180)   # shallow: keeps the camera off the inverted head's look-at singularity
    low=body_for_head(at_cam(.70,-.12),pitch=6,roll=180);reb=body_for_head(at_cam(.68,.05),pitch=2,roll=180);hold=body_for_head(at_cam(.68,.0),pitch=3,roll=180)
    strike=body_for_head(at_cam(.50,.0),pitch=-8,roll=180);over=body_for_head(at_cam(.46,-.01),pitch=-10,roll=180)
    bk=T([(0,hang),(D0,hang2,'smooth'),(D1,low,'in'),(D1+3,reb,'out'),(R1,hold,'smooth'),(S0,hold),(S1,strike,'snap'),(S1+2,over,'out'),(S1+5,strike,'smooth'),(N,strike)])
    hd=T([(0,H(look=.8,turn=0)),(D0,H(look=.8)),(D1,H(look=1,nod=10),'in'),(R1,H(look=1,nod=12,tilt=0),'smooth'),(S0,H(look=1,nod=14,tilt=-34),'smooth'),(S1,H(look=1,maw=1,tilt=0),'snap'),(S1+2,H(look=1,maw=1,nod=3,tilt=0),'out'),(S1+5,H(look=1,maw=1,tilt=0),'smooth'),(N,H(look=1,maw=1,tilt=0))])
    hood,chomp=reveal_curves(S0)
    def head(f):
        h=dict(hd(f));h['stab']=0;h['hood']=hood(f);h['chomp']=chomp(f);return h
    p=Perf('Jumpscare_Ceiling',N+1,events={'hang':0.,'drop':[D0/FPS,D1/FPS],'hold':[R1/FPS,S0/FPS],'strike_start':S0/FPS,'strike':S1/FPS,
          'hood_open':[S0/FPS,(S0+3)/FPS],'chomp':[(S0+1)/FPS,(S0+4)/FPS],'fade_starts_at':round(N/FPS-.8,3),'capture_root_distance_m':CAM_D,
          'staging':'actor upright on the floor; the body hangs inverted from ~2.5 m (robe gathered up to the ceiling)'})
    # inverted (roll 180): body +X maps to world -X, so a NEGATIVE lateral keeps each limb on its own shoulder's side
    for s_ in 'LR':
        kk='1'+s_;p.arms[kk]=[('free',[(0,fp(-.42,-.25,2.50,(1,0,0),('rel',30,0,0),.6)),(D0,fp(-.42,-.27,2.52,(1,0,0),('rel',30,0,0),.6)),
             (D1+2,fp(-.40,-.95,.82,(1,.1,0),('rel',-20,0,0),SPLAY),'out'),(S0,fp(-.38,-.95,.80,(1,.1,0),('rel',-20,0,0),[-.05,-.15,-.1,-.05,0])),
             (S1,fp(-.26,-1.0,.92,(1,.1,0),('rel',0,0,0),.35),'snap'),(N,fp(-.26,-1.0,.92,(1,.1,0),('rel',0,0,0),GRIP))])]
        a='2'+s_;p.arms[a]=[('free',[(0,fp(-.45,-.05,2.50,(1,0,.1),('rel',30,0,0),.6)),(D0+2,fp(-.45,-.05,2.50,(1,0,.1),('rel',30,0,0),.6)),
             (D1+3,fp(-.48,-.78,.62,(1,.1,0),('rel',-20,0,0),SPLAY),'out'),(S1,fp(-.44,-.86,.66,(1,.1,0),('rel',0,0,0),.4),'snap'),(N,fp(-.44,-.86,.66,(1,.1,0),('rel',0,0,0),.55))])]
        b='3'+s_;p.arms[b]=[('free',[(0,fp(-.42,.20,2.50,(0,1,.2),('rel',30,0,0),.6)),(D1,fp(-.44,-.20,2.40,(0,1,.2),('rel',20,0,0),.5),'smooth'),(N,fp(-.46,-.30,2.30,(0,1,.2),('rel',20,0,0),.45),'smooth')])]
    p.extra=tremor_extra(S1+1,2.0,.16,chomp_pulse=.22,seed=3,hold=(R1,S0))
    return p,bk,head
@clip
def Jumpscare_Grab():
    # C. Dread. Long fingers curl in round the left and right edges of the view and pull close; the masked head rises SLOWLY
    # from below into the middle and stops in the child's face; a long silent beat; then the only fast thing: split + strike.
    H0,H1,S0,S1=6,30,48,51;N=73
    low=body_for_head(at_cam(.90,-.58),pitch=42);mid=body_for_head(at_cam(.78,-.20),pitch=36);rise=body_for_head(at_cam(.70,.04),pitch=30)
    strike=body_for_head(at_cam(.50,.04),pitch=34);over=body_for_head(at_cam(.46,.03),pitch=35)
    bk=T([(0,low),(H0,low),(H0+12,mid,'smooth'),(H1,rise,'smooth'),(S0,body_for_head(at_cam(.69,.04),pitch=30,roll=1.5),'smooth'),(S1,strike,'snap'),(S1+2,over,'out'),(S1+5,strike,'smooth'),(N,strike)])
    hd=T([(0,H(look=1,nod=-8)),(H1,H(look=1,nod=10,tilt=12),'smooth'),(S0,H(look=1,nod=12,tilt=16),'smooth'),(S1,H(look=1,maw=1,tilt=0),'snap'),(S1+2,H(look=1,maw=1,nod=3,tilt=0),'out'),(S1+5,H(look=1,maw=1,tilt=0),'smooth'),(N,H(look=1,maw=1,tilt=0))])
    hood,chomp=reveal_curves(S0,3,0,3)
    def head(f):
        h=dict(hd(f));h['stab']=0;h['hood']=hood(f);h['chomp']=chomp(f);return h
    p=Perf('Jumpscare_Grab',N+1,events={'hands_in':[0,14/FPS],'head_rise':[H0/FPS,H1/FPS],'hold':[H1/FPS,S0/FPS],'strike_start':S0/FPS,'strike':S1/FPS,
          'hood_open':[S0/FPS,(S0+3)/FPS],'chomp':[S0/FPS,(S0+3)/FPS],'fade_starts_at':round(N/FPS-.8,3),'capture_root_distance_m':CAM_D})
    for s_ in 'LR':
        kk='1'+s_;edge=lambda x,y,z,c,fl=0:fp(x,y,z,HP_REACH,('rel',fl,0,0),c)
        p.arms[kk]=[('free',[(0,edge(.50,-.60,.80,SPLAY,-30)),(10,edge(.30,-.84,1.18,[.0,-.05,0,.05,.1],-25),'smooth'),(14,edge(.25,-.86,1.20,[.35,.5,.55,.55,.5],15),'smooth'),
             (H1,edge(.24,-.86,1.19,[.5,.65,.7,.7,.65],25),'smooth'),(S0,edge(.25,-.85,1.19,[.55,.7,.75,.75,.7],28)),(S1,edge(.36,-.84,1.16,[.75,.85,.9,.9,.85],40),'snap'),(N,edge(.36,-.84,1.16,[.75,.85,.9,.9,.85],40))])]
        a='2'+s_;p.arms[a]=[('free',[(0,fp(.50,-.45,.25,(.3,.4,-.85),('rel',-10,0,0),.2)),(16,fp(.36,-.80,.72,(.3,.4,-.85),('rel',10,0,0),.5),'smooth'),(N,fp(.34,-.84,.76,(.3,.4,-.85),('rel',20,0,0),.7),'smooth')])]
        b='3'+s_;p.arms[b]=[('free',[(0,fp(.45,-.25,.32,(.4,.3,-.85),('rel',20,0,0),.5)),(N,fp(.45,-.38,.42,(.4,.3,-.85),('rel',20,0,0),.5),'smooth')])]
    p.extra=tremor_extra(S1+1,2.0,.14,chomp_pulse=.22,seed=7,hold=(H1,S0))
    def extra2(f,P,base=p.extra):
        base(f,P)
        if H1<=f<S0:P['body']['mantle']=1-.05*math.sin((f-H1)/(S0-H1)*math.pi)   # the silent beat: a breath in the mantle
    p.extra=extra2
    return p,bk,head
