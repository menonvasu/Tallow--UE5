import json
from pathlib import Path
import unreal as u
ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'Tallow/Saved/Integration'
OUT.mkdir(parents=True, exist_ok=True)
report = {}
default = u.get_default_object(u.EditorAssetLibrary.load_blueprint_class('/Game/FirstPerson/Blueprints/BP_TallowCharacter'))
for c in default.get_components_by_class(u.ActorComponent):
    if isinstance(c, u.CharacterMovementComponent):
        report['movement'] = {n: str(c.get_editor_property(n)) for n in ('max_step_height', 'walkable_floor_angle', 'max_walk_speed')}
    if isinstance(c, u.CapsuleComponent):
        report['capsule'] = {'radius': c.get_unscaled_capsule_radius(), 'halfheight': c.get_unscaled_capsule_half_height()}
report['input_actions'] = {}
for n in ('drop_action', 'cup_action', 'relight_action', 'stillness_action', 'interact_action'):
    a = default.get_editor_property(n)
    report['input_actions'][n] = a.get_path_name() if a else None
report['mappings'] = []
report['exported'] = []
registry = u.AssetRegistryHelpers.get_asset_registry()
registry.search_all_assets(True)
for info in registry.get_assets_by_path('/Game', recursive=True):
    if str(info.asset_class_path.asset_name) == 'InputMappingContext':
        asset = info.get_asset()
        for m in asset.get_editor_property('mappings'):
            report['mappings'].append({'context':asset.get_name(),'action':m.action.get_name() if m.action else None,'key':str(m.key)})
    if str(info.asset_class_path.asset_name) == 'StaticMesh' and any(s in str(info.asset_name).lower() for s in ('crowbar', 'fuse')):
        mesh = info.get_asset()
        target = OUT / (mesh.get_name() + '.fbx')
        task = u.AssetExportTask()
        task.object = mesh
        task.filename = str(target)
        task.automated = True
        task.prompt = False
        task.replace_identical = True
        task.exporter = u.StaticMeshExporterFBX()
        task.options = u.FbxExportOption()
        task.options.collision = False
        assert u.Exporter.run_asset_export_task(task)
        bounds = mesh.get_bounding_box()
        report['exported'].append({'asset':mesh.get_path_name(),'file':str(target),'bounds':str(bounds)})
for name in ('Fuse', 'Crowbar'):
    definition = u.load_asset('/Game/Items/DA_' + name)
    mesh = definition.get_editor_property('held_mesh')
    if not mesh:
        continue
    target = OUT / ('Held_' + name + '.fbx')
    task = u.AssetExportTask()
    task.object = mesh
    task.filename = str(target)
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    task.exporter = u.StaticMeshExporterFBX()
    task.options = u.FbxExportOption()
    task.options.collision = False
    assert u.Exporter.run_asset_export_task(task)
    report['exported'].append({'item':name,'asset':mesh.get_path_name(),'file':str(target),
        'bounds':str(mesh.get_bounding_box()),'held_transform':str(definition.get_editor_property('held_relative_transform'))})
(OUT/'runtime-assets.json').write_text(json.dumps(report,indent=2))
u.log('TALLOW_RUNTIME_ASSET_AUDIT '+json.dumps(report))
