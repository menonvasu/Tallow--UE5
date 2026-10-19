"""Repair missing V06 dependencies only; no actor/Blueprint/map changes.
Run once in a fresh editor after the broken import. Run again with
runpy.run_path(path, init_globals={'AUDIT_ONLY': True}) after restarting to verify.
Normal reruns refuse to reimport once dependency persistence is healthy.
This presentation-only namespace intentionally has no physics asset when FBX
reimport omits it. UE5.8 PhysicsAssetFactory opens a modal generation dialog;
collision/ragdoll integration remains unvalidated and outside this repair.
Legacy FBX is forced per import and the previous Interchange FBX CVar restored.
Measured contract: this FBX imports at 2.4 cm with legacy scale1; scale100
produces the intended 240 cm character. Mesh and all clips use scale100.
"""
from pathlib import Path
import json
import unreal as u

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / 'Tallow/SourceAssets/Keeper/SleevedV06/Export'
DEST = '/Game/Presentation/KeeperGeneratedV06'
MESH_PATH = DEST + '/SK_Keeper_Tripo_Direct'
E = u.EditorAssetLibrary


def disk_path(asset):
    package = asset.get_path_name().split('.')[0]
    if not package.startswith(DEST + '/'):
        raise RuntimeError('Refusing asset outside V06 staging: ' + package)
    return Path(u.Paths.project_content_dir()) / (package[len('/Game/'):] + '.uasset')


def persisted(asset):
    return asset is not None and disk_path(asset).is_file()


def save(asset):
    if asset is None or not E.save_loaded_asset(asset, False):
        raise RuntimeError('Could not save Keeper dependency: ' + str(asset))
    if not persisted(asset):
        raise RuntimeError('Saved Keeper package absent on disk: ' + str(asset))


def update_stored_import_scale(asset, scale, is_mesh):
    """Reimport reads persisted metadata, overriding the task's FbxImportUI."""
    assert asset and asset.get_path_name().startswith(DEST + '/'), 'Refusing external asset metadata'
    data = asset.get_editor_property('asset_import_data')
    assert data, 'Missing stored import metadata; refusing blank replacement'
    u.log('Keeper stored metadata: ' + asset.get_path_name() + ' class=' + data.get_class().get_name())
    if isinstance(data, u.FbxAssetImportData):
        previous = data.get_editor_property('import_uniform_scale')
        u.log('Keeper stored FBX scale before=' + str(previous) + ' requested=' + str(scale))
        data.set_editor_property('import_uniform_scale', scale)
        assert abs(data.get_editor_property('import_uniform_scale') - scale) < .000001
        if is_mesh:
            assert isinstance(data, u.FbxSkeletalMeshImportData), 'Unexpected mesh FBX metadata class'
            u.log('Keeper reference pose update before=' + str(data.get_editor_property('update_skeleton_reference_pose')))
            data.set_editor_property('import_morph_targets', True)
            data.set_editor_property('update_skeleton_reference_pose', True)
    elif isinstance(data, u.InterchangeAssetImportData):
        pipelines = data.get_pipelines()
        found = 0
        for pipeline in pipelines:
            if isinstance(pipeline, u.InterchangeGenericAssetsPipeline):
                found += 1
                previous = pipeline.get_editor_property('import_offset_uniform_scale')
                u.log('Keeper stored Interchange offset scale before=' + str(previous) + ' requested=' + str(scale))
                pipeline.set_editor_property('import_offset_uniform_scale', scale)
                assert abs(pipeline.get_editor_property('import_offset_uniform_scale') - scale) < .000001
                if is_mesh:
                    mesh_pipeline = pipeline.get_editor_property('mesh_pipeline')
                    assert mesh_pipeline, 'Missing stored mesh pipeline'
                    u.log('Keeper pipeline reference pose update before=' + str(mesh_pipeline.get_editor_property('update_skeleton_reference_pose')))
                    mesh_pipeline.set_editor_property('import_morph_targets', True)
                    mesh_pipeline.set_editor_property('update_skeleton_reference_pose', True)
        assert found == 1, 'Expected exactly one generic assets pipeline; refusing ambiguous metadata'
        data.set_pipelines(pipelines)  # Reserialize CachedPipelines before built-in legacy conversion.
    else:
        raise RuntimeError('Unsupported stored import metadata: ' + data.get_class().get_name())
    save(asset)


def audit():
    mesh = u.load_asset(MESH_PATH)
    if not mesh:
        raise RuntimeError('Keeper staged mesh missing')
    skeleton = mesh.get_editor_property('skeleton')
    physics = mesh.get_editor_property('physics_asset')
    assert persisted(mesh) and persisted(skeleton), 'Keeper dependency missing on disk'
    assert physics is None or persisted(physics), 'Dangling physics asset reference'
    files = sorted(SOURCE.glob('AN_Keeper_*.fbx'))
    assert len(files) == 13, 'Expected 13 source clips'
    clips = {}
    for source in files:
        clip = u.load_asset(DEST + '/' + source.stem)
        assert persisted(clip), 'Missing saved clip: ' + source.stem
        bound = clip.get_editor_property('skeleton')
        assert bound and bound.get_path_name() == skeleton.get_path_name(), 'Wrong/null skeleton: ' + source.stem
        clips[source.stem] = bound.get_path_name()
    mat = u.load_asset(DEST + '/M_KeeperTripo_Production')
    slots = mesh.get_editor_property('materials')
    assert mat and slots and all(slot.get_editor_property('material_interface') == mat for slot in slots), 'Keeper material assignment lost'
    result = {'mesh': mesh.get_path_name(), 'skeleton': skeleton.get_path_name(),
              'physics_asset': physics.get_path_name() if physics else None, 'clips': clips,
              'physics_status': 'Presentation only; no gameplay collision/ragdoll validation. Null physics intentionally permitted.',
              'fresh_process_required': 'Run AUDIT_ONLY in a new editor process to verify reload; this run verifies disk files and current references.'}
    report = ROOT / 'Tallow/Saved/Presentation/keeper-dependency-audit.json'
    report.parent.mkdir(parents=True, exist_ok=True)
    report.write_text(json.dumps(result, indent=2) + '\n')
    u.log('Keeper V06 dependencies and all 13 clip skeletons verified: ' + str(report))


