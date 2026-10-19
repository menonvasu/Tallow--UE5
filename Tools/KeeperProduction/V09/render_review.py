"""V09 maw review: capture pose (EXPORT_AN_Keeper_JumpscareDeath, frame 40), player eye 1.1 m, ~1.2 m from the head, candle-lit
corridor (held candle beside the camera, a far candle behind the Keeper, near-black ambient). Cycles, run under the heavy-render flock.
Usage: blender -b --python render_review.py -- [samples] [names...]"""
import bpy,sys,math,json
from pathlib import Path
from mathutils import Vector
R=Path(__file__).resolve().parents[3];O=R/'Tallow/SourceAssets/Keeper/V09Mouth';RV=O/'Review';RV.mkdir(exist_ok=True)
a=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [];SAMPLES=int(a[0]) if a else 96;ONLY=set(a[1:])
bpy.ops.wm.open_mainfile(filepath=str(O/'keeper-v09-mouth-export.blend'))
s=bpy.context.scene;r=next(x for x in bpy.data.objects if x.type=='ARMATURE');o=next(x for x in bpy.data.objects if x.type=='MESH');ks=o.data.shape_keys.key_blocks
act=bpy.data.actions['EXPORT_AN_Keeper_JumpscareDeath'];r.animation_data_create();r.animation_data.action=act;r.animation_data.action_slot=act.slots[0];r.data.pose_position='POSE'
if o.data.shape_keys.animation_data:o.data.shape_keys.animation_data.action=None
s.frame_set(40)
s.render.engine='CYCLES'
prefs=bpy.context.preferences.addons['cycles'].preferences
for t in ('OPTIX','CUDA'):
 try:prefs.compute_device_type=t;prefs.get_devices();[setattr(d,'use',d.type==t) for d in prefs.devices];s.cycles.device='GPU';break
 except TypeError:continue
s.cycles.samples=SAMPLES;s.cycles.use_denoising=True;s.render.resolution_x=1280;s.render.resolution_y=720;s.view_settings.view_transform='AgX';s.view_settings.look='AgX - Medium High Contrast'
w=s.world or bpy.data.worlds.new('W');s.world=w;w.use_nodes=True;w.node_tree.nodes['Background'].inputs['Color'].default_value=(.0015,.0016,.002,1)
# corridor: floor, walls, ceiling in dark damp plaster
def plane(name,loc,rot,size):
 bpy.ops.mesh.primitive_plane_add(size=1,location=loc,rotation=rot);p=bpy.context.object;p.name=name;p.scale=size
 m=bpy.data.materials.get('Corridor') or bpy.data.materials.new('Corridor');m.use_nodes=True;b=m.node_tree.nodes['Principled BSDF'];b.inputs['Base Color'].default_value=(.06,.05,.04,1);b.inputs['Roughness'].default_value=.85;p.data.materials.append(m);return p
plane('Floor',(0,-1,0),(0,0,0),(3,8,1));plane('WallL',(-1.25,-1,1.4),(0,math.radians(90),0),(3,8,1));plane('WallR',(1.25,-1,1.4),(0,math.radians(90),0),(3,8,1));plane('Ceil',(0,-1,2.7),(0,0,0),(3,8,1))
ks['HOOD_OPEN'].value=1;bpy.context.view_layer.update();ev=o.evaluated_get(bpy.context.evaluated_depsgraph_get()).to_mesh()
mv={v for p in ev.polygons if p.material_index==2 for v in p.vertices};mouth=sum((ev.vertices[i].co for i in mv),Vector())/len(mv);mouth=o.matrix_world@mouth
fwd=Vector((mouth.x,mouth.y-.6,mouth.z));o.evaluated_get(bpy.context.evaluated_depsgraph_get()).to_mesh_clear();ks['HOOD_OPEN'].value=0
def light(name,loc,power,color=(1.,.52,.22),radius=.015):
 d=bpy.data.lights.new(name,'POINT');d.energy=power;d.color=color;d.shadow_soft_size=radius;l=bpy.data.objects.new(name,d);s.collection.objects.link(l);l.location=loc;return l
eye=Vector((mouth.x+.08,mouth.y-1.2,1.10))
light('HeldCandle',eye+Vector((.24,.20,-.24)),20.)
light('FarCandle',Vector((-.6,2.2,1.35)),14.,(1.,.48,.2),.02);s.view_settings.exposure=.6
cd=bpy.data.cameras.new('Player');cam=bpy.data.objects.new('Player',cd);s.collection.objects.link(cam);s.camera=cam;cd.sensor_fit='HORIZONTAL';cd.sensor_width=36;cd.clip_start=.02
def shoot(name,loc,target,hfov,hood,chomp):
 if ONLY and name not in ONLY:return
 cam.location=loc;cam.rotation_euler=(target-loc).to_track_quat('-Z','Y').to_euler();cd.lens=18/math.tan(math.radians(hfov)/2)
 for k in ks:k.value=0
 ks['HOOD_OPEN'].value=hood;ks['CHOMP'].value=chomp;s.render.filepath=str(RV/f'{name}.png');bpy.ops.render.render(write_still=True);print('SHOT',name,flush=True)
shots=[('capture-hood0',eye,mouth,90,0,0),('capture-hood050',eye,mouth,90,.5,0),('capture-hood1',eye,mouth,90,1,0),('capture-hood1-chomp',eye,mouth,90,1,1),
 ('close-hood1',eye,mouth,40,1,0),('close-hood1-chomp',eye,mouth,40,1,1),('close-hood050',eye,mouth,40,.5,0),('close-hood0',eye,mouth,40,0,0),
 ('close-threequarter',mouth+Vector((-.62,-.86,.12)),mouth,40,1,0),('close-low',mouth+Vector((.25,-.85,-.35)),mouth,40,1,0)]
for sh in shots:shoot(*sh)
print('RESULT '+json.dumps({'mouth_centre':list(mouth),'renders':[s_[0] for s_ in shots if not ONLY or s_[0] in ONLY],'samples':SAMPLES,'pose':'EXPORT_AN_Keeper_JumpscareDeath f40','eye':list(eye)}))
