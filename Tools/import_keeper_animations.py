"""Import authored Keeper FBXs and update only its visual in the existing M3 map.

Run after building TallowEditor and exporting the FBXs, through UE's Python
commandlet or editor. No level rebuild: traversal actors and collision survive.
Optional keeper-import-settings.json supplies mesh_location_cm, mesh_rotation_deg
(pitch,yaw,roll), and mesh_scale; defaults assume a 230 cm Z-up, +X-facing export.
"""
import json
from pathlib import Path
import unreal as u

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'Tallow/SourceAssets/Keeper/Animated'
DEST = '/Game/Characters/Keeper/Animated'
LEVEL = '/Game/Maps/Lvl_Tallow_M3_Greybox'
NAMES = ('Idle', 'HostWalk', 'Reveal', 'Walk', 'Sneak', 'Chase', 'RunChase',
         'WallCrawl', 'CeilingCrawl', 'SearchUnder', 'SearchOver', 'JumpscareDeath', 'Squeeze')
AUDIT = ROOT / 'Tools/keeper-animation-import.json'
settings_path = ROOT / 'Tools/keeper-import-settings.json'
settings = json.loads(settings_path.read_text()) if settings_path.exists() else {}
# Check the entire batch before importing anything or replacing the map visual.
for name in ('SK_Keeper',) + tuple('AN_Keeper_' + n for n in NAMES):
    path = SOURCE / (name + '.fbx')
    assert path.is_file() and path.stat().st_size > 0, 'Missing export: ' + str(path)

editor = u.get_editor_subsystem(u.LevelEditorSubsystem)
actors = u.get_editor_subsystem(u.EditorActorSubsystem)
assert editor.load_level(LEVEL), 'Cannot load ' + LEVEL
world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
u.SystemLibrary.execute_console_command(world, 'Interchange.FeatureFlags.Import.FBX 0')
asset_tools = u.AssetToolsHelpers.get_asset_tools()


