"""Capture-camera emulation of TallowGameMode/TallowPlayDirector: player eye 1.1 m, Keeper root D m ahead facing it; control
rotation snapped to the `head` socket at capture, then RInterpTo(16/s) + Perlin tremor (1.4 deg after 0.6 s); FOV 90->70
(InterpEaseOut exp 3 over 0.35 s); 16:9. Every frame rendered; strip annotated with head/maw frame fill and lens distance.
blender -b --factory-startup --threads 4 <blend> --python render_capture_v08.py -- <out_dir> <prefix> <Clip,Clip> <D,D,..> [every] [cycles]"""
import bpy,sys,math,json
from pathlib import Path
from mathutils import Vector,Euler
from bpy_extras.object_utils import world_to_camera_view
from PIL import Image,ImageDraw,ImageFont
import numpy as np
argv=sys.argv[sys.argv.index('--')+1:];OUT=Path(argv[0]);PREFIX=argv[1];CLIPS=argv[2].split(',');DS=[float(x) for x in argv[3].split(',')]
EVERY=argv[4] if len(argv)>4 else '1';ONLYF={int(x) for x in EVERY.split('+')} if '+' in EVERY else None;EVERY=1 if ONLYF else int(EVERY);CYC=len(argv)>5 and argv[5]=='cycles'
OUT.mkdir(parents=True,exist_ok=True);TMP=OUT/'_cap';TMP.mkdir(exist_ok=True)
s=bpy.context.scene;rig=bpy.data.objects.get('RIG_KeeperV07') or next(o for o in bpy.data.objects if o.type=='ARMATURE')
mesh=next(o for o in bpy.data.objects if o.type=='MESH' and o.parent==rig);mesh.hide_viewport=False
for o in bpy.data.objects:
    if o.type!='MESH' or o.name.startswith('WGT'):o.hide_render=True
W,Hh=(640,360) if not CYC else (960,540);s.render.resolution_x=W;s.render.resolution_y=Hh;s.render.resolution_percentage=100
bpy.ops.mesh.primitive_plane_add(size=40);floor=bpy.context.object;fm=bpy.data.materials.new('floor');fm.diffuse_color=(.12,.11,.10,1);floor.data.materials.append(fm)
cam=bpy.data.objects.new('CaptureCam',bpy.data.cameras.new('CaptureCam'));s.collection.objects.link(cam);s.camera=cam;cam.data.sensor_fit='HORIZONTAL';cam.data.sensor_width=36;cam.data.clip_start=.10
if CYC:
    s.render.engine='CYCLES';s.cycles.device='CPU';s.cycles.samples=24;s.cycles.use_denoising=True;s.render.threads_mode='FIXED';s.render.threads=4
    s.world.color=(.004,.004,.005);s.view_settings.view_transform='AgX';s.view_settings.look='AgX - Medium High Contrast';s.view_settings.exposure=.6
    fm.use_nodes=True;fm.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value=(.05,.045,.04,1)
    ld=bpy.data.lights.new('Candle','POINT');ld.energy=18;ld.color=(1,.62,.32);ld.shadow_soft_size=.02;candle=bpy.data.objects.new('Candle',ld);s.collection.objects.link(candle)
    rd=bpy.data.lights.new('RoomBounce','AREA');rd.energy=6;rd.size=4;rd.color=(.55,.6,.75);rb=bpy.data.objects.new('RoomBounce',rd);s.collection.objects.link(rb);rb.location=(1.5,-.5,3.2);rb.rotation_euler=(0.4,0.3,0)
else:
    s.render.engine='BLENDER_WORKBENCH';sh=s.display.shading;sh.light='STUDIO';sh.color_type='TEXTURE';sh.show_cavity=True;sh.cavity_type='BOTH';sh.show_shadows=True;sh.shadow_intensity=.5
    s.world.color=(.05,.05,.055);s.view_settings.view_transform='Standard';s.view_settings.exposure=1.1
# head-region vertices (dominant weight on `head`) and the maw (tiny closed cluster at the mouth centre in Basis)
gi=mesh.vertex_groups['head'].index
headv=np.array([v.index for v in mesh.data.vertices if v.groups and max(v.groups,key=lambda g:g.weight).group==gi])
basis=np.array([list(v.co) for v in mesh.data.shape_keys.key_blocks['Basis'].data]);maw=np.where(np.linalg.norm(basis-np.array([-.04,.015,2.275]),axis=1)<.006)[0]
def perlin(x):
    i=math.floor(x);f=x-i;u=f*f*(3-2*f);g=lambda n:math.sin(n*127.1+311.7)*43758.5453%1*2-1
    return (g(i)*(1-u)+g(i+1)*u)
