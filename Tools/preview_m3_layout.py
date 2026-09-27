import bpy,os
from mathutils import Vector
root='/home/tyron/Projects/cgra252-team9'
col=bpy.data.collections.new('M3 Furniture');bpy.context.scene.collection.children.link(col)
def box(label,x,y,z,sx,sy,sz):
 bpy.ops.mesh.primitive_cube_add(size=1,location=(x,y,z+sz/2));o=bpy.context.object;o.name=label;o.scale=(sx,sy,sz);o.color=(.18,.48,.47,1) if label.startswith(('CLIMB','TEST')) else (.47,.42,.35,1)
 for c in list(o.users_collection):c.objects.unlink(o)
 col.objects.link(o);return o
src=open(root+'/Tools/build_m3_greybox.py').read();src=src.split('# Main route:')[1].split('# Working gameplay')[0];src=src[src.index('\n')+1:];src=src.replace('a.tags=','a["tags"]=');exec(src)
bpy.ops.import_scene.fbx(filepath=root+'/Tallow/SourceAssets/Keeper/SM_Keeper_Host_Proxy.fbx')
for o in bpy.context.selected_objects:
 o.location+=(Vector((11,3,-3.3)));o.scale=(.58,.8,1);o.color=(.15,.12,.1,1)
bpy.context.view_layer.update()
scene=bpy.context.scene;scene.render.engine='BLENDER_WORKBENCH';scene.display.shading.color_type='OBJECT';scene.display.shading.light='STUDIO';scene.display.shading.show_cavity=True;scene.display.shading.background_type='VIEWPORT';scene.display.shading.background_color=(.06,.07,.08);scene.render.resolution_x=1400;scene.render.resolution_y=1200;scene.render.resolution_percentage=100
out=root+'/Tools/previews';os.makedirs(out,exist_ok=True)
for floor,z in [('Basement',-3.3),('Ground',0),('Upper',3.9)]:
 for o in scene.objects:
  if o.type=='MESH':
   bounds=[o.matrix_world@Vector(c) for c in o.bound_box];lo=min(p.z for p in bounds);hi=max(p.z for p in bounds)
   o.hide_render=o.name.startswith(('UCX_','Scale_')) or lo>={'Basement':-.4,'Ground':3.5,'Upper':7.1}[floor] or hi<z-.5
 cam=bpy.data.objects.new('M3_Plan_'+floor,bpy.data.cameras.new('M3_Plan_'+floor));scene.collection.objects.link(cam);cam.location=(7.8,6.5,25);cam.rotation_euler=(0,0,0);cam.data.type='ORTHO';cam.data.ortho_scale=19;scene.camera=cam;scene.render.filepath=out+'/'+floor+'.png';bpy.ops.render.render(write_still=True)
result={'previews':out}
