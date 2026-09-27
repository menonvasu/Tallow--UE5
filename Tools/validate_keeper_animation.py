"""NullRHI PIE regression for authored Keeper animation; never saves the map.
Run a normal UnrealEditor with -ExecCmds='py /absolute/path/to/this/script'.
Pose samples prove skeletal/morph evaluation, not rendered visual quality.
"""
import json, os, time, traceback
import unreal as u
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, 'Tools', 'keeper-animation-validation.json')
BASE = '/Game/Characters/Keeper/Animated/'
NAMES = ['Idle','HostWalk','Reveal','Walk','Sneak','Chase','RunChase','WallCrawl','CeilingCrawl','SearchUnder','SearchOver','JumpscareDeath','Squeeze']
CURVES = ['FULL_HORROR','CHOMP','Blink_L','Blink_R']
report = {'tests':[], 'errors':[], 'samples':{}, 'rendered_visual_review':False}
state = {'start':time.monotonic(), 'stage':'startup', 'stage_start':time.monotonic(), 'done':False, 'index':0}
ed = u.get_editor_subsystem(u.LevelEditorSubsystem)
assert ed.load_level('/Game/Maps/Lvl_Tallow_M3_Greybox')
u.SystemLibrary.execute_console_command(u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world(), 'Editor.AsyncAssetCompilationFinishAll')

def vec(v): return [round(v.x,4),round(v.y,4),round(v.z,4)]
def check(name, passed, **details):
 report['tests'].append(dict(name=name,passed=bool(passed),**details)); u.log('KEEPER_ANIMATION_TEST: '+name+' '+str(bool(passed)))
def stage(name): state.update(stage=name, stage_start=time.monotonic())
def finish():
 if state['done']: return
 state['done']=True; report['passed']=bool(report['tests']) and all(t['passed'] for t in report['tests']) and not report['errors']
 with open(OUT,'w') as f: json.dump(report,f,indent=2)
 ed.editor_request_end_play(); state['exit']=time.monotonic()
def active():
 instance=mesh.get_anim_instance(); asset=instance.get_animation_asset()
 return asset.get_name() if asset else None

def sample():
 instance=mesh.get_anim_instance()
 return {'bones':{n:vec(mesh.get_socket_location(n)) for n in bones},
  'rotations':{n:str(u.MathLibrary.make_relative_transform(mesh.get_socket_transform(n), mesh.get_socket_transform(mesh.get_parent_bone(n))).rotation) for n in bones},
  'curves':{n:float(instance.get_curve_value(n)) for n in CURVES},
  'position_seconds':float(mesh.get_position()), 'active':active(),
  'actor_position':vec(creature.get_actor_location()), 'mesh_relative_scale':vec(mesh.get_editor_property('relative_scale3d')),'mesh_relative_rotation':str(mesh.get_editor_property('relative_rotation')),'mesh_up':vec(mesh.get_up_vector()),'mesh_forward':vec(mesh.get_forward_vector())}

def start_clip():
 if state['index']>=len(NAMES):
  animation.clear_traversal_animation(); animation.reset_playback(); creature.set_creature_visual_state(u.TallowCreatureState.PATROL); stage('mapping_idle'); return
 name=NAMES[state['index']]; report['samples'][name]=[]; animation.play_traversal_animation(clips[name], True, 1.0); state['last_sample']=0; stage('clip')