font=ImageFont.load_default(size=14);small=ImageFont.load_default(size=12);REPORT={}
for name in CLIPS:
    act=bpy.data.actions['AN_Keeper_'+name];rig.animation_data.action=act;rig.animation_data.action_slot=act.slots[0];f0,f1=[int(round(x)) for x in act.frame_range]
    for D in DS:
        eye=Vector((0,-D,1.1));rot=None;rows=[];tiles=[]
        for f in range(f0,f1+1):
            s.frame_set(f);bpy.context.view_layer.update();t=(f-f0)/24
            head=rig.matrix_world@rig.pose.bones['head'].head;d=(head-eye).normalized();aim=(math.degrees(math.asin(d.z)),math.degrees(math.atan2(d.x,d.y)))
            if rot is None:rot=list(aim)
            else:
                a=min(1,16/24);rot=[rot[0]+(aim[0]-rot[0])*a,rot[1]+((aim[1]-rot[1]+180)%360-180)*a]
            tr=1.4*min(1,t/.6);p=rot[0]+perlin(t*21)*tr;y=rot[1]+perlin(t*17+5)*tr
            fov=90+(70-90)*(1-(1-min(1,t/.35))**3);cam.data.lens=18/math.tan(math.radians(fov/2))
            cam.location=eye;cam.rotation_euler=Euler((math.radians(90+p),0,math.radians(-y)),'XYZ')
            if CYC:candle.location=eye+cam.matrix_world.to_3x3()@Vector((.12,-.18,-.35))
            bpy.context.view_layer.update()
            ev=mesh.evaluated_get(bpy.context.evaluated_depsgraph_get());me=ev.to_mesh();co=np.empty(len(me.vertices)*3,np.float32);me.vertices.foreach_get('co',co);co=co.reshape(-1,3);ev.to_mesh_clear()
            M=mesh.matrix_world;def_=lambda ids:[world_to_camera_view(s,cam,M@Vector(c)) for c in co[ids]]
            def fill(ids):
                pts=def_(ids);ys=[q.y for q in pts if q.z>0];xs=[q.x for q in pts if q.z>0]
                if not ys:return 0,0,None
                y0,y1=max(0,min(ys)),min(1,max(ys));x0,x1=max(0,min(xs)),min(1,max(xs));c=sum(pts,Vector())/len(pts)
                return max(0,y1-y0),max(0,x1-x0),(c.x,c.y)
            hf=fill(headv);mf=fill(maw);lens_d=float(np.linalg.norm(co[headv]-np.array(eye),axis=1).min())
            row={'frame':f,'t':round(t,3),'head_fill_h':round(hf[0],3),'maw_fill_h':round(mf[0],3),'maw_centre':[round(x,3) for x in mf[2]] if mf[2] else None,'head_to_lens_m':round(lens_d,3),'fov':round(fov,1)}
            rows.append(row)
            if (ONLYF and f in ONLYF) or (not ONLYF and ((f-f0)%EVERY==0 or f==f1)):
                fp=TMP/f'{PREFIX}_{name}_{D}_{f:03d}.png';s.render.filepath=str(fp);bpy.ops.render.render(write_still=True);tiles.append((row,fp))
        REPORT[f'{name}@{D}']=rows
        cols=8 if not CYC else 4;tw,th=(W//2,Hh//2) if not CYC else (W//2,Hh//2);rws=(len(tiles)+cols-1)//cols
        sheet=Image.new('RGB',(cols*tw,24+rws*th),(10,10,10));dr=ImageDraw.Draw(sheet)
        dr.text((6,4),f'{PREFIX} {name}  capture cam: eye 1.1 m, root {D} m ahead, tracks head (RInterp 16), FOV 90->70',fill=(240,240,240),font=font)
        for i,(r,fp) in enumerate(tiles):
            im=Image.open(fp).convert('RGB').resize((tw,th));x=(i%cols)*tw;yy=24+(i//cols)*th;sheet.paste(im,(x,yy))
            dr.rectangle((x,yy,x+tw,yy+16),fill=(0,0,0));dr.text((x+3,yy+1),f"f{r['frame']} {r['t']:.2f}s head {r['head_fill_h']*100:.0f}% maw {r['maw_fill_h']*100:.0f}% {r['head_to_lens_m']:.2f}m",fill=(255,235,120),font=small)
        sheet.save(OUT/f'{PREFIX}-{name}-D{D}.png');print('CAPTURE',name,D,flush=True)
(OUT/f'{PREFIX}-capture-metrics.json').write_text(json.dumps(REPORT,indent=1))
