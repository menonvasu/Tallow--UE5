import bpy, math, json
from mathutils import Vector,Matrix
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'Tallow/SourceAssets/Player/InteractionV05';COUT=ROOT/'Tallow/SourceAssets/Production/CandleV02'
s=bpy.context.scene;r=bpy.data.objects['RIG_Tallow_Generated'];full=bpy.data.objects['SK_Tallow_Generated_Full'];part=bpy.data.objects['SK_Tallow_Generated_Partial']
r.animation_data.action=None
for p in r.pose.bones:p.matrix_basis.identity()
full.hide_render=True;part.hide_render=False;part.hide_set(False)
# Use export-compatible linear skinning and soften longitudinal finger transitions.
for ob in [full,part]:
 for mod in ob.modifiers:
  if mod.type=='ARMATURE':mod.use_deform_preserve_volume=False
 for v in ob.data.vertices:
  names=[ob.vertex_groups[g.group].name for g in v.groups]
  digits=[n.rsplit('_',2)[0] for n in names if n.startswith(('thumb_','index_','middle_','ring_','pinky_'))]
  if not digits and not(abs(v.co.x)>.205 and .48<v.co.z<.606):continue
  if not digits:digits=['thumb']
  digit=max(set(digits),key=digits.count);side='r' if v.co.x>0 else 'l';chain=[f'{digit}_{j:02d}_{side}' for j in range(1,4)]+['hand_'+side]
  ds=[]
  for n in chain:
   b=r.data.bones[n];d=b.tail_local-b.head_local;t=max(0,min(1,(v.co-b.head_local).dot(d)/d.length_squared));ds.append(((v.co-(b.head_local+t*d)).length,n))
  ds=sorted(ds)[:2];weights=[1/(d+.005)**2 for d,n in ds];total=sum(weights)
  for g in list(v.groups):ob.vertex_groups[g.group].remove([v.index])
  for (_,n),w in zip(ds,weights):ob.vertex_groups[n].add([v.index],w/total,'REPLACE')
for o in list(bpy.data.objects):
 if o.name.startswith('REFERENCE_'):bpy.data.objects.remove(o,do_unlink=True)
# Original crooked brass spool holder: stable scalloped saucer, narrow child-sized grip,
# offset candle socket, hand-worked honey wax, asymmetric cooled drips.
materials=[]
for name,color,rough,metal in [('M_CandleV02_Brass',(.19,.115,.037,1),.48,.76),('M_CandleV02_Wax',(.58,.38,.13,1),.66,0),('M_CandleV02_Wick',(.025,.012,.007,1),.9,0)]:
 m=bpy.data.materials.new(name);m.use_nodes=True;bs=m.node_tree.nodes.get('Principled BSDF');bs.inputs['Base Color'].default_value=color;bs.inputs['Roughness'].default_value=rough;bs.inputs['Metallic'].default_value=metal
 noise=m.node_tree.nodes.new('ShaderNodeTexNoise');noise.inputs['Scale'].default_value=135;noise.inputs['Detail'].default_value=3
 bump=m.node_tree.nodes.new('ShaderNodeBump');bump.inputs['Strength'].default_value=.15;bump.inputs['Distance'].default_value=.0004;m.node_tree.links.new(noise.outputs['Fac'],bump.inputs['Height']);m.node_tree.links.new(bump.outputs['Normal'],bs.inputs['Normal']);materials.append(m)
verts=[];faces=[];mats=[]
def lathe(profile,mat,offset=(0,0,0),n=48,warp=0):
 profile=[(z+(.025 if z>=.070 else 0),rad) for z,rad in profile]
 start=len(verts)
 for j,(z,rad) in enumerate(profile):
  for k in range(n):
   a=2*math.pi*k/n;rr=rad*(1+warp*math.sin(3*a+j*.17));cx=.005*(z/.23)**2 if mat==1 else 0
   verts.append((offset[0]+cx+rr*math.cos(a),offset[1]+rr*math.sin(a),offset[2]+z+(warp*.004*math.sin(3*a+.4) if j==len(profile)-2 else 0)))
 for j in range(len(profile)-1):
  for k in range(n):faces.append((start+j*n+k,start+j*n+(k+1)%n,start+(j+1)*n+(k+1)%n,start+(j+1)*n+k));mats.append(mat)
 faces.append(tuple(start+k for k in reversed(range(n))));mats.append(mat);faces.append(tuple(start+(len(profile)-1)*n+k for k in range(n)));mats.append(mat)
