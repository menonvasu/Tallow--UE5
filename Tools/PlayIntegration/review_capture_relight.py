"""Focused offscreen review in an unobstructed hall; never saves game/map state."""
import unreal as u,time,json,traceback
from pathlib import Path
R=Path(__file__).resolve().parents[2];O=R/'Tallow/Saved/Integration/Review';ED=u.get_editor_subsystem(u.LevelEditorSubsystem)
assert ED.load_level('/Game/Presentation/Playable/Lvl_Tallow_Playable')
S={'errors':[],'samples':[]};START=time.monotonic()
def xyz(v):return [v.x,v.y,v.z]
def capture(name):
 u.SystemLibrary.execute_console_command(S['pc'],'HighResShot 1280x720 filename='+str(O/(name+'.png')));S['samples'].append(name)
def main():
 yield 3;ED.editor_request_begin_play();yield 4
 w=u.EditorLevelLibrary.get_game_world();p=u.GameplayStatics.get_player_pawn(w,0);pc=u.GameplayStatics.get_player_controller(w,0);c=p.get_candle_component();chair=u.GameplayStatics.get_actor_of_class(w,u.TallowChairEscape);k=u.GameplayStatics.get_actor_of_class(w,u.TallowCreature);S['pc']=pc
 while p.is_tied_to_chair():
  if chair.is_at_rest() or chair.is_timing_window_open():chair.attempt_rock()
  yield .03
 u.GameplayStatics.get_actor_of_class(w,u.TallowCandlePickup).on_interact(p)
 p.set_actor_location(u.Vector(320,-350,-268),False,True);pc.set_control_rotation(u.Rotator(pitch=-8,yaw=-90,roll=0));yield 1
 c.set_candle_state(u.CandleState.OUT);p.do_relight();yield .63
 capture('04b-relight-match')
 S['match_components']=[{'name':a.get_name(),'location':xyz(a.get_world_location()),'rotation':str(a.get_world_rotation()),'visible':a.is_visible(),'hidden':a.get_editor_property('hidden_in_game')} for a in p.get_components_by_class(u.SceneComponent) if 'Match' in a.get_name()]
 S['flames']=[{'name':a.get_name(),'standalone':a.standalone,'state':str(a.standalone_state),'location':xyz(a.get_visual_mesh().get_world_location()),'scale':xyz(a.get_visual_mesh().get_world_scale()),'visible':a.get_visual_mesh().is_visible(),'hidden':a.get_visual_mesh().get_editor_property('hidden_in_game')} for a in p.get_components_by_class(u.TallowFlameVisualComponent)]
 yield 2
 k.get_controller().get_component_by_class(u.BrainComponent).stop_logic('Capture review');k.get_controller().stop_movement()
 p.set_actor_location(u.Vector(780,-740,-268),False,True);pc.set_control_rotation(u.Rotator(pitch=5,yaw=90,roll=0))
 k.set_actor_location_and_rotation(u.Vector(780,-500,-234),u.Rotator(pitch=0,yaw=-90,roll=0),False,True);yield 2
 S['encounter']={'player':xyz(p.get_actor_location()),'keeper':xyz(k.get_actor_location()),'head':xyz(k.get_component_by_class(u.SkeletalMeshComponent).get_socket_location('head'))}
 capture('10-keeper-idle');yield 1
 k.get_controller().trigger_player_capture();yield .65
 capture('11-keeper-capture');yield .75
 capture('12-keeper-open');yield 6
 S['reset_to_chair']=p.is_tied_to_chair();assert S['reset_to_chair']
 ED.editor_request_end_play();yield 2
def done(e=None):
 if e:S['errors'].append(e);u.log_error(e)
 (O/'capture-relight-review.json').write_text(json.dumps({k:v for k,v in S.items() if k!='pc'},indent=2));ED.editor_request_end_play();u.unregister_slate_post_tick_callback(H);u.SystemLibrary.quit_editor()
G=main();NEXT=0
def tick(dt):
 global NEXT
 if time.monotonic()-START>90:done('90s timeout');return
 if time.monotonic()<NEXT:return
 try:NEXT=time.monotonic()+next(G)
 except StopIteration:done()
 except Exception:done(traceback.format_exc())
H=u.register_slate_post_tick_callback(tick)
