"""Import the corrupted narration lines as SoundWaves under /Game/FinalPass/Corrupted.

Reads Lore/Corrupted/final/C001.wav .. C081.wav (rendered by corrupt.py), imports each
as VO_Corrupted_<id>, sets the Voice sound group and compression quality 85 like the
voice pass, checks the duration and saves those assets only. Re-runnable: it replaces
the same assets. Touches no map and no other asset.

    UnrealEditor Tallow/Tallow.uproject -nullrhi -unattended -nosound -ExecCmds="py <this file>"
"""
from pathlib import Path
import json, time, traceback
import unreal as u

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
FINAL = ROOT / 'Tallow/SourceAssets/Lore/Corrupted/final'
DEST = '/Game/FinalPass/Corrupted'
START = time.monotonic()


def run():
    files = sorted(FINAL.glob('C[0-9][0-9][0-9].wav'))
    assert files, 'No rendered lines in ' + str(FINAL)
    tasks = []
    for f in files:
        t = u.AssetImportTask()
        t.filename = str(f)
        t.destination_path = DEST
        t.destination_name = 'VO_Corrupted_' + f.stem
        t.automated = True
        t.replace_existing = True
        t.save = True
        tasks.append(t)
    u.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    done, missing = [], []
    for f in files:
        path = f'{DEST}/VO_Corrupted_{f.stem}'
        sound = u.EditorAssetLibrary.load_asset(path)
        if not isinstance(sound, u.SoundWave):
            missing.append(f.stem)
            continue
        sound.set_editor_property('compression_quality', 85)
        sound.set_editor_property('sound_group', u.SoundGroup.SOUNDGROUP_VOICE)
        seconds = sound.get_editor_property('duration')
        assert seconds > 0, path + ' is empty'
        u.EditorAssetLibrary.save_loaded_asset(sound)
        done.append({'line': f.stem, 'asset': f'{path}.VO_Corrupted_{f.stem}', 'seconds': round(seconds, 3)})
    result = {'imported': len(done), 'missing': missing, 'assets': done}
    (HERE / 'import-result.json').write_text(json.dumps(result, indent=1) + '\n')
    u.log('CORRUPTED_IMPORT ' + json.dumps({'imported': len(done), 'missing': missing}))
    if missing:
        raise RuntimeError('Some corrupted lines did not import: ' + ', '.join(missing))


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
