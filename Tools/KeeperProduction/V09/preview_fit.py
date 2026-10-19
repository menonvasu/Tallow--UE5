"""Quick Workbench fit preview (no rig): Keeper body at HOOD_OPEN=t (old maw removed) + V09 maw at the same t, rest pose.
Usage: blender -b <keeper export blend> --python preview_fit.py -- <mouth-low.blend> <out_prefix> [t ...]"""
import bpy,bmesh,sys,numpy as np
from pathlib import Path
from mathutils import Vector
sys.path.append(str(Path(__file__).parent));import mouth_rig as MR
a=sys.argv[sys.argv.index('--')+1:];LOW,OUT=a[0],a[1];TS=[float(x) for x in a[2:]] or [0,.5,1]
o=next(x for x in bpy.data.objects if x.type=='MESH');me=o.data;ks=me.shape_keys.key_blocks
bm=bmesh.new();bm.from_mesh(me);bm.verts.ensure_lookup_table();bmesh.ops.delete(bm,geom=[f for f in bm.faces if f.material_index>0],context='FACES');bm.to_mesh(me);bm.free()
for m in o.modifiers:m.show_viewport=False
with bpy.data.libraries.load(LOW) as (src,dst):dst.objects=['MOUTH_Flesh','MOUTH_Teeth']
ms=[x for x in dst.objects];s=bpy.context.scene
for x in ms:s.collection.objects.link(x)
def co(m):v=np.empty(len(m.vertices)*3);m.vertices.foreach_get('co',v);return v.reshape(-1,3)
OPEN={x.name:co(x.data) for x in ms}
s.render.engine='BLENDER_WORKBENCH';s.display.shading.light='STUDIO';s.display.shading.color_type='MATERIAL';s.display.shading.show_backface_culling=False
s.render.resolution_x=900;s.render.resolution_y=900
for x,c in [(o,(.42,.40,.38,1)),(ms[0],(.55,.18,.2,1)),(ms[1],(.9,.85,.6,1))]:
 for mat in x.data.materials:
  if mat:mat.diffuse_color=c
cd=bpy.data.cameras.new('c');cam=bpy.data.objects.new('c',cd);s.collection.objects.link(cam);s.camera=cam;cd.lens=50
C=Vector((-.04,-.1,2.24))
for t in TS:
 for k in ks:k.value=0
 ks['HOOD_OPEN'].value=t
 for x in ms:P=OPEN[x.name];Q=MR.closed(P);x.data.vertices.foreach_set('co',(Q+t*(P-Q)).ravel());x.data.update()
 for name,d in [('front',Vector((0,-1,0))),('tq',Vector((-.55,-1,.25)).normalized()),('low',Vector((.3,-1,-.35)).normalized()),('top',Vector((.15,-.6,1)).normalized())]:
  cam.location=C+d*1.1;cam.rotation_euler=(-d).to_track_quat('-Z','Y').to_euler()
  s.render.filepath=f'{OUT}_t{t:.2f}_{name}.png';bpy.ops.render.render(write_still=True)
print('RESULT ok')