lathe([(0,.024),(.003,.031),(.006,.034),(.009,.034),(.011,.029),(.010,.018),(.014,.013),(.025,.010),(.035,.0105),(.070,.0105),(.076,.014),(.080,.014),(.083,.022),(.087,.023),(.092,.020),(.094,.013)],0,warp=.025)
lathe([(.09,.010),(.095,.013),(.12,.0125),(.15,.012),(.18,.0118),(.21,.011),(.218,.012),(.221,.009),(.217,.003)],1,warp=.20)
# Drips are contiguous-looking overlapping wax beads, retained as intentional closed shells.
for a,z,length in [(.3,.206,.036),(2.2,.213,.056),(3.8,.216,.023),(5.0,.207,.043)]:
 rad=.0115;lathe([(0,.001),(.003,.003),(.009,.0027),(length,.0012)],1,(rad*math.cos(a),rad*math.sin(a),z-length+.025),n=12,warp=.1)
lathe([(.216,.0012),(.224,.0011),(.230,.0007)],2,offset=(.004,0,0),n=10)
me=bpy.data.meshes.new('CandleV02_OwnGeometry');me.from_pydata(verts,[],faces);me.update();c=bpy.data.objects.new('SM_CandleV02_Crooked',me);s.collection.objects.link(c)
for m in materials:me.materials.append(m)
for p,idx in zip(me.polygons,mats):p.material_index=idx;p.use_smooth=True
bpy.ops.object.select_all(action='DESELECT');c.select_set(True);bpy.context.view_layer.objects.active=c;bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT');bpy.ops.mesh.normals_make_consistent(inside=False) if hasattr(bpy.ops.mesh,'normals_make_consistent') else None
bpy.ops.uv.smart_project(angle_limit=math.radians(65),island_margin=.018);bpy.ops.object.mode_set(mode='OBJECT');me.uv_layers.active.name='UV_CandleV02'
# Candle base within the held frame; stem diameter21mm, controlled grasp center.
c.location=(.225,.245,.825)
def update():bpy.context.view_layer.update()
def aim(n,target):
 update();p=r.pose.bones[n];h=p.head.copy();q=(p.tail-h).rotation_difference(Vector(target)-h);p.matrix=Matrix.Translation(h)@q.to_matrix().to_4x4()@Matrix.Translation(-h)@p.matrix;update()
def arm(side,target,D,N):
 target=Vector(target);h=r.pose.bones['upperarm_'+side].head.copy();d=target-h;L=r.data.bones['upperarm_'+side].length;M=r.data.bones['lowerarm_'+side].length
 assert d.length<L+M
 axis=d.normalized();a=(L*L-M*M+d.length_squared)/(2*d.length);pole=Vector((-1 if side=='l' else 1,0,-.7));pole=(pole-axis*pole.dot(axis)).normalized();elbow=h+axis*a+pole*math.sqrt(max(0,L*L-a*a));aim('upperarm_'+side,elbow);aim('lowerarm_'+side,target)
 b=r.data.bones['hand_'+side];rd=(b.tail_local-b.head_local).normalized();rb=r.data.bones['pinky_01_'+side].head_local-r.data.bones['index_01_'+side].head_local;rb=(rb-rd*rb.dot(rd)).normalized();rn=rb.cross(rd).normalized();D=Vector(D).normalized();N=Vector(N);N=(N-D*N.dot(D)).normalized();B=D.cross(N).normalized()
 rot=Matrix((B,D,N)).transposed()@Matrix((rb,rd,rn)).transposed().transposed()@b.matrix_local.to_3x3()
 # Distribute desired axial wrist rotation through the forearm instead of collapsing the skin seam.
 lower=r.pose.bones['lowerarm_'+side];axis=(lower.tail-lower.head).normalized();wanted=rot@b.matrix_local.to_3x3().inverted()@r.data.bones['lowerarm_'+side].matrix_local.to_3x3();cur=lower.matrix.to_3x3().col[0].copy();want=wanted.col[0].copy();cur=(cur-axis*cur.dot(axis)).normalized();want=(want-axis*want.dot(axis)).normalized();angle=math.atan2(axis.dot(cur.cross(want)),cur.dot(want));head=lower.head.copy();lower.matrix=Matrix.Translation(head)@Matrix.Rotation(angle,4,axis)@Matrix.Translation(-head)@lower.matrix;update()
 p=r.pose.bones['hand_'+side];p.matrix=Matrix.Translation(p.head)@rot.to_4x4();update()
def reset():
 r.animation_data.action=None
 for p in r.pose.bones:p.matrix_basis.identity()
 update()
