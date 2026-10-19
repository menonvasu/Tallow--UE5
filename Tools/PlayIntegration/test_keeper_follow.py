"""Can the Keeper actually follow the player everywhere? Finite PIE check on Lvl_Tallow_Playable; never saves.
1. Room tour: the Keeper walks room to room with its REAL capsule and CharacterMovement through AI path
   following (brain stopped, its own nav filter). A leg fails if it stalls or times out; it is then placed
   at the target so the tour continues and every failure is recorded.
2. Floor grid (25cm): navmesh coverage against where each capsule physically fits.
   '.' nav+Keeper fits  'x' nav but Keeper does NOT fit (it plans paths it gets stuck on)
   'o' Keeper fits but no nav (hole: it can stand there, never paths there)
   'p' only the player fits (child squeeze)  '#' blocked  ' ' no floor
"""
import json,math,time,traceback
from pathlib import Path
import unreal as u
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'Tallow/Saved/Integration/keeper-follow.json'
MAP='/Game/Presentation/Playable/Lvl_Tallow_Playable';ED=u.get_editor_subsystem(u.LevelEditorSubsystem)
FLOORS={'basement':-330,'ground':0,'upper':390};BOUNDS=(0,1575,-1300,25);STEP=25
TOUR=[('start',300,-350,'basement'),('workshop_bench',300,-960,'basement'),('workshop_cabinet',440,-860,'basement'),
      ('basement_hall',780,-700,'basement'),('boiler',1380,-420,'basement'),('laundry',1425,-1000,'basement'),
      ('basement_hall_return',780,-700,'basement'),('ground_hall',780,-350,'ground'),('parlour',400,-520,'ground'),
      ('parlour_sofa_exit',430,-60,'ground'),('dining',450,-1000,'ground'),('study',1300,-330,'ground'),
      ('study_cupboard_exit',1320,-100,'ground'),('upper_hall',780,-1050,'upper'),('toy',300,-350,'upper'),
      ('bedroom',1200,-500,'upper'),('collection',300,-1000,'upper'),('start_return',300,-350,'basement')]
REPORT={'map':MAP,'legs':[],'grid':{},'counts':{},'errors':[]}
def prop(a,n):return a.get_editor_property(n)
def xyz(v):return [round(v.x,1),round(v.y,1),round(v.z,1)]
def flat(a,b):return math.hypot(a.x-b.x,a.y-b.y)

def grid(w,profiles,ignore):
    x0,x1,y0,y1=BOUNDS
    for floor,z in FLOORS.items():
        rows=[];counts={}
        for y in range(y1,y0-1,-STEP):
            row=''
            for x in range(x0,x1+1,STEP):
                floor_hit=u.SystemLibrary.line_trace_single(w,u.Vector(x,y,z+45),u.Vector(x,y,z-30),u.TraceTypeQuery.TRACE_TYPE_QUERY1,False,ignore,u.DrawDebugTrace.NONE,True)
                if floor_hit is None:row+=' ';continue
                proj=u.NavigationSystemV1.project_point_to_navigation(w,u.Vector(x,y,z+20),None,None,u.Vector(8,8,45))
                nav=proj is not None and abs(proj.z-z)<45 and flat(proj,u.Vector(x,y,z))<12
                keeper=u.SystemLibrary.capsule_trace_single_by_profile(w,u.Vector(x,y,z+99),u.Vector(x,y,z+99.1),42,96,profiles[1],False,ignore,u.DrawDebugTrace.NONE,True) is None
                player=u.SystemLibrary.capsule_trace_single_by_profile(w,u.Vector(x,y,z+63),u.Vector(x,y,z+63.1),30,60,profiles[0],False,ignore,u.DrawDebugTrace.NONE,True) is None
                c='.' if nav and keeper else 'x' if nav else 'o' if keeper else 'p' if player else '#'
                counts[c]=counts.get(c,0)+1;row+=c
            rows.append(row)
        REPORT['grid'][floor]=rows;REPORT['counts'][floor]=counts
        yield .05

