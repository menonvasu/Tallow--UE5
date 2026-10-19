"""MP4 review videos (EEVEE, 1280x720, 30 fps): player's view (eye 1.1 m, 3 m, Keeper facing camera), side view for crawl
and jumpscare clips, and the emulated capture camera for jumpscares. The actor travels at the clip's reference speed with
the camera following, over a floor grid whose spacing divides one cycle's travel, so one rendered cycle repeats seamlessly
and planted hands must visibly stick. Warm candle-ish key from the camera side, dim cool fill. Frames -> ffmpeg (libx264).
blender -b --factory-startup --threads 4 <animated.blend> --python render_video_v08.py -- <video_dir> <Clip,Clip|all> <front,side,capture>"""
import bpy,sys,json,math,subprocess,shutil
from pathlib import Path
from mathutils import Vector,Euler
argv=sys.argv[sys.argv.index('--')+1:];VD=Path(argv[0]);ONLY=None if argv[1]=='all' else argv[1].split(',');VIEWS=argv[2].split(',')
VD.mkdir(parents=True,exist_ok=True);TMP=VD/'_frames'
s=bpy.context.scene;rig=bpy.data.objects['RIG_KeeperV07'];mesh=bpy.data.objects['SK_KeeperRiggedV07'];mesh.hide_viewport=False
for o in bpy.data.objects:
    if o.type!='MESH' or o.name.startswith('WGT'):o.hide_render=True
contract={c['name']:c for c in json.loads(Path(bpy.data.filepath).with_name('animation-contract.json').read_text())['clips']}
s.render.engine='BLENDER_EEVEE';s.eevee.taa_render_samples=6;s.render.resolution_x=1280;s.render.resolution_y=720;s.render.resolution_percentage=100;s.render.fps=30
s.view_settings.view_transform='AgX';s.view_settings.look='AgX - Medium High Contrast';s.view_settings.exposure=.35
s.world.use_nodes=True;bg=s.world.node_tree.nodes['Background'];bg.inputs[0].default_value=(.010,.011,.014,1)
s.render.use_stamp=True;s.render.use_stamp_note=True;s.render.stamp_font_size=22;s.render.stamp_background=(0,0,0,.6)
for a in ['use_stamp_date','use_stamp_time','use_stamp_render_time','use_stamp_frame','use_stamp_frame_range','use_stamp_memory','use_stamp_hostname','use_stamp_camera','use_stamp_lens','use_stamp_scene','use_stamp_marker','use_stamp_filename','use_stamp_sequencer_strip']:
    if hasattr(s.render,a):setattr(s.render,a,False)
# floor with a procedural grid (spacing set per clip)
bpy.ops.mesh.primitive_plane_add(size=60);floor=bpy.context.object;fm=bpy.data.materials.new('ReviewFloor');fm.use_nodes=True;nt=fm.node_tree;bs=nt.nodes['Principled BSDF']
tc=nt.nodes.new('ShaderNodeTexCoord');sep=nt.nodes.new('ShaderNodeSeparateXYZ');nt.links.new(tc.outputs['Object'],sep.inputs[0])
def gridline(axis):
    d=nt.nodes.new('ShaderNodeMath');d.operation='DIVIDE';nt.links.new(sep.outputs[axis],d.inputs[0]);fr=nt.nodes.new('ShaderNodeMath');fr.operation='FRACT';nt.links.new(d.outputs[0],fr.inputs[0])
    lt=nt.nodes.new('ShaderNodeMath');lt.operation='LESS_THAN';lt.inputs[1].default_value=.03;nt.links.new(fr.outputs[0],lt.inputs[0]);return d,lt
dx,lx=gridline('X');dy,ly=gridline('Y');mx=nt.nodes.new('ShaderNodeMath');mx.operation='MAXIMUM';nt.links.new(lx.outputs[0],mx.inputs[0]);nt.links.new(ly.outputs[0],mx.inputs[1])
mix=nt.nodes.new('ShaderNodeMix');mix.data_type='RGBA';mix.inputs['A'].default_value=(.075,.07,.065,1);mix.inputs['B'].default_value=(.02,.02,.02,1);nt.links.new(mx.outputs[0],mix.inputs['Factor']);nt.links.new(mix.outputs['Result'],bs.inputs['Base Color']);bs.inputs['Roughness'].default_value=.85
floor.data.materials.append(fm)
def set_grid(sp):dx.inputs[1].default_value=sp;dy.inputs[1].default_value=sp
cam=bpy.data.objects.new('ReviewCam',bpy.data.cameras.new('ReviewCam'));s.collection.objects.link(cam);s.camera=cam;cam.data.clip_start=.05;cam.data.sensor_fit='HORIZONTAL'
key=bpy.data.lights.new('Key','AREA');key.energy=320;key.size=1.2;key.color=(1,.64,.36);ko=bpy.data.objects.new('Key',key);s.collection.objects.link(ko)
fill=bpy.data.lights.new('Fill','AREA');fill.energy=110;fill.size=3;fill.color=(.42,.52,.85);fo=bpy.data.objects.new('Fill',fill);s.collection.objects.link(fo)
def aim(ob,loc,tgt):ob.location=loc;ob.rotation_euler=(Vector(tgt)-Vector(loc)).to_track_quat('-Z','Y').to_euler()
HOST={'Idle','HostWalk','Reset','Reveal','Idle_Twitch','Idle_HeadTilt','Idle_Breathe','WallEnter','CeilingEnter','WallExit','CeilingExit'}
CRAWLV={'Walk','Sneak','Stalk','Chase','RunChase','WallCrawl','CeilingCrawl','Squeeze','Listen','Alert','SearchUnder','SearchOver','Search_LookAround','Search_Peek','Sniff','Turn_L90','Turn_R90','Lunge','LostYou','Reveal','Reset','WallEnter','WallExit','CeilingEnter','CeilingExit','JumpscareDeath','Jumpscare_Ceiling','Jumpscare_Grab'}
def perlin(x):
    i=math.floor(x);f=x-i;u=f*f*(3-2*f);g=lambda n:math.sin(n*127.1+311.7)*43758.5453%1*2-1;return g(i)*(1-u)+g(i+1)*u
