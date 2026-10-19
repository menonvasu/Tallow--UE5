"""Construct new animator-facing V07 rig over unchanged V06 UV/weighted surface."""
import bpy,json,math
from pathlib import Path
from mathutils import Vector,Matrix
ROOT=Path(__file__).resolve().parents[3];SRC=ROOT/'Tallow/SourceAssets/Keeper/SleevedV06';OUT=ROOT/'Tallow/SourceAssets/Keeper/RiggedV07';OUT.mkdir(exist_ok=True)
bpy.ops.wm.open_mainfile(filepath=str(SRC/'keeper-direct-delivery.blend'))
o=bpy.data.objects['SK_Keeper_Tripo_Direct'];old=bpy.data.objects['KEEPER_PRODUCTION_RIG']
source=json.loads((SRC/'production-audit.json').read_text());arms=source['arms']
old.animation_data_clear();o.data.shape_keys.animation_data_clear()
for k in o.data.shape_keys.key_blocks:k.value=0
old.data.pose_position='REST';bpy.context.view_layer.update()
layout=[dict(name=b.name,head=b.head_local.copy(),tail=b.tail_local.copy(),matrix=b.matrix_local.copy(),parent=b.parent.name if b.parent else None) for b in old.data.bones if b.use_deform]
for ob in list(bpy.data.objects):
 if ob not in [o,old]:bpy.data.objects.remove(ob,do_unlink=True)
for act in list(bpy.data.actions):bpy.data.actions.remove(act)
arm=bpy.data.armatures.new('KeeperV07_ControlSkeleton');rig=bpy.data.objects.new('RIG_KeeperV07',arm);bpy.context.scene.collection.objects.link(rig)
rig.show_in_front=False;rig.display_type='WIRE';arm.display_type='STICK'
for vg in o.vertex_groups:
 if vg.name not in ['root','head']:vg.name='DEF_'+vg.name
for vertex in o.data.vertices:
 total=sum(g.weight for g in vertex.groups)
 if total:
  for g in list(vertex.groups):o.vertex_groups[g.group].add([vertex.index],g.weight/total,'REPLACE')
o.name='SK_KeeperRiggedV07'
bpy.ops.object.select_all(action='DESELECT');rig.select_set(True);bpy.context.view_layer.objects.active=rig;bpy.ops.object.mode_set(mode='EDIT')
def dname(n):return n if n in ['root','head'] else 'DEF_'+n
def add(n,h,t,parent=None,deform=False,matrix=None):
 b=arm.edit_bones.new(n);b.head=h;b.tail=t
 if matrix is not None:b.matrix=matrix;b.length=(Vector(t)-Vector(h)).length
 b.use_deform=deform
 if parent:b.parent=arm.edit_bones[parent]
 return b
for row in layout:add(dname(row['name']),row['head'],row['tail'],dname(row['parent']) if row['parent'] else None,True,row['matrix'])
add('CTRL_root',(0,0,0),(0,0,.25))
for n,parent in [('body','CTRL_root'),('head','CTRL_body')]+[(f'cloak_{i}','CTRL_body') for i in range(3)]:
 row=next(r for r in layout if r['name']==n);add('CTRL_'+n,row['head'],row['tail'],parent,False,row['matrix'])
for a in arms:
 for key in ['upper','lower']:
  n=a[key];row=next(r for r in layout if r['name']==n);parent='DEF_body' if key=='upper' else 'MCH_'+a['upper'];add('MCH_'+n,row['head'],row['tail'],parent,False,row['matrix'])
 row=next(r for r in layout if r['name']==a['hand']);add('CTRL_'+a['hand'],row['head'],row['tail'],'CTRL_root',False,row['matrix'])
 sign=1 if a['side']=='L' else -1;add('CTRL_POLE_'+a['hand'],(sign*.8,-.55,1.4),(sign*.8,-.55,1.52),'CTRL_root')
bpy.ops.object.mode_set(mode='OBJECT')
for mod in o.modifiers:
 if mod.type=='ARMATURE':mod.object=rig
if o.parent==old:o.parent=rig
bpy.data.objects.remove(old,do_unlink=True)
for p in rig.pose.bones:p.rotation_mode='QUATERNION'
for n in ['root','body','head']+[f'cloak_{i}' for i in range(3)]:
 p=rig.pose.bones[dname(n)];c=p.constraints.new('COPY_TRANSFORMS');c.target=rig;c.subtarget='CTRL_'+n
