"""PIE-only stair traversal check; pauses AI decisions, preserving its movement/collision."""
import unreal as u,json,os,time
ROOT=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ed=u.get_editor_subsystem(u.LevelEditorSubsystem);ed.load_level('/Game/Maps/Lvl_Tallow_M3_Greybox')
started=time.monotonic();stage=0;report={'checks':[],'errors':[]};pawn=None;controller=None
cases=[('Basement stair',(1480,-800,-232),(1248,-1230,0)),('Main stair',(780,-220,98),(780,-220,390))]
def begin_case(i):
 name,start,goal=cases[i];controller.stop_movement();pawn.set_actor_location(u.Vector(*start),False,True)
 result=controller.move_to_location(u.Vector(*goal),35,True,True,True,False,None,False)
 report['checks'].append({'name':name,'request':str(result),'start_cm':start,'goal_floor_cm':goal})
def sample(i):
 p=pawn.get_actor_location();goal=u.Vector(*cases[i][2]);goal.z+=96
 report['checks'][i].update({'end_cm':[p.x,p.y,p.z],'distance_to_goal_cm':(p-goal).length(),'reached':(p-goal).length()<130,'move_status':str(controller.get_move_status())})
def tick(dt):
 global stage,pawn,controller
 t=time.monotonic()-started
 try:
  if stage==0 and t>5:ed.editor_request_begin_play();stage=1
  elif stage==1 and t>10:
   gw=u.EditorLevelLibrary.get_game_world();pawn=next(a for a in u.GameplayStatics.get_all_actors_of_class(gw,u.Character) if 'Creature' in a.get_class().get_name());controller=pawn.get_controller()
   for brain in controller.get_components_by_class(u.BrainComponent):brain.stop_logic('Greybox stair movement validation')
   movement=pawn.get_component_by_class(u.CharacterMovementComponent);report['movement']={'step_height_cm':movement.max_step_height,'speed_cm_s':movement.max_walk_speed}
   begin_case(0);stage=2
  elif stage==2 and t>25:sample(0);begin_case(1);stage=3
  elif stage==3 and t>43:sample(1);stage=4
 except Exception as exc:report['errors'].append(repr(exc));stage=4
 if stage==4:
  open(os.path.join(ROOT,'Tools/m3-stair-movement.json'),'w').write(json.dumps(report,indent=2));ed.editor_request_end_play();stage=5
 if stage==5 and t>46:u.unregister_slate_post_tick_callback(handle);u.SystemLibrary.quit_editor()
handle=u.register_slate_post_tick_callback(tick)
