"""Import the original transparent title as a cooked UI texture."""
from pathlib import Path
import unreal as u

root = Path(__file__).resolve().parents[2]
task = u.AssetImportTask()
task.filename = str(root / 'Tallow/SourceAssets/ArtDirection/Typography/tallow-wordmark-v02.png')
task.destination_path = '/Game/Presentation/UI/Textures'
task.destination_name = 'T_TallowWordmark'
task.automated = task.save = task.replace_existing = True
u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
tex = u.load_asset('/Game/Presentation/UI/Textures/T_TallowWordmark')
assert tex
tex.set_editor_property('compression_settings', u.TextureCompressionSettings.TC_EDITOR_ICON)
tex.set_editor_property('lod_group', u.TextureGroup.TEXTUREGROUP_UI)
tex.set_editor_property('mip_gen_settings', u.TextureMipGenSettings.TMGS_NO_MIPMAPS)
tex.set_editor_property('srgb', True)
assert u.EditorAssetLibrary.save_loaded_asset(tex, False)
