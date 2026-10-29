"""Shared tick harness for fix-play editor scripts. Not run directly."""
import json, os, time, traceback
from pathlib import Path
import unreal as u

ED = u.get_editor_subsystem(u.LevelEditorSubsystem)
ACT = u.get_editor_subsystem(u.EditorActorSubsystem)
OUT = Path(os.environ.get('FIXPLAY_OUT', '/tmp/fix-play-out'))
MAP = '/Game/Presentation/Playable/Lvl_Tallow_Playable'


def run_once(name, fn, delay=3):
    R = {'errors': []}
    t0 = time.monotonic(); s = {'ran': False}
    def tick(dt):
        if not s['ran']:
            if time.monotonic() - t0 < delay: return
            s['ran'] = True
            try: fn(R)
            except Exception: R['errors'].append(traceback.format_exc())
            OUT.mkdir(parents=True, exist_ok=True)
            (OUT / (name + '.json')).write_text(json.dumps(R, indent=1, default=str))
            u.log('FIXPLAY_' + name.upper() + ' done errors=%d' % len(R['errors']))
            s['quit'] = time.monotonic() + 2; return
        if time.monotonic() < s['quit']: return
        u.unregister_slate_post_tick_callback(h); u.SystemLibrary.quit_editor()
    h = u.register_slate_post_tick_callback(tick)
