"""Frames of the real in-game capture (JumpscareDeath) from the player's camera, timed against its strike.
Basement hall, Keeper ~2 m away; never saves. Run with -RenderOffscreen. Output: Saved/Integration/Review/js-*.png
"""
import unreal as u,time,traceback
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'Tallow/Saved/Integration/Review';OUT.mkdir(parents=True,exist_ok=True)
ED=u.get_editor_subsystem(u.LevelEditorSubsystem);S={}
assert ED.load_level('/Game/Presentation/Playable/Lvl_Tallow_Playable')
TIMES=[.3,.6,.85,.95,1.02,1.07,1.12]  # strike 1.042 s, game fade from ~1.12 s
def main():
    yield 9
    ED.editor_request_begin_play();yield 6
    w=u.EditorLevelLibrary.get_game_world();pc=u.GameplayStatics.get_player_controller(w,0);p=u.GameplayStatics.get_player_pawn(w,0)
    k=u.GameplayStatics.get_actor_of_class(w,u.TallowCreature);chair=u.GameplayStatics.get_actor_of_class(w,u.TallowChairEscape)
    for _ in range(1000):
        if not p.is_tied_to_chair():break
        if chair.is_at_rest() or chair.is_timing_window_open():chair.attempt_rock()
        yield .03
    u.GameplayStatics.get_actor_of_class(w,u.TallowCandlePickup).on_interact(p);yield 2
    k.get_controller().get_component_by_class(u.BrainComponent).stop_logic('Jumpscare review');k.get_controller().stop_movement()
    p.set_actor_location(u.Vector(780,-740,-268),False,True);pc.set_control_rotation(u.Rotator(pitch=0,yaw=90,roll=0))
    k.set_actor_location_and_rotation(u.Vector(780,-540,-234),u.Rotator(pitch=0,yaw=-90,roll=0),False,True);yield 2.5
    # Slow motion so every screenshot lands on its game time (offscreen frames are slow and shots get dropped).
    u.SystemLibrary.execute_console_command(pc,'slomo 0.1');yield .5
    k.get_controller().trigger_player_capture();start=u.GameplayStatics.get_time_seconds(w)
    for t in TIMES:
        while u.GameplayStatics.get_time_seconds(w)-start<t:yield .01
        u.SystemLibrary.execute_console_command(pc,'HighResShot 1280x720 filename='+str(OUT/('js-%04d.png'%int(t*1000))));yield 1.2
    u.SystemLibrary.execute_console_command(pc,'slomo 1');yield 4
    ED.editor_request_end_play();yield 2
def done(e=None):
    if e:u.log_error(e)
    ED.editor_request_end_play();u.unregister_slate_post_tick_callback(H);u.SystemLibrary.quit_editor()
G=main();NEXT=0
def tick(dt):
    global NEXT
    if time.monotonic()<NEXT:return
    try:NEXT=time.monotonic()+next(G)
    except StopIteration:done()
    except Exception:done(traceback.format_exc())
H=u.register_slate_post_tick_callback(tick)
