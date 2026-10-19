"""Root executes ONLY with existing playable copy loaded. Never duplicates/loads maps."""
import unreal as u,json
from pathlib import Path
R=Path(u.Paths.project_dir()).resolve().parent;S=R/'Tallow/SourceAssets/Production/HouseIntegrationV01';DEST='/Game/Presentation/Environment/HouseIntegrationV01';ed=u.get_editor_subsystem(u.EditorActorSubsystem);world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
assert world.get_path_name().split('.')[0]=='/Game/Presentation/Playable/Lvl_Tallow_Playable','Refusing to mutate any other map'
actors=ed.get_all_level_actors();bylabel={a.get_actor_label():a for a in actors};byname={a.get_name():a for a in actors};cfg=json.loads((S/'dressing.json').read_text())
# Re-run safely replaces only our explicitly tagged additions.
for a in actors:
 if 'HouseIntegrationV01' in [str(t) for t in a.tags]:ed.destroy_actor(a)
def mesh(label,path,loc=(0,0,0),scale=(1,1,1),yaw=0,collision=True):
 asset=u.load_asset(path);assert asset,'Missing import '+path;a=ed.spawn_actor_from_class(u.StaticMeshActor,u.Vector(*loc),u.Rotator(pitch=0,yaw=yaw,roll=0));a.set_actor_label('HI_'+label);a.tags=[u.Name('HouseIntegrationV01')];c=a.static_mesh_component;c.set_static_mesh(asset);c.set_collision_profile_name('BlockAll' if collision else 'NoCollision');a.set_actor_scale3d(u.Vector(*scale));return a
for row in json.loads((S/'Architecture/manifest.json').read_text()):
 if 'GateDoor_' in row['mesh'] or 'GateFrame_' in row['mesh'] or 'SplinteredBoard' in row['mesh']:continue
 old=bylabel.get(row.get('original_actor_label'))
 if row.get('original_actor_label'):assert old,'Source actor not found '+row['original_actor_label']
 mesh(row['mesh'],DEST+'/Architecture/'+row['mesh'],collision=row['collision']=='complex')
 if old:old.set_actor_hidden_in_game(True);old.set_actor_enable_collision(False);old.set_is_temporarily_hidden_in_editor(True)
for name in cfg['replace_prototype_actor_names']:
 a=byname.get(name)
 if a:a.set_actor_hidden_in_game(True);a.set_actor_enable_collision(False);a.set_is_temporarily_hidden_in_editor(True)
for row in cfg['props']:mesh(row['label'],row['asset'],row['location'],row['scale'],row['yaw'])
for a in actors:
 if a.get_actor_label().startswith('M3_WorkLight'):a.set_actor_hidden_in_game(True)
for row in cfg['lights']:
 a=ed.spawn_actor_from_class(u.PointLight,u.Vector(*row['location']));a.set_actor_label('HI_'+row['label']);a.tags=[u.Name('HouseIntegrationV01')];c=a.point_light_component;c.set_editor_property('intensity_units',u.LightUnits.LUMENS);c.set_intensity(row['lumens']);c.set_light_color(u.LinearColor(*row['color']));c.set_attenuation_radius(row['radius'])
# Gameplay exposure applies throughout room traversal, not only cinematic cameras.
a=ed.spawn_actor_from_class(u.PostProcessVolume,u.Vector(0,0,0));a.set_actor_label('HI_GameplayExposure');a.tags=[u.Name('HouseIntegrationV01')];a.set_editor_property('unbound',True);a.set_editor_property('priority',20);s=a.get_editor_property('settings');s.set_editor_property('override_auto_exposure_method',True);s.set_editor_property('auto_exposure_method',u.AutoExposureMethod.AEM_MANUAL);s.set_editor_property('override_auto_exposure_bias',True);s.set_editor_property('auto_exposure_bias',0);s.set_editor_property('override_auto_exposure_apply_physical_camera_exposure',True);s.set_editor_property('auto_exposure_apply_physical_camera_exposure',False);a.set_editor_property('settings',s)
u.EditorLevelLibrary.save_current_level();print('HOUSE_DRESSING_COMPLETE: visual and traversal checks still required')
