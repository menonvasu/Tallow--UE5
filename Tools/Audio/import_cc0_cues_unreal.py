"""Import the interim CC0 cues (Tools/Audio/build_cc0_cues.py) into /Game/Audio/CC0 and lay the garden wind
under the ending. Re-runnable: replaces the cues and only the Tallow.Audio-tagged outro actor.
"""
from pathlib import Path
import unreal as u

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT/'Tallow/SourceAssets/Audio/CC0/Cues'
DEST = '/Game/Audio/CC0'
LOOPS = {'H_Power_Hum', 'A_Outro_Wind'}
E = u.EditorAssetLibrary
tasks = []
for wav in sorted(SRC.glob('*.wav')):
    t = u.AssetImportTask(); t.filename = str(wav); t.destination_path = DEST; t.destination_name = wav.stem
    t.automated = True; t.save = True; t.replace_existing = True; tasks.append(t)
u.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
for t in tasks:
    sound = u.load_asset(DEST+'/'+t.destination_name); assert isinstance(sound, u.SoundWave), t.destination_name
    sound.set_editor_property('looping', t.destination_name in LOOPS); E.save_loaded_asset(sound, False)

ED = u.get_editor_subsystem(u.LevelEditorSubsystem); A = u.get_editor_subsystem(u.EditorActorSubsystem)
assert ED.load_level('/Game/Presentation/Cinematics/Maps/Lvl_OutroPresentation')
for a in A.get_all_level_actors():
    if 'Tallow.Audio' in [str(x) for x in a.tags]: A.destroy_actor(a)
wind = A.spawn_actor_from_class(u.AmbientSound, u.Vector(100, -400, 200)); wind.set_actor_label('AUDIO_OutroWind'); wind.tags = [u.Name('Tallow.Audio')]
c = wind.get_component_by_class(u.AudioComponent); c.set_sound(u.load_asset(DEST+'/A_Outro_Wind'))
c.set_editor_property('volume_multiplier', .45); c.set_editor_property('allow_spatialization', False)
assert ED.save_current_level()
u.log('TALLOW_CC0_CUES_IMPORTED %d' % len(tasks))
