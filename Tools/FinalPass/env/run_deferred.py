"""Wait for editor startup, then run one env-pass script against the project.

TALLOW_ENV_SCRIPT is its absolute path (it must live next to this file). The script defines main(), a
generator yielding seconds to wait; this driver steps it from the slate tick so loads, shader compilation
and PIE can settle between steps.
"""
import json
import os
import sys
import time
import traceback
from pathlib import Path
import unreal as u

HERE = Path(__file__).resolve().parent
TARGET = Path(os.environ['TALLOW_ENV_SCRIPT']).resolve()
assert TARGET.parent == HERE and TARGET.is_file(), TARGET
sys.path.insert(0, str(HERE))
OUT = Path(os.environ['TALLOW_ENV_OUT'])
LABEL = os.environ.get('TALLOW_ENV_LABEL', TARGET.stem)
START = time.monotonic()
LIMIT = float(os.environ.get('TALLOW_ENV_LIMIT', '1500'))
S = {'gen': None, 'next': 0.0, 'busy': False}


def finish(error=None):
    if error:
        u.log_error('ENV_ERROR ' + error)
        (OUT / 'Results').mkdir(parents=True, exist_ok=True)
        (OUT / 'Results' / (LABEL + '-error.json')).write_text(json.dumps({'script': str(TARGET), 'error': error}, indent=2))
    u.log('ENV_DONE ' + LABEL)
    u.unregister_slate_post_tick_callback(HANDLE)
    u.SystemLibrary.quit_editor()


def tick(dt):
    now = time.monotonic()
    if S['busy'] or now < S['next']:
        return
    if S['gen'] is None:
        if now - START < 3:
            return
        context = {'__file__': str(TARGET), '__name__': 'env_task'}
        try:
            exec(compile(TARGET.read_text(), str(TARGET), 'exec'), context)
            S['gen'] = context['main']()
        except Exception:
            finish(traceback.format_exc())
        return
    if now - START > LIMIT:
        finish('timeout')
        return
    S['busy'] = True
    try:
        S['next'] = time.monotonic() + float(next(S['gen']) or 0)
    except StopIteration:
        finish()
    except Exception:
        finish(traceback.format_exc())
    finally:
        S['busy'] = False


HANDLE = u.register_slate_post_tick_callback(tick)
