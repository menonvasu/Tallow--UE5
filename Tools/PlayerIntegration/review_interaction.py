import bpy,json
from pathlib import Path
from mathutils import Matrix,Vector
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'Tallow/SourceAssets/Player/InteractionV05';r=bpy.data.objects['RIG_Tallow_Generated'];s=bpy.context.scene
poses=json.loads((OUT/'pose-bases.json').read_text())
for name in ['Hold','Cup']:
 for p in r.pose.bones:p.matrix_basis=Matrix(poses[name][p.name])
 bpy.context.view_layer.update();s.render.filepath=str(OUT/('review-FP-'+name+'.png'));bpy.ops.render.render(write_still=True)
 if name=='Hold':
  old=s.camera.matrix_world.copy();s.camera.location=(.65,.85,1.12);s.camera.rotation_euler=(Vector((.26,.245,.92))-s.camera.location).to_track_quat('-Z','Y').to_euler();s.camera.data.lens=65;s.render.filepath=str(OUT/'review-grip-quarter.png');bpy.ops.render.render(write_still=True);s.camera.matrix_world=old;s.camera.data.lens=16
print('RESULT review completed')
