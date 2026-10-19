"""Restore existing V06 mantle morph from immutable FBX, keeping measured legacy scale100.
Only the staged mesh is reimported. No source geometry, animation, actor or map edits.
"""
from pathlib import Path
import json
import unreal as u

ROOT = Path(__file__).resolve().parents[2]
DEST = '/Game/Presentation/KeeperGeneratedV06'
SOURCE = ROOT / 'Tallow/SourceAssets/Keeper/SleevedV06/Export'
E = u.EditorAssetLibrary
REPORT = ROOT / 'Tallow/Saved/Presentation/keeper-morph-repair.json'
REPORT.parent.mkdir(parents=True, exist_ok=True)


def save(asset):
    assert asset and asset.get_path_name().startswith(DEST + '/')
    assert E.save_loaded_asset(asset, False)
    package = asset.get_path_name().split('.')[0]
    assert (Path(u.Paths.project_content_dir()) / (package[len('/Game/'):] + '.uasset')).is_file()


def inspect(mesh):
    data = mesh.get_editor_property('asset_import_data')
    assert isinstance(data, u.FbxSkeletalMeshImportData), 'Requires existing legacy skeletal import metadata'
    return {'height_cm': float(mesh.get_imported_bounds().box_extent.z) * 2,
            'import_scale': float(data.get_editor_property('import_uniform_scale')),
            'import_morph_targets': data.get_editor_property('import_morph_targets'),
            'morphs': [target.get_name() for target in mesh.get_editor_property('morph_targets')]}


def main():
    mesh = u.load_asset(DEST + '/SK_Keeper_Tripo_Direct')
    assert mesh
    before = inspect(mesh)
    result = {'before': before, 'status': 'audited before mutation'}
    REPORT.write_text(json.dumps(result, indent=2) + '\n')
    u.log('Keeper morph before: ' + json.dumps(before))
    assert 220 <= before['height_cm'] <= 260 and abs(before['import_scale'] - 100) < .001
    skeleton = mesh.get_editor_property('skeleton')
    material = u.load_asset(DEST + '/M_KeeperTripo_Production')
    assert skeleton and skeleton.get_path_name().startswith(DEST + '/') and material
    files = sorted(SOURCE.glob('AN_Keeper_*.fbx'))
    assert len(files) == 13
    clips = [u.load_asset(DEST + '/' + source.stem) for source in files]
    for clip in clips:
        assert clip and clip.get_editor_property('skeleton') == skeleton, 'Pre-repair clip skeleton mismatch'
    if 'full_horror' not in [name.lower() for name in before['morphs']]:
        data = mesh.get_editor_property('asset_import_data')
        data.set_editor_property('import_morph_targets', True)
        data.set_editor_property('import_uniform_scale', 100.0)
        data.set_editor_property('update_skeleton_reference_pose', True)
        save(mesh)
        options = u.FbxImportUI()
        options.automated_import_should_detect_type = False
        options.import_mesh = True
        options.import_as_skeletal = True
        options.import_animations = False
        options.import_materials = False
        options.import_textures = False
        options.create_physics_asset = False
        options.skeleton = skeleton
        options.mesh_type_to_import = u.FBXImportType.FBXIT_SKELETAL_MESH
        options.skeletal_mesh_import_data.set_editor_property('import_uniform_scale', 100.0)
        options.skeletal_mesh_import_data.set_editor_property('import_morph_targets', True)
        options.skeletal_mesh_import_data.set_editor_property('update_skeleton_reference_pose', True)
        task = u.AssetImportTask()
        task.filename = str(SOURCE / 'SK_Keeper_Tripo_Direct.fbx')
        task.destination_path = DEST
        task.destination_name = 'SK_Keeper_Tripo_Direct'
        task.automated = True
        task.replace_existing = True
        task.replace_existing_settings = True
        task.save = True
        task.options = options
        task.factory = u.FbxFactory()
        flag = 'Interchange.FeatureFlags.Import.FBX'
        previous = u.SystemLibrary.get_console_variable_int_value(flag)
        try:
            u.SystemLibrary.execute_console_command(None, flag + ' 0')
            assert u.SystemLibrary.get_console_variable_int_value(flag) == 0
            u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        finally:
            u.SystemLibrary.execute_console_command(None, flag + ' ' + str(previous))
        mesh = u.load_asset(DEST + '/SK_Keeper_Tripo_Direct')
    mesh.get_editor_property('asset_import_data').set_editor_property('import_morph_targets', True)
    slots = mesh.get_editor_property('materials')
    assert slots
    for i in range(len(slots)):
        slot = slots[i]
        slot.set_editor_property('material_interface', material)
        slots[i] = slot
    mesh.set_editor_property('materials', slots)
    mesh.set_editor_property('physics_asset', None)
    save(skeleton)
    save(mesh)
    assert E.save_directory(DEST, False, True)
    after = inspect(mesh)
    result.update(after=after, status='Reimport completed; validating morph, bounds and clip references')
    REPORT.write_text(json.dumps(result, indent=2) + '\n')
    assert 'full_horror' in [name.lower() for name in after['morphs']], 'FULL_HORROR still absent: ' + json.dumps(after)
    assert 220 <= after['height_cm'] <= 260 and abs(after['import_scale'] - 100) < .001
    assert mesh.get_editor_property('skeleton') == skeleton
    for clip in clips:
        assert clip.get_editor_property('skeleton') == skeleton, 'Post-repair clip skeleton mismatch: ' + clip.get_name()
    assert all(slot.get_editor_property('material_interface') == material for slot in mesh.get_editor_property('materials'))
    result.update(status='FULL_HORROR and 13 clip references verified; fresh-process pose rendering still required', clip_skeleton_count=13)
    REPORT.write_text(json.dumps(result, indent=2) + '\n')
    u.log('Keeper morph repair: ' + json.dumps(result))


main()
