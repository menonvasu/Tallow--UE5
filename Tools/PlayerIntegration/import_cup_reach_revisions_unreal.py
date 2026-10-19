"""Import CupShield revision 02 (palm curved round the flame, candle drawn in) and Reach revision 01
(palm-down reach, grab, tug, release) as separate clips; the originals stay untouched."""
from pathlib import Path
import unreal as u

ROOT = Path(__file__).resolve().parents[2]
BASE = ROOT/'Tallow/SourceAssets/Player/InteractionV05'
DEST = '/Game/Presentation/PlayerInteractionV05'
CLIPS = [('CupShieldRevision02/AN_PlayerV05_CupShield.fbx', 'AN_PlayerV05_CupShield_02', 2.0),
         ('ReachRevision01/AN_PlayerV05_Reach.fbx', 'AN_PlayerV05_Reach_01', 1.0),
         # Locomotion revision 01: the candle bobs and rolls with each step, the free fist pumps when running.
         ('LocomotionRevision01/AN_PlayerV05_Walk.fbx', 'AN_PlayerV05_Walk_01', 1.0),
         ('LocomotionRevision01/AN_PlayerV05_Sprint.fbx', 'AN_PlayerV05_Sprint_01', .6),
         ('LocomotionRevision01/AN_PlayerV05_Crouch.fbx', 'AN_PlayerV05_Crouch_01', 2.0),
         # Tool revision 01: crowbar held as a tool and levered, a three-finger fuse insert, the candle lifted into Hold.
         ('ToolRevision01/AN_PlayerV05_ToolHold.fbx', 'AN_PlayerV05_ToolHold_01', 2.0),
         ('ToolRevision01/AN_PlayerV05_CrowbarPry.fbx', 'AN_PlayerV05_CrowbarPry_01', 2.0),
         ('ToolRevision01/AN_PlayerV05_FuseInsert.fbx', 'AN_PlayerV05_FuseInsert_01', 1.1),
         ('ToolRevision01/AN_PlayerV05_Pickup.fbx', 'AN_PlayerV05_Pickup_01', 1.5),
         # CupShield revision 03: fingers pressed together, curled into a C round the back of the flame.
         ('CupShieldRevision03/AN_PlayerV05_CupShield.fbx', 'AN_PlayerV05_CupShield_03', 2.0)]
E = u.EditorAssetLibrary
mesh = u.load_asset(DEST+'/SK_PlayerV05_Full')
assert mesh
for source, name, seconds in CLIPS:
    assert (BASE/source).is_file(), source
    if not E.does_asset_exist(DEST+'/'+name):
        options = u.FbxImportUI()
        for key, value in {'automated_import_should_detect_type':False, 'import_mesh':False,
                           'import_as_skeletal':True, 'import_animations':True,
                           'import_materials':False, 'import_textures':False,
                           'mesh_type_to_import':u.FBXImportType.FBXIT_ANIMATION,
                           'skeleton':mesh.skeleton}.items():
            options.set_editor_property(key,value)
        task = u.AssetImportTask()
        task.filename = str(BASE/source)
        task.destination_path = DEST
        task.destination_name = name
        task.options = options
        task.automated = True
        task.replace_existing = False
        task.save = True
        u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        assert task.imported_object_paths, name
    clip = u.load_asset(DEST+'/'+name)
    assert isinstance(clip,u.AnimSequence) and clip.get_editor_property('skeleton') == mesh.skeleton
    assert abs(clip.get_play_length()-seconds)<.04, (name, clip.get_play_length())
    assert E.save_loaded_asset(clip,False)
u.log('TALLOW_CUP_REACH_REVISIONS_IMPORTED')
