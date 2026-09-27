"""Run headlessly on the latest hybrid .blend; never overwrites the source file."""
import bpy,os,json,math
from mathutils import Matrix,Vector
ROOT='/home/tyron/Projects/cgra252-team9'
OUT=ROOT+'/Tallow/SourceAssets/Keeper/Animated';os.makedirs(OUT,exist_ok=True)
s=bpy.context.scene;source=bpy.data.objects['KEEPER_RIG'];ad=source.animation_data
for t in ad.nla_tracks:t.mute=True
clips=['Reveal','Walk','Sneak','Chase','RunChase','WallCrawl','CeilingCrawl','SearchUnder','SearchOver','JumpscareDeath']
rest={b.name:b.matrix_local.copy() for b in source.data.bones}
# Sample authored poses and facial controls before modifying export copies.
sampled={}
for name in clips:
 a=bpy.data.actions['KEEPER_'+name];ad.action=a;ad.action_slot=a.slots[0]
 frames=[]
 for f in range(1,int(a.frame_range[1])+1):
  s.frame_set(f);bpy.context.view_layer.update();inv=(source.pose.bones['root'].matrix@rest['root'].inverted()).inverted()
  frames.append(({b.name:inv@b.matrix.copy() for b in source.pose.bones},{k:float(source.get(k,0)) for k in ['rage','bite','blink_L','blink_R']}))
 sampled[name]=frames
ad.action=None;source.data.pose_position='REST'
rig=source.copy();rig.data=source.data.copy();s.collection.objects.link(rig);rig.name='KeeperSkeleton';rig.animation_data_clear();rig.data.pose_position='REST'
for p in rig.pose.bones:
 for c in list(p.constraints):p.constraints.remove(c)
 p.matrix_basis=Matrix.Identity(4)
parts=[]
for ob in list(s.objects):
 if ob.type!='MESH' or not ob.name.startswith('HYBRID |') or ob.hide_render:continue
 dup=ob.copy();dup.data=ob.data.copy();s.collection.objects.link(dup);dup.parent=None;dup.matrix_world=ob.matrix_world.copy();dup.name='EXPORT_'+ob.name
 if dup.data.shape_keys:
  dup.data.shape_keys.animation_data_clear()
  for key in dup.data.shape_keys.key_blocks:key.value=0;key.name=key.name.replace(' ','_')
  # Mesh vertices can retain the last evaluated horror shape; FBX uses these as its base.
  for v,b in zip(dup.data.vertices,dup.data.shape_keys.key_blocks[0].data):v.co=b.co
  dup.data.update()
 bpy.ops.object.select_all(action='DESELECT');dup.select_set(True);bpy.context.view_layer.objects.active=dup
 for mod in list(dup.modifiers):
  if mod.type=='ARMATURE':dup.modifiers.remove(mod)
  elif mod.type=='MASK' and mod.show_viewport:bpy.ops.object.modifier_apply(modifier=mod.name)
  else:dup.modifiers.remove(mod)
 if not dup.data.shape_keys and len(dup.data.vertices)>10000:
  dec=dup.modifiers.new('Game export reduction','DECIMATE');dec.ratio=.28;bpy.ops.object.modifier_apply(modifier=dec.name)
 parts.append(dup)
# Joining retains common facial keys and weights while reducing UE draw sections.
bpy.ops.object.select_all(action='DESELECT')
for o in parts:o.select_set(True)
bpy.context.view_layer.objects.active=next(o for o in parts if 'traced mask' in o.name)
bpy.ops.object.join();mesh=bpy.context.object;mesh.name='SK_Keeper';mesh.animation_data_clear()
if mesh.data.shape_keys:mesh.data.shape_keys.animation_data_clear()
mod=mesh.modifiers.new('Keeper deform rig','ARMATURE');mod.object=rig
mesh.parent=rig;mesh.matrix_parent_inverse=rig.matrix_world.inverted()
# Preserve at most four normalized bone influences, excluding temporary authoring groups.
bone_names=set(rest);groups={g.index:g.name for g in mesh.vertex_groups}
for v in mesh.data.vertices:
 weights=sorted([(g.weight,g.group) for g in v.groups if groups[g.group] in bone_names and g.weight>1e-6],reverse=True)[:4]
 if not weights:weights=[(1.0,mesh.vertex_groups['body'].index)]
 total=sum(w for w,_ in weights)
 for group in list(v.groups):mesh.vertex_groups[group.group].remove([v.index])
 for w,g in weights:mesh.vertex_groups[g].add([v.index],w/total,'REPLACE')
