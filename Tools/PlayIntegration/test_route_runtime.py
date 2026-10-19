"""Finite PIE movement/nav verification; only Lvl_Tallow_Playable, never saves assets.
Run in a fresh editor Python batch. Ends PIE and quits editor. One teleport at each
case start; every remaining tread/landing is reached only with AddMovementInput.
Failure or unsupported API is recorded, never replaced by a ray-only pass.
"""
import json, math, random, time, traceback
from pathlib import Path
import unreal as u

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / 'Tallow/SourceAssets/Production/HouseIntegrationV01'
OUT = ROOT / 'Tallow/Saved/Integration/route-runtime.json'
PLAN = json.loads((SOURCE / 'route-plan.json').read_text())
CLEARANCE = json.loads((SOURCE / 'source-clearance.json').read_text())
MAP = '/Game/Presentation/Playable/Lvl_Tallow_Playable'
assert PLAN['map'] == MAP
ED = u.get_editor_subsystem(u.LevelEditorSubsystem)
REPORT = {'map': MAP, 'seed': 252, 'cases': [], 'navigation': [], 'errors': [],
          'method': 'actual CharacterMovement input + floor sweep + profile capsule sweep; no per-target teleport',
          'limits': 'Deterministic geometry test with AI paused; does not validate pursuit or gate progression.'}
STATE = {}
START = time.monotonic()

def xyz(v): return [v.x, v.y, v.z]
def vector(v): return u.Vector(*v)
def distance2(a, b): return math.hypot(a.x-b.x, a.y-b.y)
def prop(obj, key): return obj.get_editor_property(key)
def floor_record(actor):
    movement = actor.get_component_by_class(u.CharacterMovementComponent)
    floor = movement.find_floor(actor.get_actor_location())
    return {'blocking': bool(prop(floor, 'blocking_hit')),
            'walkable': bool(prop(floor, 'walkable_floor')),
            'floor_distance_cm': float(prop(floor, 'floor_dist'))}

def profile_sweep(actor, start, end):
    cap = actor.get_component_by_class(u.CapsuleComponent)
    result = u.SystemLibrary.capsule_trace_single_by_profile(
        STATE['world'], start, end, cap.get_scaled_capsule_radius(),
        cap.get_scaled_capsule_half_height(), cap.get_collision_profile_name(),
        False, [actor], u.DrawDebugTrace.NONE, True)
    # UE Python uses the bool return as success: no blocking hit returns None.
    if result is None: return {'blocking':False,'penetrating':False,'impact':None}
    hit = next((v for v in result if isinstance(v, u.HitResult)), None) if isinstance(result, tuple) else result
    if hit is None: raise RuntimeError('Capsule sweep returned no HitResult: '+str(type(result)))
    return {'blocking': bool(prop(hit, 'blocking_hit')), 'penetrating': bool(prop(hit, 'start_penetrating')),
            'impact': xyz(prop(hit, 'impact_point'))}

def routes():
    grouped = {}
    for sample in CLEARANCE['stair_headroom_samples']:
        x,y,z = sample['point_m']
        grouped.setdefault(sample['stair'], []).append([100*x,-100*y,100*z])
    assert set(grouped) == {'MainStairs','BackStairs','BasementStairs'}, grouped.keys()
    main = grouped['MainStairs']
    landing = next(i for i,p in enumerate(main) if abs(p[2]-195)<.2)
    mid = main[landing]
    main[landing:landing+1] = [[main[landing-1][0],mid[1],mid[2]], mid,
                             [main[landing+1][0],mid[1],mid[2]]]
    return grouped

def gate_probes(phase):
    rows=[];player=STATE['player'];profile=player.get_component_by_class(u.CapsuleComponent).get_collision_profile_name()
    for gate in PLAN['native_gates']:
        scale=gate.get('mesh_scale',[1,1,1]);width=(158 if 'Wide' in gate['door_mesh'] else 108)*scale[0]
        x,y,z=gate['hinge_location'];yaw=math.radians(gate['yaw']);half=30 if scale[2]<.5 else 60
        center=u.Vector(x+math.cos(yaw)*width*.5,y+math.sin(yaw)*width*.5,z+half+3)
        normal=u.Vector(-math.sin(yaw)*85,math.cos(yaw)*85,0)
        result=u.SystemLibrary.capsule_trace_single_by_profile(STATE['world'],center-normal,center+normal,30,half,profile,False,[player,STATE['keeper']],u.DrawDebugTrace.NONE,True)
        blocked=result is not None
        rows.append({'gate':gate['label'],'blocked':blocked,'passed':blocked==(phase=='closed')})
    REPORT[phase+'_gates']=rows