def options(animation=False, skeleton=None):
    o = u.FbxImportUI()
    o.set_editor_property('automated_import_should_detect_type', False)
    o.set_editor_property('mesh_type_to_import', u.FBXImportType.FBXIT_ANIMATION if animation else u.FBXImportType.FBXIT_SKELETAL_MESH)
    o.set_editor_property('import_as_skeletal', True)
    o.set_editor_property('import_mesh', not animation)
    o.set_editor_property('import_animations', animation)
    o.set_editor_property('create_physics_asset', False)
    o.set_editor_property('import_materials', not animation)
    o.set_editor_property('import_textures', not animation)
    o.set_editor_property('override_full_name', True)
    if skeleton:
        o.set_editor_property('skeleton', skeleton)
    m = o.get_editor_property('skeletal_mesh_import_data')
    m.set_editor_property('import_morph_targets', True)
    m.set_editor_property('update_skeleton_reference_pose', False)
    m.set_editor_property('use_t0_as_ref_pose', False)
    a = o.get_editor_property('anim_sequence_import_data')
    a.set_editor_property('animation_length', u.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
    a.set_editor_property('use_default_sample_rate', False)
    a.set_editor_property('custom_sample_rate', 0)
    a.set_editor_property('import_bone_tracks', True)
    a.set_editor_property('import_custom_attribute', True)
    a.set_editor_property('delete_existing_morph_target_curves', True)
    a.set_editor_property('do_not_import_curve_with_zero', False)
    a.set_editor_property('remove_redundant_keys', False)
    return o


def import_one(name, opts, expected_class):
    task = u.AssetImportTask()
    task.filename = str(SOURCE / (name + '.fbx'))
    task.destination_path = DEST
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.replace_existing_settings = True
    task.save = True
    task.options = opts
    task.factory = u.FbxFactory()
    asset_tools.import_asset_tasks([task])
    candidates = [u.load_asset(p) for p in task.imported_object_paths]
    assets = [a for a in candidates if isinstance(a, expected_class)]
    assert len(assets) == 1, '%s expected one %s, got %s' % (name, expected_class, task.imported_object_paths)
    asset = assets[0]
    desired = DEST + '/' + name
    if asset.get_path_name().split('.')[0] != desired:
        assert not u.EditorAssetLibrary.does_asset_exist(desired), 'Naming conflict: ' + desired
        assert u.EditorAssetLibrary.rename_asset(asset.get_path_name(), desired)
        asset = u.load_asset(desired)
    return asset


mesh = import_one('SK_Keeper', options(), u.SkeletalMesh)
skeleton = mesh.get_editor_property('skeleton')
assert skeleton, 'Imported mesh has no skeleton'
clips = {name: (import_one('AN_Keeper_' + name, options(True, skeleton), u.AnimSequence)
                if settings.get('reimport_animations', True) or name in settings.get('reimport_animation_names', []) else u.load_asset(DEST + '/AN_Keeper_' + name))
         for name in NAMES}
assert all(isinstance(c, u.AnimSequence) for c in clips.values()), 'Missing existing animation; enable reimport_animations'
expected_morphs = {'FULL_HORROR', 'CHOMP', 'Blink_L', 'Blink_R'}
actual_morphs = {m.get_name() for m in mesh.get_editor_property('morph_targets')}
assert expected_morphs <= actual_morphs, 'Missing morph targets: ' + str(expected_morphs - actual_morphs)
export_audit_path = ROOT / 'Tools/keeper-animation-export.json'
export_clips = {c['name']: c for c in json.loads(export_audit_path.read_text())['clips']} if export_audit_path.exists() else {}
audit = {'mesh': mesh.get_path_name(), 'skeleton': skeleton.get_path_name(),
         'morph_targets': [m.get_name() for m in mesh.get_editor_property('morph_targets')],
         'clips': {}, 'physics_asset_generated': False}
for name, clip in clips.items():
    assert clip.get_editor_property('skeleton') == skeleton, name + ' skeleton mismatch'
    duration = clip.get_play_length()
    assert duration > 0, name + ' has zero length'
    if name in export_clips:
        expected_seconds = (export_clips[name]['frames'] - 1) / 24.0
        assert abs(duration - expected_seconds) < 1 / 24.0, '%s duration %s differs from 24 fps export %s' % (name, duration, expected_seconds)
    audit['clips'][name] = {'path': clip.get_path_name(), 'duration_seconds': duration,
                           'curves': [str(n) for n in u.AnimationLibrary.get_animation_curve_names(clip, u.RawCurveTrackTypes.RCT_FLOAT)]}
    # Character/traversal movement owns root translation. Authored cycles are in-place.
    clip.set_editor_property('enable_root_motion', False)
    u.EditorAssetLibrary.save_loaded_asset(clip)

keepers = [a for a in actors.get_all_level_actors() if a.get_actor_label() == 'M3_Keeper']
assert len(keepers) == 1, 'Expected exactly one M3_Keeper'
keeper = keepers[0]
cls = u.load_class(None, '/Script/Tallow.TallowCreatureAnimationComponent')
assert cls, 'Build TallowEditor with TallowCreatureAnimationComponent first'
bridge = keeper.get_component_by_class(cls)
assert bridge, 'Keeper must have its native animation component; build/reload the updated class'
bridge.set_editor_property('keeper_mesh', mesh)
bridge.set_editor_property('enable_authored_playback', True)
bridge.set_editor_property('walk_reference_speed', 40.6)
bridge.set_editor_property('chase_reference_speed', 211.5)
for prop, name in {'idle_animation': 'Idle', 'walk_animation': 'HostWalk',
                   'chase_animation': 'RunChase', 'investigate_animation': 'SearchUnder', 'reveal_animation': 'Reveal',
                   'sneak_animation': 'Sneak', 'search_under_animation': 'SearchUnder',
                   'search_over_animation': 'SearchOver',
                   'capture_animation': 'JumpscareDeath', 'wall_animation': 'WallCrawl',
                   'ceiling_animation': 'CeilingCrawl', 'squeeze_animation': 'Squeeze'}.items():
    bridge.set_editor_property(prop, clips[name])
component = keeper.get_component_by_class(u.SkeletalMeshComponent)
component.set_skeletal_mesh_asset(mesh)
component.set_relative_location(u.Vector(*settings.get('mesh_location_cm', [0, 0, -96])), False, False)
rotation = settings.get('mesh_rotation_deg', [0, -90, 0])
component.set_relative_rotation(u.Rotator(pitch=rotation[0], yaw=rotation[1], roll=rotation[2]), False, False)
component.set_relative_scale3d(u.Vector(*settings.get('mesh_scale', [1, 1, 1])))
component.set_collision_enabled(u.CollisionEnabled.NO_COLLISION)
component.set_simulate_physics(False)
component.set_visibility(True, True)
component.set_hidden_in_game(False, True)
component.set_animation_mode(u.AnimationMode.ANIMATION_SINGLE_NODE)
component.set_animation(clips['Idle'])
audit['bone_count'] = component.get_num_bones()
assert audit['bone_count'] > 1, 'No usable skeletal rig imported'
audit['mesh_transform'] = {'location_cm': settings.get('mesh_location_cm', [0, 0, -96]),
                           'rotation_deg': settings.get('mesh_rotation_deg', [0, -90, 0]),
                           'scale': settings.get('mesh_scale', [1, 1, 1])}
# Keep the authored wall-to-ceiling corner clear of the full creature capsule.
for anchor in actors.get_all_level_actors():
    label = anchor.get_actor_label()
    if label.startswith('HANDOFF_Surface_'):
        location = anchor.get_actor_location()
        if any(label.startswith('HANDOFF_Surface_' + key) for key in ('B02', 'B05')):
            location.z = -140
            anchor.set_actor_location(location, False, False)
        elif any(label.startswith('HANDOFF_Surface_' + key) for key in ('B03', 'B04')):
            location.x = 780
            anchor.set_actor_location(location, False, False)
# Route guides are editor decoration, never physical traversal obstacles.
audit['collision_disabled_guide_count'] = 0
for guide in actors.get_all_level_actors():
    if guide.get_actor_label().startswith('GUIDE_'):
        guide.set_actor_enable_collision(False)
        for primitive in guide.get_components_by_class(u.PrimitiveComponent):
            primitive.set_collision_enabled(u.CollisionEnabled.NO_COLLISION)
        audit['collision_disabled_guide_count'] += 1
# Only discard the obsolete static visual after all required assets and wiring succeeded.
for actor in actors.get_all_level_actors():
    if actor.get_actor_label() == 'M3_Keeper_Visual':
        actors.destroy_actor(actor)
u.EditorAssetLibrary.save_loaded_asset(mesh)
u.EditorAssetLibrary.save_loaded_asset(skeleton)
assert editor.save_current_level(), 'Failed saving updated map'
AUDIT.write_text(json.dumps(audit, indent=2) + '\n')
u.log('KEEPER_IMPORT_OK ' + str(AUDIT))
