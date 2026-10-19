"""Pose lab: render diagnostic stills (clay + texture) of named poses from player/side/three-quarter views.
blender -b --factory-startup --threads 4 V08Animation/keeper-v08-control-rig.blend --python pose_lab_v08.py -- <out_dir> <pose_module.py> [names]"""
import bpy,sys,importlib.util,math
from pathlib import Path
from mathutils import Vector
sys.path.insert(0,str(Path(__file__).parent));import keeper_v08_lib as K
from PIL import Image,ImageDraw,ImageFont
argv=sys.argv[sys.argv.index('--')+1:];OUT=Path(argv[0]);OUT.mkdir(parents=True,exist_ok=True)
spec=importlib.util.spec_from_file_location('poses',argv[1]);mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
names=argv[2].split(',') if len(argv)>2 else list(mod.POSES)
k=K.Keeper();K._k=k;s=bpy.context.scene
for o in bpy.data.objects:
    if o.name.startswith('WGT') or o.type=='ARMATURE':o.hide_render=True
s.render.engine='BLENDER_WORKBENCH';sh=s.display.shading;sh.light='STUDIO';sh.show_cavity=True;sh.cavity_type='BOTH';sh.show_shadows=True;sh.shadow_intensity=.5;sh.show_object_outline=True;sh.single_color=(.78,.75,.70)
s.world.color=(.2,.2,.21);s.view_settings.view_transform='Standard'
TW,TH=520,560;s.render.resolution_x=TW;s.render.resolution_y=TH
bpy.ops.mesh.primitive_plane_add(size=40);fl=bpy.context.object;fm=bpy.data.materials.new('f');fm.diffuse_color=(.32,.32,.32,1);fl.data.materials.append(fm)
cam=bpy.data.objects.new('C',bpy.data.cameras.new('C'));s.collection.objects.link(cam);s.camera=cam;cam.data.clip_start=.05
font=ImageFont.load_default(size=16)
for name in names:
    P=mod.POSES[name]() if callable(mod.POSES[name]) else mod.POSES[name]
    k.mesh_live(False);k.reset();k.apply(P);k.mesh_live(True);k.upd()
    rep=[f'{a}: elbow_out {k.elbow_out(a)[0]:+.2f} wrist tw/sw {k.twist_report(a)[0]:+.0f}/{k.twist_report(a)[1]:.0f}' for a in P['arms']]
    print('POSE',name,' | '.join(rep),flush=True)
    views=getattr(mod,'VIEWS',[('player',(0,-2.6,1.1),(0,0,.8),35),('side',(2.8,-.3,1.0),(0,-.3,.75),35),('34',(1.7,-2.0,1.9),(0,-.2,.6),35)])
    tiles=[]
    for vn,loc,tgt,lens in views:
        for mode in ['SINGLE','TEXTURE']:
            if mode=='TEXTURE' and vn!='player':continue
            sh.color_type=mode;s.view_settings.exposure=1.1 if mode=='TEXTURE' else 0
            cam.location=loc;cam.rotation_euler=(Vector(tgt)-Vector(loc)).to_track_quat('-Z','Y').to_euler();cam.data.lens=lens
            fp=OUT/f'_{name}_{vn}_{mode}.png';s.render.filepath=str(fp);bpy.ops.render.render(write_still=True);tiles.append((f'{vn} {mode.lower()}',fp))
    W=TW*len(tiles);sheet=Image.new('RGB',(W,TH+46),(15,15,15));d=ImageDraw.Draw(sheet);d.text((6,4),name,fill=(255,255,255),font=font);d.text((6,24),'  '.join(rep)[:260],fill=(200,200,200),font=ImageFont.load_default(size=11))
    for i,(lab,fp) in enumerate(tiles):sheet.paste(Image.open(fp).convert('RGB'),(i*TW,46))
    sheet.save(OUT/f'pose-{name}.png')
