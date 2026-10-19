"""Every spawn candidate must have a complete route and a real E-interaction from clear floor."""
import unreal as u,time,json,traceback,math
from pathlib import Path
R=Path(__file__).resolve().parents[2];PLAN=json.loads((R/'Tallow/SourceAssets/Production/HouseIntegrationV01/route-plan.json').read_text());OUT=R/'Tallow/Saved/Integration/candidate-access.json';ED=u.get_editor_subsystem(u.LevelEditorSubsystem)
assert ED.load_level('/Game/Presentation/Playable/Lvl_Tallow_Playable')
D={'candidates':[],'errors':[]};S={}
def xyz(v):return [v.x,v.y,v.z]
def distance(a,b):return math.sqrt(sum((x-y)**2 for x,y in zip(xyz(a),xyz(b))))
def main():
 yield 2;ED.editor_request_begin_play();yield 4
 w=u.EditorLevelLibrary.get_game_world();p=u.GameplayStatics.get_player_pawn(w,0);pc=u.GameplayStatics.get_player_controller(w,0);k=u.GameplayStatics.get_actor_of_class(w,u.TallowCreature)
 chair=u.GameplayStatics.get_actor_of_class(w,u.TallowChairEscape)
 while p.is_tied_to_chair():
  if chair.is_at_rest() or chair.is_timing_window_open():chair.attempt_rock()
  yield .025
 for plank in u.GameplayStatics.get_all_actors_of_class(w,u.TallowPlank):plank.on_interact_complete(p)
 for door in u.GameplayStatics.get_all_actors_of_class(w,u.TallowDoor):door.on_interact(p)
 k.get_controller().stop_movement();k.get_controller().get_component_by_class(u.BrainComponent).stop_logic('Candidate access check')
 k.set_actor_location(u.Vector(-10000,-10000,1000),False,True);k.get_component_by_class(u.CharacterMovementComponent).disable_movement()
 yield 5
 item_actors={u.CarriedItemType.CROWBAR:u.GameplayStatics.get_actor_of_class(w,u.TallowCrowbarPickup),u.CarriedItemType.FUSE:u.GameplayStatics.get_actor_of_class(w,u.TallowFusePickup)}
 interaction=p.get_component_by_class(u.TallowInteractionComponent);camera=p.get_component_by_class(u.CameraComponent)
 for row in PLAN['crowbar_candidates']+PLAN['fuse_candidates']:
  typ=u.CarriedItemType.CROWBAR if row['id'].startswith('Crowbar') else u.CarriedItemType.FUSE
  actor=item_actors[typ];p.clear_carried_item();actor.set_actor_hidden_in_game(False);actor.set_actor_enable_collision(True)
  origin,ext=actor.get_actor_bounds(False);lift=actor.get_actor_location().z-(origin.z-ext.z)
  loc=list(row['location']);loc[2]+=lift;actor.set_actor_location(u.Vector(*loc),False,True)
  floor=-330 if loc[2]<-100 else 390 if loc[2]>300 else 0
  result={'id':row['id'],'location':loc,'lift':lift,'trials':[],'passed':False,'rejected':{'projection':0,'distance':0,'path':0,'sweep':0}};D['candidates'].append(result)
  for radius in [85,125,170]:
   if result['passed']:break
   for index in range(12):
    angle=index*math.tau/12;sample=u.Vector(loc[0]+math.cos(angle)*radius,loc[1]+math.sin(angle)*radius,floor+10)
    projected=u.NavigationSystemV1.project_point_to_navigation(w,sample,None,None,u.Vector(30,30,25))
    if projected is None:result['rejected']['projection']+=1;continue
    target_capsule=projected+u.Vector(0,0,62)
    if distance(target_capsule,actor.get_actor_location())>195:result['rejected']['distance']+=1;continue
    paths=[u.NavigationSystemV1.find_path_to_location_synchronously(w,u.Vector(780,-700,-320),projected,pawn) for pawn in [p,k]]
    if not all(path and path.is_valid() and not path.is_partial() for path in paths):result['rejected']['path']+=1;continue
    sweep=u.SystemLibrary.capsule_trace_single_by_profile(w,target_capsule,target_capsule+u.Vector(0,0,.1),30,60,p.get_component_by_class(u.CapsuleComponent).get_collision_profile_name(),False,[p,k],u.DrawDebugTrace.NONE,True)
    if sweep is not None:result['rejected']['sweep']+=1;continue
    p.set_actor_location(target_capsule,False,True);yield .12
    origin,ext=actor.get_actor_bounds(False)
    # The inherited spring arm shifts the camera as it rotates. Converge actual view aim.
    for _ in range(6):
     pc.set_control_rotation(u.MathLibrary.find_look_at_rotation(camera.get_world_location(),origin));yield .035
    ray=u.SystemLibrary.line_trace_single(w,camera.get_world_location(),camera.get_world_location()+camera.get_forward_vector()*200,u.TraceTypeQuery.TRACE_TYPE_QUERY1,False,[p],u.DrawDebugTrace.NONE,True)
    hit_names=[x.get_name() for x in ray.to_tuple() if isinstance(x,u.Actor)] if ray and hasattr(ray,'to_tuple') else [str(ray)]
    interaction.start_interact();yield .08
    picked=p.get_carried_item()==typ
    result['trials'].append({'approach':xyz(p.get_actor_location()),'complete_paths_player_keeper':True,'picked_by_camera_ray':picked,'hit_actors':hit_names,'item_bounds':xyz(origin),'camera':xyz(camera.get_world_location()),'mesh_visible':actor.get_component_by_class(u.StaticMeshComponent).is_visible(),'carried':str(p.get_carried_item())})
    if picked:result['passed']=True;break
  actor.set_actor_hidden_in_game(True);actor.set_actor_enable_collision(False)
  u.log('CANDIDATE_ACCESS '+row['id']+' '+str(result['passed']))
 expected=len(PLAN['crowbar_candidates'])+len(PLAN['fuse_candidates'])
 D['passed']=len(D['candidates'])==expected and expected>0 and all(r['passed'] for r in D['candidates'])

def done(e=None):
 if e:D['errors'].append(e);u.log_error(e)
 OUT.write_text(json.dumps(D,indent=2));ED.editor_request_end_play();u.unregister_slate_post_tick_callback(H);u.SystemLibrary.quit_editor()
G=main();NEXT=0;START=time.monotonic()
def tick(dt):
 global NEXT
 if time.monotonic()-START>100:done('100s timeout');return
 if time.monotonic()<NEXT:return
 try:NEXT=time.monotonic()+next(G)
 except StopIteration:done()
 except Exception:done(traceback.format_exc())
H=u.register_slate_post_tick_callback(tick)
