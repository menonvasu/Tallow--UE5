"""Rebuild the saved navigation of Lvl_Tallow_Playable from the level as it stands, then save. Re-runnable.

    UnrealEditor <project>/Tallow/Tallow.uproject -unattended -nosound -NullRHI -ExecCmds="py /abs/path/rebuild_nav.py"

Run it last, after every script that moves or adds geometry and saves the map without rebuilding navigation:
a standalone game loads the saved navmesh and only rebuilds around what changes at run time (doors, boards), so a
stale saved tile stays stale. Changes nothing else. Results: <project>/Review/Results/rebuild-nav.json
"""
import json
import time
import traceback
from pathlib import Path
import unreal as u

LEVEL = '/Game/Presentation/Playable/Lvl_Tallow_Playable'
OUT = Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir())).parent / 'Review' / 'Results'
ED = u.get_editor_subsystem(u.LevelEditorSubsystem)
ACT = u.get_editor_subsystem(u.EditorActorSubsystem)
R = {'errors': [], 'saved': False}


def main():
    assert ED.load_level(LEVEL), LEVEL
    u.SystemLibrary.execute_console_command(None, 'Editor.AsyncAssetCompilationFinishAll')
    yield 6
    world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
    nav = u.NavigationSystemV1.get_navigation_system(world)
    for a in ACT.get_all_level_actors():
        if isinstance(a, u.NavMeshBoundsVolume):
            nav.on_navigation_bounds_updated(a)
    u.SystemLibrary.execute_console_command(world, 'RebuildNavigation')
    start = time.monotonic()
    yield 10
    while nav.is_navigation_being_built(world) and time.monotonic() - start < 300:
        yield 2
    R['nav_seconds'] = round(time.monotonic() - start, 1)
    R['still_building'] = bool(nav.is_navigation_being_built(world))
    if not R['still_building']:
        R['saved'] = bool(ED.save_current_level())


G = main()
WAKE = [time.monotonic() + 4]
BUSY = [False]


def tick(dt):
    # Loading a level ticks Slate again from inside the step; those re-entrant ticks wait.
    if BUSY[0] or time.monotonic() < WAKE[0]:
        return
    BUSY[0] = True
    try:
        WAKE[0] = time.monotonic() + next(G)
        return
    except StopIteration:
        pass
    except Exception:
        R['errors'].append(traceback.format_exc())
    finally:
        BUSY[0] = False
    u.unregister_slate_post_tick_callback(HANDLE)
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / 'rebuild-nav.json').write_text(json.dumps(R, indent=1))
    u.log('TALLOW_REBUILD_NAV ' + json.dumps(R))
    u.SystemLibrary.quit_editor()


HANDLE = u.register_slate_post_tick_callback(tick)