# Portable material approximation, using the existing colour design.
for slot in mesh.material_slots:
 old=slot.material
 if not old:continue
 mat=bpy.data.materials.get('Keeper_'+old.name)
 if not mat:
  mat=bpy.data.materials.new('Keeper_'+old.name);mat.diffuse_color=old.diffuse_color;mat.use_nodes=True
  p=mat.node_tree.nodes.get('Principled BSDF');p.inputs['Base Color'].default_value=old.diffuse_color;p.inputs['Roughness'].default_value=.8
 slot.material=mat
# Stable project scale: about 190 cm hunched host / 270 cm raised legs. Keep mesh and clips identical.
rig.data.pose_position='POSE'
for b in rig.pose.bones:b.matrix=sampled['Reveal'][19][0][b.name]
bpy.context.view_layer.update();dg=bpy.context.evaluated_depsgraph_get();evalob=mesh.evaluated_get(dg);points=[mesh.matrix_world@v.co for v in evalob.data.vertices]
height=max(p.z for p in points)-min(p.z for p in points);export_scale=0.4896857534347332
rig.data.pose_position='REST';s.frame_start=1;s.frame_end=1
for v,b in zip(mesh.data.vertices,mesh.data.shape_keys.key_blocks[0].data):v.co=b.co
mesh.data.update()
bpy.ops.object.select_all(action='DESELECT');mesh.select_set(True);rig.select_set(True);bpy.context.view_layer.objects.active=rig
opts=dict(use_selection=True,object_types={'MESH','ARMATURE'},global_scale=export_scale,apply_scale_options='FBX_SCALE_ALL',axis_forward='-Y',axis_up='Z',add_leaf_bones=False,use_armature_deform_only=False,mesh_smooth_type='FACE',use_mesh_modifiers=False,bake_anim_use_all_actions=False,bake_anim_use_nla_strips=False,bake_anim_simplify_factor=0.0,bake_anim_step=1.0)
bpy.ops.export_scene.fbx(filepath=OUT+'/SK_Keeper.fbx',bake_anim=False,**opts)
# Tiny animation-only morph carrier avoids duplicating the complete mesh in every source FBX.
carrier_data=bpy.data.meshes.new('Facial curve carrier');carrier_data.from_pydata([(0,0,0),(.01,0,0),(0,.01,0)],[],[(0,1,2)])
carrier=bpy.data.objects.new('FacialCurveCarrier',carrier_data);s.collection.objects.link(carrier);carrier.parent=rig;carrier.vertex_groups.new(name='head').add([0,1,2],1,'REPLACE');carrier.modifiers.new('Skin','ARMATURE').object=rig
carrier.shape_key_add(name='Basis')
controls={'FULL_HORROR':'rage','CHOMP':'bite','Blink_L':'blink_L','Blink_R':'blink_R'}
for i,key in enumerate(controls):
 k=carrier.shape_key_add(name=key);k.data[1].co.z+=.001*(i+1)
# Export derivatives preserve the authored body/hand/head animation, with concealed limbs for Host locomotion.
def host_frames(base,squeeze=False):
 result=[]
 for pose,values in sampled[base]:
  pose={k:v.copy() for k,v in pose.items()};values=dict(values);body_delta=pose['body']@rest['body'].inverted()
  # The authored concealed reveal pose supplies folded leg geometry.
  closed=sampled['Reveal'][0][0];closed_body=closed['body']@rest['body'].inverted();delta=body_delta@closed_body.inverted()
  for name in pose:
   if name.startswith(('leg.','socket.')):pose[name]=delta@closed[name]
  if squeeze:
   # Small lower-body compression and a head bow, applied in local body space.
   shift=Matrix.Translation((0,0,-.28))
   for name in pose:
    if name!='root':pose[name]=shift@pose[name]
  values['rage']=.1;result.append((pose,values))
 return result
