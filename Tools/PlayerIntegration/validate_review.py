import bpy,math,json,numpy as np
from pathlib import Path
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'Tallow/SourceAssets/Player/InteractionV05';COUT=ROOT/'Tallow/SourceAssets/Production/CandleV02';s=bpy.context.scene;r=bpy.data.objects['RIG_Tallow_Generated'];p=bpy.data.objects['SK_Tallow_Generated_Partial'];full=bpy.data.objects['SK_Tallow_Generated_Full'];c=bpy.data.objects['SM_CandleV02_Crooked']
# Tangent atlas check: count overlapping triangle interiors at delivered 2K texel centers.
size=2048;occupancy=np.zeros((size,size),np.uint16);uv=c.data.uv_layers.active;c.data.calc_loop_triangles()
for tri in c.data.loop_triangles:
 q=np.array([uv.data[i].uv[:] for i in tri.loops])*size;lo=np.maximum(np.floor(q.min(axis=0)).astype(int),0);hi=np.minimum(np.ceil(q.max(axis=0)).astype(int),size-1)
 if np.any(hi<lo):continue
 xs=np.arange(lo[0],hi[0]+1)+.5;ys=np.arange(lo[1],hi[1]+1)+.5;X,Y=np.meshgrid(xs,ys);a,b,d=q;den=(b[1]-d[1])*(a[0]-d[0])+(d[0]-b[0])*(a[1]-d[1]);A=((b[1]-d[1])*(X-d[0])+(d[0]-b[0])*(Y-d[1]))/den;B=((d[1]-a[1])*(X-d[0])+(a[0]-d[0])*(Y-d[1]))/den
 occupancy[lo[1]:hi[1]+1,lo[0]:hi[0]+1]+=((A>1e-5)&(B>1e-5)&(1-A-B>1e-5)).astype(np.uint16)
report={'candle_uv_overlap_texels':int((occupancy>1).sum()),'candle_uv_occupied_texels':int((occupancy>0).sum()),'contact_samples':{},'skeleton_rest_bones':len(r.data.bones)}
ids=[]
for v in p.data.vertices:
 if any(p.vertex_groups[g.group].name.endswith('_r') and p.vertex_groups[g.group].name.startswith(('hand','thumb','index','middle','ring','pinky')) for g in v.groups):ids.append(v.index)
for name in ['Hold','Cup','Walk','Relight','Pickup']:
 a=bpy.data.actions['AN_PlayerV05_'+name];r.animation_data.action=a;r.animation_data.action_slot=a.slots[0];s.frame_set(18);dg=bpy.context.evaluated_depsgraph_get();ev=p.evaluated_get(dg);me=ev.to_mesh();inv=c.matrix_world.inverted();dist=[]
 for i in ids:
  pos=inv@(p.matrix_world@me.vertices[i].co)
  if .025<pos.z<.093:dist.append(math.hypot(pos.x,pos.y)-.0105)
 ev.to_mesh_clear();report['contact_samples'][name]={'stem_skin_min_clearance_m':min(dist) if dist else None,'vertices_penetrating_more_than_1mm':sum(d<-.001 for d in dist)}
(OUT/'contact-uv-validation.json').write_text(json.dumps(report,indent=2));print('AUDIT',json.dumps(report),flush=True)
assert report['candle_uv_overlap_texels']==0
# Actual asset, PBR textures, and measured attachment; temporary emissive flame reference for scale only.
bpy.ops.mesh.primitive_uv_sphere_add(segments=16,ring_count=8,radius=1);flame=bpy.context.object;flame.name='REVIEW_FlameScaleOnly';flame.scale=(.004,.004,.015);flame.parent=c;flame.location=(.004,0,.269)
mat=bpy.data.materials.new('REVIEW_Flame');mat.use_nodes=True;bs=mat.node_tree.nodes.get('Principled BSDF');bs.inputs['Base Color'].default_value=(1,.55,.12,1);bs.inputs['Emission Color'].default_value=(1,.55,.12,1);bs.inputs['Emission Strength'].default_value=3;flame.data.materials.append(mat)
s.render.engine='CYCLES';s.cycles.device='CPU';s.cycles.samples=12;s.render.threads_mode='FIXED';s.render.threads=4;s.render.resolution_x=1280;s.render.resolution_y=720
s.camera.data.lens=16;s.camera.location=(0,-.20,1.10);s.camera.rotation_euler=(Vector((0,1,1.10))-s.camera.location).to_track_quat('-Z','Y').to_euler()
full.hide_render=True;p.hide_render=False
for name,frame in [('Hold',1),('Cup',18),('Relight',20),('Pickup',1),('Pickup',24),('Walk',8)]:
 a=bpy.data.actions['AN_PlayerV05_'+name];r.animation_data.action=a;r.animation_data.action_slot=a.slots[0];s.frame_set(frame);s.render.filepath=str(OUT/f'FINAL-FP-{name}-{frame}.png');bpy.ops.render.render(write_still=True)
 if name=='Hold':
  old=s.camera.matrix_world.copy();s.camera.location=(.65,.85,1.12);s.camera.rotation_euler=(Vector((.225,.245,.93))-s.camera.location).to_track_quat('-Z','Y').to_euler();s.camera.data.lens=65;s.render.filepath=str(OUT/'FINAL-grip-quarter.png');bpy.ops.render.render(write_still=True);s.camera.matrix_world=old;s.camera.data.lens=16
# Actual held tools use the same mirrored measured palm axis.
with bpy.data.libraries.load(str(ROOT/'Tallow/SourceAssets/Production/ToolProps/ToolProps_Production.blend'),link=False) as (src,dst):
 dst.objects=['SM_ToolV02_Crowbar','SM_ToolV02_Fuse']
for tool in dst.objects:
 s.collection.objects.link(tool);tool.hide_render=True
a=bpy.data.actions['AN_PlayerV05_ToolHold'];r.animation_data.action=a;r.animation_data.action_slot=a.slots[0];s.frame_set(1)
for tool,z in zip(dst.objects,[.75,.83]):
 tool.location=(-.225,.245,z);tool.hide_render=False;s.render.filepath=str(OUT/('FINAL-FP-'+tool.name+'.png'));bpy.ops.render.render(write_still=True);tool.hide_render=True
full.hide_render=False;p.hide_render=True;c.hide_render=True;flame.hide_render=True;s.camera.location=(1.5,2.4,1.1);s.camera.rotation_euler=(Vector((0,.05,.62))-s.camera.location).to_track_quat('-Z','Y').to_euler();s.camera.data.lens=45
for name,frame in [('IntroBound',1),('ChairEscape',23),('ChairEscape',46)]:
 a=bpy.data.actions['AN_PlayerV05_'+name];r.animation_data.action=a;r.animation_data.action_slot=a.slots[0];s.frame_set(frame);s.render.filepath=str(OUT/f'FINAL-body-{name}-{frame}.png');bpy.ops.render.render(write_still=True)
print('RESULT validation and ten review images complete')