for a in arms:
 for key in ['upper','lower']:
  c=rig.pose.bones['DEF_'+a[key]].constraints.new('COPY_TRANSFORMS');c.target=rig;c.subtarget='MCH_'+a[key]
 c=rig.pose.bones['MCH_'+a['lower']].constraints.new('IK');c.name='SixArm_IK';c.target=rig;c.subtarget='CTRL_'+a['hand'];c.pole_target=rig;c.pole_subtarget='CTRL_POLE_'+a['hand'];c.chain_count=2;c.use_stretch=False
 for n in [a['upper'],a['lower']]:rig.pose.bones['MCH_'+n].ik_stretch=0
 best=(1e9,0)
 for step in range(144):
  c.pole_angle=step*2*math.pi/144;bpy.context.view_layer.update();err=(rig.pose.bones['MCH_'+a['upper']].tail-Vector(a['elbow'])).length
  if err<best[0]:best=(err,c.pole_angle)
 c.pole_angle=best[1]
 control=rig.pose.bones['CTRL_'+a['hand']];control['elbow_roll']=best[1];control.id_properties_ui('elbow_roll').update(min=-math.pi*2,max=math.pi*2,description='Elbow fold plane; pole control provides direction')
 fc=c.driver_add('pole_angle');drv=fc.driver;drv.expression='roll';v=drv.variables.new();v.name='roll';v.targets[0].id=rig;v.targets[0].data_path=control.path_from_id()+'["elbow_roll"]'
 # Hand location comes from solved wrist; independent world orientation from animator hand control.
 hand=rig.pose.bones['DEF_'+a['hand']];cl=hand.constraints.new('COPY_ROTATION');cl.target=rig;cl.subtarget=control.name;cl.owner_space='WORLD';cl.target_space='WORLD'
 for digit,label in enumerate(['thumb','index','middle','ring','little'],1):
  control[label+'_curl']=0.0;control.id_properties_ui(label+'_curl').update(min=-.25,max=1.0,description='Independent '+label+' curl; 0 open, 1 clasp')
  for p in rig.pose.bones:
   if p.name.startswith('DEF_'+a['hand'].replace('_hand',f'_digit{digit}_')):
    p.rotation_mode='XYZ';fc=p.driver_add('rotation_euler',0);drv=fc.driver;drv.expression='curl*0.8';v=drv.variables.new();v.name='curl';v.targets[0].id=rig;v.targets[0].data_path=control.path_from_id()+f'["{label}_curl"]'
# Shape parameters live on body/head controls; export bake will retain curves.
for prop,key,owner in [('mantle_gather','FULL_HORROR','CTRL_body')]:
 p=rig.pose.bones[owner];p[prop]=0.;p.id_properties_ui(prop).update(min=0,max=1,description='Gather cloth before lowering into crawl')
 fc=o.data.shape_keys.key_blocks[key].driver_add('value');drv=fc.driver;drv.expression='value';v=drv.variables.new();v.name='value';v.targets[0].id=rig;v.targets[0].data_path=p.path_from_id()+f'["{prop}"]'
# Custom widgets kept outside exported mesh collection.
widgets=bpy.data.collections.new('WIDGETS_KeeperV07');bpy.context.scene.collection.children.link(widgets);widgets.hide_render=True
for typ,radius,points in [('ring',1,32),('diamond',1,4),('square',1,4)]:
 verts=[(radius*math.cos(i*2*math.pi/points),0,radius*math.sin(i*2*math.pi/points)) for i in range(points)]
 me=bpy.data.meshes.new('WGT_'+typ);me.from_pydata(verts,[(i,(i+1)%points) for i in range(points)],[]);ob=bpy.data.objects.new('WGT_'+typ,me);widgets.objects.link(ob);ob.hide_render=True;ob.hide_set(True)
controls=arm.collections.new('ANIMATE • Controls');deforms=arm.collections.new('EXPORT • Deform');mechanism=arm.collections.new('INTERNAL • Solvers');deforms.is_visible=False;mechanism.is_visible=False
for b in arm.bones:
 if b.name.startswith('CTRL_'):
  controls.assign(b);p=rig.pose.bones[b.name];typ='diamond' if 'POLE' in b.name else 'ring';p.custom_shape=bpy.data.objects['WGT_'+typ];p.use_custom_shape_bone_size=False;p.custom_shape_scale_xyz=(.07,.07,.07) if 'hand' in b.name else (.25,.25,.25);b.color.palette='THEME04' if '.L' in b.name else ('THEME03' if '.R' in b.name else 'THEME02')
 elif b.name.startswith('MCH_'):mechanism.assign(b)
 else:deforms.assign(b)
rig['rig_version']='V07 newly constructed CTRL/MCH/DEF';rig['usage']='Pose CTRL bones only. Hand custom properties curl five fingers independently. Body mantle_gather lifts cloak. Pole/elbow_roll controls folding.'
scene=bpy.context.scene;scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=1;scene.render.fps=24
weights=[sum(g.weight>1e-6 for g in v.groups) for v in o.data.vertices];sums=[sum(g.weight for g in v.groups) for v in o.data.vertices]
result={'new_control_bones':sum(b.name.startswith('CTRL') for b in arm.bones),'deform_bones':sum(b.use_deform for b in arm.bones),'mechanism_bones':sum(b.name.startswith('MCH') for b in arm.bones),'max_influences':max(weights),'unweighted':sum(w==0 for w in weights),'max_weight_sum_error':max(abs(s-1) for s in sums),'retained_surface_vertices':len(o.data.vertices),'uv_names':[uv.name for uv in o.data.uv_layers],'controls':rig['usage'],'arm_landmarks':arms}
assert result['max_influences']<=4 and result['unweighted']==0
(OUT/'rig-audit.json').write_text(json.dumps(result,indent=2));bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'keeper-v07-control-rig.blend'));print('RESULT '+json.dumps({k:v for k,v in result.items() if k!='arm_landmarks'}))