sampled['Idle']=host_frames('Sneak');sampled['HostWalk']=host_frames('Walk');sampled['Squeeze']=host_frames('Sneak',True)
rig.data.pose_position='POSE';audit=[]
for name,frames in sampled.items():
 rig.animation_data_clear();rig.animation_data_create();a=bpy.data.actions.new('AN_Keeper_'+name);a.use_fake_user=True;slot=a.slots.new(id_type='OBJECT',name=rig.name);rig.animation_data.action=a;rig.animation_data.action_slot=slot
 carrier.data.shape_keys.animation_data_clear()
 for f,(pose,values) in enumerate(frames,1):
  # Set parent-relative matrices directly: no dependency-graph ordering ambiguity.
  for b in rig.pose.bones:
   parent=b.parent
   b.matrix_basis=(rest[b.name].inverted()@rest[parent.name]@pose[parent.name].inverted()@pose[b.name]) if parent else rest[b.name].inverted()@pose[b.name]
   b.rotation_mode='QUATERNION'
   for prop in ['location','rotation_quaternion','scale']:b.keyframe_insert(prop,frame=f,group=b.name)
  for key,control in controls.items():
   k=carrier.data.shape_keys.key_blocks[key];k.value=values[control];k.keyframe_insert('value',frame=f)
 for layer in a.layers:
  for st in layer.strips:
   for bag in st.channelbags:
    for fc in bag.fcurves:
     for k in fc.keyframe_points:k.interpolation='LINEAR'
 s.frame_start=1;s.frame_end=len(frames);s.frame_set(1);bpy.context.view_layer.update()
 bpy.ops.object.select_all(action='DESELECT');rig.select_set(True);carrier.select_set(True);bpy.context.view_layer.objects.active=rig
 bpy.ops.export_scene.fbx(filepath=OUT+'/AN_Keeper_'+name+'.fbx',bake_anim=True,**opts)
 audit.append({'name':name,'frames':len(frames),'fps':24,'seconds':(len(frames)-1)/24,'source_action':'KEEPER_'+({'Idle':'Sneak','HostWalk':'Walk','Squeeze':'Sneak'}.get(name,name)),'root_motion':'removed; gameplay movement controls placement'})
# Save only a separate export preview file. Original hybrid is untouched.
for ob in s.objects:ob.hide_render=ob not in [rig,mesh]
carrier.hide_render=True
rig.animation_data.action=bpy.data.actions['AN_Keeper_HostWalk'];rig.animation_data.action_slot=rig.animation_data.action.slots[0]
s.frame_set(10);bpy.context.view_layer.update()
# The main mesh receives the sampled face curves when reviewing the export.
for key in mesh.data.shape_keys.key_blocks:
 if key.name!='Basis':key.value=sampled['HostWalk'][9][1].get(controls.get(key.name,''),0)
s.render.engine='BLENDER_WORKBENCH';s.display.shading.color_type='MATERIAL';s.display.shading.light='STUDIO';s.display.shading.show_cavity=True;s.render.resolution_x=900;s.render.resolution_y=900;s.render.resolution_percentage=100
cam=bpy.data.objects.new('ExportReviewCamera',bpy.data.cameras.new('ExportReviewCamera'));s.collection.objects.link(cam);cam.location=(7,-13,6);cam.rotation_euler=(Vector((0,0,2.7))-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.type='ORTHO';cam.data.ortho_scale=6.6;s.camera=cam
for marker in s.timeline_markers:marker.camera=None
os.makedirs(ROOT+'/Tools/previews/keeper',exist_ok=True);s.render.filepath=ROOT+'/Tools/previews/keeper/HostWalk.png';bpy.ops.render.render(write_still=True)
report={'source':bpy.data.filepath,'mesh_vertices':len(mesh.data.vertices),'bones':len(rig.data.bones),'morphs':[k.name for k in mesh.data.shape_keys.key_blocks],'export_scale':export_scale,'height_source_units':height,'clips':audit}
open(ROOT+'/Tools/keeper-animation-export.json','w').write(json.dumps(report,indent=2))
bpy.ops.wm.save_as_mainfile(filepath='/home/tyron/Projects/the-keeper/keeper-unreal-export.blend')
result=report
