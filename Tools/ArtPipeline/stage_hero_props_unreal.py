"""Lead-run only: clone isolated WIP benchmark into NEW hero review maps, never gameplay maps."""
from pathlib import Path
import json
import unreal as u
ROOT=Path(__file__).resolve().parents[2];SOURCE='/Game/Presentation/Environment/Maps/Lvl_AtticBenchmark';BASE='/Game/Presentation/Environment/Maps/';E=u.EditorAssetLibrary;ed=u.get_editor_subsystem(u.LevelEditorSubsystem);actors=u.get_editor_subsystem(u.EditorActorSubsystem)
assert E.does_asset_exist(SOURCE),'Import the private attic kit baseline first'
rows=json.loads((ROOT/'Tallow/SourceAssets/Production/HeroPropStaging/hero-placements.json').read_text())['placements'];meshes={}
for prop in ['RestraintChair','CollectionDoll']:
 meshes[prop]=u.load_asset('/Game/Presentation/Environment/HeroProps/'+prop+'/SM_'+prop);assert meshes[prop]
variants=[('Lvl_AtticDetailedBenchmark','CollectionDoll',(65,250,110),(-15,-105,112)),('Lvl_ChairCornerBenchmark','RestraintChair',(-40,240,110),(25,-25,108))]
assert all(not E.does_asset_exist(BASE+name) for name,_,_,_ in variants),'Review destination exists; do not overwrite'
for name,prop,camera_pos,target in variants:
 # Worlds must be copied through the level subsystem. Asset duplication retains
 # a standalone UWorld and can trigger a fatal world leak during LoadLevel.
 assert ed.new_level_from_template(BASE+name,SOURCE)
 for row in rows:
  if row['asset']!=prop:continue
  x,y,z=row['location_m'];a=actors.spawn_actor_from_class(u.StaticMeshActor,u.Vector(x*100,-y*100,z*100),u.Rotator(pitch=0,yaw=-row['yaw_degrees'],roll=0));a.set_actor_label('HERO_'+row['label']);a.static_mesh_component.set_static_mesh(meshes[prop]);s=row['uniform_scale'];a.set_actor_scale3d(u.Vector(s,s,s));a.set_actor_enable_collision(False);a.set_folder_path('Presentation/HeroProps')
 for actor in actors.get_all_level_actors():
  if isinstance(actor,u.CameraActor):
   actor.set_actor_location(u.Vector(*camera_pos),False,False);actor.set_actor_rotation(u.MathLibrary.find_look_at_rotation(u.Vector(*camera_pos),u.Vector(*target)),False)
 assert ed.save_current_level()
 # Release Python actor wrappers before the next template world is loaded.
 del actor, a
u.log('TALLOW new detailed review maps saved; base staging and gameplay maps untouched. Review lighting, normals and scale in-engine.')
