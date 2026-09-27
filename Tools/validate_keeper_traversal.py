"""PIE-only traversal regression. Run normal UnrealEditor -nullrhi -ExecCmds='py /absolute/path'.
Teleports and the temporary blocker belong to this isolated test world; the production component sweeps.
Normalizes the approved surface contacts before PIE; never saves the PIE world. Produces keeper-traversal-validation.json.
"""
import unreal as u
import json, os, time, traceback
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, 'Tools', 'keeper-traversal-validation.json')
ed = u.get_editor_subsystem(u.LevelEditorSubsystem)
assert ed.load_level('/Game/Maps/Lvl_Tallow_M3_Greybox')
world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
u.SystemLibrary.execute_console_command(world, 'Editor.AsyncAssetCompilationFinishAll')
u.SystemLibrary.execute_console_command(world,'Log LogTemp Verbose')
route_changed=False
for actor in u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors():
 label=actor.get_actor_label(); p=actor.get_actor_location(); q=u.Vector(p.x,p.y,p.z)
 if label in ('HANDOFF_Surface_B02_Wall','HANDOFF_Surface_B05_Wall'): q.z=-140
 if label in ('HANDOFF_Surface_B03_Ceiling','HANDOFF_Surface_B04_Ceiling'): q.x=780
 if (p-q).length()>.01: actor.set_actor_location(q,False,False); route_changed=True
if route_changed: assert ed.save_current_level()
test_blocker_editor = u.get_editor_subsystem(u.EditorActorSubsystem).spawn_actor_from_class(u.StaticMeshActor, u.Vector(0,0,-100000))
test_blocker_editor.tags=['Keeper.Automation.Blocker']
test_blocker_editor.static_mesh_component.set_static_mesh(u.load_asset('/Engine/BasicShapes/Cube'))
test_blocker_editor.static_mesh_component.set_mobility(u.ComponentMobility.MOVABLE)
test_blocker_editor.static_mesh_component.set_collision_enabled(u.CollisionEnabled.NO_COLLISION)
report = {'tests': [], 'errors': [], 'test_only_teleports': True, 'samples': [], 'observed_clips': []}
state = {'stage': 'startup', 'start': time.monotonic(), 'stage_start': time.monotonic(), 'playing': False, 'finished': False, 'last_sample': 0, 'index': 0, 'blocker': None}

def vec(v): return [round(v.x, 3), round(v.y, 3), round(v.z, 3)]
def log(message): u.log('KEEPER_TRAVERSAL_TEST: ' + message)
def add(name, passed, **details):
 report['tests'].append(dict(name=name, passed=bool(passed), **details)); log(name + ': ' + str(bool(passed)))
def stage(name): state['stage'] = name; state['stage_start'] = time.monotonic(); log('Starting ' + name)
def destroy_blocker():
 if state['blocker']:
  state['blocker'].static_mesh_component.set_collision_enabled(u.CollisionEnabled.NO_COLLISION); state['blocker'] = None

def reset_at(position):
 comp.reset_traversal(); controller.stop_movement()
 creature.set_actor_location_and_rotation(position, u.Rotator(pitch=0.0, yaw=90.0, roll=0.0), False, True)
 creature.character_movement.stop_movement_immediately()
 creature.character_movement.set_movement_mode(u.MovementMode.MOVE_FLYING)
 creature.set_creature_visual_state(u.TallowCreatureState.PATROL)

def begin_route(name):
 reset_at(u.Vector(800, -900, -224))
 state['wall'] = False; state['ceiling'] = False; state['collision_clear'] = True; state['collision_enabled'] = True
 state['route_started'] = comp.start_surface_route()
 add(name + ' starts', state['route_started'])
 stage(name)