def hiding_checks(player):
    rows=[];REPORT['hiding']=rows
    for hide in u.GameplayStatics.get_all_actors_of_class(STATE['world'],u.TallowHideVolume):
        if 'Tallow.Integration' not in [str(t) for t in hide.tags]:continue
        hide.on_interact(player);yield .1
        entered=prop(player,'current_hide_volume')==hide
        hide.on_interact(player);yield .25
        cap=player.get_component_by_class(u.CapsuleComponent)
        exit_pos=hide.get_exit_point().get_world_location();actual=player.get_actor_location()
        sweep=profile_sweep(player,actual,actual+u.Vector(0,0,.1))
        rows.append({'hide':hide.get_actor_label(),'entered':entered,'exited':prop(player,'current_hide_volume') is None,
          'exit_distance_cm':math.sqrt(sum((a-b)**2 for a,b in zip(xyz(actual),xyz(exit_pos)))),
          'floor':floor_record(player),'exit_penetrating':sweep['penetrating']})

def navigation(actor):
    rng = random.Random(252)
    pools = PLAN.get('crowbar_candidates', []) + PLAN.get('fuse_candidates', [])
    for item in pools:
        target = item['location']; floor_z = -330 if item.get('floor')=='basement' or target[2]<-100 else 390 if target[2]>300 else 0
        for attempt in range(3):
            angle = rng.uniform(0,math.tau); radius = rng.uniform(65,115)
            approach = [target[0]+math.cos(angle)*radius,target[1]+math.sin(angle)*radius,floor_z+10]
            start = actor.get_actor_location()
            path = u.NavigationSystemV1.find_path_to_location_synchronously(STATE['world'],start,vector(approach),actor)
            REPORT['navigation'].append({'actor':actor.get_name(),'candidate':item['id'],'attempt':attempt,
                'start':xyz(start),'approach':approach,'valid':bool(path and path.is_valid()),
                'partial':bool(path and path.is_partial()),'points':[xyz(v) for v in path.path_points] if path else []})

def walk_case(actor, name, points):
    movement = actor.get_component_by_class(u.CharacterMovementComponent); cap = actor.get_component_by_class(u.CapsuleComponent)
    half = cap.get_scaled_capsule_half_height()
    result = {'actor':actor.get_name(),'stair':name,'targets':points,'samples':[], 'passed':False}
    REPORT['cases'].append(result)
    # Setup teleport only. Collision remains enabled; initial overlap is explicitly queried.
    actor.set_actor_location(vector([points[0][0],points[0][1],points[0][2]+half+3]),False,True)
    movement.stop_movement_immediately(); movement.set_movement_mode(u.MovementMode.MOVE_WALKING)
    movement.set_editor_property('max_walk_speed',180.0)
    yield .35
    result['start_sweep'] = profile_sweep(actor,actor.get_actor_location(),actor.get_actor_location()+u.Vector(0,0,.1))
    deadline = time.monotonic()+10.5
    for index, point in enumerate(points):
        target=vector(point); reached=False
        while time.monotonic()<deadline:
            pos=actor.get_actor_location(); horizontal=distance2(pos,target); height_error=pos.z-half-target.z
            if horizontal<12 and abs(height_error)<28:
                floor=floor_record(actor)
                result['samples'].append({'index':index,'position':xyz(pos),'height_error_cm':height_error,'floor':floor})
                reached=floor['walkable'] and floor['blocking']
                break
            if horizontal>.1:
                actor.add_movement_input(u.Vector((target.x-pos.x)/horizontal,(target.y-pos.y)/horizontal,0),1.0,True)
            yield 0
        if not reached:
            pos=actor.get_actor_location()
            result['failure']={'index':index,'target':point,'actual':xyz(pos),'floor':floor_record(actor),
                'capsule_to_target':profile_sweep(actor,pos,vector([target.x,target.y,target.z+half+3])),
                'reason':'timeout or non-walkable contact'}
            movement.stop_movement_immediately();return
    movement.stop_movement_immediately();result['passed']=not result['start_sweep']['penetrating']


