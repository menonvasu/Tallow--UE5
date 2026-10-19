"""Root-only import and bounded story dressing. Requires loaded playable map; never launches PIE."""
import unreal as u,json
from pathlib import Path
R=Path(u.Paths.project_dir()).resolve().parent;S=R/'Tallow/SourceAssets/Production/HouseStoryV02';D='/Game/Presentation/Environment/HouseStoryV02';cfg=json.loads((S/'manifest.json').read_text());placement=json.loads((S/'placement.json').read_text());lib=u.EditorAssetLibrary;at=u.AssetToolsHelpers.get_asset_tools();ed=u.get_editor_subsystem(u.EditorActorSubsystem);world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world();assert world.get_path_name().split('.')[0]==placement['map'],'Story pass only supports already-loaded playable copy'
def imp(file,folder,mesh=False):
 t=u.AssetImportTask();t.set_editor_property('filename',str(file));t.set_editor_property('destination_path',D+'/'+folder);t.set_editor_property('automated',True);t.set_editor_property('replace_existing',True);t.set_editor_property('save',True)
 if mesh:
  o=u.FbxImportUI();o.set_editor_property('import_materials',False);o.set_editor_property('import_textures',False);o.set_editor_property('import_as_skeletal',False);o.set_editor_property('mesh_type_to_import',u.FBXImportType.FBXIT_STATIC_MESH);o.get_editor_property('static_mesh_import_data').set_editor_property('combine_meshes',True);o.get_editor_property('static_mesh_import_data').set_editor_property('auto_generate_collision',False);t.set_editor_property('options',o)
 at.import_asset_tasks([t]);return lib.load_asset(D+'/'+folder+'/'+file.stem)
materials={}
for name,row in cfg['materials'].items():
 path=D+'/Materials/'+name
 m=lib.load_asset(path) if lib.does_asset_exist(path) else at.create_asset(name,D+'/Materials',u.Material,u.MaterialFactoryNew());u.MaterialEditingLibrary.delete_all_material_expressions(m);m.set_editor_property('two_sided',row['two_sided'])
 if row['texture']:
  t=imp(S/row['texture'],'Textures');c=u.MaterialEditingLibrary.create_material_expression(m,u.MaterialExpressionTextureSample,0,0);c.set_editor_property('texture',t);u.MaterialEditingLibrary.connect_material_property(c,'RGB',u.MaterialProperty.MP_BASE_COLOR)
 else:
  c=u.MaterialEditingLibrary.create_material_expression(m,u.MaterialExpressionConstant3Vector,0,0);c.set_editor_property('constant',u.LinearColor(*row['color']));u.MaterialEditingLibrary.connect_material_property(c,'',u.MaterialProperty.MP_BASE_COLOR)
 for key,prop in [('roughness',u.MaterialProperty.MP_ROUGHNESS),('metallic',u.MaterialProperty.MP_METALLIC)]:
  c=u.MaterialEditingLibrary.create_material_expression(m,u.MaterialExpressionConstant,0,0);c.set_editor_property('r',row[key]);u.MaterialEditingLibrary.connect_material_property(c,'',prop)
 u.MaterialEditingLibrary.recompile_material(m);lib.save_loaded_asset(m);materials[name]=m
meshes={}
for row in cfg['meshes']:
 m=imp(S/(row['mesh']+'.fbx'),'Meshes',True)
 for i,n in enumerate(row['materials']):m.set_material(i,materials[n])
 lib.save_loaded_asset(m);meshes[row['mesh']]=m
for a in ed.get_all_level_actors():
 if 'HouseStoryV02' in [str(t) for t in a.tags]:ed.destroy_actor(a)
for row in placement['placements']:
 a=ed.spawn_actor_from_class(u.StaticMeshActor,u.Vector(*row['location_cm']),u.Rotator(pitch=row['tilt'],yaw=row['yaw'],roll=0));a.set_actor_label('STORY_'+row['label']);a.tags=[u.Name('HouseStoryV02')];a.set_actor_scale3d(u.Vector(*row['scale']));a.static_mesh_component.set_static_mesh(meshes[row['mesh']]);a.static_mesh_component.set_collision_profile_name('NoCollision')
u.EditorLevelLibrary.save_current_level();print('HOUSE_STORY_V02_IMPORTED_AND_PLACED: no game launch')
