"""Import the unmodified OFL font into a new UI namespace in a full editor.

Use -ExecCmds="py /path/to/this_file.py"; the font factory needs Slate and
cannot run in a PythonScript commandlet, even when import is automated.
"""
from pathlib import Path
import unreal as u

root = Path(__file__).resolve().parents[2]
destination = '/Game/Presentation/UI/Fonts'
existing = u.EditorAssetLibrary.list_assets(destination) if u.EditorAssetLibrary.does_directory_exist(destination) else []
assert all(p.rsplit('/', 1)[-1].split('.')[0] in ('LiberationSerif', 'LiberationSerif_Font') for p in existing), 'Unexpected font assets; refusing to replace them'
factory = u.FontFileImportFactory()
factory.set_editor_property('batch_create_font_asset', u.BatchCreateFontAsset.YES)
task = u.AssetImportTask()
task.filename = str(root / 'Tallow/SourceAssets/ArtDirection/Typography/LiberationSerif-Regular.ttf')
task.destination_path = destination
task.destination_name = 'LiberationSerif'
task.automated = True
task.save = True
task.replace_existing = bool(existing)
task.factory = factory
u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
for path in u.EditorAssetLibrary.list_assets(destination):
    asset = u.load_asset(path)
    assert u.EditorAssetLibrary.save_loaded_asset(asset, False), f'Could not save {path}'
    u.log(f'TALLOW title font import: {path} ({asset.get_class().get_name()})')
