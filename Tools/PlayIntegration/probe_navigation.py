"""Diagnose exact floor projection and opened-door connectivity; never saves gameplay state."""
import unreal as u,time,json,traceback,runpy,math
from pathlib import Path
R=Path(__file__).resolve().parents[2];OUT=R/'Tallow/Saved/Integration/navigation-probe.json';ED=u.get_editor_subsystem(u.LevelEditorSubsystem);A=u.get_editor_subsystem(u.EditorActorSubsystem)
ED.load_level('/Game/Presentation/Playable/Lvl_Tallow_Playable')
DATA={'errors':[],'paths':[],'projections':[],'nav':[]};S={}
for a in A.get_all_level_actors():
 if isinstance(a,u.RecastNavMesh):
  row={}
  for key in ['runtime_generation','agent_radius','agent_height','cell_size','cell_height','min_region_area','merge_region_size']:
   try:row[key]=str(a.get_editor_property(key))
   except Exception as e:row[key]=str(e)
  DATA['nav'].append(row)
def xyz(v):return [v.x,v.y,v.z] if v else None
def main():
 yield 2;ED.editor_request_begin_play();yield 4
 w=u.EditorLevelLibrary.get_game_world();p=u.GameplayStatics.get_player_pawn(w,0);k=u.GameplayStatics.get_actor_of_class(w,u.TallowCreature)
 chair=u.GameplayStatics.get_actor_of_class(w,u.TallowChairEscape)
 while p.is_tied_to_chair():
  if chair.is_at_rest() or chair.is_timing_window_open():chair.attempt_rock()
  yield .025
 for plank in u.GameplayStatics.get_all_actors_of_class(w,u.TallowPlank):plank.on_interact_complete(p)
 for door in u.GameplayStatics.get_all_actors_of_class(w,u.TallowDoor):
  door.on_interact(p)
 box=u.GameplayStatics.get_actor_of_class(w,u.TallowFuseBox)
 for _ in range(3):p.set_carried_item(u.CarriedItemType.FUSE);box.on_interact_complete(p)
 p.clear_carried_item();k.get_controller().stop_movement();k.get_controller().get_component_by_class(u.BrainComponent).stop_logic('Navigation diagnosis')
 yield 5
 anchors={'basement_hall':[780,-700,-320],'start':[320,-350,-320],'workshop':[400,-900,-320],'dining':[450,-1000,10],'parlour':[300,-350,10],'ground_hall':[780,-350,10],'toy':[300,-350,400],'collection':[400,-1000,400]}
 for phase in ['before_rebuild','after_rebuild']:
  if phase=='after_rebuild':u.SystemLibrary.execute_console_command(w,'RebuildNavigation');yield 8
  for label,point in anchors.items():
   dest=u.NavigationSystemV1.project_point_to_navigation(w,u.Vector(*point),None,None,u.Vector(40,40,30))
   DATA['projections'].append({'phase':phase,'label':label,'input':point,'projected':xyz(dest)})
   if dest:
    path=u.NavigationSystemV1.find_path_to_location_synchronously(w,u.Vector(780,-350,10),dest,p)
    DATA['paths'].append({'phase':phase,'label':label,'valid':bool(path and path.is_valid()),'partial':bool(path and path.is_partial()),'points':[xyz(v) for v in path.path_points] if path else []})
  for x in range(100,600,100):
   for y in range(-1100,-100,100):
    dest=u.NavigationSystemV1.project_point_to_navigation(w,u.Vector(x,y,-320),None,None,u.Vector(20,20,30))
    DATA['projections'].append({'phase':phase,'label':'west_basement_grid','input':[x,y,-320],'projected':xyz(dest)})
 yield .1

def done(e=None):
 if e:DATA['errors'].append(e);u.log_error(e)
 OUT.write_text(json.dumps(DATA,indent=2));ED.editor_request_end_play();u.unregister_slate_post_tick_callback(H);u.SystemLibrary.quit_editor()
G=main();NEXT=0;START=time.monotonic()
def tick(dt):
 global NEXT
 if time.monotonic()-START>80:done('80s timeout');return
 if time.monotonic()<NEXT:return
 try:NEXT=time.monotonic()+next(G)
 except StopIteration:done()
 except Exception:done(traceback.format_exc())
H=u.register_slate_post_tick_callback(tick)
