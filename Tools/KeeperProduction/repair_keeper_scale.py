"""Bounds-gated V06 scale repair. No map, raw FBX or source geometry changes.
Repairs the measured 2.4cm import to 240cm, or resumes mismatched clip imports.
AUDIT_ONLY=True measures and validates without mutation.
Run again after restart to validate persisted bounds and all 13 clip skeletons.
"""
from pathlib import Path
import json
import unreal as u

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / 'Tallow/SourceAssets/Keeper/SleevedV06/Export'
DEST = '/Game/Presentation/KeeperGeneratedV06'
E = u.EditorAssetLibrary
REPORT = ROOT / 'Tallow/Saved/Presentation/keeper-scale-audit.json'
REPORT.parent.mkdir(parents=True, exist_ok=True)


def report(data):
    REPORT.write_text(json.dumps(data, indent=2) + '\n')
    u.log('Keeper scale audit: ' + json.dumps(data))


def save(asset):
    assert asset and asset.get_path_name().startswith(DEST + '/'), 'Refusing external/missing asset'
    assert E.save_loaded_asset(asset, False), 'Keeper save failed'
    package = asset.get_path_name().split('.')[0]
    assert (Path(u.Paths.project_content_dir()) / (package[len('/Game/'):] + '.uasset')).is_file()


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


