"""Run with UnrealEditor -nullrhi -ExecCmds="py ..." (not a commandlet)."""
import unreal as u,json,os,time
ROOT=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ed=u.get_editor_subsystem(u.LevelEditorSubsystem);assert ed.load_level('/Game/Maps/Lvl_Tallow_M3_Greybox')
world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world();actors=u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors()
r={'floor_checks':[],'patrol_paths':[],'collision_modes':{},'errors':[]}
for a in actors:
 if isinstance(a,u.StaticMeshActor) and a.get_actor_label().startswith('House_'):
  m=a.static_mesh_component.static_mesh;r['collision_modes'][m.get_name()]=str(m.get_editor_property('body_setup').get_editor_property('collision_trace_flag'))
 if isinstance(a,u.PlayerStart):a.set_actor_location(u.Vector(286,-500,-260),False,False)
 if isinstance(a,u.RecastNavMesh):
  a.set_editor_property('runtime_generation',u.RuntimeGenerationType.DYNAMIC)
  a.set_editor_property('fixed_tile_pool_size',True)
  a.set_editor_property('tile_pool_size',1024)
  a.set_editor_property('average_layers_per_tile',8.0)
  params=a.get_editor_property('nav_mesh_resolution_params')
  for i,param in enumerate(params):
   param.set_editor_property('cell_size',10.0);param.set_editor_property('cell_height',5.0);param.set_editor_property('agent_max_step_height',45.0);params[i]=param
  a.set_editor_property('nav_mesh_resolution_params',params)
  r['nav_resolution']=[{'cell_size':p.get_editor_property('cell_size'),'cell_height':p.get_editor_property('cell_height'),'step_height':p.get_editor_property('agent_max_step_height')} for p in a.get_editor_property('nav_mesh_resolution_params')]
u.SystemLibrary.execute_console_command(world,'Editor.AsyncAssetCompilationFinishAll')
ed.save_current_level()
u.EditorLevelLibrary.set_level_viewport_camera_info(u.Vector(286,-450,-210),u.Rotator(0,90,0))


navsys=u.NavigationSystemV1.get_navigation_system(world)
for a in actors:
 if isinstance(a,u.NavMeshBoundsVolume):navsys.on_navigation_bounds_updated(a)
u.SystemLibrary.execute_console_command(world,'RebuildNavigation')
started=time.monotonic();done=False;playing=False;built=False

def finish():
 r['passed']=not r['errors'] and len(r['floor_checks'])==9 and all(p['hit'] for p in r['floor_checks']) and len(r['patrol_paths'])==5 and all(p['valid'] and not p['partial'] for p in r['patrol_paths']+r.get('floor_connections',[])) and all(p['ground_capsule_clear'] for p in r.get('door_clearances',[])) and all(p['hit'] for p in r.get('surface_contacts',[]))
 open(os.path.join(ROOT,'Tools/m3-validation.json'),'w').write(json.dumps(r,indent=2));ed.editor_request_end_play()

