"""Custom six-arm support rig and stepped in-place animation source. No gameplay assets overwritten."""
import bpy, math, json
from pathlib import Path
from mathutils import Vector, Matrix
from bpy_extras.anim_utils import action_get_channelbag_for_slot
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'Tallow/SourceAssets/Keeper/SleevedV06'
bpy.ops.wm.open_mainfile(filepath=str(OUT/'keeper-direct-rigged.blend'))
obj=bpy.data.objects['SK_Keeper_Tripo_Direct'];rig=bpy.data.objects['KEEPER_PRODUCTION_RIG'];scene=bpy.context.scene
report=json.loads((OUT/'production-audit.json').read_text());arms=report['arms'];parts=report['parts']
horror=obj.data.shape_keys.key_blocks['FULL_HORROR']
# Root-space target and pole controls are outside their driven chains.
bpy.ops.object.select_all(action='DESELECT');rig.select_set(True);bpy.context.view_layer.objects.active=rig;bpy.ops.object.mode_set(mode='EDIT')
for arm in arms:
    name=arm['hand'];w=Vector(arm['wrist']);handtail=rig.data.edit_bones[name].tail.copy()
    for kind,head,tail in [('IK',w,handtail),('POLE',Vector(((-1 if arm['side']=='R' else 1)*.8,-.55,1.4)),Vector(((-1 if arm['side']=='R' else 1)*.8,-.55,1.52)))]:
        bn=rig.data.edit_bones.new(kind+'_'+name);bn.head=head;bn.tail=tail;bn.parent=rig.data.edit_bones['root'];bn.use_deform=False
bpy.ops.object.mode_set(mode='OBJECT')
for arm in arms:
    c=rig.pose.bones[arm['lower']].constraints.new('IK');c.name='Planted support / reach';c.target=rig;c.subtarget='IK_'+arm['hand'];c.pole_target=rig;c.pole_subtarget='POLE_'+arm['hand'];c.chain_count=2;c.use_stretch=False
    # Do not force hand rotation: fingers retain deliberate authorable FK pose.
    for name in [arm['upper'],arm['lower']]:rig.pose.bones[name].ik_stretch=0
    # Find rest-plane pole roll numerically so the new controls do not twist the source pose.
    best=(1e9,0)
    for step in range(72):
        c.pole_angle=step*2*math.pi/72;bpy.context.view_layer.update()
        err=(rig.pose.bones[arm['upper']].tail-Vector(arm['elbow'])).length
        if err<best[0]:best=(err,c.pole_angle)
    c.pole_angle=best[1]
for p in rig.pose.bones:p.rotation_mode='QUATERNION'
restmat={p.name:p.bone.matrix_local.copy() for p in rig.pose.bones}
CrawlPole={a['lower']:rig.pose.bones[a['lower']].constraints['Planted support / reach'].pole_angle for a in arms}
HostPole={}


def reset():
    for p in rig.pose.bones:p.matrix_basis=Matrix.Identity(4)
    for key in obj.data.shape_keys.key_blocks:key.value=0


def target(name,position):
    m=restmat[name].copy();m.translation=Vector(position);rig.pose.bones[name].matrix=m


def body_offset(z,tilt=0):
    m=restmat['body'].copy();position=m.translation+Vector((0,0,z));m=Matrix.Rotation(math.radians(tilt),4,'X')@m;m.translation=position;rig.pose.bones['body'].matrix=m
    bpy.context.view_layer.update()


