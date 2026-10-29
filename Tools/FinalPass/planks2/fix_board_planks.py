"""Give each optional-board set the plank blueprint, so the set is built from separate planks (pried one per hold)
instead of one mesh that comes off in a single hold. Re-runnable. Run:
UnrealEditor <project>/Tallow/Tallow.uproject -unattended -nosound -NullRHI -ExecCmds="py <this file>"   (PLANKS2_DRY=1 reports only)
Run Tools/FinalPass/map/rebuild_nav.py last after the save."""
import os, sys; sys.path.insert(0, str(__import__('pathlib').Path(__file__).parent))
from _harness import *
DRY = os.environ.get('PLANKS2_DRY') == '1'
CLS = '/Game/Tasks/Crowbar_Plank/BP_TallowPlank.BP_TallowPlank_C'
def fn(R):
    assert ED.load_level(MAP)
    cls = u.load_class(None, CLS); assert cls, CLS
    R['set'] = []
    for a in ACT.get_all_level_actors():
        if isinstance(a, u.TallowOptionalBoards):
            R['set'].append(a.get_actor_label())
            if not DRY:
                a.set_editor_property('plank_class', cls); a.set_editor_property('plank_count', 3)
    if not DRY: ED.save_current_level()
run_once('fix_board_planks' + ('_dry' if DRY else ''), fn)
