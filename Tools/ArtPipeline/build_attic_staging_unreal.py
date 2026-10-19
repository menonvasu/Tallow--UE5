"""Fresh editor/commandlet only: create NEW presentation map, never overwrite a level."""
from pathlib import Path
import json
import unreal as u
ROOT=Path(__file__).resolve().parents[2];SRC=ROOT/'Tallow/SourceAssets/Production/AtticKit';LEVEL='/Game/Presentation/Environment/Maps/Lvl_AtticBenchmark'
E=u.EditorAssetLibrary;assert not E.does_asset_exist(LEVEL),'Staging map exists: do not overwrite'
placements=json.loads((SRC/'staging-placements.json').read_text())['placements'];assets={}
for p in placements:
 n=p['asset'];folder='Cabinet' if n=='SM_Cabinet_SageTimber' else 'AtticKit';assets[n]=u.load_asset('/Game/Presentation/Environment/'+folder+'/'+n);assert assets[n],n
ed=u.get_editor_subsystem(u.LevelEditorSubsystem);actors=u.get_editor_subsystem(u.EditorActorSubsystem);assert ed.new_level(LEVEL)
for p in placements:
 x,y,z=p['location_m'];rx,ry,rz=p['rotation_degrees_xyz'];a=actors.spawn_actor_from_class(u.StaticMeshActor,u.Vector(x*100,-y*100,z*100),u.Rotator(pitch=-ry,yaw=-rz,roll=rx));a.set_actor_label('ART_'+p['label']);a.static_mesh_component.set_static_mesh(assets[p['asset']]);a.set_actor_scale3d(u.Vector(*p['scale']));a.set_actor_enable_collision(False);a.set_folder_path('Presentation/AtticBenchmark')
# Explicit presentation-only lights; values are engine starting points, not copied physical Blender watts.
for label,loc,color,intensity,radius in [('ExitCold',(90,-220,190),u.LinearColor(.49,.65,1,1),2.4,450),('CandleWarm',(-80,150,65),u.LinearColor(1,.56,.25,1),.75,260)]:
 a=actors.spawn_actor_from_class(u.PointLight,u.Vector(*loc));a.set_actor_label(label);a.point_light_component.set_intensity(intensity);a.point_light_component.set_attenuation_radius(radius);a.point_light_component.set_light_color(color)
start=u.Vector(80,280,110);target=u.Vector(-10,-95,115);camera=actors.spawn_actor_from_class(u.CameraActor,start,u.MathLibrary.find_look_at_rotation(start,target));camera.set_actor_label('Benchmark_ChildEye_110cm');camera.camera_component.set_field_of_view(65)
post=u.PostProcessSettings()
for name,value in [('override_auto_exposure_method',True),('auto_exposure_method',u.AutoExposureMethod.AEM_MANUAL),('override_auto_exposure_apply_physical_camera_exposure',True),('auto_exposure_apply_physical_camera_exposure',False),('override_auto_exposure_bias',True),('auto_exposure_bias',0.0),('override_motion_blur_amount',True),('motion_blur_amount',0.0)]: post.set_editor_property(name,value)
camera.camera_component.set_editor_property('post_process_settings',post)
world=u.EditorLevelLibrary.get_editor_world();world.get_world_settings().set_editor_property('default_game_mode',u.GameModeBase)
assert ed.save_current_level();u.log('TALLOW created NEW presentation-only attic benchmark; no gameplay/collision/navigation. Validate orientation, exposure and light in viewport before use.')
