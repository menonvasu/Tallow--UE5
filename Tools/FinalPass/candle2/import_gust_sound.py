"""Import the candle's gust (build_gust_sound.py -> <project>/Tallow/SourceAssets/FinalPass/Candle/gust-sound.json) into
/Game/FinalPass/Candle as S_Candle_Gust, on the player's foley class like her other candle sounds. UTallowCandleFlameComponent
loads it by path and plays it as a gust starts to take the flame. Saves only this new asset; re-runnable.

    UnrealEditor <project>/Tallow/Tallow.uproject -unattended -nosound -nullrhi -ExecCmds="py <this file>"
"""
from pathlib import Path
import json, time, traceback
import unreal as u

DEST = '/Game/FinalPass/Candle'
LIB = u.EditorAssetLibrary
TOOLS = u.AssetToolsHelpers.get_asset_tools()
ROWS = Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir())) / 'SourceAssets/FinalPass/Candle/gust-sound.json'


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
        sound.set_editor_property('sound_class_object', LIB.load_asset('/Game/Audio/Mix/SC_Tallow_PlayerFoley'))
        sound.set_editor_property('looping', False)
        sound.set_editor_property('volume', 1.)
        sound.set_editor_property('compression_quality', 85)
        LIB.save_loaded_asset(sound)
        done.append({'asset': path, 'duration': round(sound.get_editor_property('duration'), 2),
                     'class': sound.get_editor_property('sound_class_object').get_name()})
    u.log('CANDLE_GUST_IMPORTED ' + json.dumps({'imported': done, 'missing': missing}))
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
