"""Revision02: the Revision01 seam split, applied to the mesh vertices as well as every shape key.
Revision01 only moved shape-key data; FBX writes mesh.vertices as the base, so its seam stayed welded.
Exports SK_KeeperRevealRevision02.fbx and proves the split survives a fresh FBX import, by vertex index.
"""
import bpy,json,hashlib
from pathlib import Path
R=Path(__file__).resolve().parents[3];BASE=R/'Tallow/SourceAssets/Keeper/RiggedV07';O=R/'Tallow/SourceAssets/Keeper/V07RevealRevision02';(O/'Export').mkdir(parents=True,exist_ok=True)
groups=json.loads((R/'Tallow/SourceAssets/Keeper/V07RevealRevision01/coincident-seam-audit.json').read_text());HALF=.0005
bpy.ops.wm.open_mainfile(filepath=str(BASE/'keeper-v07-export.blend'))
o=next(o for o in bpy.data.objects if o.type=='MESH' and o.data.shape_keys and 'HOOD_OPEN' in o.data.shape_keys.key_blocks)
ks=o.data.shape_keys.key_blocks;assert all((o.data.vertices[i].co-ks['Basis'].data[i].co).length<1e-7 for g in groups for i in g['ids']),'mesh and Basis already differ'
ids=[i for g in groups for i in g['ids']];sign={i:(1 if ks['HOOD_OPEN'].data[i].co.x>ks['Basis'].data[i].co.x else -1) for i in ids}
for i in ids:
    for key in ks:key.data[i].co.x+=sign[i]*HALF
    o.data.vertices[i].co=ks['Basis'].data[i].co
o.data.update()
r=next(x for x in bpy.data.objects if x.type=='ARMATURE');assert len(r.data.bones)==109
bpy.ops.object.select_all(action='DESELECT');r.select_set(True);o.select_set(True);bpy.context.view_layer.objects.active=r
r.animation_data.action=None;o.data.shape_keys.animation_data.action=None;r.data.pose_position='REST'
for key in ks:key.value=0
fbx=O/'Export/SK_KeeperRevealRevision02.fbx'
bpy.ops.export_scene.fbx(filepath=str(fbx),use_selection=True,object_types={'MESH','ARMATURE'},global_scale=100.,apply_scale_options='FBX_SCALE_ALL',axis_forward='-Y',axis_up='Z',add_leaf_bones=False,use_armature_deform_only=True,mesh_smooth_type='FACE',use_mesh_modifiers=False,path_mode='RELATIVE',bake_anim=False)
bpy.ops.wm.save_as_mainfile(filepath=str(O/'keeper-reveal-revision02-export.blend'))
W=o.matrix_world.copy();source={i:(W@ks['Basis'].data[i].co,W@ks['HOOD_OPEN'].data[i].co-W@ks['Basis'].data[i].co) for i in range(len(ks['Basis'].data))}
src_h=max(v[0].z for v in source.values())-min(v[0].z for v in source.values())
# Fresh FBX import keeps control-point order: compare by index, not by nearest position.
bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=str(fbx))
m=next(x for x in bpy.data.objects if x.type=='MESH');mk=m.data.shape_keys.key_blocks;M=m.matrix_world.copy()
wb=[M@v.co for v in mk['Basis'].data];wo=[M@v.co for v in mk['HOOD_OPEN'].data];scale=src_h/(max(v.z for v in wb)-min(v.z for v in wb));zoff=min(v[0].z for v in source.values())-scale*min(v.z for v in wb)
from mathutils import Vector
pos=lambda v:v*scale+Vector((0,0,zoff))
assert len(m.data.vertices)==len(source)
worst_base=max((pos(wb[i])-source[i][0]).length for i in ids)
worst_delta=max(((wo[i]-wb[i])*scale-source[i][1]).length for i in range(len(source)))
gaps=[min((wb[a]-wb[b]).length*scale for a in g['ids'] for b in g['ids'] if sign[a]!=sign[b]) for g in groups]
report={'changed_vertices':len(ids),'seam_gap_m':HALF*2,'fbx_base_matches_split_basis_m':worst_base,'fbx_hood_delta_error_m':worst_delta,
        'min_cross_half_gap_in_fbx_m':min(gaps),'fbx_sha256':hashlib.sha256(fbx.read_bytes()).hexdigest()}
assert worst_base<1e-5 and worst_delta<1e-4 and min(gaps)>HALF,report
(O/'revision-audit.json').write_text(json.dumps(report,indent=2));print('RESULT '+json.dumps(report))
