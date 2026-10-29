"""Run the delivered, scoped environment importer then dress the integration copy."""
from pathlib import Path
import runpy
import unreal as u
ROOT = Path(__file__).resolve().parents[2]
runpy.run_path(str(ROOT/'Tools/EnvironmentIntegration/import_house.py'), run_name='__main__')
assert u.get_editor_subsystem(u.LevelEditorSubsystem).load_level('/Game/Presentation/Playable/Lvl_Tallow_Playable')
runpy.run_path(str(ROOT/'Tools/EnvironmentIntegration/dress_playable_house.py'), run_name='__main__')
u.log('TALLOW_ENVIRONMENT_BATCH_COMPLETE')
