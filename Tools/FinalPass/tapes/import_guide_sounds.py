"""Import the objective sounds (build_guide_sounds.py -> <project>/Tallow/SourceAssets/FinalPass/Tapes/guide-sounds.json)
into /Game/FinalPass/Tapes, on the Ambience class like the house beds (Tools/FinalPass/sound/import_sound.py), a little
above them in priority so the way is the last of the house to go if voices run short. They loop and keep time while out
of earshot. Also builds M_TapeIndicator, the chapter tape machine's lamp (ATallowStoryObject drives its "Glow": RGB
times A into emissive over a dark red glass bead). Saves only these new assets; re-runnable.

    UnrealEditor <project>/Tallow/Tallow.uproject -unattended -nosound -nullrhi -ExecCmds="py <this file>"
"""
from pathlib import Path
import json, time, traceback
import unreal as u

DEST = '/Game/FinalPass/Tapes'
LIB = u.EditorAssetLibrary
TOOLS = u.AssetToolsHelpers.get_asset_tools()
ROWS = Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir())) / 'SourceAssets/FinalPass/Tapes/guide-sounds.json'


def build_indicator():
    L = u.MaterialEditingLibrary; path = DEST + '/M_TapeIndicator'
    if LIB.does_asset_exist(path):
        m = LIB.load_asset(path); L.delete_all_material_expressions(m)
    else:
        m = TOOLS.create_asset('M_TapeIndicator', DEST, u.Material, u.MaterialFactoryNew())
    glass = L.create_material_expression(m, u.MaterialExpressionConstant3Vector, -500, -200)
    glass.set_editor_property('constant', u.LinearColor(.12, .02, .015, 1.))
    L.connect_material_property(glass, '', u.MaterialProperty.MP_BASE_COLOR)
    rough = L.create_material_expression(m, u.MaterialExpressionConstant, -500, -60); rough.set_editor_property('r', .25)
    L.connect_material_property(rough, '', u.MaterialProperty.MP_ROUGHNESS)
    glow = L.create_material_expression(m, u.MaterialExpressionVectorParameter, -600, 80)
    glow.set_editor_property('parameter_name', 'Glow'); glow.set_editor_property('default_value', u.LinearColor(.09, .012, .008, 1.))
    mul = L.create_material_expression(m, u.MaterialExpressionMultiply, -300, 80)
    L.connect_material_expressions(glow, 'RGB', mul, 'A'); L.connect_material_expressions(glow, 'A', mul, 'B')
    L.connect_material_property(mul, '', u.MaterialProperty.MP_EMISSIVE_COLOR)
    L.recompile_material(m); LIB.save_loaded_asset(m, False)
    return path


def run():
    rows = json.loads(ROWS.read_text()); tasks = []
    for row in rows:
        source = ROWS.parent / (row['name'] + '.wav')
        assert source.is_file(), source
        task = u.AssetImportTask(); task.filename = str(source); task.destination_path = DEST
        task.destination_name = row['name']; task.automated = True; task.replace_existing = True; task.save = True
        tasks.append(task)
    TOOLS.import_asset_tasks(tasks)
    done, missing = [], []
    for row in rows:
        path = DEST + '/' + row['name']; sound = LIB.load_asset(path)
        if not isinstance(sound, u.SoundWave): missing.append(path); continue
        sound.set_editor_property('sound_class_object', LIB.load_asset('/Game/Audio/Mix/SC_Tallow_Ambience'))
        sound.set_editor_property('looping', True)
        sound.set_editor_property('volume', 1.)
        sound.set_editor_property('compression_quality', 85)
        sound.set_editor_property('priority', .45)
        sound.set_editor_property('virtualization_mode', u.VirtualizationMode.PLAY_WHEN_SILENT)
        LIB.save_loaded_asset(sound)
        done.append({'asset': path, 'duration': round(sound.get_editor_property('duration'), 2), 'looping': sound.get_editor_property('looping')})
    indicator = build_indicator()
    u.log('TAPES_GUIDE_IMPORTED ' + json.dumps({'imported': done, 'missing': missing, 'indicator': indicator}))
    assert not missing


START = time.monotonic()


def tick(dt):
    if time.monotonic() - START < 3:
        return
    u.unregister_slate_post_tick_callback(H)
    try:
        run()
    except Exception:
        u.log_error(traceback.format_exc())
    u.SystemLibrary.quit_editor()


H = u.register_slate_post_tick_callback(tick)
