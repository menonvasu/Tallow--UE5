"""Root: targeted joinery import + add only these new actors to loaded playable copy."""
import unreal as u,json
from pathlib import Path
R=Path(u.Paths.project_dir()).resolve().parent;S=R/'Tallow/SourceAssets/Production/HouseIntegrationV01';D='/Game/Presentation/Environment/HouseIntegrationV01';lib=u.EditorAssetLibrary;ed=u.get_editor_subsystem(u.EditorActorSubsystem);world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world();assert world.get_path_name().split('.')[0]=='/Game/Presentation/Playable/Lvl_Tallow_Playable'
for r in json.loads((S/'joinery-manifest.json').read_text()):
 name=r['mesh'];t=u.AssetImportTask();t.set_editor_property('filename',str(S/'Architecture'/(name+'.fbx')));t.set_editor_property('destination_path',D+'/Architecture');t.set_editor_property('automated',True);t.set_editor_property('replace_existing',True);t.set_editor_property('save',True);o=u.FbxImportUI();o.set_editor_property('import_materials',False);o.set_editor_property('import_textures',False);o.set_editor_property('import_as_skeletal',False);o.set_editor_property('mesh_type_to_import',u.FBXImportType.FBXIT_STATIC_MESH);o.get_editor_property('static_mesh_import_data').set_editor_property('combine_meshes',True);t.set_editor_property('options',o);u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t]);m=lib.load_asset(D+'/Architecture/'+name)
 for i,mat in enumerate(r['materials']):
  path=D+'/Materials/M_House_Limewash' if mat.endswith('LimePlaster') else '/Game/Presentation/Environment/HeroRoomV02/Materials/'+mat;m.set_material(i,lib.load_asset(path))
 lib.save_loaded_asset(m)
 if 'SplinteredBoard' in name:continue
 for old in ed.get_all_level_actors():
  if old.get_actor_label()=='HI_'+name and 'HouseIntegrationV01' in [str(t) for t in old.tags]:ed.destroy_actor(old)
 a=ed.spawn_actor_from_class(u.StaticMeshActor,u.Vector(0,0,0));a.set_actor_label('HI_'+name);a.tags=[u.Name('HouseIntegrationV01')];a.static_mesh_component.set_static_mesh(m);a.static_mesh_component.set_collision_profile_name('NoCollision')
u.EditorLevelLibrary.save_current_level();print('HOUSE_JOINERY_IMPORTED')
