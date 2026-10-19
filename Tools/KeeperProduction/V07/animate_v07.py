"""Fresh V07 performance library: animator control keys, contact correction and explicit beats."""
import bpy,math,json
from pathlib import Path
from mathutils import Vector,Matrix,Quaternion
from bpy_extras.anim_utils import action_get_channelbag_for_slot
ROOT=Path(__file__).resolve().parents[3];OUT=ROOT/'Tallow/SourceAssets/Keeper/RiggedV07';SRC=ROOT/'Tallow/SourceAssets/Keeper/SleevedV06'
bpy.ops.wm.open_mainfile(filepath=str(OUT/'keeper-v07-face-rig.blend'))
obj=bpy.data.objects['SK_KeeperRiggedV07'];rig=bpy.data.objects['RIG_KeeperV07'];scene=bpy.context.scene
source=json.loads((SRC/'production-audit.json').read_text());arms=source['arms'];parts=source['parts']
restmat={p.name:p.bone.matrix_local.copy() for p in rig.pose.bones}
CrawlPole={a['lower']:rig.pose.bones['CTRL_'+a['hand']]['elbow_roll'] for a in arms};HostPole={};stride=.16;duty=.6;bounce=0.;motion_style="ground"
def phase_offset(pair,side):
    if motion_style=="CeilingCrawl":return {(2,"L"):0.,(2,"R"):.25,(3,"L"):.5,(3,"R"):.75}.get((pair,side),0.)
    if motion_style=="WallCrawl":return {(2,"L"):0.,(2,"R"):.33,(3,"L"):.66,(3,"R"):.83}.get((pair,side),0.)
    return 0. if (pair==2)==(side=="L") else .5
def reset():
    for p in rig.pose.bones:p.matrix_basis=Matrix.Identity(4)
    for a in arms:
        for label in ['thumb','index','middle','ring','little']:rig.pose.bones['CTRL_'+a['hand']][label+'_curl']=0.
    rig.pose.bones['CTRL_head']['hood_open']=0.
    rig.pose.bones['CTRL_body']['mantle_gather']=0.


def target(name,position):
    m=restmat[name].copy();m.translation=Vector(position);rig.pose.bones[name].matrix=m


def body_offset(z,tilt=0):
    m=restmat['CTRL_body'].copy();position=m.translation+Vector((0,0,z));m=Matrix.Rotation(math.radians(tilt),4,'X')@m;m.translation=position;rig.pose.bones['CTRL_body'].matrix=m
    bpy.context.view_layer.update()


def pose(amount=0,phase=0,gait=False,head_angle=0,capture=0,squeeze=0):
    reset();body_offset(-1.10*amount+bounce,25*amount)
    rig.pose.bones['CTRL_body']['mantle_gather']=float(min(1.,amount*1.25))
    head=rig.pose.bones['CTRL_head'];head.rotation_mode='XYZ';head.rotation_euler=(math.radians(-8*amount+8*capture),math.radians(head_angle),math.radians(5*amount));head.rotation_mode='QUATERNION'
    for a in arms:
        sign=-1 if a['side']=='R' else 1;pair=a['pair'];hand=a['hand']
        if pair==1:
            host=Vector((sign*.24,-.22,1.59))
            crawl=Vector((sign*(.36-.08*capture),-.39-.05*capture,.87+.10*capture))
            pole_host=(sign*.65,-.35,1.4);pole_crawl=(sign*.8,-.4,1.2)
        else:
            host=Vector((sign*.13,-.08,1.60 if pair==2 else 1.32))
            crawl=Vector((sign*.23,-.25 if pair==2 else -.01,.18))
            pole_host=(sign*.06,.12,2.8 if pair==2 else 2.45);pole_crawl=(sign*.8,-.16 if pair==2 else .3,.6)
            if gait:
                # Diagonal alternating support: grounded half-cycle, deliberate lifted recovery half.
                q=(phase+phase_offset(pair,a["side"]))%1
                if q<duty:crawl.y+=-stride*.5+stride*q/duty
                else:
                    r=(q-duty)/(1-duty);crawl.y+=stride*.5-stride*r;crawl.z+=.14*math.sin(math.pi*r)
        point=host.lerp(crawl,amount)
        if squeeze:point.x*=1-.55*squeeze
        target('CTRL_'+hand,point)
        target('CTRL_POLE_'+hand,Vector(pole_host).lerp(Vector(pole_crawl),amount))
    for a in arms:
        if a['lower'] in HostPole:
            ha=HostPole[a['lower']];ca=CrawlPole[a['lower']];diff=(ca-ha+math.pi)%(2*math.pi)-math.pi
            rig.pose.bones['CTRL_'+a['hand']]['elbow_roll']=ha+diff*amount
    bpy.context.view_layer.update()
    # Fixed world-facing palms after the IK solve, including folded hands hidden under shroud.
    def orient_hands():
        for a in arms:
            p=rig.pose.bones['CTRL_'+a['hand']]
            host_dir=Vector((0,0,-1))
            crawl_dir=Vector((0,-.10,-1)) if a['pair']>1 else Vector((0,-.8,-.55))
            desired=host_dir.lerp(crawl_dir,amount).normalized()
            basis=desired.to_track_quat('Y','Z').to_matrix().to_4x4();basis.translation=p.head.copy();p.matrix=basis
        bpy.context.view_layer.update()
    orient_hands()
    # Correct supporting fingertips to the actual surface, not merely wrist targets.
    if amount>.99:
        for iteration in range(5):
            ev=obj.evaluated_get(bpy.context.evaluated_depsgraph_get());me=ev.to_mesh()
            offsets=[]
            for a in arms:
                if a['pair']==1:continue
                sign=-1 if a['side']=='R' else 1
                q=(phase+phase_offset(a['pair'],a['side']))%1
                lift=.14*math.sin(math.pi*(q-duty)/(1-duty)) if gait and q>=duty else 0
                prefix=f"arm_{a['pair']}.{a['side']}"
                ids=[i for part in parts if part['name'].startswith(prefix+'_digit') for i in part['indices']]
                minimum=min(me.vertices[i].co.z for i in ids)
                offsets.append(('CTRL_'+a['hand'],lift-minimum))
            ev.to_mesh_clear()
            for name,dz in offsets:
                p=rig.pose.bones[name];m=p.matrix.copy();m.translation.z+=dz;p.matrix=m
            bpy.context.view_layer.update();orient_hands()


