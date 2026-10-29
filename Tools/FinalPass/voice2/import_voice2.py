"""Import the harsher corrupted lines and bring the menu waltz down to the game's level. Re-runnable.

1. Tools/FinalPass/voice2/final/C001.wav .. C081.wav (corrupt2.py) replace /Game/FinalPass/Corrupted/VO_Corrupted_C001
   .. C081, same names and paths, set up as the first import_corrupted.py does: Voice sound group, quality 85,
   volume 1, no class of their own (TallowCorruptedVoice puts them on SC_Tallow_Narration when it plays them).
2. /Game/FinalPass/Menu/Audio/MUS_BrokenDollsWaltz: volume set from the source's integrated loudness so the menu
   plays it at WALTZ_MENU_LUFS. It has no sound class, so it plays on the engine's default class at 1, outside
   SC_Tallow_Master (.78) that the game sits under; the target is measured as heard. The opening-music levels
   (menu 1, under narration .22, under Nell .18, house .7) stay relative to this.

Saves only those 82 assets. Writes Tools/FinalPass/voice2/import-result.json.

    UnrealEditor <project>/Tallow/Tallow.uproject -unattended -nosound -run=pythonscript -script=/abs/path/import_voice2.py
"""
from pathlib import Path
import json, math, re, subprocess
import unreal as u

HERE = Path(__file__).resolve().parent
FINAL = HERE / 'final'
DEST = '/Game/FinalPass/Corrupted'
WALTZ = '/Game/FinalPass/Menu/Audio/MUS_BrokenDollsWaltz'
WALTZ_SOURCE = Path.home() / 'Downloads/Broken Doll’s Waltz.wav'
# The menu as loud as the stills' narration that follows it (-26.8 LUFS heard: slides .85 x SC_Tallow_Master .78
# on files at -23.2), 9.5 dB under the first import's -17.3. Readings sit at -28.7, the calm house at -44.
WALTZ_MENU_LUFS = -29.5  # the engine drops mono narration 3 dB, so the menu sits level with a reading


def integrated_lufs(path):
    log = subprocess.run(['ffmpeg', '-nostdin', '-hide_banner', '-i', str(path), '-af', 'ebur128', '-f', 'null', '-'],
                         capture_output=True).stderr.decode(errors='replace')
    return float(re.findall(r'I:\s+(-?[\d.]+) LUFS', log)[-1])


def corrupted():
    files = sorted(FINAL.glob('C[0-9][0-9][0-9].wav'))
    assert len(files) == 81, 'expected 81 rendered lines in %s, found %d' % (FINAL, len(files))
    tasks = []
    for f in files:
        t = u.AssetImportTask()
        t.filename = str(f)
        t.destination_path = DEST
        t.destination_name = 'VO_Corrupted_' + f.stem
        t.automated = True
        t.replace_existing = True
        t.save = False
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
        sound.set_editor_property('volume', 1.0)
        seconds = sound.get_editor_property('duration')
        assert seconds > 0, path + ' is empty'
        u.EditorAssetLibrary.save_loaded_asset(sound, False)
        done.append({'line': f.stem, 'asset': f'{path}.VO_Corrupted_{f.stem}', 'seconds': round(seconds, 3)})
    return done, missing


def waltz():
    wave = u.EditorAssetLibrary.load_asset(WALTZ)
    assert isinstance(wave, u.SoundWave), WALTZ
    before = wave.get_editor_property('volume')
    source = integrated_lufs(WALTZ_SOURCE)
    volume = round(10 ** ((WALTZ_MENU_LUFS - source) / 20), 4)
    wave.set_editor_property('volume', volume)
    u.EditorAssetLibrary.save_loaded_asset(wave, False)
    return {'asset': WALTZ, 'source_lufs': source, 'volume_before': round(before, 4), 'volume': volume,
            'menu_lufs_before': round(source + 20 * math.log10(before), 2), 'menu_lufs': WALTZ_MENU_LUFS,
            'change_db': round(20 * math.log10(volume / before), 2),
            'sound_class': wave.get_editor_property('sound_class_object').get_path_name() if wave.get_editor_property('sound_class_object') else None}


def main():
    done, missing = corrupted()
    w = waltz()
    result = {'imported': len(done), 'missing': missing, 'waltz': w, 'assets': done}
    (HERE / 'import-result.json').write_text(json.dumps(result, indent=1) + '\n')
    u.log('VOICE2_IMPORT ' + json.dumps({'imported': len(done), 'missing': missing, 'waltz': w}))
    if missing:
        raise RuntimeError('Some corrupted lines did not import: ' + ', '.join(missing))


main()
