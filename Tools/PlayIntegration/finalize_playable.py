import unreal as u,runpy,time,traceback
from pathlib import Path
R=Path(__file__).resolve().parents[2];ED=u.get_editor_subsystem(u.LevelEditorSubsystem);A=u.get_editor_subsystem(u.EditorActorSubsystem)
try:
 assert ED.load_level('/Game/Presentation/Playable/Lvl_Tallow_Playable')
 runpy.run_path(str(R/'Tools/EnvironmentIntegration/dress_playable_house.py'),run_name='__main__')
 runpy.run_path(str(R/'Tools/PlayIntegration/configure_playable.py'),run_name='__main__')
 assert ED.load_level('/Game/Presentation/Playable/Lvl_Tallow_Playable')
 for a in A.get_all_level_actors():
  if isinstance(a,u.RecastNavMesh):a.set_editor_property('runtime_generation',u.RuntimeGenerationType.DYNAMIC)
 world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
 u.SystemLibrary.execute_console_command(world,'RebuildNavigation')
 START=time.monotonic()
 def tick(dt):
  if time.monotonic()-START>15:
   ED.save_current_level();u.unregister_slate_post_tick_callback(H);u.log('TALLOW_PLAYABLE_FINALIZED');u.SystemLibrary.quit_editor()
 H=u.register_slate_post_tick_callback(tick)
except Exception:
 u.log_error(traceback.format_exc());u.SystemLibrary.quit_editor()
