"""Import the contact-preserving relight sleeve correction as a separate clip."""
from pathlib import Path
import unreal as u

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT/'Tallow/SourceAssets/Player/InteractionV05/RelightMatchRevision01/AN_PlayerV05_RelightMatch.fbx'
DEST = '/Game/Presentation/PlayerInteractionV05'
NAME = 'AN_PlayerV05_RelightMatch_01'
E = u.EditorAssetLibrary
assert SOURCE.is_file()
mesh = u.load_asset(DEST+'/SK_PlayerV05_Full')
assert mesh
if not E.does_asset_exist(DEST+'/'+NAME):
    options = u.FbxImportUI()
    for key, value in {'automated_import_should_detect_type':False, 'import_mesh':False,
                       'import_as_skeletal':True, 'import_animations':True,
                       'import_materials':False, 'import_textures':False,
                       'mesh_type_to_import':u.FBXImportType.FBXIT_ANIMATION,
                       'skeleton':mesh.skeleton}.items():
        options.set_editor_property(key,value)
    task = u.AssetImportTask()
    task.filename = str(SOURCE)
    task.destination_path = DEST
    task.destination_name = NAME
    task.options = options
    task.automated = True
    task.replace_existing = False
    task.save = True
    u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    assert task.imported_object_paths
clip = u.load_asset(DEST+'/'+NAME)
assert isinstance(clip,u.AnimSequence) and clip.get_editor_property('skeleton') == mesh.skeleton
assert abs(clip.get_play_length()-1.2)<.04
assert E.save_loaded_asset(clip,False)
u.log('TALLOW_RELIGHT_REVISION_IMPORTED')