def main():
    assert ED.load_level(MAP)
    yield 3
    ED.editor_request_begin_play();yield 4
    w=u.EditorLevelLibrary.get_game_world();p=u.GameplayStatics.get_player_pawn(w,0);k=u.GameplayStatics.get_actor_of_class(w,u.TallowCreature)
    assert w and p and k
    chair=u.GameplayStatics.get_actor_of_class(w,u.TallowChairEscape);deadline=time.monotonic()+18
    while p.is_tied_to_chair() and time.monotonic()<deadline:
        if chair.is_at_rest() or chair.is_timing_window_open():chair.attempt_rock()
        yield .025
    director=u.GameplayStatics.get_actor_of_class(w,u.TallowPlayDirector)
    if director:director.set_actor_tick_enabled(False)
    for plank in u.GameplayStatics.get_all_actors_of_class(w,u.TallowPlank):plank.call_method('OnInteractComplete',(p,))
    for door in u.GameplayStatics.get_all_actors_of_class(w,u.TallowDoor):door.on_interact(p)
    box=u.GameplayStatics.get_actor_of_class(w,u.TallowFuseBox)
    for _ in range(int(prop(box,'required_fuses'))):
        if int(prop(box,'current_fuses'))>=int(prop(box,'required_fuses')):break
        p.set_carried_item(u.CarriedItemType.FUSE);box.call_method('OnInteractComplete',(p,))
    p.clear_carried_item()
    ai=k.get_controller();brain=ai.get_component_by_class(u.BrainComponent)
    if brain:brain.stop_logic('Keeper follow check')
    ai.stop_movement()
    for component in k.get_components_by_class(u.ActorComponent):
        if component.get_class().get_name()=='TallowSurfaceTraversalComponent':component.set_component_tick_enabled(False)
    # Park the player out of sight so perception never interferes; the Keeper walks alone.
    p.set_actor_location(u.Vector(-10000,-10000,1000),False,True);p.get_component_by_class(u.CharacterMovementComponent).disable_movement()
    move=k.get_component_by_class(u.CharacterMovementComponent);move.set_movement_mode(u.MovementMode.MOVE_WALKING);move.set_editor_property('max_walk_speed',420)
    capsule=k.get_component_by_class(u.CapsuleComponent)
    REPORT['keeper_capsule']=[capsule.get_scaled_capsule_radius(),capsule.get_scaled_capsule_half_height()]
    for nav in u.GameplayStatics.get_all_actors_of_class(w,u.RecastNavMesh):
        REPORT['navmesh']={'agent_radius':prop(nav,'agent_radius'),'agent_height':prop(nav,'agent_height')}
    yield 3  # gates swing open; dynamic navmesh rebuilds around them
    for name,x,y,floor in TOUR:
        z=FLOORS[floor];goal=u.NavigationSystemV1.project_point_to_navigation(w,u.Vector(x,y,z+20),None,None,u.Vector(80,80,60))
        leg={'leg':name,'target':[x,y,z],'from':xyz(k.get_actor_location())}
        if goal is None:leg.update(result='no navmesh near target');REPORT['legs'].append(leg);continue
        leg['goal']=xyz(goal)
        path=u.NavigationSystemV1.find_path_to_location_synchronously(w,k.get_actor_location(),goal,k)
        leg['path_valid']=bool(path and path.is_valid());leg['path_partial']=bool(path and path.is_partial())
        leg['path']=[xyz(v) for v in path.get_editor_property('path_points')] if path else []
        # The same query through the AI's own phase filter (Move To falls back to DefaultNavigationFilterClass).
        fpath=u.NavigationSystemV1.find_path_to_location_synchronously(w,k.get_actor_location(),goal,ai,u.TallowNavFilter_PhaseOne)
        leg['ai_filter_partial']=bool(fpath and fpath.is_partial());leg['ai_path']=[xyz(v) for v in fpath.get_editor_property('path_points')] if fpath else []
        ai.move_to_location(goal,acceptance_radius=35,stop_on_overlap=True,use_pathfinding=True,project_destination_to_navigation=False,can_strafe=False,filter_class=None,allow_partial_path=True)
        # Stalled = the body stops moving while the goal is still away (stairs and detours legitimately move away from it).
        # Its own animation component holds it near patrol pace (~65cm/s); allow 60cm/s along the planned path.
        length=sum((u.Vector(*a)-u.Vector(*b)).length() for a,b in zip(leg['ai_path'],leg['ai_path'][1:]));limit=15+length/60
        start=time.monotonic();best=flat(k.get_actor_location(),goal);anchor=k.get_actor_location();moved_at=start;trail=[]
        while True:
            yield .2
            here=k.get_actor_location();d=flat(here,goal);trail.append(xyz(here));best=min(best,d)
            if (here-anchor).length()>25:anchor=here;moved_at=time.monotonic()
            if d<90 and abs(here.z-goal.z)<130:leg['result']='arrived';break
            if time.monotonic()-moved_at>3:leg['result']='stalled';break
            if time.monotonic()-start>limit:leg['result']='timeout';break
        leg.update(seconds=round(time.monotonic()-start,1),end=xyz(k.get_actor_location()),remaining_cm=round(best),trail=trail[::3])
        REPORT['legs'].append(leg);u.log('KEEPER_FOLLOW '+name+' '+leg['result']+' remaining='+str(round(best)))
        if leg['result']!='arrived':
            ai.stop_movement();k.set_actor_location(goal+u.Vector(0,0,capsule.get_scaled_capsule_half_height()+5),False,True);yield .3
    ai.stop_movement();k.set_actor_location(u.Vector(-10000,-9000,1000),False,True);move.disable_movement()
    yield from grid(w,[p.get_component_by_class(u.CapsuleComponent).get_collision_profile_name(),capsule.get_collision_profile_name()],[p,k])
    REPORT['passed']=all(l.get('result')=='arrived' for l in REPORT['legs'])

def done(e=None):
    if e:REPORT['errors'].append(e);u.log_error(e)
    OUT.write_text(json.dumps(REPORT,indent=1));ED.editor_request_end_play();u.unregister_slate_post_tick_callback(H);u.SystemLibrary.quit_editor()
G=main();NEXT=0;START=time.monotonic()
def tick(dt):
    global NEXT
    if time.monotonic()-START>600:done('600s timeout');return
    if time.monotonic()<NEXT:return
    try:NEXT=time.monotonic()+next(G)
    except StopIteration:done()
    except Exception:done(traceback.format_exc())
H=u.register_slate_post_tick_callback(tick)
