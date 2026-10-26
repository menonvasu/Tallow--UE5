"""Finite PIE checks for the projector and keypad in Vasu's greybox; no asset saves.

Run in a separate editor with -nosound and -ExecCmds="py <this file>".
Exercises actual pickup, threading, projection timers and capture/reset callbacks.
The final presentation-map placement and keyboard focus need separate playtesting.
"""
import json, os, time, traceback
from pathlib import Path
import unreal as u

OUT = Path(os.environ.get('TALLOW_PROJECTOR_TEST_OUT', str(Path(u.Paths.project_saved_dir()) / 'Tests/projector-runtime.json')))
OUT.parent.mkdir(parents=True, exist_ok=True)
RESULT = {'checks': [], 'errors': [], 'actors': {}}
ED = u.get_editor_subsystem(u.LevelEditorSubsystem)
DEADLINE = time.monotonic() + 100

def check(name, passed, detail=None):
    RESULT['checks'].append({'name': name, 'passed': bool(passed), 'detail': detail})
    u.log('PROJECTOR_REVIEW ' + name + ' ' + str(bool(passed)))

def main():
    yield 3
    assert ED.load_level('/Game/Maps/Lvl_Tallow_M3_Greybox')
    yield 2
    ED.editor_request_begin_play()
    yield 4
    w = u.EditorLevelLibrary.get_game_world()
    p = u.GameplayStatics.get_player_pawn(w, 0)
    pc = u.GameplayStatics.get_player_controller(w, 0)
    gm = u.GameplayStatics.get_game_mode(w)
    for name in ['TallowProjector', 'TallowFilmReelPickup', 'TallowEscapePoint', 'TallowFuseBox', 'TallowPlayDirector']:
        actors = u.GameplayStatics.get_all_actors_of_class(w, getattr(u, name))
        RESULT['actors'][name] = [a.get_name() for a in actors]
    check('greybox has player and game mode', bool(p and gm), str(gm))
    ai = None
    for creature in u.GameplayStatics.get_all_actors_of_class(w, u.TallowCreature):
        ai = creature.get_controller()
        if ai:
            brain = ai.get_component_by_class(u.BrainComponent)
            if brain: brain.stop_logic('Isolated projector review')
    projector = u.GameplayStatics.get_actor_of_class(w, u.TallowProjector)
    reel = u.GameplayStatics.get_actor_of_class(w, u.TallowFilmReelPickup)
    escape = u.GameplayStatics.get_actor_of_class(w, u.TallowEscapePoint)
    box = u.GameplayStatics.get_actor_of_class(w, u.TallowFuseBox)
    check('projector and reel placed', bool(projector and reel))
    check('escape point placed', bool(escape))
    if not (projector and reel and escape and p and pc and box): return
    check('projector has decal material', bool(projector.get_editor_property('DecalMaterialBase')))
    check('escape has keypad widget', bool(escape.get_editor_property('KeypadWidgetClass')))
    check('projector linked to fuse box', projector.get_editor_property('LinkedFuseBox') == box)
    check('escape linked to projector', escape.get_editor_property('LinkedProjector') == projector)
    RESULT['projector_audio'] = {k: bool(projector.get_editor_property(k)) for k in ['ClatterLoopSound', 'ProjectorLoopSound']}
    RESULT['asset_audit'] = {}
    for label, actor, component_name in [('projector', projector, 'ProjectorMesh'), ('spool', projector, 'SpoolMesh'), ('reel_pickup', reel, 'FilmReelMesh'), ('escape_latch', escape, 'Latch')]:
        mesh = actor.get_editor_property(component_name).static_mesh
        RESULT['asset_audit'][label] = mesh.get_path_name() if mesh else None
    p.clear_carried_item()
    reel.on_interact(p)
    check('film reel pickup sets carried item', p.get_carried_item() == u.CarriedItemType.FILM_REEL)
    projector.on_interact(p)
    check('projector consumes film reel', p.get_carried_item() == u.CarriedItemType.NONE)
    ai.trigger_player_capture()
    yield 6
    check('loaded reel stays inserted on reset', reel.get_editor_property('hidden'))
    for _ in range(12): projector.on_interact(p)
    yield .8
    check('threading completes', projector.get_thread_progress() >= 1.0)
    check('projection needs power', not projector.can_interact(p))
    for _ in range(box.required_fuses):
        p.set_carried_item(u.CarriedItemType.FUSE)
        box.on_interact_complete(p)
    check('powered projector can start', projector.can_interact(p))
    projector.on_interact(p)
    yield .2
    check('projection locks interaction while playing', not projector.can_interact(p))
    yield 11
    check('projection can be replayed', projector.can_interact(p))
    pc.reset_ignore_move_input()
    pc.reset_ignore_look_input()
    escape.on_interact(p)
    yield .2
    widgets = u.WidgetLibrary.get_all_widgets_of_class(w, u.TallowKeypadWidget, True)
    check('keypad opens and locks player input', bool(widgets) and pc.is_move_input_ignored() and pc.is_look_input_ignored())
    escape.on_interact(p)
    check('repeated interact keeps a single keypad', len(u.WidgetLibrary.get_all_widgets_of_class(w, u.TallowKeypadWidget, True)) == 1)
    widgets[0].remove_from_parent()
    yield .1
    check('removing keypad releases its input locks', not pc.is_move_input_ignored() and not pc.is_look_input_ignored())
    escape.close_keypad()
    check('closing an already closed keypad is safe', not pc.is_move_input_ignored() and not pc.is_look_input_ignored())
    escape.on_interact(p)
    codes = list(projector.get_editor_property('Codes'))
    wrong = list(codes)
    wrong[0] = (wrong[0] + 1) % 10
    check('incorrect code is rejected', not escape.check_submitted_code(wrong))
    noise = next((obj for obj in u.ObjectIterator(u.TallowNoiseSubsystem) if obj.get_outer() == w), None)
    assert noise, 'PIE world noise subsystem was not found'
    events = [event for event in noise.get_recent_noises(.5) if event.type == u.TallowNoiseType.CUSTOM]
    check('incorrect code emits an audible AI alert', bool(events) and events[-1].range >= 500 and events[-1].get_editor_property('bAlertsAI'),
          {'configured_range_cm': escape.get_editor_property('InputFailedRange'), 'event_range_cm': events[-1].range if events else None})
    escape.close_keypad()
    pc.set_ignore_move_input(True)
    pc.set_ignore_look_input(True)
    escape.on_interact(p)
    check('correct code is accepted', escape.check_submitted_code(codes))
    yield .1
    check('success removes keypad and preserves other input locks', not u.WidgetLibrary.get_all_widgets_of_class(w, u.TallowKeypadWidget, True) and pc.is_move_input_ignored() and pc.is_look_input_ignored())
    pc.set_ignore_move_input(False)
    pc.set_ignore_look_input(False)
    check('success releases exactly one pair of input locks', not pc.is_move_input_ignored() and not pc.is_look_input_ignored())
    ai.trigger_player_capture()
    yield 6
    escape.on_interact(p)
    yield .1
    check('retry requires code entry again', bool(u.WidgetLibrary.get_all_widgets_of_class(w, u.TallowKeypadWidget, True)))
    ai.trigger_player_capture()
    yield .1
    check('capture immediately removes active keypad', not u.WidgetLibrary.get_all_widgets_of_class(w, u.TallowKeypadWidget, True))
    yield 6
    widgets = u.WidgetLibrary.get_all_widgets_of_class(w, u.TallowKeypadWidget, True)
    check('reset closes active keypad', not widgets, [a.get_name() for a in widgets])
    check('reset releases keypad move lock', not pc.is_move_input_ignored())

def finish(error=None):
    if error: RESULT['errors'].append(error)
    RESULT['passed'] = not RESULT['errors'] and all(c['passed'] for c in RESULT['checks'])
    OUT.write_text(json.dumps(RESULT, indent=2))
    ED.editor_request_end_play()
    u.unregister_slate_post_tick_callback(HANDLE)
    u.SystemLibrary.quit_editor()

GEN = main()
NEXT = time.monotonic()
def tick(dt):
    global NEXT
    if time.monotonic() > DEADLINE: finish('Timed out'); return
    if time.monotonic() < NEXT: return
    try: NEXT = time.monotonic() + next(GEN)
    except StopIteration: finish()
    except Exception: finish(traceback.format_exc())
HANDLE = u.register_slate_post_tick_callback(tick)