def repair():
    mesh = u.load_asset(MESH_PATH)
    if not mesh:
        raise RuntimeError('Repair requires existing staged V06 mesh')
    physics = mesh.get_editor_property('physics_asset')
    if persisted(mesh.get_editor_property('skeleton')) and (physics is None or persisted(physics)) and E.get_metadata_tag(mesh, 'TallowDependencyRepair') == 'visual-only-v1':
        raise RuntimeError('Dependency loss already resolved; refusing redundant reimport. For unit/clip scale repair use repair_keeper_scale.py; run AUDIT_ONLY after restart.')
    material = u.load_asset(DEST + '/M_KeeperTripo_Production')
    files = sorted(SOURCE.glob('AN_Keeper_*.fbx'))
    assert material and (SOURCE / 'SK_Keeper_Tripo_Direct.fbx').is_file() and len(files) == 13
    assets = u.AssetToolsHelpers.get_asset_tools()

    def import_file(source, options):
        existing = u.load_asset(DEST + "/" + source.stem)
        if existing:
            update_stored_import_scale(existing, 100.0, isinstance(existing, u.SkeletalMesh))
        task = u.AssetImportTask()
        task.filename = str(source)
        task.destination_path = DEST
        task.destination_name = source.stem
        task.automated = True
        task.replace_existing = True
        task.replace_existing_settings = True
        task.save = True
        task.options = options
        task.factory = u.FbxFactory()
        fbx_flag = 'Interchange.FeatureFlags.Import.FBX'
        previous_fbx_flag = u.SystemLibrary.get_console_variable_int_value(fbx_flag)
        try:
            u.SystemLibrary.execute_console_command(None, fbx_flag + ' 0')
            assert u.SystemLibrary.get_console_variable_int_value(fbx_flag) == 0, 'Could not select legacy FBX importer'
            assets.import_asset_tasks([task])
        finally:
            u.SystemLibrary.execute_console_command(None, fbx_flag + ' ' + str(previous_fbx_flag))
        result = u.load_asset(DEST + '/' + source.stem)
        assert result, 'Import failed: ' + str(source)
        return result

    opts = u.FbxImportUI()
    opts.automated_import_should_detect_type = False
    opts.import_mesh = True
    opts.import_as_skeletal = True
    opts.import_animations = False
    opts.import_materials = False
    opts.import_textures = False
    opts.create_physics_asset = False
    existing_skeleton = mesh.get_editor_property('skeleton')
    if existing_skeleton:
        assert existing_skeleton.get_path_name().startswith(DEST + '/'), 'Refusing external skeleton'
        opts.skeleton = existing_skeleton
    opts.skeletal_mesh_import_data.set_editor_property('update_skeleton_reference_pose', True)
    opts.mesh_type_to_import = u.FBXImportType.FBXIT_SKELETAL_MESH
    opts.skeletal_mesh_import_data.set_editor_property('import_morph_targets', True)
    opts.skeletal_mesh_import_data.set_editor_property('import_uniform_scale', 100.0)
    mesh = import_file(SOURCE / 'SK_Keeper_Tripo_Direct.fbx', opts)
    height_cm = float(mesh.get_imported_bounds().box_extent.z) * 2.0
    u.log('Keeper dependency repair imported height cm: ' + str(height_cm))
    assert 220.0 <= height_cm <= 260.0, 'Keeper import scale not honored; stop before clips: ' + str(height_cm)
    skeleton = mesh.get_editor_property('skeleton')
    save(skeleton)
    physics = mesh.get_editor_property('physics_asset')
    if physics is not None:
        save(physics)
    else:
        # PhysicsAssetFactory always opens OpenNewBodyDlg in UE5.8.
        # Clear dangling reference; never manufacture collision proof for staging.
        mesh.set_editor_property('physics_asset', None)
        u.log_warning('Keeper V06 presentation mesh has no physics asset; collision/ragdoll not validated.')
    slots = mesh.get_editor_property('materials')
    for i in range(len(slots)):
        slot = slots[i]
        slot.set_editor_property('material_interface', material)
        slots[i] = slot
    mesh.set_editor_property('materials', slots)
    save(mesh)
    for source in files:
        opts = u.FbxImportUI()
        opts.automated_import_should_detect_type = False
        opts.import_mesh = False
        opts.import_as_skeletal = True
        opts.import_animations = True
        opts.import_materials = False
        opts.import_textures = False
        opts.mesh_type_to_import = u.FBXImportType.FBXIT_ANIMATION
        opts.skeleton = skeleton
        opts.anim_sequence_import_data.set_editor_property('import_uniform_scale', 100.0)
        save(import_file(source, opts))
    save(skeleton)
    E.set_metadata_tag(mesh, 'TallowDependencyRepair', 'visual-only-v1')
    save(mesh)
    assert E.save_directory(DEST, False, True), 'Failed to save staging directory'
    audit()


if globals().get('AUDIT_ONLY', False):
    audit()
else:
    repair()