# Fold-plane calibration, now driven by an animator-facing roll property.
pose(amount=0)
for a in arms:
    if a['pair']==1:continue
    control=rig.pose.bones['CTRL_'+a['hand']];best=(1e9,0);sh=rig.pose.bones['MCH_'+a['upper']].head.copy();desired=sh+Vector((0,.10,.30))
    for step in range(72):
        control['elbow_roll']=step*2*math.pi/72;rig.update_tag();bpy.context.view_layer.update()
        err=(rig.pose.bones['MCH_'+a['upper']].tail-desired).length
        if err<best[0]:best=(err,control['elbow_roll'])
    HostPole[a['lower']]=best[1]

def new_action(idblock,name):
    idblock.animation_data_create();act=bpy.data.actions.new(name);act.use_fake_user=True;slot=act.slots.new(id_type=idblock.id_type,name=idblock.name);idblock.animation_data.action=act;idblock.animation_data.action_slot=slot;return act,slot

def ramp(t,a,b):
    q=max(0,min(1,(t-a)/(b-a)));return q*q*(3-2*q)

# All existing entrypoints remain, plus explicit non-looping transition performances.
clips=[('Idle',97,False,.0),('HostWalk',49,False,.0),('Walk',25,True,.22),('Sneak',49,True,.16),('Chase',17,True,.24),('RunChase',13,True,.28),('WallCrawl',49,True,.18),('CeilingCrawl',73,True,.14),('SearchUnder',97,False,0),('SearchOver',97,False,0),('Reveal',73,False,0),('JumpscareDeath',73,False,0),('Squeeze',73,True,.10),('Listen',73,False,0),('Alert',25,False,0),('Reset',73,False,0),('WallEnter',49,False,0),('WallExit',49,False,0),('CeilingEnter',49,False,0),('CeilingExit',49,False,0)]
all_reports=[]
for name,frames,gait,clipstride in clips:
    rig.animation_data_clear();act,slot=new_action(rig,'AN_Keeper_'+name);stride=clipstride or .16;motion_style=name;duty=.25 if name=='RunChase' else .8 if name=='CeilingCrawl' else .68 if name=='WallCrawl' else .6;duration=(frames-1)/24;speed=clipstride/(duty*duration) if gait else 0.;contacts={};samples=[]
    for frame in range(1,frames+1):
        # Upper-body performance held on twos. IK contact tracks advance at24fps so world contacts can remain planted.
        t=(frame-1)/(frames-1);held=(2*((frame-1)//2))/(frames-1);wave=math.sin(held*math.tau)
        amount=1.;capture=0.;head_angle=0.;hood=0.;gait_now=gait
        half_phase=(t*2)%1;bounce=.075*math.sin(math.pi*(half_phase-.5)/.5) if name=='RunChase' and half_phase>.5 else 0.
        if name in ['Idle','HostWalk']:
            amount=0.;head_angle=1.7*wave
        elif name=='Reveal':
            amount=ramp(held,.38,.82);head_angle=15*ramp(held,.10,.28)*(1-amount)
        elif name=='Reset':
            amount=1-ramp(held,.28,.92);head_angle=-8*math.sin(held*math.pi)
        elif name=='Alert':
            amount=1;head_angle=12*(ramp(held,.08,.25)-ramp(held,.5,.85))
        elif name=='JumpscareDeath':
            capture=ramp(held,.18,.38);hood=ramp(held,.28,.46);head_angle=7*math.sin(held*math.pi)
        elif name in ['SearchUnder','SearchOver','Listen']:
            head_angle=(22 if name!='Listen' else 16)*(ramp(held,.15,.32)-2*ramp(held,.46,.65)+ramp(held,.78,.95))
        elif name in ['WallEnter','CeilingEnter']:amount=ramp(held,.08,.6)
        elif name in ['WallExit','CeilingExit']:amount=1-ramp(held,.32,.94)
        pose(amount=amount,phase=t,gait=gait_now,head_angle=head_angle,capture=capture,squeeze=.35*math.sin(held*math.pi)**2 if name=='Squeeze' else 0)
        if name=='HostWalk':body_offset(.026+.008*wave,1.3*math.sin(held*math.tau))
        elif name=='Idle':body_offset(.006+.004*wave,0)
        # Hand acting: one leads the attention; partner follows rather than mirrored gestures.
        for a in arms:
            ctrl=rig.pose.bones['CTRL_'+a['hand']];pair=a['pair'];side=1 if a['side']=='L' else -1
            if pair==1:
                m=ctrl.matrix.copy()
                if name in ['Idle','HostWalk']:
                    m.translation.y+=.015*math.sin(held*math.tau+(0 if side==1 else .7));m.translation.z+=.008*wave
                elif name in ['SearchUnder','SearchOver','Listen']:
                    reach=ramp(held,.23 if side==1 else .3,.48)-ramp(held,.7,.95)
                    m.translation.y-=.10*reach;m.translation.z+=(.14 if name=='SearchOver' else -.10 if name=='SearchUnder' else .04)*reach
                elif name=='WallCrawl':
                    m.translation.x*=.88;m.translation.y-=.055*math.sin(t*math.tau+(0 if side==1 else math.pi));m.translation.z+=.035*wave
                elif name=='CeilingCrawl':
                    m.translation.x*=1.12;m.translation.z-=.05;m.translation.y-=.025*math.sin(t*math.tau+(0 if side==1 else math.pi))
                elif name=='Alert':m.translation.y-=.06*ramp(held,.1,.3)
                elif name in ['WallEnter','WallExit']:
                    m.translation.x*=1-.12*amount;m.translation.y-=.06*math.sin(held*math.pi)*(1 if side==1 else .4)
                elif name in ['CeilingEnter','CeilingExit']:
                    m.translation.x*=1+.12*amount;m.translation.z-=.05*amount;m.translation.y-=.10*math.sin(held*math.pi)*(1 if side==-1 else .5)
                ctrl.matrix=m
                curl=.16+.18*capture+.08*(.5+.5*wave) if amount>.9 else .08
                for j,label in enumerate(['thumb','index','middle','ring','little']):ctrl[label+'_curl']=curl*(.6+.1*j)
        # Cloth controls lag the body by chain height without moving the contact root.
        for i in range(3):
            p=rig.pose.bones['CTRL_cloak_'+str(i)];p.rotation_quaternion=Quaternion((0,1,0),(.009 if amount<.5 else .006)*math.sin((held-.04*i)*math.tau))
        rig.pose.bones['CTRL_head']['hood_open']=float(hood)
        if name in ['SearchUnder','SearchOver','Listen']:
            h=rig.pose.bones['CTRL_head'];nod=(18 if name=='SearchUnder' else -16 if name=='SearchOver' else 6)*math.sin(held*math.pi);h.rotation_quaternion=Quaternion((1,0,0),math.radians(nod))@h.rotation_quaternion
        if name=='JumpscareDeath':
            head=rig.pose.bones['CTRL_head'];head.rotation_quaternion=Quaternion((1,0,0),math.radians(-8+18*capture))@head.rotation_quaternion
        rig.update_tag();bpy.context.view_layer.update()
        # During folding, move the whole controlled pose above the measured hem/contact plane.
        # Root stays fixed: this is an authored body rise, never mesh scaling or floor hiding.
        if .001 < amount < .999:
            ev=obj.evaluated_get(bpy.context.evaluated_depsgraph_get());me=ev.to_mesh();minimum=min(v.co.z for v in me.vertices);ev.to_mesh_clear()
            if minimum<.001:
                for control in rig.pose.bones:
                    if control.name.startswith('CTRL_') and control.parent and control.parent.name=='CTRL_root':
                        matrix=control.matrix.copy();matrix.translation.z+=.001-minimum;control.matrix=matrix
                bpy.context.view_layer.update()
        # Pin the same actual fingertip vertex throughout each stance, accounting for actor movement.
        handstates=[]
        if amount>.999:
            for a in arms:
                if a['pair']==1:continue
                prefix=f"arm_{a['pair']}.{a['side']}";ids=[i for part in parts if part['name'].startswith(prefix+'_digit') for i in part['indices']]
                q=(t+phase_offset(a['pair'],a['side']))%1
                stance=(q<duty) if gait_now else True
                ev=obj.evaluated_get(bpy.context.evaluated_depsgraph_get());me=ev.to_mesh();idx=min(ids,key=lambda i:me.vertices[i].co.z);point=me.vertices[idx].co.copy();ev.to_mesh_clear()
                if stance:
                    old=contacts.get(prefix)
                    if old is None or q<old['q']:
                        old={'index':idx,'anchor':point+Vector((0,-speed*(frame-1)/24,0))};old['anchor'].z=0;contacts[prefix]=old
                    old['q']=q;desired=old['anchor']+Vector((0,speed*(frame-1)/24,0))
                    for iteration in range(4):
                        ev=obj.evaluated_get(bpy.context.evaluated_depsgraph_get());me=ev.to_mesh();point=me.vertices[old['index']].co.copy();ev.to_mesh_clear();delta=desired-point
                        if delta.length<.00001:break
                        control=rig.pose.bones['CTRL_'+a['hand']];m=control.matrix.copy();m.translation+=delta;control.matrix=m;bpy.context.view_layer.update()
                    handstates.append({'hand':prefix,'stance':True,'vertex':old['index'],'desired':list(desired),'error_m':delta.length})
                else:contacts.pop(prefix,None);handstates.append({'hand':prefix,'stance':False})
        for p in rig.pose.bones:
            if not p.name.startswith('CTRL_'):continue
            p.keyframe_insert(data_path='location',frame=frame);p.keyframe_insert(data_path='rotation_quaternion',frame=frame);p.keyframe_insert(data_path='scale',frame=frame)
            for prop in p.keys():
                if isinstance(p[prop],(float,int)):p.keyframe_insert(data_path='["'+prop+'"]',frame=frame)
        samples.append({'frame':frame,'support':handstates,'root':list(rig.pose.bones['root'].matrix.translation)})
    for fc in action_get_channelbag_for_slot(act,slot).fcurves:
        for key in fc.keyframe_points:key.interpolation='LINEAR'
    # Hold upper body and finger performance while continuous IK handles surface contact.
    for fc in action_get_channelbag_for_slot(act,slot).fcurves:
        if any(x in fc.data_path for x in ['CTRL_head','CTRL_body','CTRL_cloak','curl']):
            for key in fc.keyframe_points:key.interpolation='CONSTANT'
    all_reports.append({'name':name,'frames':frames,'seconds':duration,'loop':name in ['Idle','HostWalk','Walk','Sneak','Chase','RunChase','WallCrawl','CeilingCrawl'], 'reference_speed_cm_s':speed*100 if gait else (65 if name=='HostWalk' else 0),'stride_m':clipstride,'stance_duty':duty,'contact_plane':'+Z local floor; runtime rotates surface','contacts':samples,'max_contact_error_m':max((h['error_m'] for s in samples for h in s['support'] if h['stance']),default=0)})
    print('CLIP '+name+' '+str(all_reports[-1]['max_contact_error_m']),flush=True)
# Store explicit animation-event timings; integration owns behavior (never write AI state from animation).
events={'Reveal':{'anticipation':.3,'unfold_start':1.14,'planted':2.46},'JumpscareDeath':{'reach_start':.54,'hood_split':.84,'mouth_exposed':1.38,'capture_hold':2.0,'finished':3.0},'Alert':{'orient':.08,'ready':.8},'Reset':{'fold_start':.84,'host_ready':2.76}}
(OUT/'animation-contract.json').write_text(json.dumps({'clips':[{k:v for k,v in row.items() if k!='contacts'} for row in all_reports],'events_seconds':events,'root_motion':False,'body_pose_rate':12,'contact_pose_rate':24,'source_forward':'-Y','engine_legacy_forward':'+Y','engine_mesh_yaw':-90,'mesh_scale':1,'head_bone':'head','runtime_owner':'Vasu AI unchanged; root playback adapter selects/crossfades only','quality_status':'source keyed; saved-source deformation/render verification pending'},indent=2))
(OUT/'contact-generation.json').write_text(json.dumps(all_reports,indent=2))
rig.animation_data.action=bpy.data.actions['AN_Keeper_Idle'];rig.animation_data.action_slot=rig.animation_data.action.slots[0];scene.frame_set(1);scene.frame_start=1;scene.frame_end=97
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'keeper-v07-animated.blend'));print('RESULT '+json.dumps({'clips':len(all_reports),'output':'keeper-v07-animated.blend'}))
