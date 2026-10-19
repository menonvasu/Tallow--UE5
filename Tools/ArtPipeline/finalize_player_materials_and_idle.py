"""Fix owned player slot assignment and add the separately validated seated loop."""
from pathlib import Path
import json
import unreal as u

ROOT = Path(__file__).resolve().parents[2]
DEST = '/Game/Presentation/PlayerGenerated'
E = u.EditorAssetLibrary
mat = u.load_asset(DEST + '/M_PlayerGenerated')
assert mat
mat.set_editor_property('used_with_skeletal_mesh', True)
u.MaterialEditingLibrary.recompile_material(mat)
report = {}
for name in ['Full', 'Partial']:
    mesh = u.load_asset(DEST + '/SK_PlayerGenerated_' + name)
    assert mesh
    slots = mesh.get_editor_property('materials')
    for i in range(len(slots)):
        slot = slots[i]
        slot.set_editor_property('material_interface', mat)
        slots[i] = slot
    mesh.set_editor_property('materials', slots)
    assert E.save_loaded_asset(mesh, False)
    actual = [s.get_editor_property('material_interface') for s in mesh.get_editor_property('materials')]
    assert actual and all(m == mat for m in actual)
    report[name] = [m.get_path_name() for m in actual]
assert E.save_loaded_asset(mat, False)

name = 'AN_PlayerGenerated_SeatedBoundIdle'
assert not E.does_asset_exist(DEST + '/' + name), 'Review the existing seated clip before any explicit reimport'
source = ROOT / 'Tallow/SourceAssets/Player/SeatedV04' / (name + '.fbx')
assert source.is_file()
ui = u.FbxImportUI()
ui.automated_import_should_detect_type = False
ui.import_mesh = False
ui.import_as_skeletal = True
ui.import_animations = True
ui.import_materials = False
ui.import_textures = False
ui.mesh_type_to_import = u.FBXImportType.FBXIT_ANIMATION
ui.skeleton = mesh.skeleton
task = u.AssetImportTask()
task.filename = str(source)
task.destination_path = DEST
task.destination_name = name
task.automated = True
task.replace_existing = False
task.save = True
task.options = ui
task.factory = u.FbxFactory()
u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
clip = u.load_asset(DEST + '/' + name)
assert isinstance(clip, u.AnimSequence)
assert clip.get_editor_property('skeleton') == mesh.skeleton
report['seated_clip'] = clip.get_path_name()
report['skeletal_material_usage'] = mat.get_editor_property('used_with_skeletal_mesh')
(ROOT / 'Tallow/Saved/Presentation/player-material-audit.json').write_text(json.dumps(report, indent=2))
u.log('TALLOW: generated player materials assigned and audited; seated loop imported separately.')
