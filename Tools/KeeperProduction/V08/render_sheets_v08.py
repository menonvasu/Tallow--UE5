"""Workbench review sheets from the PLAYER's view (eye 1.1 m) + side view, with optional in-game travel (actor moved at the
clip's reference speed so planted hands must stick to the floor grid) and a wrist/head arc overlay.
blender -b --factory-startup --threads 4 <blend> --python render_sheets_v08.py -- <out_dir> <prefix> <Clip,Clip|all> [frames] [travel|still] [clay|texture]"""
import bpy,sys,json,math
from pathlib import Path
from mathutils import Vector
from bpy_extras.object_utils import world_to_camera_view
from PIL import Image,ImageDraw,ImageFont
argv=sys.argv[sys.argv.index('--')+1:];OUT=Path(argv[0]);PREFIX=argv[1];ONLY=None if argv[2]=='all' else argv[2].split(',');N=int(argv[3]) if len(argv)>3 else 8
TRAVEL=(argv[4]=='travel') if len(argv)>4 else True;MODE=(argv[5] if len(argv)>5 else 'texture').upper()
OUT.mkdir(parents=True,exist_ok=True);TMP=OUT/'_tiles';TMP.mkdir(exist_ok=True)
s=bpy.context.scene;rig=bpy.data.objects.get('RIG_KeeperV07') or next(o for o in bpy.data.objects if o.type=='ARMATURE')
mesh=next(o for o in bpy.data.objects if o.type=='MESH' and o.parent==rig);mesh.hide_viewport=False
for o in bpy.data.objects:
    if o.type!='MESH' or o.name.startswith('WGT'):o.hide_render=True
s.render.engine='BLENDER_WORKBENCH';sh=s.display.shading;sh.light='STUDIO';sh.color_type='TEXTURE' if MODE=='TEXTURE' else 'SINGLE';sh.single_color=(.78,.75,.70)
sh.show_cavity=True;sh.cavity_type='BOTH';sh.show_shadows=True;sh.shadow_intensity=.55;sh.show_object_outline=True;sh.object_outline_color=(0,0,0)
s.world.color=(.20,.20,.21);s.view_settings.view_transform='Standard';s.view_settings.exposure=1.1 if MODE=='TEXTURE' else 0
TW,TH=260,330;s.render.resolution_x=TW;s.render.resolution_y=TH;s.render.resolution_percentage=100
bpy.ops.mesh.primitive_plane_add(size=40);floor=bpy.context.object;m=bpy.data.materials.new('floor');m.diffuse_color=(.30,.30,.30,1);floor.data.materials.append(m)
gm=bpy.data.materials.new('grid');gm.diffuse_color=(.17,.17,.17,1)
for i in range(-8,9):
    for axis in 'xy':
        bpy.ops.mesh.primitive_plane_add(size=1);g=bpy.context.object;g.scale=(.008,10,1) if axis=='x' else (10,.008,1);g.location=(i*.5,0,.0005) if axis=='x' else (0,i*.5,.0005);g.data.materials.append(gm)
cam=bpy.data.objects.new('PlayerCam',bpy.data.cameras.new('PlayerCam'));s.collection.objects.link(cam);s.camera=cam;cam.data.clip_start=.05
def aim(loc,target,lens):
    cam.location=loc;cam.rotation_euler=(Vector(target)-Vector(loc)).to_track_quat('-Z','Y').to_euler();cam.data.lens=lens;bpy.context.view_layer.update()
SPEEDS={'Walk':.3667,'Sneak':.1333,'Chase':.60,'RunChase':2.24,'WallCrawl':.13235,'CeilingCrawl':.05833,'Squeeze':.05556,'HostWalk':.65,'Stalk':.24}
LOOPS={'Idle','HostWalk','Walk','Sneak','Chase','RunChase','WallCrawl','CeilingCrawl','Stalk','Idle_Breathe','Idle_Twitch','Idle_HeadTilt','Squeeze'}
for p in [Path(bpy.data.filepath).with_name('animation-contract.json')]:
    if p.is_file():
        for c in json.loads(p.read_text())['clips']:
            SPEEDS[c['name']]=c.get('reference_speed_cm_s',0)/100;(LOOPS.add if c.get('loop') else LOOPS.discard)(c['name'])