def encode(frames_dir,out,plays,fps=30):
    cmd=['ffmpeg','-y','-loglevel','error','-framerate',str(fps),'-i',str(frames_dir/'f_%04d.png')]
    if plays>1:
        tmp=frames_dir/'once.mp4';subprocess.run(cmd+['-c:v','libx264','-pix_fmt','yuv420p','-crf','19','-preset','medium',str(tmp)],check=True)
        subprocess.run(['ffmpeg','-y','-loglevel','error','-stream_loop',str(plays-1),'-i',str(tmp),'-c','copy',str(out)],check=True)
    else:subprocess.run(cmd+['-c:v','libx264','-pix_fmt','yuv420p','-crf','19','-preset','medium',str(out)],check=True)
made=[]
for name,c in sorted(contract.items()):
    if ONLY and name not in ONLY:continue
    act=bpy.data.actions['AN_Keeper_'+name];rig.animation_data.action=act;rig.animation_data.action_slot=act.slots[0]
    T=c['seconds'];v=c['reference_speed_cm_s']/100;loop=c['loop'];n30=int(round(T*30));ev=c.get('events_seconds',{})
    yawrate=ev.get('actor_yaw_rate_deg_s',0) if isinstance(ev,dict) else 0
    plays=max(2,min(8,math.ceil(5/T))) if loop else 1
    travel=v*T;set_grid(travel/max(1,round(travel/.45)) if travel>0 else .5)
    tall=name in HOST;aimz=1.2 if tall else .7
    views=[vw for vw in VIEWS if vw=='front' or (vw=='side' and name in CRAWLV) or (vw=='capture' and name.startswith('Jumpscare'))]
    for vw in views:
        d=TMP/f'{name}_{vw}';shutil.rmtree(d,ignore_errors=True);d.mkdir(parents=True)
        nfr=n30 if loop else n30+1;rot=None
        for i in range(nfr):
            t=i/30;fr=1+t*24;fi=int(math.floor(fr));s.frame_set(fi,subframe=fr-fi)
            y0=-v*t+(v*T/2 if loop else 0);rig.location=(0,y0,0);rig.rotation_euler=(0,0,math.radians(yawrate*t));bpy.context.view_layer.update()
            if vw=='capture':
                D=1.25;eye=Vector((0,-D,1.1));head=rig.matrix_world@rig.pose.bones['head'].head;dd=(head-eye).normalized();aim_=(math.degrees(math.asin(dd.z)),math.degrees(math.atan2(dd.x,dd.y)))
                rot=list(aim_) if rot is None else [rot[0]+(aim_[0]-rot[0])*min(1,16/30),rot[1]+((aim_[1]-rot[1]+180)%360-180)*min(1,16/30)]
                tr=1.4*min(1,t/.6);fov=90+(70-90)*(1-(1-min(1,t/.35))**3);cam.data.lens=18/math.tan(math.radians(fov/2))
                cam.location=eye;cam.rotation_euler=Euler((math.radians(90+rot[0]+perlin(t*21)*tr),0,math.radians(-(rot[1]+perlin(t*17+5)*tr))),'XYZ')
                aim(ko,eye+Vector((.3,.15,-.25)),head);aim(fo,Vector((-1.5,1.5,2.6)),head)
                dh=(head-eye).length;key.energy=320*min(1,(dh/2.6)**2);fill.energy=110
            else:
                base=Vector((0,y0,0));dist=3.6 if tall else 3.0
                if name.startswith('Jumpscare'):dist=3.4
                loc=base+(Vector((0,-dist,1.1)) if vw=='front' else Vector((dist+.3,-.25,1.1)));tgt=base+Vector((0,-.25 if vw=='side' else 0,aimz+(.35 if name.startswith('Jumpscare') else 0)))
                cam.data.lens=30 if (tall or name.startswith('Jumpscare')) else 34;aim(cam,loc,tgt);key.energy=320
                aim(ko,loc+Vector((.9,.6,.4)) if vw=='front' else loc+Vector((-.2,-.9,.4)),tgt);aim(fo,base+Vector((-2.2,1.6,2.6)),tgt)
            s.render.stamp_note_text=f"AN_Keeper_{name}   {vw}   {'loop' if loop else 'one-shot'}   {v*100:.1f} cm/s   t={t:4.2f}s"
            s.render.filepath=str(d/f'f_{i:04d}.png');bpy.ops.render.render(write_still=True)
        out=VD/f'AN_Keeper_{name}_{vw}.mp4';encode(d,out,plays if vw!='capture' else 1);shutil.rmtree(d);made.append(out.name);print('VIDEO',out.name,nfr,'frames x',plays,flush=True)
rig.location=(0,0,0);rig.rotation_euler=(0,0,0);print('RESULT',len(made),'videos')
