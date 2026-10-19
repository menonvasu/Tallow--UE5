"""Editor-only native UI/flame visual proof. Run in a fresh editor with no unsaved work.
Creates a new isolated review map; never opens the gameplay map.
Run: UnrealEditor Tallow.uproject /Engine/Maps/Entry -RenderOffscreen -nosound
-EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities -ExecCmds="py /absolute/path/to/this_file.py"
"""
import importlib.util,json,time,traceback
from pathlib import Path
import unreal as u
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'Tallow/Saved/Presentation/Preview/Flame'
OUT.mkdir(parents=True,exist_ok=True)
S={'start':time.monotonic(),'phase':'setup','errors':[]}
ed=u.get_editor_subsystem(u.LevelEditorSubsystem)
eas=u.get_editor_subsystem(u.EditorActorSubsystem)
def finish(error=None):
 if error:
  S['errors'].append(error);u.log_error(error)
 (OUT/'render-result.json').write_text(json.dumps({'errors':S['errors'],'phase':'complete' if not S['errors'] else 'failed'},indent=2))
 S['phase']='finished';S['end']=time.monotonic()
 try: ed.editor_request_end_play()
 except Exception: pass
try:
 p=ROOT/'Tools/Presentation/create_flame_assets.py'
 spec=importlib.util.spec_from_file_location('flame_assets',p);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
 if not u.EditorAssetLibrary.does_asset_exist('/Game/Presentation/Flame/SM_TallowFlame'): m.create_assets()
 review_level='/Game/Presentation/Maps/Lvl_FlameReview_v3'
 assert not u.EditorAssetLibrary.does_asset_exist(review_level), 'Review level already exists; inspect it instead of overwriting'
 assert ed.new_level(review_level)
 world=u.EditorLevelLibrary.get_editor_world()
 world.get_world_settings().set_editor_property('default_game_mode',u.GameModeBase)
 mesh=u.load_asset('/Game/Presentation/Flame/SM_TallowFlame')
 mat=u.load_asset('/Game/Presentation/Flame/M_TallowFlame')
 cylinder=u.load_asset('/Engine/BasicShapes/Cylinder')
 for i,(name,exposure,state) in enumerate([('StillAir',0,u.CandleState.LIT),('Draft',1,u.CandleState.LIT),('CuppedDraft',1,u.CandleState.CUPPED)]):
  a=eas.spawn_actor_from_class(u.TallowCandlePresentationActor,u.Vector(0,(i-1)*4.7,0))
  a.set_actor_label(name)
  a.candle.set_static_mesh(cylinder)
  a.candle.set_material(0,u.load_asset('/Engine/BasicShapes/BasicShapeMaterial'))
  a.candle.set_relative_scale3d(u.Vector(.015,.015,.05))
  a.wick.set_relative_location(u.Vector(0,0,50),False,False)
  a.flame.flame_mesh=mesh;a.flame.flame_material=mat
  a.flame_state=state;a.draft_exposure=exposure
  a.flame.initialize_visual()
  text=eas.spawn_actor_from_class(u.TextRenderActor,u.Vector(-1,(i-1)*4.7,-3.4),u.Rotator(pitch=0,yaw=180,roll=0))
  tr=text.get_component_by_class(u.TextRenderComponent)
  tr.set_text(['STILL AIR','DRAFT','CUPPED'][i]);tr.set_world_size(.75)
  tr.set_horizontal_alignment(u.HorizTextAligment.EHTA_CENTER)
 light=eas.spawn_actor_from_class(u.PointLight,u.Vector(-12,0,14))
 light.point_light_component.set_intensity(25)
 camera=eas.spawn_actor_from_class(u.CameraActor,u.Vector(-35,0,1.2),u.Rotator(0,0,0))
 camera.set_actor_label('FlameReviewCamera')
 camera.camera_component.set_field_of_view(30)
 post=u.PostProcessSettings()
 for name,value in [('override_auto_exposure_method',True),('auto_exposure_method',u.AutoExposureMethod.AEM_MANUAL),('override_auto_exposure_apply_physical_camera_exposure',True),('auto_exposure_apply_physical_camera_exposure',False),('override_auto_exposure_bias',True),('auto_exposure_bias',0.0),('override_motion_blur_amount',True),('motion_blur_amount',0.0),('override_bloom_intensity',True),('bloom_intensity',.2)]: post.set_editor_property(name,value)
 camera.camera_component.set_editor_property('post_process_settings',post)
 ed.save_current_level()
 S['phase']='warmup';S['start']=time.monotonic()
except Exception: finish(traceback.format_exc())
def tick(dt):
 try:
  now=time.monotonic()
  if S['phase']=='finished':
   if now-S['end']>3:u.unregister_slate_post_tick_callback(H);u.SystemLibrary.quit_editor()
   return
  if S['phase']=='warmup' and now-S['start']>8:
   u.SystemLibrary.execute_console_command(None,'Tallow.Presentation.ExportUI')
   ed.editor_request_begin_play();S['phase']='pie';S['start']=now;return
  if S['phase']=='pie' and now-S['start']>7:
   gw=u.EditorLevelLibrary.get_game_world();pc=u.GameplayStatics.get_player_controller(gw,0)
   cameras=u.GameplayStatics.get_all_actors_of_class(gw,u.CameraActor)
   pc.set_view_target_with_blend(cameras[0],0)
   S['pc']=pc;S['world']=gw;S['phase']='capture';S['next']=now+2;S['i']=0;return
  if S['phase']=='capture' and now>S['next']:
   if S['i']>=8: finish();return
   path=str(OUT/f'flame-{S["i"]:03d}.png')
   u.SystemLibrary.execute_console_command(S['pc'],'HighResShot 1280x720 filename='+path)
   S['i']+=1;S['next']=now+1.3
 except Exception: finish(traceback.format_exc())
H=u.register_slate_post_tick_callback(tick)