def main():
    mesh = u.load_asset(DEST + '/SK_Keeper_Tripo_Direct')
    assert mesh, 'Missing staged mesh'
    height = float(mesh.get_imported_bounds().box_extent.z) * 2.0
    result = {'before_height_cm': height, 'expected_height_cm': 240.0, 'status': 'measured before mutation'}
    report(result)
    mesh_correct = 220.0 <= height <= 260.0
    skeleton = mesh.get_editor_property('skeleton')
    files = sorted(SOURCE.glob('AN_Keeper_*.fbx'))
    assert skeleton and len(files) == 13
    def clip_status(source):
        clip = u.load_asset(DEST + '/' + source.stem)
        data = clip.get_editor_property('asset_import_data') if clip else None
        bound = clip.get_editor_property('skeleton') if clip else None
        scale = float(data.get_editor_property('import_uniform_scale')) if isinstance(data, u.FbxAssetImportData) else None
        return {'clip': source.stem, 'metadata': data.get_class().get_name() if data else None,
                'scale': scale, 'skeleton': bound.get_path_name() if bound else None,
                'valid': bool(isinstance(data, u.FbxAnimSequenceImportData) and scale is not None and abs(scale - 100.0) < .001 and bound == skeleton)}
    result['clip_metadata_before'] = [clip_status(source) for source in files]
    report(result)
    wrong_clips = [source for source, state in zip(files, result['clip_metadata_before']) if not state['valid']]
    if mesh_correct and not wrong_clips:
        result['status'] = 'Mesh size and 13 clip scales/skeletons verified; no mutation. Actual poses require visual review.'
        report(result)
        return
    if globals().get('AUDIT_ONLY', False):
        result['status'] = 'Audit failed; no mutation. Mesh bounds or clip metadata require repair.'
        report(result)
        raise RuntimeError('Keeper audit mismatch: ' + json.dumps(result))
    if mesh_correct or 2.2 <= height <= 2.6:
        import_scale = 100.0
    else:
        raise RuntimeError('Unexpected Keeper height; refusing speculative scaling: ' + str(height))
    result['requested_import_uniform_scale'] = import_scale
    result['mesh_reimport_required'] = not mesh_correct
    report(result)
    material = u.load_asset(DEST + '/M_KeeperTripo_Production')
    files = sorted(SOURCE.glob('AN_Keeper_*.fbx'))
    assert skeleton and material and len(files) == 13
    assert skeleton.get_path_name().startswith(DEST + '/'), 'External skeleton forbidden'
    skeleton_path = skeleton.get_path_name()
    assets = u.AssetToolsHelpers.get_asset_tools()

    def reimport(source, is_mesh):
        opts = u.FbxImportUI()
        opts.automated_import_should_detect_type = False
        opts.import_mesh = is_mesh
        opts.import_as_skeletal = True
        opts.import_animations = not is_mesh
        opts.import_materials = False
        opts.import_textures = False
        opts.create_physics_asset = False
        opts.skeleton = skeleton
        opts.mesh_type_to_import = u.FBXImportType.FBXIT_SKELETAL_MESH if is_mesh else u.FBXImportType.FBXIT_ANIMATION
        if is_mesh:
            data = opts.skeletal_mesh_import_data
            data.set_editor_property('import_morph_targets', True)
            data.set_editor_property('import_morph_targets', True)
            data.set_editor_property('update_skeleton_reference_pose', True)
        else:
            data = opts.anim_sequence_import_data
        data.set_editor_property('import_uniform_scale', import_scale)
        task = u.AssetImportTask()
        task.filename = str(source)
        task.destination_path = DEST
        task.destination_name = source.stem
        task.automated = True
        task.replace_existing = True
        task.replace_existing_settings = True
        task.save = True
        task.options = opts
        task.factory = u.FbxFactory()
        for attempt in range(2):
            existing = u.load_asset(DEST + '/' + source.stem)
            if existing:
                update_stored_import_scale(existing, import_scale, is_mesh)
            fbx_flag = 'Interchange.FeatureFlags.Import.FBX'
            previous_fbx_flag = u.SystemLibrary.get_console_variable_int_value(fbx_flag)
            try:
                u.SystemLibrary.execute_console_command(None, fbx_flag + ' 0')
                assert u.SystemLibrary.get_console_variable_int_value(fbx_flag) == 0, 'Could not select legacy FBX importer'
                assets.import_asset_tasks([task])
            finally:
                u.SystemLibrary.execute_console_command(None, fbx_flag + ' ' + str(previous_fbx_flag))
            asset = u.load_asset(DEST + '/' + source.stem)
            stored = asset.get_editor_property('asset_import_data') if asset else None
            actual_scale = stored.get_editor_property('import_uniform_scale') if isinstance(stored, u.FbxAssetImportData) else None
            u.log('Keeper post-import ' + source.stem + ' attempt=' + str(attempt + 1) + ' stored_scale=' + str(actual_scale))
            if actual_scale is not None and abs(actual_scale - import_scale) < .001:
                break
            if attempt == 1:
                raise RuntimeError('Keeper metadata failed after one retry: ' + source.stem + ' scale=' + str(actual_scale) + ' expected=' + str(import_scale))
        bound = asset.get_editor_property('skeleton')
        assert bound and bound.get_path_name() == skeleton_path, 'Skeleton mismatch: ' + source.stem

        return asset

    if not mesh_correct:
        mesh = reimport(SOURCE / 'SK_Keeper_Tripo_Direct.fbx', True)
    new_height = float(mesh.get_imported_bounds().box_extent.z) * 2.0
    result['after_height_cm'] = new_height
    result['requested_import_uniform_scale'] = import_scale
    report(result)
    assert 220.0 <= new_height <= 260.0, 'Scale reimport did not yield expected height; stop for diagnosis'
    # This namespace is visual-only; previous dependency repair explicitly permitted no physics.
    mesh.set_editor_property('physics_asset', None)
    slots = mesh.get_editor_property('materials')
    for i in range(len(slots)):
        slot = slots[i]
        slot.set_editor_property('material_interface', material)
        slots[i] = slot
    mesh.set_editor_property('materials', slots)
    save(skeleton)
    save(mesh)
    clip_paths = []
    for source in (wrong_clips if mesh_correct else files):
        clip = reimport(source, False)
        save(clip)
        clip_paths.append(clip.get_path_name())
    save(skeleton)
    assert E.save_directory(DEST, False, True)
    result['clip_metadata_after'] = [clip_status(source) for source in files]
    report(result)
    assert all(state['valid'] for state in result['clip_metadata_after']), 'Clip scale/skeleton mismatch: ' + json.dumps(result['clip_metadata_after'])
    result.update(status='Scale metadata repaired; actual poses unverified; fresh-process bounds/pose review required', clips=clip_paths,
                  skeleton=skeleton_path, physics_status='No physics asset; visual-only, collision/ragdoll unvalidated')
    report(result)


main()
