import bpy,json,math
from pathlib import Path
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'Tallow/SourceAssets/Player/InteractionV05/CupShieldRevision01';s=bpy.context.scene;r=bpy.data.objects['RIG_Tallow_Generated'];part=bpy.data.objects['SK_Tallow_Generated_Partial'];bpy.data.objects['SK_Tallow_Generated_Full'].hide_render=True;part.hide_render=False;c=bpy.data.objects['SM_CandleV02_Crooked']
s.render.engine='CYCLES';s.cycles.device='CPU';s.cycles.samples=16;s.render.threads_mode='FIXED';s.render.threads=4;s.render.resolution_x=1280;s.render.resolution_y=720;s.camera.location=(0,-.20,1.10);s.camera.rotation_euler=Vector((0,1,0)).to_track_quat('-Z','Y').to_euler();s.camera.data.lens=16
report={}
for name in ['SOURCE_CupShield','AN_PlayerV05_CupShield']:
 a=bpy.data.actions[name];r.animation_data.action=a;r.animation_data.action_slot=a.slots[0];s.frame_set(18);bpy.context.view_layer.update();dg=bpy.context.evaluated_depsgraph_get();ev=part.evaluated_get(dg);me=ev.to_mesh();inv=c.matrix_world.inverted();dist=[]
 for v in part.data.vertices:
  if any(part.vertex_groups[g.group].name.endswith('_r') and part.vertex_groups[g.group].name.startswith(('hand','thumb','index','middle','ring','pinky')) for g in v.groups):
   p=inv@(part.matrix_world@me.vertices[v.index].co)
   if .025<p.z<.093:dist.append(math.hypot(p.x,p.y)-.0105)
 ev.to_mesh_clear();report[name]={'right_stem_min_clearance_m':min(dist),'vertices_penetrating_over_1mm':sum(d<-.001 for d in dist)};s.render.filepath=str(OUT/(name+'.png'));bpy.ops.render.render(write_still=True)
(OUT/'contact-validation.json').write_text(json.dumps(report,indent=2));print('RESULT',json.dumps(report))
