"""Finite PIE integration checks through the existing interactions and menu widget. No asset saves."""
import unreal as u,time,json,traceback
from pathlib import Path
R=Path(__file__).resolve().parents[2];OUT=R/'Tallow/Saved/Integration/flow-test.json';ED=u.get_editor_subsystem(u.LevelEditorSubsystem)
RESULT={'checks':[],'errors':[]};S={};DEADLINE=time.monotonic()+180
assert ED.load_level('/Game/Presentation/Cinematics/Maps/Lvl_MenuPresentation')
def check(name,value,detail=None):
 RESULT['checks'].append({'name':name,'passed':bool(value),'detail':detail});u.log('FLOW_CHECK '+name+' '+str(bool(value)))
def world():return u.EditorLevelLibrary.get_game_world()
def one(cls):return u.GameplayStatics.get_actor_of_class(world(),cls)
def actors(cls):return u.GameplayStatics.get_all_actors_of_class(world(),cls)
def pos(a):
 v=a.get_actor_location();return [round(v.x,3),round(v.y,3),round(v.z,3)]
def freeze():
 k=one(u.TallowCreature)
 if k:
  ai=k.get_controller();ai.stop_movement();ai.get_component_by_class(u.BrainComponent).stop_logic('Deterministic integration checks')
def main():
 yield 2
 ED.editor_request_begin_play();yield 2
 d=one(u.TallowPlayDirector);check('main menu owns input',d.is_menu_open())
 menu=u.WidgetLibrary.get_all_widgets_of_class(world(),u.TallowMenuWidget,True)[0];menu.activate_command(u.TallowMenuCommand.START);yield 2
 check('start plays intro before map travel','MenuPresentation' in world().get_name() and not d.is_menu_open())
 d.skip_intro();yield 4
 d=one(u.TallowPlayDirector);p=u.GameplayStatics.get_player_pawn(world(),0);c=p.get_candle_component();chair=one(u.TallowChairEscape)
 check('intro loads playable chair',p.is_tied_to_chair() and 'Playable' in world().get_name())
 seed=d.active_run_seed;placements={a.get_name():pos(a) for a in actors(u.TallowFusePickup)+actors(u.TallowCrowbarPickup)}
 check('new run chooses seeded placements',seed!=0 and len(placements)==4,placements)
 d.toggle_pause();yield .1
 check('pause suspends world',u.GameplayStatics.is_game_paused(world()))
 u.WidgetLibrary.get_all_widgets_of_class(world(),u.TallowMenuWidget,True)[0].activate_command(u.TallowMenuCommand.RESUME);yield .1
 check('resume returns to game',not u.GameplayStatics.is_game_paused(world()) and not d.is_menu_open())
 for _ in range(900):
  if not p.is_tied_to_chair():break
  if chair.is_at_rest() or chair.is_timing_window_open():chair.attempt_rock()
  yield .02
 check('existing chair minigame completes',not p.is_tied_to_chair())
 one(u.TallowCandlePickup).on_interact(p);yield .1
 check('candle pickup gives new presentation',c.is_acquired and next(x for x in p.get_components_by_class(u.StaticMeshComponent) if x.get_name()=='IntegratedCandle').is_visible())
 p.set_actor_location(u.Vector(80,-200,-268),False,True);p.do_cup();yield 1.8
 check('cupping protects flame in draft',c.get_candle_state()==u.CandleState.CUPPED)
 p.do_cup_end();yield 1.8
 check('unprotected draft extinguishes candle',c.get_candle_state()==u.CandleState.OUT)
 p.set_actor_location(u.Vector(320,-350,-268),False,True);yield .1
 p.do_relight();yield 1.5
 check('relight finishes after animation duration',c.get_candle_state()==u.CandleState.LIT)
 bar=one(u.TallowCrowbarPickup);bar.on_interact(p);yield .1
 check('original crowbar pickup feeds held mesh',p.get_carried_item()==u.CarriedItemType.CROWBAR and next(x for x in p.get_components_by_class(u.StaticMeshComponent) if x.get_name()=='IntegratedHeldTool').static_mesh.get_name()=='SM_ToolV02_Crowbar')
 planks=actors(u.TallowPlank)
 for plank in planks:
  plank.on_interact_start(p);plank.on_interact_complete(p);yield .1
 freeze();yield 2
 check('all boards release companion route',d.boards_cleared)
 check('final gate still requires power',not d.can_escape())
 # Drop uses the existing item registry; the presentation adapter skins newly spawned actors.
 p.set_actor_location(u.Vector(780,-325,-268),False,True);yield .2
 old_count=len(actors(u.TallowCrowbarPickup));p.do_drop_item();yield .3
 dropped=actors(u.TallowCrowbarPickup)
 check('drop preserves gameplay and new prop',p.get_carried_item()==u.CarriedItemType.NONE and len(dropped)==old_count+1 and all(a.get_component_by_class(u.StaticMeshComponent).static_mesh.get_name()=='SM_ToolV02_Crowbar' for a in dropped))
 box=one(u.TallowFuseBox)
 for fuse in actors(u.TallowFusePickup):
  fuse.on_interact(p);yield .1
  check('fuse picked up '+fuse.get_name(),p.get_carried_item()==u.CarriedItemType.FUSE)
  box.on_interact_start(p);box.on_interact_complete(p);yield .1
 freeze();yield 2
 check('three existing fuse insertions restore power',box.current_fuses==3 and d.power_restored and d.can_escape())
 lights=u.GameplayStatics.get_all_actors_with_tag(world(),'Tallow.PowerLight')
 check('power turns on authored practicals',bool(lights) and all(l.get_component_by_class(u.LightComponent).is_visible() for l in lights))
 # Existing capture/reset must preserve the seed/placement and close all progression again.
 one(u.TallowCreature).get_controller().trigger_player_capture();yield .15
 one(u.TallowEscapePoint).on_interact_complete(p);yield .15
 check('capture blocks a late escape interaction',not d.can_escape() and 'Playable' in world().get_name())
 yield 6.7
 check('capture returns to chair',p.is_tied_to_chair())
 check('capture preserves rolled items',d.active_run_seed==seed and {a.get_name():pos(a) for a in actors(u.TallowFusePickup)+actors(u.TallowCrowbarPickup)}==placements)
 check('capture clears power and route',not d.power_restored and not d.boards_cleared and box.current_fuses==0)
 # Exercise final travel and retry through existing callbacks after reset.
 p.set_carried_item(u.CarriedItemType.CROWBAR)
 for plank in planks:plank.on_interact_complete(p)
 p.clear_carried_item()
 for fuse in actors(u.TallowFusePickup):fuse.on_interact(p);box.on_interact_complete(p)
 freeze();yield .2
 one(u.TallowEscapePoint).on_interact_complete(p);yield 3
 check('release latch loads ending','OutroPresentation' in world().get_name())
 yield 10
 d=one(u.TallowPlayDirector);check('outro ends on replay menu',d.is_menu_open())
 u.WidgetLibrary.get_all_widgets_of_class(world(),u.TallowMenuWidget,True)[0].activate_command(u.TallowMenuCommand.RESTART);yield 4
 d=one(u.TallowPlayDirector)
 check('replay creates a fresh chair run','Playable' in world().get_name() and d.active_run_seed!=seed and u.GameplayStatics.get_player_pawn(world(),0).is_tied_to_chair())
 p=u.GameplayStatics.get_player_pawn(world(),0);c=p.get_candle_component();light=p.get_component_by_class(u.PointLightComponent)
 # Regression: resetting an already-out candle must cancel its outstanding relight.
 c.acquire_candle(True);c.set_candle_state(u.CandleState.OUT);c.start_relighting();yield .2
 c.reset_candle();yield 1.3
 check('reset cancels relight and hidden light stays off',not c.is_acquired and c.get_candle_state()==u.CandleState.OUT and c.get_editor_property('match_count')==3 and not light.is_visible())
 c.acquire_candle(True);c.set_candle_state(u.CandleState.OUT);c.start_relighting();yield .2
 c.reset_candle();c.acquire_candle(False);yield 1.3
 check('reacquisition cannot revive an old relight timer',c.get_candle_state()==u.CandleState.OUT and c.get_editor_property('match_count')==3)
 c.start_relighting();yield 1.3
 check('fresh relight still consumes one match',c.get_candle_state()==u.CandleState.LIT and c.get_editor_property('match_count')==2 and light.is_visible())
 ED.editor_request_end_play();yield 2

def done(error=None):
 if error:RESULT['errors'].append(error);u.log_error(error)
 RESULT['passed']=not RESULT['errors'] and all(c['passed'] for c in RESULT['checks'])
 OUT.write_text(json.dumps(RESULT,indent=2));ED.editor_request_end_play();u.unregister_slate_post_tick_callback(H);u.SystemLibrary.quit_editor()
GEN=main();NEXT=time.monotonic()
def tick(dt):
 global NEXT
 if time.monotonic()>DEADLINE:done('Timed out after180s');return
 if time.monotonic()<NEXT:return
 try:NEXT=time.monotonic()+next(GEN)
 except StopIteration:done()
 except Exception:done(traceback.format_exc())
H=u.register_slate_post_tick_callback(tick)