def begin_squeeze():
 if state['index'] >= len(pairs):
  entry,exit_actor,label=pairs[0]
  direction=exit_actor.get_actor_location()-entry.get_actor_location()
  state['auto_direction']=direction/direction.length()
  state['squeeze_exit']=exit_actor.get_actor_location()+u.Vector(0,0,99)
  reset_at(entry.get_actor_location()+u.Vector(0,0,99)-state['auto_direction']*70)
  creature.set_creature_visual_state(u.TallowCreatureState.CHASING)
  brain.stop_logic('Automation approach')
  comp.set_editor_property('automatic_traversal',True)
  state['auto_started']=False
  stage('automatic chase squeeze'); return
 entry, exit_actor, label = pairs[state['index']]
 reset_at(entry.get_actor_location() + u.Vector(0, 0, 99))
 state['collision_clear'] = True; state['collision_enabled'] = True
 state['squeeze_exit'] = exit_actor.get_actor_location() + u.Vector(0, 0, 99)
 state['squeeze_label'] = label
 add(label + ' starts', comp.start_squeeze(entry, exit_actor)); stage('squeeze')

def finish():
 if state['finished']: return
 state['finished'] = True
 try: destroy_blocker(); comp.reset_traversal()
 except Exception: pass
 add('automatic traversal clips selected', all(any(wanted in name for name in report['observed_clips']) for wanted in ('WallCrawl','CeilingCrawl','Squeeze')), observed=report['observed_clips'])
 report['passed'] = bool(report['tests']) and all(t['passed'] for t in report['tests']) and not report['errors']
 with open(OUT, 'w') as f: json.dump(report, f, indent=2)
 log('Finished. Passed=' + str(report['passed']) + ' report=' + OUT)
 ed.editor_request_end_play(); state['exit_time'] = time.monotonic()

