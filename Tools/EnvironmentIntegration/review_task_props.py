import bpy,math,json
from pathlib import Path
from mathutils import Vector
R=Path(__file__).resolve().parents[2];O=R/'Tallow/SourceAssets/Production/HouseIntegrationV01/TaskProps';bpy.ops.wm.open_mainfile(filepath=str(O/'TaskProps.blend'));s=bpy.context.scene;s.render.engine='CYCLES';s.cycles.device='CPU';s.cycles.samples=24;s.cycles.use_denoising=True;s.render.threads_mode='FIXED';s.render.threads=4;s.render.resolution_x=1100;s.render.resolution_y=750;s.render.resolution_percentage=100;s.world.color=(.06,.06,.06);s.view_settings.view_transform='AgX'
bpy.data.objects['SM_HouseTask_Switchboard'].location=(-.8,0,1.05);bpy.data.objects['SM_HouseTask_EscapeLatch'].location=(-.13,0,1.02);bpy.data.objects['SM_HouseTask_EscapeWindowFrame'].location=(1.0,.12,0)
for loc,energy,size in [((-2,-3,3),400,3),((2,1,3),500,2)]:
 d=bpy.data.lights.new('Softbox','AREA');d.energy=energy;d.shape='DISK';d.size=size;o=bpy.data.objects.new('Softbox',d);s.collection.objects.link(o);o.location=loc;o.rotation_euler=(Vector((0,0,1))-o.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(1.5,-5,2.4));s.camera=bpy.context.object;s.camera.rotation_euler=(Vector((.3,0,1))-s.camera.location).to_track_quat('-Z','Y').to_euler();s.camera.data.type='ORTHO';s.camera.data.ortho_scale=3.2;s.render.filepath=str(O/'review-task-props.png');bpy.ops.render.render(write_still=True)
# Proof reports actual exported source mesh UV presence and geometry, not manifest assertions.
metrics=[]
for o in s.objects:
 if o.type=='MESH':o.data.calc_loop_triangles();metrics.append(dict(mesh=o.name,triangles=len(o.data.loop_triangles),dimensions_m=list(o.dimensions),uv_layers=[u.name for u in o.data.uv_layers]))
(O/'measured-metrics.json').write_text(json.dumps(metrics,indent=2))