def grasp():
 arm('r',(.245,.190,.889),(0,1,0),(1,0,0))
 for digit in ['index','middle','ring','pinky']:
  # Each digit closes around the measured stem, in its own height plane.
  z=r.pose.bones[digit+'_01_r'].head.z
  for j,ang in enumerate([65,135,200],1):
   aim(f'{digit}_{j:02d}_r',(.225+.016*math.cos(math.radians(ang)),.245+.016*math.sin(math.radians(ang)),z))
 # Thumb opposes index on near side, rather than curling parallel to fingers.
 for j,target in enumerate([(.223,.227,.906),(.213,.234,.902),(.211,.245,.898)],1):aim(f'thumb_{j:02d}_r',target)
def pose(name):
 reset()
 if name!='Rest':grasp()
 if name in ['Cup','Relight']:
  arm('l',(.095,.195,.995),(.6,.2,.77),(-.7,-.5,.2))
  for d in ['index','middle','ring','pinky']:
   for j in range(1,4):
    update();pb=r.pose.bones[f'{d}_{j:02d}_l'];head=pb.head.copy();axis=Vector((.6,.2,.77)).cross(Vector((.7,.5,-.2))).normalized();pb.matrix=Matrix.Translation(head)@Matrix.Rotation(.23,4,axis)@Matrix.Translation(-head)@pb.matrix
 elif name=='ToolHold':
  mirror=Matrix.Diagonal((-1,1,1,1))
  for rp in r.pose.bones:
   if rp.name.endswith('_r') and rp.name.startswith(('clavicle','upperarm','lowerarm','hand','thumb','index','middle','ring','pinky')):
    left=rp.name[:-1]+'l';r.pose.bones[left].matrix=mirror@rp.matrix@r.data.bones[rp.name].matrix_local.inverted()@mirror@r.data.bones[left].matrix_local;update()
 elif name=='Reach':arm('l',(-.05,.29,.9),(0,1,.1),(0,0,1))
 update()
# Minimal contact correction on the generated surface: preserve topology/UV and push only
# penetrating grasp vertices onto the 21mm stem with a 0.5mm skin clearance.
pose('Hold');corrections={}
for ob in [full,part]:
 corrected=0;maxmove=0.0
 for v in ob.data.vertices:
  groupnames=[ob.vertex_groups[g.group].name for g in v.groups]
  if not any(n.endswith('_r') and n.startswith(('hand','thumb','index','middle','ring','pinky')) for n in groupnames):continue
  skin=Matrix(((0,0,0,0),)*4)
  for g in v.groups:
   name=ob.vertex_groups[g.group].name;bm=r.pose.bones[name].matrix@r.data.bones[name].matrix_local.inverted()
   for row in range(4):
    for col in range(4):skin[row][col]+=g.weight*bm[row][col]
  p=skin@v.co;rad=math.hypot(p.x-.225,p.y-.245)
  if .850<p.z<.918 and rad<.011:
   target=p.copy();scale=.011/max(rad,1e-6);target.x=.225+(p.x-.225)*scale;target.y=.245+(p.y-.245)*scale
   new=skin.inverted_safe()@target;maxmove=max(maxmove,(new-v.co).length);v.co=new;corrected+=1
 ob.data.update();corrections[ob.name]={'corrected_vertices':corrected,'max_rest_delta_m':maxmove}
(OUT/'grip-surface-corrections.json').write_text(json.dumps(corrections,indent=2))
poses={}
for name in ['Rest','Hold','Cup','Relight','Reach','ToolHold']:
 pose(name);poses[name]={p.name:p.matrix_basis.copy() for p in r.pose.bones}
with bpy.data.libraries.load(str(OUT/'05_interaction-textured.blend'),link=False) as (src,dst):dst.materials=['M_CandleV02_Baked']
me.materials.clear();me.materials.append(dst.materials[0])
for poly in me.polygons:poly.material_index=0
# Review first before producing animation and UV bake.
pose('Hold');r['interaction_v05']='Original generated surface, corrected articulated grip; review stage'
s.render.engine='CYCLES';s.cycles.device='CPU';s.cycles.samples=16;s.render.threads_mode='FIXED';s.render.threads=4;s.render.resolution_x=1280;s.render.resolution_y=720;s.render.resolution_percentage=100
s.camera.data.type='PERSP';s.camera.data.sensor_width=32;s.camera.data.lens=16;s.camera.data.clip_start=.01;s.camera.location=(0,-.20,1.10);s.camera.rotation_euler=(Vector((0,1,1.1))-s.camera.location).to_track_quat('-Z','Y').to_euler()
# Keep generated material's tactile fabric intact.
(OUT/'pose-bases.json').write_text(json.dumps({n:{b:[list(row) for row in m] for b,m in ps.items()} for n,ps in poses.items()}))
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'05_interaction-blocking.blend'))
print('RESULT',json.dumps({'saved':str(OUT/'05_interaction-blocking.blend'),'candle_triangles':sum(len(p.vertices)-2 for p in me.polygons),'wick_local_m':[.004,0,.255]}))