def pose(amount=0,phase=0,gait=False,head_angle=0,capture=0,squeeze=0):
    reset();body_offset(-1.10*amount,25*amount)
    horror.value=amount
    head=rig.pose.bones['head'];head.rotation_mode='XYZ';head.rotation_euler=(math.radians(-8*amount+8*capture),math.radians(head_angle),math.radians(5*amount));head.rotation_mode='QUATERNION'
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
                q=(phase+(0 if (pair==2)==(sign==1) else .5))%1
                if q<.6:crawl.y+=-.08+.16*q/.6
                else:
                    r=(q-.6)/.4;crawl.y+=.08-.16*r;crawl.z+=.14*math.sin(math.pi*r)
        point=host.lerp(crawl,amount)
        if squeeze:point.x*=1-.55*squeeze
        target('IK_'+hand,point)
        target('POLE_'+hand,Vector(pole_host).lerp(Vector(pole_crawl),amount))
    for a in arms:
        if a['lower'] in HostPole:
            ha=HostPole[a['lower']];ca=CrawlPole[a['lower']];diff=(ca-ha+math.pi)%(2*math.pi)-math.pi
            rig.pose.bones[a['lower']].constraints['Planted support / reach'].pole_angle=ha+diff*amount
    bpy.context.view_layer.update()
    # Fixed world-facing palms after the IK solve, including folded hands hidden under shroud.
    def orient_hands():
        for a in arms:
            p=rig.pose.bones[a['hand']]
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
                q=(phase+(0 if (a['pair']==2)==(sign==1) else .5))%1
                lift=.14*math.sin(math.pi*(q-.6)/.4) if gait and q>=.6 else 0
                prefix=f"arm_{a['pair']}.{a['side']}"
                ids=[i for part in parts if part['name'].startswith(prefix+'_digit') for i in part['indices']]
                minimum=min(me.vertices[i].co.z for i in ids)
                offsets.append(('IK_'+a['hand'],lift-minimum))
            ev.to_mesh_clear()
            for name,dz in offsets:
                p=rig.pose.bones[name];m=p.matrix.copy();m.translation.z+=dz;p.matrix=m
            bpy.context.view_layer.update();orient_hands()


def new_action(idblock,name):
    idblock.animation_data_create();act=bpy.data.actions.new(name);act.use_fake_user=True;slot=act.slots.new(id_type=idblock.id_type,name=idblock.name);idblock.animation_data.action=act;idblock.animation_data.action_slot=slot
    return act,slot


def key(frame):
    for p in rig.pose.bones:
        if p.name.startswith(('IK_','POLE_')) or p.name in ['body','head'] or p.name.endswith('_hand') or '_digit' in p.name:
            p.keyframe_insert(data_path='location',frame=frame);p.keyframe_insert(data_path='rotation_quaternion',frame=frame);p.keyframe_insert(data_path='scale',frame=frame)
    for a in arms:rig.pose.bones[a['lower']].constraints['Planted support / reach'].keyframe_insert(data_path='pole_angle',frame=frame)
    for name in ['FULL_HORROR','CHOMP','Blink_L','Blink_R']:obj.data.shape_keys.key_blocks[name].keyframe_insert(data_path='value',frame=frame)

# Find fold planes that stack concealed elbows upward inside the narrow mantle.
pose(amount=0)
for a in arms:
    if a['pair']==1:continue
    c=rig.pose.bones[a['lower']].constraints['Planted support / reach'];best=(1e9,0)
    sh=rig.pose.bones[a['upper']].head.copy();desired=sh+Vector((0,.10,.30))
    for step in range(72):
        c.pole_angle=step*2*math.pi/72;bpy.context.view_layer.update()
        elbow=rig.pose.bones[a['upper']].tail
        err=(elbow-desired).length
        if err<best[0]:best=(err,c.pole_angle)
    HostPole[a['lower']]=best[1]

