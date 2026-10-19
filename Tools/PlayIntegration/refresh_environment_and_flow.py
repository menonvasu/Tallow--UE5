from pathlib import Path
import runpy
import unreal as u
ROOT=Path(__file__).resolve().parents[2]
assert u.get_editor_subsystem(u.LevelEditorSubsystem).load_level('/Game/Presentation/Playable/Lvl_Tallow_Playable')
for script in ['Tools/EnvironmentIntegration/refresh_upper_floor_unreal.py',
               'Tools/EnvironmentIntegration/import_finish_joinery.py',
               'Tools/EnvironmentIntegration/import_task_props.py',
               'Tools/EnvironmentIntegration/dress_playable_house.py',
               'Tools/EnvironmentIntegration/refresh_house_surfaces.py',
               'Tools/KeeperProduction/V07/import_keeper_v07.py',
               'Tools/PlayIntegration/configure_playable.py']:
    runpy.run_path(str(ROOT/script),run_name='__main__')
