"""Root-only targeted collection redress; preserves runtime actors and exposure tuning."""
import unreal as u,json
from pathlib import Path
R=Path(u.Paths.project_dir()).resolve().parent;S=R/'Tallow/SourceAssets/Production/HouseIntegrationV01';cfg=json.loads((S/'dressing.json').read_text());change=json.loads((S/'collection-refinement.json').read_text());ed=u.get_editor_subsystem(u.EditorActorSubsystem);world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world();assert world.get_path_name().split('.')[0]=='/Game/Presentation/Playable/Lvl_Tallow_Playable'
labels=set(change['changed_labels']+change['removed_labels']+['CollectionChairFocus'])
for a in ed.get_all_level_actors():
 if a.get_actor_label() in {'HI_'+n for n in labels} and 'HouseIntegrationV01' in [str(t) for t in a.tags]:ed.destroy_actor(a)
for row in cfg['props']:
 if row['label'] not in labels:continue
 asset=u.load_asset(row['asset']);assert asset,row['asset'];a=ed.spawn_actor_from_class(u.StaticMeshActor,u.Vector(*row['location']),u.Rotator(pitch=0,yaw=row['yaw'],roll=0));a.set_actor_label('HI_'+row['label']);a.tags=[u.Name('HouseIntegrationV01')];a.static_mesh_component.set_static_mesh(asset);a.static_mesh_component.set_collision_profile_name('BlockAll');a.set_actor_scale3d(u.Vector(*row['scale']))
for row in cfg['lights']:
 if row['label']!='CollectionChairFocus':continue
 a=ed.spawn_actor_from_class(u.PointLight,u.Vector(*row['location']));a.set_actor_label('HI_'+row['label']);a.tags=[u.Name('HouseIntegrationV01')];c=a.point_light_component;c.set_editor_property('intensity_units',u.LightUnits.LUMENS);c.set_intensity(row['lumens']);c.set_light_color(u.LinearColor(*row['color']));c.set_attenuation_radius(row['radius'])
u.EditorLevelLibrary.save_current_level();print('COLLECTION_ONLY_REDRESSED')