def tick(dt):
 global gw, creature, comp, controller, brain, pairs
 try:
  now = time.monotonic()
  if state['finished']:
   if now - state['exit_time'] > 2:
    u.get_editor_subsystem(u.EditorActorSubsystem).destroy_actor(test_blocker_editor)
    u.unregister_slate_post_tick_callback(handle); u.SystemLibrary.quit_editor()
   return
  if now - state['start'] > 100:
   report['errors'].append('Overall test timed out at stage ' + state['stage']); finish(); return
  if not state['playing']:
   if now - state['start'] > 5:
    state['playing'] = True; ed.editor_request_begin_play(); stage('waiting for PIE')
   return
  if state['stage'] == 'waiting for PIE':
   if now - state['stage_start'] < 4: return
   gw = u.EditorLevelLibrary.get_game_world()
   creature = u.GameplayStatics.get_actor_of_class(gw, u.TallowCreature)
   if not creature: raise RuntimeError('No Keeper in PIE world')
   comp = creature.get_component_by_class(u.TallowSurfaceTraversalComponent)
   if not comp: raise RuntimeError('Keeper lacks traversal component')
   comp.set_editor_property('automatic_traversal', False)
   comp.set_editor_property('crawl_speed', 180.0); comp.set_editor_property('ceiling_speed', 180.0); comp.set_editor_property('squeeze_speed', 150.0)
   controller = creature.get_controller(); brain = controller.get_component_by_class(u.BrainComponent)
   for pawn in u.GameplayStatics.get_all_actors_of_class(gw, u.Pawn):
    if pawn != creature:
     pawn.set_actor_enable_collision(False)
     pawn.set_actor_location(u.Vector(100000, 100000, 10000), False, True)
   controller.set_editor_property('is_target_tied_to_chair', True)
   actors = u.GameplayStatics.get_all_actors_of_class(gw, u.Actor)
   pairs = []
   for door in ('ParlourDoor', 'KitchenDoor', 'FilmRoomDoor'):
    relevant = [a for a in actors if a.actor_has_tag('Tallow.Traversal.Squeeze') and a.actor_has_tag(door)]
    entry = next(a for a in relevant if a.actor_has_tag('Entry')); exit_actor = next(a for a in relevant if a.actor_has_tag('Exit'))
    pairs.extend([(entry, exit_actor, door + ' forward'), (exit_actor, entry, door + ' reverse')])
   begin_route('surface route'); return
  elapsed = now - state['stage_start']
  if comp.is_traversing():
   instance=creature.mesh.get_anim_instance()
   asset=instance.get_animation_asset() if instance else None
   if asset and asset.get_name() not in report['observed_clips']: report['observed_clips'].append(asset.get_name())
   normal = comp.get_surface_normal()
   state['wall'] = state.get('wall', False) or abs(normal.z) < .15
   state['ceiling'] = state.get('ceiling', False) or normal.z < -.95
   state['collision_clear'] = state.get('collision_clear', True) and comp.is_current_pose_clear()
   state['collision_enabled'] = state.get('collision_enabled', True) and creature.capsule_component.get_collision_enabled() != u.CollisionEnabled.NO_COLLISION
   if now - state['last_sample'] > .2:
    state['last_sample'] = now
    report['samples'].append({'stage': state['stage'], 'position': vec(creature.get_actor_location()), 'normal': vec(normal), 'pose_clear': comp.is_current_pose_clear()})
  if state['stage'] == 'surface route':
   if not comp.is_traversing() or elapsed > 20:
    add('floor-wall-ceiling-floor completion', not comp.is_traversing() and state['wall'] and state['ceiling'], wall=state['wall'], ceiling=state['ceiling'], position=vec(creature.get_actor_location()))
    add('route collision retained and no overlapping poses', state['collision_clear'] and state['collision_enabled'])
    add('brain resumed at ground', not brain.is_paused() and creature.get_actor_up_vector().z > .98 and creature.character_movement.gravity_scale > 0)
    begin_route('blocked route')
    # Spawn only after route endpoint validation, so this tests dynamic obstruction handling.
    blocker = next(a for a in u.GameplayStatics.get_all_actors_of_class(gw,u.StaticMeshActor) if a.actor_has_tag('Keeper.Automation.Blocker'))
    blocker.set_actor_location(u.Vector(850,-700,-136),False,True)
    blocker.set_actor_scale3d(u.Vector(2,.2,2.2))
    blocker.static_mesh_component.set_collision_profile_name('BlockAll')
    blocker.static_mesh_component.set_collision_enabled(u.CollisionEnabled.QUERY_AND_PHYSICS)
    state['blocker'] = blocker; state['max_y'] = -900
  elif state['stage'] == 'blocked route':
   state['max_y'] = max(state['max_y'], creature.get_actor_location().y)
   if not comp.is_traversing() or elapsed > 30:
    add('dynamic blocker causes safe grounded retreat', not comp.is_traversing() and state['max_y'] < -710 and creature.get_actor_up_vector().z > .98, max_y=state['max_y'], position=vec(creature.get_actor_location()))
    add('blocked route collision retained', state['collision_clear'] and state['collision_enabled'])
    destroy_blocker(); begin_squeeze()
  elif state['stage'] == 'squeeze':
   if not comp.is_traversing() or elapsed > 7:
    distance = (creature.get_actor_location() - state['squeeze_exit']).length()
    add(state['squeeze_label'] + ' crosses doorway', not comp.is_traversing() and distance < 12 and state['collision_clear'] and state['collision_enabled'], distance_to_exit_cm=distance)
    state['index'] += 1; begin_squeeze()
  elif state['stage'] == 'automatic chase squeeze':
   if comp.is_squeezing(): state['auto_started']=True
   if not state['auto_started']:
    creature.character_movement.set_editor_property('velocity',state['auto_direction']*260)
   if (state['auto_started'] and not comp.is_traversing()) or elapsed>6:
    distance=(creature.get_actor_location()-state['squeeze_exit']).length()
    add('automatic chase squeeze despite surface cooldown',state['auto_started'] and not comp.is_traversing() and distance<12, distance_to_exit_cm=distance)
    comp.set_editor_property('automatic_traversal',False)
    brain.restart_logic()
    begin_route('capture freeze'); state['freeze_done']=False
  elif state['stage'] == 'capture freeze':
   if not state['freeze_done'] and elapsed > 1:
    comp.freeze_for_capture(); state['frozen_position'] = creature.get_actor_location(); state['frozen_rotation'] = creature.get_actor_rotation(); state['freeze_done'] = True
   if elapsed > 2:
    add('capture freezes traversal without pose snap', (creature.get_actor_location()-state['frozen_position']).length() < .1 and creature.get_actor_rotation() == state['frozen_rotation'])
    comp.reset_traversal()
    add('reset clears traversal and resumes brain', not comp.is_traversing() and not brain.is_paused() and creature.character_movement.gravity_scale > 0)
    reset_at(u.Vector(800,-900,-224))
    add('route can restart after capture reset', comp.start_surface_route()); finish()
 except Exception:
  report['errors'].append(traceback.format_exc()); log(report['errors'][-1]); finish()
handle = u.register_slate_post_tick_callback(tick)