def tick(dt):
 global creature, mesh, animation, traversal, controller, clips, bones
 try:
  now=time.monotonic(); elapsed=now-state['stage_start']
  if state['done']:
   if now-state['exit']>2: u.unregister_slate_post_tick_callback(handle); u.SystemLibrary.quit_editor()
   return
  if now-state['start']>130: raise RuntimeError('Animation test timed out at '+state['stage'])
  if state['stage']=='startup':
   if elapsed>4: ed.editor_request_begin_play(); stage('pie')
   return
  if state['stage']=='pie':
   if elapsed<4: return
   world=u.EditorLevelLibrary.get_game_world(); creature=u.GameplayStatics.get_actor_of_class(world,u.TallowCreature)
   assert creature, 'Keeper missing'
   mesh=creature.mesh; animation=creature.get_component_by_class(u.TallowCreatureAnimationComponent); traversal=creature.get_component_by_class(u.TallowSurfaceTraversalComponent)
   traversal.set_editor_property('automatic_traversal',False); traversal.reset_traversal()
   controller=creature.get_controller(); controller.stop_movement(); controller.get_component_by_class(u.BrainComponent).stop_logic('Isolated animation regression')
   controller.set_editor_property('is_target_tied_to_chair',True)
   for pawn in u.GameplayStatics.get_all_actors_of_class(world,u.Pawn):
    if pawn!=creature: pawn.set_actor_enable_collision(False); pawn.set_actor_location(u.Vector(100000,100000,10000),False,True)
   creature.set_actor_enable_collision(False); creature.set_actor_location(u.Vector(800,-900,1000),False,True)
   creature.character_movement.set_movement_mode(u.MovementMode.MOVE_FLYING)
   creature.character_movement.stop_movement_immediately()
   clips={name:u.load_asset(BASE+'AN_Keeper_'+name) for name in NAMES}
   check('all thirteen clips present',all(clips.values()), assets={n:a.get_path_name() if a else None for n,a in clips.items()})
   asset=mesh.get_skeletal_mesh_asset(); morphs=[str(m.get_name()) for m in asset.get_editor_property('morph_targets')]
   check('four mesh morph targets',set(CURVES).issubset(set(morphs)),morph_targets=morphs)
   skeleton=asset.get_editor_property('skeleton')
   check('all clips share mesh skeleton', all(a and a.get_editor_property('skeleton')==skeleton for a in clips.values()))
   allbones=[str(mesh.get_bone_name(i)) for i in range(mesh.get_num_bones())]
   report['bone_names']=allbones
   fingers=[b for b in allbones if any(k in b.lower() for k in ['finger','index','thumb'])]
   bones=list(dict.fromkeys([n for n in ['head','root','hand.L','hand.R'] if n in allbones]+fingers[:4]))
   check('head and articulated finger bones imported','head' in bones and bool(fingers), sampled_bones=bones)
   origin,extent=creature.get_actor_bounds(False)
   check('mesh local up agrees with standing actor',mesh.get_up_vector().z>.95, mesh_up=vec(mesh.get_up_vector()), relative_rotation=str(mesh.get_editor_property('relative_rotation')))
   report['initial_bounds']={'origin':vec(origin),'extent':vec(extent),'mesh_relative_location':vec(mesh.get_editor_property('relative_location')),'mesh_relative_scale':vec(mesh.get_editor_property('relative_scale3d')),'mesh_relative_rotation':str(mesh.get_editor_property('relative_rotation')),'mesh_up':vec(mesh.get_up_vector()),'mesh_forward':vec(mesh.get_forward_vector())}
   start_clip(); return
  if state['stage']=='clip':
   name=NAMES[state['index']]
   if now-state['last_sample']>.08:
    state['last_sample']=now; report['samples'][name].append(sample())
   if elapsed>min(float(clips[name].get_play_length())+0.15,5.2):
    samples=report['samples'][name]
    origin,extent=creature.get_actor_bounds(False)
    report.setdefault('clip_bounds',{})[name]={'origin':vec(origin),'extent':vec(extent)}
    check(name+' clip plays',all(s['active']=='AN_Keeper_'+name for s in samples))
    check(name+' timeline advances',max(s['position_seconds'] for s in samples)-min(s['position_seconds'] for s in samples)>.15)
    if name in ['Reveal','Chase','RunChase','JumpscareDeath']:
     check(name+' head animated',len({str(s['bones'].get('head'))+str(s['rotations'].get('head')) for s in samples})>2)
    state['index']+=1; start_clip()
  elif state['stage']=='mapping_idle' and elapsed>.3:
   check('stationary patrol maps Idle',active()=='AN_Keeper_Idle'); check('patrol speed uses authored setting',abs(creature.character_movement.max_walk_speed-animation.get_editor_property('patrol_speed'))<.1); stage('mapping_walk')
  elif state['stage']=='mapping_walk':
   creature.character_movement.velocity=u.Vector(65,0,0)
   if elapsed>.4:
    check('moving patrol maps HostWalk',active()=='AN_Keeper_HostWalk'); creature.set_creature_visual_state(u.TallowCreatureState.CHASING); stage('mapping_chase')
  elif state['stage']=='mapping_chase':
   creature.character_movement.velocity=u.Vector(260,0,0)
   if .15<elapsed<.3: state['saw_reveal']=active()=='AN_Keeper_Reveal'
   if elapsed>3.4:
    check('chase speed uses authored setting',abs(creature.character_movement.max_walk_speed-animation.get_editor_property('chase_speed'))<.1); check('chase begins with reveal',state.get('saw_reveal',False)); check('chase maps RunChase or Chase after reveal',active() in ['AN_Keeper_RunChase','AN_Keeper_Chase'])
    creature.character_movement.stop_movement_immediately(); animation.play_capture_animation(); stage('capture')
  elif state['stage']=='capture' and elapsed>.5:
   check('capture overrides locomotion',active()=='AN_Keeper_JumpscareDeath'); check('capture duration exposes authored three seconds',abs(animation.get_capture_duration()-3)<.05)
   animation.reset_playback(); creature.set_creature_visual_state(u.TallowCreatureState.PATROL); stage('reset')
  elif state['stage']=='reset' and elapsed>.3:
   check('capture reset returns Idle',active()=='AN_Keeper_Idle')
   samples=[s for rows in report['samples'].values() for s in rows]
   for curve in CURVES:
    values=[s['curves'][curve] for s in samples]
    check(curve+' evaluated varying curve',max(values)-min(values)>.01,minimum=min(values),maximum=max(values))
   fingerbones=[b for b in bones if b!='head' and any(k in b.lower() for k in ['finger','index','thumb'])]
   check('fingers animate across authored clips',any(len({s['rotations'][b] for s in samples})>3 for b in fingerbones))
   finish()
 except Exception:
  report['errors'].append(traceback.format_exc()); u.log_error(report['errors'][-1]); finish()
handle=u.register_slate_post_tick_callback(tick)
