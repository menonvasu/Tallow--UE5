import bpy
from mathutils import Vector
s=bpy.context.scene;r=bpy.data.objects['RIG_Tallow_Generated'];r.animation_data.action=None
for p in r.pose.bones:p.matrix_basis.identity()
bpy.data.objects['SK_Tallow_Generated_Full'].hide_render=True;bpy.data.objects['SK_Tallow_Generated_Partial'].hide_render=False
bpy.data.objects['SM_CandleV02_Crooked'].hide_render=True;s.camera.location=(.65,.65,.68);s.camera.rotation_euler=(Vector((.275,.04,.57))-s.camera.location).to_track_quat('-Z','Y').to_euler();s.camera.data.lens=65;s.render.resolution_x=1000;s.render.resolution_y=750;s.cycles.samples=8;s.render.threads_mode='FIXED';s.render.threads=4;s.render.filepath='/home/tyron/Projects/tallow-current/Tallow/SourceAssets/Player/InteractionV05/rest-hand-diagnostic.png';bpy.ops.render.render(write_still=True)
