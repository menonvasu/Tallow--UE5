import bpy,os
from mathutils import Vector
scene=bpy.context.scene;scene.frame_set(20);bpy.context.view_layer.update();dg=bpy.context.evaluated_depsgraph_get()
parts=[]
for ob in list(scene.objects):
 if ob.type!='MESH' or not ob.name.startswith('HYBRID |') or ob.hide_render:continue
 mesh=bpy.data.meshes.new_from_object(ob.evaluated_get(dg),depsgraph=dg)
 copy=bpy.data.objects.new('Proxy_'+ob.name,mesh);scene.collection.objects.link(copy);copy.matrix_world=ob.matrix_world.copy();parts.append(copy)
bpy.ops.object.select_all(action='DESELECT')
for o in parts:o.select_set(True)
bpy.context.view_layer.objects.active=parts[0];bpy.ops.object.join();host=bpy.context.object;host.name='SM_Keeper_Host_Proxy'
# A frozen host mesh for prototype AI placement, not the final skeletal asset.
pts=[host.matrix_world@v.co for v in host.data.vertices];lo=Vector(tuple(min(p[i] for p in pts) for i in range(3)));hi=Vector(tuple(max(p[i] for p in pts) for i in range(3)));scale=2.30/(hi.z-lo.z);mid=(lo+hi)*.5
for v,p in zip(host.data.vertices,pts):v.co=((p.x-mid.x)*scale,(p.y-mid.y)*scale,(p.z-lo.z)*scale)
host.matrix_world.identity()
dec=host.modifiers.new('Prototype triangle budget','DECIMATE');dec.ratio=.16;bpy.ops.object.modifier_apply(modifier=dec.name)
# Replace procedural materials with portable diffuse placeholders.
for slot in host.material_slots:
 old=slot.material;mat=bpy.data.materials.new('Proxy_'+old.name);mat.diffuse_color=old.diffuse_color;mat.use_nodes=True;p=mat.node_tree.nodes.get('Principled BSDF');p.inputs['Base Color'].default_value=old.diffuse_color;p.inputs['Roughness'].default_value=.8;slot.material=mat
path='/home/tyron/Projects/cgra252-team9/Tallow/SourceAssets/Keeper/SM_Keeper_Host_Proxy.fbx'
bpy.ops.export_scene.fbx(filepath=path,use_selection=True,object_types={'MESH'},bake_anim=False,axis_forward='-Y',axis_up='Z',apply_scale_options='FBX_SCALE_UNITS',mesh_smooth_type='FACE')
result={'path':path,'vertices':len(host.data.vertices),'height_m':2.3}