host={'Idle','HostWalk','Reset','Reveal','Idle_Twitch','Idle_HeadTilt','Idle_Breathe','WallEnter','CeilingEnter','WallExit','CeilingExit'}
font=ImageFont.load_default(size=15);small=ImageFont.load_default(size=12)
COL=[(255,80,80),(255,170,60),(80,200,255),(60,120,255),(120,255,120),(40,190,90),(255,255,255)]
TRACK=['DEF_arm_1.L_hand','DEF_arm_1.R_hand','DEF_arm_2.L_hand','DEF_arm_2.R_hand','DEF_arm_3.L_hand','DEF_arm_3.R_hand','head']
for act in sorted([a for a in bpy.data.actions if a.name.startswith('AN_Keeper_')],key=lambda a:a.name):
    name=act.name[len('AN_Keeper_'):]
    if ONLY and name not in ONLY:continue
    rig.animation_data.action=act;rig.animation_data.action_slot=act.slots[0]
    f0,f1=[int(round(x)) for x in act.frame_range];loop=name in LOOPS;n=min(N,f1-f0+1)
    frames=sorted(set(int(round(f0+(f1-f0)*i/(n if loop else n-1))) for i in range(n)))
    v=SPEEDS.get(name,0) if TRAVEL else 0;T=(f1-f0)/24
    def place(f):rig.location.y=(v*T/2-v*(f-f0)/24) if v else 0
    tall=name in host or name.startswith('Idle');aimz=1.25 if tall else .72;dist=3.6 if tall else 3.0;lens=36 if tall else 50
    if name=='JumpscareDeath':dist,aimz,lens=1.5,1.35,24
    views=[('front',(0,-dist,1.1),(0,0,aimz),lens),('side',(dist+.4,-.15,1.1),(0,-.15,aimz),lens)]
    tiles=[];tracks={}
    for vname,loc,tgt,ln in views:
        aim(loc,tgt,ln);row=[];pts={b:[] for b in TRACK}
        for f in range(f0,f1+1):
            s.frame_set(f);place(f);bpy.context.view_layer.update()
            for b in TRACK:
                pb=rig.pose.bones.get(b)
                if pb:c=world_to_camera_view(s,cam,rig.matrix_world@(pb.head if b!='head' else pb.tail));pts[b].append((c.x*TW,(1-c.y)*TH))
        for f in frames:
            s.frame_set(f);place(f);bpy.context.view_layer.update();fp=TMP/f'{PREFIX}_{name}_{vname}_{f:03d}.png';s.render.filepath=str(fp);bpy.ops.render.render(write_still=True);row.append((f,fp))
        tiles.append((vname,row));tracks[vname]=pts
    rig.location.y=0
    W=TW*len(frames);H=28+TH*len(tiles);sheet=Image.new('RGB',(W,H),(20,20,20));d=ImageDraw.Draw(sheet)
    d.text((6,5),f'{PREFIX.upper()}  AN_Keeper_{name}   frames {f0}-{f1}  {"loop" if loop else "one-shot"}  travel {v*100:.1f} cm/s  (player eye 1.1 m @ {dist} m; side)',fill=(235,235,235),font=font)
    for r,(vname,row) in enumerate(tiles):
        for c,(f,fp) in enumerate(row):
            im=Image.open(fp).convert('RGB');sheet.paste(im,(c*TW,28+r*TH));d.rectangle((c*TW,28+r*TH,c*TW+92,28+r*TH+20),fill=(0,0,0));d.text((c*TW+5,28+r*TH+2),f'{vname} f{f}',fill=(255,240,120),font=font)
    sheet.save(OUT/f'{PREFIX}-{name}.png')
    # Arc overlay: every frame of each wrist/head path over the middle frame, front + side, 2x size.
    over=Image.new('RGB',(TW*4,TH*2),(20,20,20))
    for c,(vname,row) in enumerate(tiles):
        mid=row[len(row)//2][1];im=Image.open(mid).convert('RGB').resize((TW*2,TH*2));dd=ImageDraw.Draw(im)
        for i,b in enumerate(TRACK):
            p=[(x*2,y*2) for x,y in tracks[vname][b]]
            if len(p)>1:dd.line(p,fill=COL[i],width=2)
            for x,y in p:dd.ellipse((x-1.5,y-1.5,x+1.5,y+1.5),fill=COL[i])
        dd.text((6,4),f'{name} {vname} arcs (1L red 1R orange 2L cyan 2R blue 3L green 3R dkgreen head white)',fill=(255,255,255),font=small);over.paste(im,(c*TW*2,0))
    over.save(OUT/f'{PREFIX}-{name}-arcs.png');print('SHEET',name,flush=True)
