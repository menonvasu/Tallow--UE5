"""Root-only import and bounded story dressing. Requires loaded playable map; never launches PIE."""
import unreal as u,json
from pathlib import Path
R=Path(u.Paths.project_dir()).resolve().parent;S=R/'Tallow/SourceAssets/Production/HouseOutroV01';D='/Game/Presentation/Environment/HouseOutroV01';cfg=json.loads((S/'manifest.json').read_text());placement=cfg;lib=u.EditorAssetLibrary;at=u.AssetToolsHelpers.get_asset_tools();ed=u.get_editor_subsystem(u.EditorActorSubsystem);world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world();assert world.get_path_name().split('.')[0]==placement['map'],'Outro set requires the already-loaded outro presentation map'
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
 if 'HouseOutroV01' in [str(t) for t in a.tags]:ed.destroy_actor(a)
for row in placement['placements']:
 a=ed.spawn_actor_from_class(u.StaticMeshActor,u.Vector(*row['location_cm']),u.Rotator(pitch=row['tilt'],yaw=row['yaw'],roll=0));a.set_actor_label('OUTRO_'+row['label']);a.tags=[u.Name('HouseOutroV01')];a.set_actor_scale3d(u.Vector(*row['scale']));a.static_mesh_component.set_static_mesh(meshes[row['mesh']]);a.static_mesh_component.set_collision_profile_name('NoCollision')
# Hide only the known obsolete close emissive plane; retain all camera/candle/sequence actors.
for a in ed.get_all_level_actors():
 if a.get_actor_label()=='ColdSky_Backdrop':
  a.set_actor_hidden_in_game(True)
  c=a.get_component_by_class(u.StaticMeshComponent)
  if c:c.set_visibility(False)
# Private low luminance backdrop beyond the entire garden, removing daytime sky.
mname=D+'/Materials/M_Outro_NightBackdrop'
m=lib.load_asset(mname) if lib.does_asset_exist(mname) else at.create_asset('M_Outro_NightBackdrop',D+'/Materials',u.Material,u.MaterialFactoryNew())
u.MaterialEditingLibrary.delete_all_material_expressions(m);m.set_editor_property('shading_model',u.MaterialShadingModel.MSM_UNLIT);m.set_editor_property('two_sided',True)
c=u.MaterialEditingLibrary.create_material_expression(m,u.MaterialExpressionConstant3Vector,0,0);c.set_editor_property('constant',u.LinearColor(.004,.008,.017,1));u.MaterialEditingLibrary.connect_material_property(c,'',u.MaterialProperty.MP_EMISSIVE_COLOR);u.MaterialEditingLibrary.recompile_material(m);lib.save_loaded_asset(m)
a=ed.spawn_actor_from_class(u.StaticMeshActor,u.Vector(100,-1400,400),u.Rotator(pitch=0,yaw=0,roll=90));a.set_actor_label('OUTRO_NightBackdrop');a.tags=[u.Name('HouseOutroV01')];a.set_actor_scale3d(u.Vector(20,14,1));a.static_mesh_component.set_static_mesh(lib.load_asset('/Engine/BasicShapes/Plane'));a.static_mesh_component.set_material(0,m);a.static_mesh_component.set_collision_profile_name('NoCollision');a.static_mesh_component.set_cast_shadow(False)
for name,pos,power,radius,color in [('NearWarm',(100,-200,110),35,450,(1,.48,.18)),('ColdRim',(-100,-600,450),180,1200,(.2,.37,.7)),('FarGate',(100,-1100,220),100,650,(.16,.28,.54))]:
 a=ed.spawn_actor_from_class(u.PointLight,u.Vector(*pos));a.set_actor_label('OUTRO_'+name);a.tags=[u.Name('HouseOutroV01')];c=a.get_component_by_class(u.PointLightComponent);c.set_editor_property('intensity_units',u.LightUnits.LUMENS);c.set_intensity(power);c.set_attenuation_radius(radius);c.set_light_color(u.LinearColor(*color,1))
# Keep fog isolated by ownership tag; existing unrelated fog is left alone.
a=ed.spawn_actor_from_class(u.ExponentialHeightFog,u.Vector(100,-600,0));a.set_actor_label('OUTRO_GardenMist');a.tags=[u.Name('HouseOutroV01')];c=a.get_component_by_class(u.ExponentialHeightFogComponent)
for key,value in [('fog_density',.025),('fog_height_falloff',.25),('fog_max_opacity',.82),('start_distance',75.0),('fog_inscattering_color',u.LinearColor(.018,.028,.048,1))]:
 try:c.set_editor_property(key,value)
 except Exception as e:u.log_warning('Outro fog optional property '+key+': '+str(e))
u.EditorLevelLibrary.save_current_level();print('HOUSE_OUTRO_V01_IMPORTED_AND_PLACED: camera and timing unchanged; no game launch')