clips=[('Idle',97),('HostWalk',49),('Walk',49),('Sneak',97),('Chase',25),('RunChase',17),('WallCrawl',49),('CeilingCrawl',49),('SearchUnder',73),('SearchOver',73),('Reveal',73),('JumpscareDeath',73),('Squeeze',49)]
actions={}; audits=[]
scene.render.fps=24
for name,frames in clips:
    rig.animation_data_clear();obj.data.shape_keys.animation_data_clear();act,slot=new_action(rig,'AN_Keeper_'+name);ma,ms=new_action(obj.data.shape_keys,'MORPH_Keeper_'+name)
    errors=[];lowest=[]
    for frame in range(1,frames+1,2):
        t=(frame-1)/(frames-1)
        if name in ['Idle','HostWalk']:
            pose(head_angle=2*math.sin(t*2*math.pi));body_offset(.012*math.sin(t*2*math.pi));
        elif name=='Reveal':
            # 0-.25: stillness/read. .25-.45: neck tilt. .45-.78: violent unfold. .78-1: plant/settle.
            amount=0 if t<.45 else min(1,(t-.45)/.33)
            pose(amount=amount,head_angle=(12*min(1,t/.4))*(1-amount))
        elif name=='JumpscareDeath':
            pulse=0 if t<.18 else (min(1,(t-.18)/.12) if t<.62 else max(0,1-(t-.62)/.2))
            pose(amount=1,capture=pulse,head_angle=15*pulse);obj.data.shape_keys.key_blocks['CHOMP'].value=pulse
        elif name=='Squeeze':pose(amount=1,squeeze=math.sin(t*math.pi)**2)
        elif name in ['SearchUnder','SearchOver']:
            pose(amount=1 if name=='SearchUnder' else 0,head_angle=18*math.sin(t*2*math.pi))
        else:pose(amount=1,phase=t,gait=True,head_angle=3*math.sin(t*2*math.pi))
        for a in arms:
            errors.append((rig.pose.bones[a['lower']].tail-rig.pose.bones['IK_'+a['hand']].head).length)
        if name=='JumpscareDeath':
            for p in rig.pose.bones:
                if p.name.startswith('arm_1.') and '_digit' in p.name and p.name.endswith(('_b','_c')):
                    from mathutils import Quaternion
                    p.rotation_quaternion=Quaternion((1,0,0),math.radians(12*pulse))
        key(frame)
    for ac,sl in [(act,slot),(ma,ms)]:
        cb=action_get_channelbag_for_slot(ac,sl)
        for fc in cb.fcurves:
            for k in fc.keyframe_points:k.interpolation='CONSTANT'
    actions[name]=(act,slot,ma,ms)
    audits.append({'name':name,'frames':frames,'seconds':(frames-1)/24,'pose_rate':12,'max_wrist_target_error_m':max(errors),'root_motion':'none','note':'Wall/Ceiling use same contact gait as crawl; actual traversal surface alignment belongs to gameplay and requires route review.' if name in ['WallCrawl','CeilingCrawl'] else ''})
# Render key stages of actual evaluated source animations.
scene.render.engine='BLENDER_WORKBENCH';scene.display.shading.color_type='MATERIAL';scene.display.shading.show_specular_highlight=False
scene.render.resolution_x=900;scene.render.resolution_y=1000;scene.render.resolution_percentage=100
camd=bpy.data.cameras.new('DirectPoseCamera');cam=bpy.data.objects.new('DirectPoseCamera',camd);scene.collection.objects.link(cam);scene.camera=cam;camd.type='ORTHO';cam.data.ortho_scale=2.85;cam.location=(2.5,-5,1.45);cam.rotation_euler=(Vector((0,0,1.15))-cam.location).to_track_quat('-Z','Y').to_euler()
for name,frame in [('Idle',1),('Reveal',37),('Reveal',73),('Chase',7),('JumpscareDeath',19)]:
    ac,sl,ma,ms=actions[name];rig.animation_data.action=ac;rig.animation_data.action_slot=sl;obj.data.shape_keys.animation_data.action=ma;obj.data.shape_keys.animation_data.action_slot=ms
    scene.frame_set(frame);scene.render.filepath=str(OUT/f'pose-{name}-{frame:03d}.png');bpy.ops.render.render(write_still=True)
# Leave host Idle as opening pose, animation library attached for next production gate.
ac,sl,ma,ms=actions['Idle'];rig.animation_data.action=ac;rig.animation_data.action_slot=sl;obj.data.shape_keys.animation_data.action=ma;obj.data.shape_keys.animation_data.action_slot=ms;scene.frame_set(1);scene.frame_start=1;scene.frame_end=97
(OUT/'animation-audit.json').write_text(json.dumps({'stage':'staged_motion_source_pending_engine_review','clips':audits,'gameplay_mutations':False,'root_translation_keys':False,'morph_semantics':{'FULL_HORROR':'gathers lower mantle only; head and hood unchanged','CHOMP':'identity placeholder; centre opening hood/mask and mouth deferred issue44','Blink_L':'identity placeholder; head unchanged','Blink_R':'identity placeholder; head unchanged'}},indent=2))
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'keeper-direct-animated.blend'))
print('KEEPER_ANIMATION '+json.dumps(audits))
