"""Actual gameplay camera, inventory, lighting and chair review. Does not save assets."""
import unreal as u,time,json,traceback
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'Tallow/Saved/Integration/Review';OUT.mkdir(parents=True,exist_ok=True)
ED=u.get_editor_subsystem(u.LevelEditorSubsystem)
S={'errors':[],'samples':[],'audit':{}}
assert ED.load_level('/Game/Presentation/Playable/Lvl_Tallow_Playable')
def vec(v):return [v.x,v.y,v.z]
def capture(name):
 u.SystemLibrary.execute_console_command(S['pc'],'HighResShot 1280x720 filename='+str(OUT/(name+'.png')));S['samples'].append(name)
def move(pos,yaw,pitch=-5):
 S['player'].set_actor_location(u.Vector(*pos),False,True);S['pc'].set_control_rotation(u.Rotator(pitch=pitch,yaw=yaw,roll=0))
def main():
 yield 9
 ED.editor_request_begin_play();yield 6
 w=u.EditorLevelLibrary.get_game_world();pc=u.GameplayStatics.get_player_controller(w,0);p=u.GameplayStatics.get_player_pawn(w,0);c=p.get_candle_component()
 d=u.GameplayStatics.get_actor_of_class(w,u.TallowPlayDirector);assert d,'No game director'
 k=u.GameplayStatics.get_actor_of_class(w,u.TallowCreature);chair=u.GameplayStatics.get_actor_of_class(w,u.TallowChairEscape)
 S.update(world=w,pc=pc,player=p,candle=c,keeper=k,director=d)
 S['audit']['initial_seed']=d.active_run_seed
 S['audit']['controls']=[{'action':str(m.action.get_name()),'key':str(u.InputLibrary.key_get_display_name(m.key))} for m in u.load_asset('/Game/Input/IMC_Default').get_editor_property('default_key_mappings').get_editor_property('mappings') if m.action]
 capture('01-chair');yield 2
 for _ in range(1000):
  if not p.is_tied_to_chair():break
  if chair.is_at_rest() or chair.is_timing_window_open():chair.attempt_rock()
  yield .03
 assert not p.is_tied_to_chair(),'Chair timing inputs failed'
 u.GameplayStatics.get_actor_of_class(w,u.TallowCandlePickup).on_interact(p);move((320,-350,-268),-90,-8);yield 3
 capture('02-candle-lit');yield 1
 p.do_cup();yield 2
 capture('03-candle-cupped');yield 1
 p.do_cup_end();c.set_candle_state(u.CandleState.OUT);yield 2
 capture('04-candle-out');yield 1
 p.do_relight();yield .62
 capture('04b-relight-match');yield 1.5
 assert c.get_candle_state()==u.CandleState.LIT,'Relight did not finish'
 move((780,-380,62),-55,-8);yield 3
 capture('05-main-stair');yield 1
 p.set_carried_item(u.CarriedItemType.CROWBAR);yield 2
 capture('07-crowbar-grip');yield 1
 p.clear_carried_item();p.set_carried_item(u.CarriedItemType.FUSE);yield 2
 capture('08-fuse-grip');yield 1
 p.clear_carried_item();move((460,-1040,452),170,-3);yield 3
 capture('06-collection');yield 1
 move((400,-1180,452),156,20);yield 2
 capture('06b-collection-story');yield 1
 move((400,-1070,-268),-77,15);yield 2
 capture('06c-workshop-portrait');yield 1
 move((840,-320,62),-66,25);yield 2
 capture('06d-stair-portrait');yield 1
 S['audit']['player_components']=[{'name':x.get_name(),'class':x.get_class().get_name(),'position':vec(x.get_world_location()),'visible':x.is_visible(),'hidden':x.get_editor_property('hidden_in_game'),'asset':str(x.get_editor_property('static_mesh')) if isinstance(x,u.StaticMeshComponent) else str(x.get_editor_property('skeletal_mesh_asset')) if isinstance(x,u.SkeletalMeshComponent) else ''} for x in p.get_components_by_class(u.PrimitiveComponent)]
 d.toggle_pause();yield 1
 u.SystemLibrary.execute_console_command(pc,'Shot showui filename='+str(OUT/'09-pause-ui.png'));yield 2
 u.WidgetLibrary.get_all_widgets_of_class(w,u.TallowMenuWidget,True)[0].activate_command(u.TallowMenuCommand.RESUME);yield 1
 assert not u.GameplayStatics.is_game_paused(w),'Pause did not resume'
 S['audit']['chair_and_pause']='passed'
 # Actual gameplay Keeper and capture camera, not an isolated pose-review actor.
 k.get_controller().get_component_by_class(u.BrainComponent).stop_logic('Visual review')
 k.get_controller().stop_movement()
 # Stay in the open basement hall. Across the ground-floor study wall is not a valid encounter.
 move((780,-740,-268),90,5)
 k.set_actor_location_and_rotation(u.Vector(780,-500,-234),u.Rotator(pitch=0,yaw=-90,roll=0),False,True)
 yield 2
 capture('10-keeper-idle');yield 1
 k.get_controller().trigger_player_capture();yield .65
 capture('11-keeper-capture');yield 1
 capture('12-keeper-open');yield 6
 assert p.is_tied_to_chair(),'Capture failed to reset to chair'
 S['audit']['capture_reset']='passed'
 # Real directors and cinematics after map travel.
 u.GameplayStatics.open_level(w,'/Game/Presentation/Cinematics/Maps/Lvl_OutroPresentation');yield 3
 w=u.EditorLevelLibrary.get_game_world();S['pc']=u.GameplayStatics.get_player_controller(w,0)
 capture('13-outro-new-candle');yield 9
 u.SystemLibrary.execute_console_command(S['pc'],'Shot showui filename='+str(OUT/'14-outro-ui.png'));yield 2
 u.GameplayStatics.open_level(w,'/Game/Presentation/Cinematics/Maps/Lvl_MenuPresentation');yield 4
 w=u.EditorLevelLibrary.get_game_world();S['pc']=u.GameplayStatics.get_player_controller(w,0)
 u.SystemLibrary.execute_console_command(S['pc'],'Shot showui filename='+str(OUT/'00-main-menu.png'));yield 2
 u.WidgetLibrary.get_all_widgets_of_class(w,u.TallowMenuWidget,True)[0].activate_command(u.TallowMenuCommand.START);yield 3
 capture('00b-intro');yield 1
 ED.editor_request_end_play();yield 3

def done(error=None):
 if error:S['errors'].append(error);u.log_error(error)
 (OUT/'runtime-review.json').write_text(json.dumps({k:S[k] for k in ['errors','samples','audit']},indent=2))
 ED.editor_request_end_play();u.unregister_slate_post_tick_callback(H);u.SystemLibrary.quit_editor()
GEN=main();NEXT=time.monotonic()
def tick(dt):
 global NEXT
 if time.monotonic()<NEXT:return
 try:NEXT=time.monotonic()+next(GEN)
 except StopIteration:done()
 except Exception:done(traceback.format_exc())
H=u.register_slate_post_tick_callback(tick)
