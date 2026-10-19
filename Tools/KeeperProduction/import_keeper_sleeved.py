"""Opt-in UE5.8 staging import only. Does not touch existing Keeper, actors, maps or Blueprints.
Run in Unreal Python after reviewing export-audit.json and source renders.
Legacy FBX is forced per import and the previous Interchange FBX CVar restored.
Measured contract: this FBX imports at 2.4 cm with legacy scale1; scale100
produces the intended 240 cm character. Mesh and all clips use scale100.
"""
from pathlib import Path
import unreal as u
ROOT=Path(__file__).resolve().parents[2]
SOURCE=ROOT/'Tallow/SourceAssets/Keeper/SleevedV06'
DEST='/Game/Presentation/KeeperGeneratedV06'
if u.EditorAssetLibrary.does_directory_exist(DEST):
    raise RuntimeError('Staging destination already exists; inspect it instead of overwriting: '+DEST)
required=[SOURCE/'Export/SK_Keeper_Tripo_Direct.fbx']+[SOURCE/'Textures'/('T_KeeperTripo_'+s+'.png') for s in ['BaseColor','Roughness','Normal','Metallic']]
if not all(p.is_file() for p in required):raise RuntimeError('Curated V06 source files missing; refusing partial import')
if len(list((SOURCE/'Export').glob('AN_Keeper_*.fbx')))!=13:raise RuntimeError('Expected all 13 V06 animation exports before import')
assets=u.AssetToolsHelpers.get_asset_tools()

def run_import(file,name,options=None):
    task=u.AssetImportTask();task.filename=str(file);task.destination_path=DEST;task.destination_name=name;task.automated=True;task.save=True;task.replace_existing=False
    if options:
        task.options=options;task.factory=u.FbxFactory()
    fbx_flag = 'Interchange.FeatureFlags.Import.FBX'
    previous_fbx_flag = u.SystemLibrary.get_console_variable_int_value(fbx_flag)
    try:
        u.SystemLibrary.execute_console_command(None, fbx_flag + ' 0')
        assert u.SystemLibrary.get_console_variable_int_value(fbx_flag) == 0, 'Could not select legacy FBX importer'
        assets.import_asset_tasks([task])
    finally:
        u.SystemLibrary.execute_console_command(None, fbx_flag + ' ' + str(previous_fbx_flag))
    result=u.load_asset(DEST+'/'+name)
    if not result:raise RuntimeError('Import failed: '+str(file))
    return result

options=u.FbxImportUI();options.automated_import_should_detect_type=False;options.import_mesh=True;options.import_as_skeletal=True;options.import_animations=False;options.import_materials=False;options.import_textures=False;options.mesh_type_to_import=u.FBXImportType.FBXIT_SKELETAL_MESH
options.create_physics_asset=False # Isolated visual presentation; gameplay collision is unvalidated.
options.skeletal_mesh_import_data.set_editor_property('import_morph_targets',True)
options.skeletal_mesh_import_data.set_editor_property('import_uniform_scale',100.0)
mesh=run_import(SOURCE/'Export/SK_Keeper_Tripo_Direct.fbx','SK_Keeper_Tripo_Direct',options)
height_cm=float(mesh.get_imported_bounds().box_extent.z)*2.0
u.log('Keeper fresh import height cm: '+str(height_cm))
assert 220.0<=height_cm<=260.0, 'Keeper import scale not honored; stop before clip import: '+str(height_cm)
skeleton=mesh.get_editor_property('skeleton')
physics=mesh.get_editor_property('physics_asset')
if physics is None:
    mesh.set_editor_property('physics_asset',None)
    u.log_warning('Presentation-only Keeper has no physics asset; gameplay collision/ragdoll unvalidated.')
for dependency in [skeleton]+([physics] if physics else []):
    if dependency is None or not u.EditorAssetLibrary.save_loaded_asset(dependency,False):
        raise RuntimeError('Missing/unsaved Keeper import dependency: '+str(dependency))
for f in sorted((SOURCE/'Export').glob('AN_Keeper_*.fbx')):
    options=u.FbxImportUI();options.automated_import_should_detect_type=False;options.import_mesh=False;options.import_as_skeletal=True;options.import_animations=True;options.import_materials=False;options.import_textures=False;options.mesh_type_to_import=u.FBXImportType.FBXIT_ANIMATION;options.skeleton=skeleton
    options.anim_sequence_import_data.set_editor_property('import_uniform_scale',100.0)
    run_import(f,f.stem,options)
