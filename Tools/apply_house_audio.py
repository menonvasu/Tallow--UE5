"""Imports placeholder sounds (credits: SourceAssets/Audio/CREDITS.md) and places the house ambience. Re-runnable."""
import unreal as u, os
ROOT=os.path.dirname(os.path.dirname(os.path.abspath(__file__))); SRC=os.path.join(ROOT,'Tallow/SourceAssets/Audio/Tallow')
ed=u.get_editor_subsystem(u.LevelEditorSubsystem); eas=u.get_editor_subsystem(u.EditorActorSubsystem); EAL=u.EditorAssetLibrary
assert ed.load_level('/Game/Maps/Lvl_Tallow_M3_Greybox')
def folder(n): return '/Game/Audio/Keeper' if ('Keeper' in n or 'Capture' in n) else '/Game/Audio/Player' if 'Player' in n else '/Game/Audio/Ambience'
tasks=[]
for f in sorted(os.listdir(SRC)):
    if not f.endswith('.wav'): continue
    n=f[:-4]; t=u.AssetImportTask(); t.filename=os.path.join(SRC,f); t.destination_path=folder(n); t.destination_name=n; t.automated=True; t.save=True; t.replace_existing=True; tasks.append(t)
u.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
for n in ('S_Amb_HouseBed','S_Amb_BasementRumble','S_Amb_Drip','S_Player_ScaredBreath'):
    s=u.load_asset(folder(n)+'/'+n); s.set_editor_property('looping',True); EAL.save_loaded_asset(s)
for a in eas.get_all_level_actors():
    if a.get_actor_label().startswith('Audio_'): eas.destroy_actor(a)
def place(label,sound,loc,vol,spatial,falloff=1500.0):
    a=eas.spawn_actor_from_class(u.AmbientSound,u.Vector(*loc)); a.set_actor_label(label); a.set_folder_path('Audio')
    c=a.get_component_by_class(u.AudioComponent); c.set_sound(u.load_asset(sound)); c.set_editor_property('volume_multiplier',vol)
    c.set_editor_property('allow_spatialization',spatial)
    if spatial:
        c.set_editor_property('override_attenuation',True)
        at=c.get_editor_property('attenuation_overrides'); at.set_editor_property('falloff_distance',falloff); at.set_editor_property('attenuation_shape_extents',u.Vector(300,0,0)); c.set_editor_property('attenuation_overrides',at)
    return a
place('Audio_HouseBed','/Game/Audio/Ambience/S_Amb_HouseBed',(780,-650,200),0.35,False)
place('Audio_BasementRumble','/Game/Audio/Ambience/S_Amb_BasementRumble',(1100,-800,-200),0.5,True,1800.0)
place('Audio_BasementDrip','/Game/Audio/Ambience/S_Amb_Drip',(200,-1150,-300),0.6,True,900.0)
assert ed.save_current_level()
u.log('AUDIO imported %d sounds, ambience placed'%len(tasks))
