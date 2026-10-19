"""Rebuild and save the playable map's navmesh after configure_playable.py moves, retires or adds blockers.
Tick-driven so the asynchronous build finishes before the save. Never starts PIE.
"""
import time,traceback
import unreal as u
MAP='/Game/Presentation/Playable/Lvl_Tallow_Playable';ED=u.get_editor_subsystem(u.LevelEditorSubsystem);S={'phase':'load','time':time.monotonic()}
def done(error=None):
    if error:u.log_error(error)
    u.unregister_slate_post_tick_callback(H);u.SystemLibrary.quit_editor()
def tick(dt):
    try:
        now=time.monotonic()
        if S['phase']=='load' and now-S['time']>2:
            # Collision is cooked asynchronously; building before it finishes loses whole storeys (seen 2026-10-19).
            assert ED.load_level(MAP);u.SystemLibrary.execute_console_command(None,'Editor.AsyncAssetCompilationFinishAll');S.update(phase='settle',time=now)
        elif S['phase']=='settle' and now-S['time']>6:
            world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world();nav=u.NavigationSystemV1.get_navigation_system(world)
            for a in u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors():
                if isinstance(a,u.NavMeshBoundsVolume):nav.on_navigation_bounds_updated(a)
            u.SystemLibrary.execute_console_command(world,'RebuildNavigation');S.update(phase='build',time=now)
        elif S['phase']=='build' and now-S['time']>10:
            world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
            if not u.NavigationSystemV1.get_navigation_system(world).is_navigation_being_built(world) or now-S['time']>240:
                ED.save_current_level();u.log('TALLOW_NAVIGATION_REBUILT seconds='+str(round(now-S['time'],1)));done()
    except Exception:done(traceback.format_exc())
H=u.register_slate_post_tick_callback(tick)