def tick(dt):
 global done,playing,built
 elapsed=time.monotonic()-started
 if not built and elapsed>4:
  built=True;u.SystemLibrary.execute_console_command(world,'RebuildNavigation')
 if not playing and elapsed>12:
  playing=True;ed.save_current_level();ed.editor_request_begin_play()
 if not done and elapsed>24:
  done=True
  try:
   gw=u.EditorLevelLibrary.get_game_world()
   r['runtime_actors']=[{'class':a.get_class().get_name(),'pos_cm':[a.get_actor_location().x,a.get_actor_location().y,a.get_actor_location().z]} for a in u.GameplayStatics.get_all_actors_of_class(gw,u.Pawn)]
   r['runtime_nav']=[a.get_name() for a in u.GameplayStatics.get_all_actors_of_class(gw,u.RecastNavMesh)]
   r['runtime_bounds']=[str(a.get_actor_bounds(False)) for a in u.GameplayStatics.get_all_actors_of_class(gw,u.NavMeshBoundsVolume)]
   for name,x,y,z in [('Chair',286,-325,-330),('Workshop',300,-900,-330),('ToyCorridor',825,-500,-330),('Boiler',1100,-350,-330),('Laundry',1400,-800,-330),('Hall',780,-220,0),('Dining',450,-1000,0),('UpperLanding',780,-220,390),('FilmRoom',350,-950,390)]:
    hit=u.SystemLibrary.line_trace_single(gw,u.Vector(x,y,z+40),u.Vector(x,y,z-50),u.TraceTypeQuery.TRACE_TYPE_QUERY1,False,[],u.DrawDebugTrace.NONE,True)
    r['floor_checks'].append({'name':name,'hit':hit is not None,'trace_floor_z_cm':z})
   r['projections']=[]
   for p in [(1100,-300,-325),(825,-500,-325),(780,-220,5),(300,-900,-325)]:
    q=u.NavigationSystemV1.project_point_to_navigation(gw,u.Vector(*p),None,None,u.Vector(500,500,500))
    r['projections'].append({'input':p,'output_cm':[q.x,q.y,q.z] if q is not None else None})
   pts=[(1100,-300,-325),(1430,-650,-325),(1430,-800,-325),(825,-1040,-325),(825,-325,-325)]
   for i,p in enumerate(pts):
    path=u.NavigationSystemV1.find_path_to_location_synchronously(gw,u.Vector(*p),u.Vector(*pts[(i+1)%len(pts)]))
    r['patrol_paths'].append({'from':i,'valid':bool(path and path.is_valid()),'partial':bool(path and path.is_partial()),'points':len(path.path_points) if path else 0,'exists':path is not None})
   r['floor_connections']=[]
   for label,start,end in [('Basement to ground',(825,-1040,-325),(780,-220,5)),('Ground to upper',(780,-220,5),(780,-220,395)),('Basement stair approach',(1430,-800,-325),(1248,-790,-270)),('Basement stair flight',(1248,-790,-270),(1248,-1230,5)),('Basement stair exit',(1248,-1230,5),(780,-220,5))]:
    path=u.NavigationSystemV1.find_path_to_location_synchronously(gw,u.Vector(*start),u.Vector(*end))
    r['floor_connections'].append({'name':label,'valid':bool(path and path.is_valid()),'partial':bool(path and path.is_partial()),'points':[list((v.x,v.y,v.z)) for v in path.path_points] if path else []})
   r['door_clearances']=[]
   route=json.load(open(os.path.join(ROOT,'Tools/traversal-routes.json')))
   for door in route['squeeze_doors']:
    start=u.Vector(*door['entry_floor_cm'])+u.Vector(0,0,98);end=u.Vector(*door['exit_floor_cm'])+u.Vector(0,0,98)
    hit=u.SystemLibrary.capsule_trace_single(gw,start,end,42,96,u.TraceTypeQuery.TRACE_TYPE_QUERY1,False,[],u.DrawDebugTrace.NONE,True)
    r['door_clearances'].append({'id':door['id'],'ground_capsule_clear':hit is None,'radius_cm':42,'half_height_cm':96})
   r['surface_contacts']=[]
   for node in route['surface_route']:
    p=u.Vector(*node['contact_cm']);n=u.Vector(*node['outward_normal'])
    hit=u.SystemLibrary.line_trace_single(gw,p+n*20,p-n*30,u.TraceTypeQuery.TRACE_TYPE_QUERY1,False,[],u.DrawDebugTrace.NONE,True)
    r['surface_contacts'].append({'id':node['id'],'hit':hit is not None})
   r['game_world']=gw.get_name()
  except Exception as e:r['errors'].append(repr(e))
  finish()
 if done and elapsed>27:
  u.unregister_slate_post_tick_callback(handle);u.SystemLibrary.quit_editor()
handle=u.register_slate_post_tick_callback(tick)