textures={}
for suffix in ['BaseColor','Roughness','Normal','Metallic']:
    tex=run_import(SOURCE/'Textures'/('T_KeeperTripo_'+suffix+'.png'),'T_KeeperTripo_'+suffix)
    tex.set_editor_property('srgb',suffix=='BaseColor')
    if suffix=='Normal':
        tex.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property('flip_green_channel',True) # Blender tangent normal is OpenGL +Y.
    elif suffix in ['Roughness','Metallic']:tex.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_MASKS)
    u.EditorAssetLibrary.save_loaded_asset(tex);textures[suffix]=tex
mat=assets.create_asset('M_KeeperTripo_Production',DEST,u.Material,u.MaterialFactoryNew())
mat.set_editor_property('two_sided',True)
mat.set_editor_property('used_with_skeletal_mesh',True)
edit=u.MaterialEditingLibrary
for row,(suffix,prop) in enumerate([('BaseColor',u.MaterialProperty.MP_BASE_COLOR),('Roughness',u.MaterialProperty.MP_ROUGHNESS),('Normal',u.MaterialProperty.MP_NORMAL),('Metallic',u.MaterialProperty.MP_METALLIC)]):
    node=edit.create_material_expression(mat,u.MaterialExpressionTextureSample,-400,row*200);node.texture=textures[suffix]
    if suffix=='Normal':node.sampler_type=u.MaterialSamplerType.SAMPLERTYPE_NORMAL
    elif suffix in ['Roughness','Metallic']:node.sampler_type=u.MaterialSamplerType.SAMPLERTYPE_MASKS
    edit.connect_material_property(node,'RGB' if suffix not in ['Roughness','Metallic'] else 'R',prop)
edit.recompile_material(mat)
slots=mesh.get_editor_property('materials')
for i in range(len(slots)):
    slot=slots[i]
    slot.set_editor_property('material_interface',mat)
    slots[i]=slot
mesh.set_editor_property('materials',slots)
u.EditorAssetLibrary.save_loaded_asset(mat);u.EditorAssetLibrary.save_loaded_asset(mesh)
saved_mesh=u.load_asset(mesh.get_path_name())
material_paths=[]
for slot in saved_mesh.get_editor_property('materials'):
    assigned=slot.get_editor_property('material_interface')
    material_paths.append(assigned.get_path_name() if assigned else None)
if not material_paths or any(path!=mat.get_path_name() for path in material_paths):
    raise RuntimeError('Keeper material assignment failed: '+repr(material_paths))
u.log('Keeper saved mesh material paths: '+repr(material_paths))
u.log('Keeper SleevedV06 staged. Mouth/centre-opening hood is explicitly deferred issue44. No actor/Blueprint swap. Verify bounds, all 13 sequences and morph curves in render editor before integration.')

# AssetImportTask.save may omit the auxiliary packages created by the FBX factory.
for dependency in [skeleton]+([physics] if physics else []):
    assert u.EditorAssetLibrary.save_loaded_asset(dependency,False)
assert u.EditorAssetLibrary.save_directory(DEST,False,True)
for asset in [mesh,skeleton]+([physics] if physics else [])+[u.load_asset(DEST+'/'+f.stem) for f in sorted((SOURCE/'Export').glob('AN_Keeper_*.fbx'))]:
    assert asset, 'Missing Keeper imported asset'
    package=asset.get_path_name().split('.')[0]
    assert package.startswith(DEST+'/'), 'Unexpected external Keeper dependency'
    assert (Path(u.Paths.project_content_dir())/(package[len('/Game/'):]+'.uasset')).is_file(), 'Missing package on disk: '+package
    if isinstance(asset,u.AnimSequence):
        bound=asset.get_editor_property('skeleton')
        assert bound and bound.get_path_name()==skeleton.get_path_name(), 'Clip skeleton mismatch: '+package
u.log('Keeper skeleton and available physics packages explicitly saved; null physics is presentation-only; fresh-process reload audit still required.')
