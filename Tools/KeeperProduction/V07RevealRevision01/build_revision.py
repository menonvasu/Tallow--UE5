import bpy,json,hashlib
from pathlib import Path
R=Path(__file__).resolve().parents[3];BASE=R/'Tallow/SourceAssets/Keeper/RiggedV07';O=R/'Tallow/SourceAssets/Keeper/V07RevealRevision01';(O/'Export').mkdir(exist_ok=True)
groups=json.loads((O/'coincident-seam-audit.json').read_text());report={}
def patch(o):
 ks=o.data.shape_keys.key_blocks;changed=0
 for group in groups:
  for i in group['ids']:
   sign=1 if ks['HOOD_OPEN'].data[i].co.x>ks['Basis'].data[i].co.x else -1
   for key in ks:key.data[i].co.x+=sign*.0005
   changed+=1
 return changed
for name in ['animated','export']:
 bpy.ops.wm.open_mainfile(filepath=str(BASE/f'keeper-v07-{name}.blend'))
 o=next(o for o in bpy.data.objects if o.type=='MESH' and o.data.shape_keys and 'HOOD_OPEN' in o.data.shape_keys.key_blocks)
 report[name]={'changed_vertices':patch(o),'max_displacement_m':.0005,'topology_unchanged':True}
 bpy.ops.wm.save_as_mainfile(filepath=str(O/f'keeper-reveal-revision01-{name}.blend'))
# The existing clean DEF skeleton, rest matrices and baked actions are reused verbatim.
r=next(o for o in bpy.data.objects if o.type=='ARMATURE');s=bpy.context.scene
assert len(r.data.bones)==109
bpy.ops.object.select_all(action='DESELECT');r.select_set(True);o.select_set(True);bpy.context.view_layer.objects.active=r
opts=dict(use_selection=True,object_types={'MESH','ARMATURE'},global_scale=100.,apply_scale_options='FBX_SCALE_ALL',axis_forward='-Y',axis_up='Z',add_leaf_bones=False,use_armature_deform_only=True,mesh_smooth_type='FACE',use_mesh_modifiers=False,path_mode='RELATIVE')
r.animation_data.action=None;o.data.shape_keys.animation_data.action=None;r.data.pose_position='REST'
for key in o.data.shape_keys.key_blocks:key.value=0
bpy.ops.export_scene.fbx(filepath=str(O/'Export/SK_KeeperRevealRevision01.fbx'),bake_anim=False,**opts)
r.data.pose_position='POSE'
for obj,actname in [(r,'EXPORT_AN_Keeper_JumpscareDeath'),(o.data.shape_keys,'EXPORT_MORPH_JumpscareDeath')]:
 act=bpy.data.actions[actname];obj.animation_data.action=act;obj.animation_data.action_slot=act.slots[0]
s.frame_start=1;s.frame_end=73;s.frame_set(1)
bpy.ops.export_scene.fbx(filepath=str(O/'Export/AN_Keeper_JumpscareDeath_Revision01.fbx'),bake_anim=True,bake_anim_use_all_actions=False,bake_anim_use_nla_strips=False,bake_anim_step=1,bake_anim_simplify_factor=0,**opts)
report.update({'seam_width_m':.001,'why':'Opposing HOOD_OPEN morph deltas shared exact Basis positions. Real1mmclosedseam prevents coordinate welding across independent halves.','bone_count':len(r.data.bones),'animation_changes':False,'UE_verification':'pending root import/render','fbx':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in (O/'Export').glob('*.fbx')}})
(O/'revision-audit.json').write_text(json.dumps(report,indent=2));print('RESULT '+json.dumps(report))