def main():
    assert ED.load_level(MAP)
    yield 3
    ED.editor_request_begin_play();yield 4
    world=u.EditorLevelLibrary.get_game_world();assert world and 'Lvl_Tallow_Playable' in world.get_name()
    player=u.GameplayStatics.get_player_pawn(world,0)
    keeper=u.GameplayStatics.get_actor_of_class(world,u.TallowCreature)
    assert player and keeper
    STATE.update(world=world,player=player,keeper=keeper)
    # Exercise the existing release mechanic; do not patch tied state or gameplay code.
    chair=u.GameplayStatics.get_actor_of_class(world,u.TallowChairEscape)
    chair_deadline=time.monotonic()+18
    while player.is_tied_to_chair() and time.monotonic()<chair_deadline:
        if chair and (chair.is_at_rest() or chair.is_timing_window_open()):chair.attempt_rock()
        yield .025
    assert not player.is_tied_to_chair(),'Player did not release from chair within18s'
    gate_probes('closed')
    director=u.GameplayStatics.get_actor_of_class(world,u.TallowPlayDirector)
    if director:director.set_actor_tick_enabled(False)
    # Open through the same interaction completions that broadcast task callbacks.
    # This is test setup only, not a claim that inventory/puzzle gameplay was exercised.
    planks=u.GameplayStatics.get_all_actors_of_class(world,u.TallowPlank)
    for plank in planks:plank.call_method('OnInteractComplete',(player,))
    # Removing boards unlocks the original door; the player still opens its leaf with E.
    for door in u.GameplayStatics.get_all_actors_of_class(world,u.TallowDoor):door.on_interact(player)
    box=u.GameplayStatics.get_actor_of_class(world,u.TallowFuseBox)
    assert box,'No fuse box for gate setup'
    for _ in range(int(prop(box,'required_fuses'))):
        if int(prop(box,'current_fuses'))>=int(prop(box,'required_fuses')):break
        player.set_carried_item(u.CarriedItemType.FUSE)
        box.call_method('OnInteractComplete',(player,))
    player.clear_carried_item()
    assert director and prop(director,'boards_cleared') and prop(director,'power_restored'), 'Gate callbacks did not complete'
    REPORT['gate_setup']={'planks_completed':len(planks),'fuses_inserted':int(prop(box,'current_fuses')),
                          'method':'existing OnInteractComplete callbacks; inventory supplied for setup'}
    REPORT['capsules']=[{'actor':a.get_name(),
        'radius_cm':a.get_component_by_class(u.CapsuleComponent).get_scaled_capsule_radius(),
        'half_height_cm':a.get_component_by_class(u.CapsuleComponent).get_scaled_capsule_half_height()} for a in (player,keeper)]
    controller=keeper.get_controller()
    if controller:
        controller.stop_movement()
        brain=controller.get_component_by_class(u.BrainComponent)
        if brain:brain.stop_logic('Deterministic stair test')
    keeper.set_actor_tick_enabled(False)
    for component in keeper.get_components_by_class(u.ActorComponent):
        if component.get_class().get_name() in ('TallowCreatureAnimationComponent','TallowSurfaceTraversalComponent'):
            component.set_component_tick_enabled(False)
    yield 2.5  # Gates animate after AI is paused; navigation receives collision updates.
    gate_probes('open')
    yield from hiding_checks(player)
    for actor,other in ((player,keeper),(keeper,player)):
        other.set_actor_location(u.Vector(-10000,-10000,1000),False,True)
        other.get_component_by_class(u.CharacterMovementComponent).disable_movement()
        for name,points in routes().items():
            for direction,ordered in (('up',points),('down',list(reversed(points)))):
                yield from walk_case(actor,name+'_'+direction,ordered)
        navigation(actor)
    REPORT['passed']=len(REPORT['cases'])==12 and all(c['passed'] for c in REPORT['cases'])
    REPORT['navigation_all_complete']=bool(REPORT['navigation']) and all(n['valid'] and not n['partial'] for n in REPORT['navigation'])
    REPORT['gates_passed']=all(r['passed'] for phase in ['closed_gates','open_gates'] for r in REPORT[phase])
    REPORT['hiding_passed']=len(REPORT['hiding'])==3 and all(r['entered'] and r['exited'] and not r['exit_penetrating'] and r['floor']['walkable'] for r in REPORT['hiding'])


def finish(error=None):
    if error:REPORT['errors'].append(error);u.log_error(error)
    REPORT['elapsed_seconds']=time.monotonic()-START
    OUT.parent.mkdir(parents=True,exist_ok=True);OUT.write_text(json.dumps(REPORT,indent=2))
    ED.editor_request_end_play();u.unregister_slate_post_tick_callback(HANDLE)
    u.SystemLibrary.quit_editor()
GEN=main();NEXT=0

def tick(delta):
    global NEXT
    if time.monotonic()-START>170:finish('170s global deadline; unfinished cases are not passes');return
    if time.monotonic()<NEXT:return
    try:NEXT=time.monotonic()+next(GEN)
    except StopIteration:finish()
    except Exception:finish(traceback.format_exc())
HANDLE=u.register_slate_post_tick_callback(tick)
