"""Candle-lit gameplay-camera captures of the spots audit_architecture.py flags (openings, floor holes).
Keeper parked far away; never saves. Run with -RenderOffscreen. Output: Saved/Integration/Review/arch-*.png
"""
import unreal as u,time,traceback
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'Tallow/Saved/Integration/Review';OUT.mkdir(parents=True,exist_ok=True)
ED=u.get_editor_subsystem(u.LevelEditorSubsystem);S={}
assert ED.load_level('/Game/Presentation/Playable/Lvl_Tallow_Playable')
# label, player capsule location (floor + 62), yaw (90 = toward the front wall), pitch
SPOTS=[('arch-study-window',(1300,-260,62),90,8),('arch-bedroom-window',(1300,-260,452),90,8),('arch-front-door',(780,-260,62),90,-18),
       ('arch-ground-west-hole',(320,-330,62),180,-38),('arch-start-room-ceiling',(260,-330,-268),180,35),('arch-front-facade',(1000,500,150),-90,10)]
def capture(name):u.SystemLibrary.execute_console_command(S['pc'],'HighResShot 1280x720 filename='+str(OUT/(name+'.png')))
def main():
    yield 9
    ED.editor_request_begin_play();yield 6
    w=u.EditorLevelLibrary.get_game_world();pc=u.GameplayStatics.get_player_controller(w,0);p=u.GameplayStatics.get_player_pawn(w,0)
    k=u.GameplayStatics.get_actor_of_class(w,u.TallowCreature);chair=u.GameplayStatics.get_actor_of_class(w,u.TallowChairEscape);S.update(pc=pc)
    for _ in range(1000):
        if not p.is_tied_to_chair():break
        if chair.is_at_rest() or chair.is_timing_window_open():chair.attempt_rock()
        yield .03
    u.GameplayStatics.get_actor_of_class(w,u.TallowCandlePickup).on_interact(p);yield 2
    k.get_controller().get_component_by_class(u.BrainComponent).stop_logic('Architecture review');k.get_controller().stop_movement()
    k.set_actor_location(u.Vector(-8000,-8000,1000),False,True);k.get_component_by_class(u.CharacterMovementComponent).disable_movement()
    move=p.get_component_by_class(u.CharacterMovementComponent)
    for name,pos,yaw,pitch in SPOTS:
        move.set_movement_mode(u.MovementMode.MOVE_FLYING)  # hold the camera exactly where placed
        p.set_actor_location(u.Vector(*pos),False,True);pc.set_control_rotation(u.Rotator(pitch=pitch,yaw=yaw,roll=0));yield 2.5
        capture(name);yield 1.5
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
