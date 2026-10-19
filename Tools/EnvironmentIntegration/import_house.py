"""Run by root in UE editor. Imports owned assets only; no level mutations."""
import unreal as u,json
from pathlib import Path
R=Path(u.Paths.project_dir()).resolve().parent;S=R/'Tallow/SourceAssets/Production/HouseIntegrationV01';DEST='/Game/Presentation/Environment/HouseIntegrationV01';lib=u.EditorAssetLibrary;at=u.AssetToolsHelpers.get_asset_tools();sm=u.get_editor_subsystem(u.StaticMeshEditorSubsystem)
def imp(file,dest,mesh=False):
 t=u.AssetImportTask();t.set_editor_property('filename',str(file));t.set_editor_property('destination_path',dest);t.set_editor_property('automated',True);t.set_editor_property('replace_existing',True);t.set_editor_property('save',True)
 if mesh:
  o=u.FbxImportUI();o.set_editor_property('import_mesh',True);o.set_editor_property('import_materials',False);o.set_editor_property('import_textures',False);o.set_editor_property('import_as_skeletal',False);o.set_editor_property('mesh_type_to_import',u.FBXImportType.FBXIT_STATIC_MESH);d=o.get_editor_property('static_mesh_import_data');d.set_editor_property('combine_meshes',True);d.set_editor_property('generate_lightmap_u_vs',True);d.set_editor_property('auto_generate_collision',True);t.set_editor_property('options',o)
 at.import_asset_tasks([t]);return lib.load_asset(dest+'/'+file.stem)
for row in json.loads((S/'Architecture/manifest.json').read_text()):
 m=imp(S/'Architecture'/(row['mesh']+'.fbx'),DEST+'/Architecture',True)
 for i,mat in enumerate(row['materials']):
  a=lib.load_asset('/Game/Presentation/Environment/HeroRoomV02/Materials/'+mat)
  if a:m.set_material(i,a)
 if row['collision']=='complex':
  body=m.get_editor_property('body_setup');body.set_editor_property('collision_trace_flag',u.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
  for i in range(m.get_num_sections(0)):sm.enable_section_collision(m,True,0,i)
 lib.save_loaded_asset(m)
for row in json.loads((S/'Furniture/manifest.json').read_text()):
 m=imp(S/'Furniture'/(row['mesh']+'.fbx'),DEST+'/Furniture',True)
 for slot,desc in enumerate(row['materials']):
  name='M_House_'+row['mesh'].removeprefix('SM_House_')+'_'+str(slot);p=DEST+'/Materials/'+name;mat=lib.load_asset(p) or at.create_asset(name,DEST+'/Materials',u.Material,u.MaterialFactoryNew());u.MaterialEditingLibrary.delete_all_material_expressions(mat)
  for role,fn in desc['textures'].items():
   tx=imp(S/'Furniture'/fn,DEST+'/Textures');e=u.MaterialEditingLibrary.create_material_expression(mat,u.MaterialExpressionTextureSample,0,0);e.set_editor_property('texture',tx)
   if role=='Normal':tx.set_editor_property('flip_green_channel',True);tx.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_NORMALMAP);tx.set_editor_property('srgb',False);e.set_editor_property('sampler_type',u.MaterialSamplerType.SAMPLERTYPE_NORMAL)
   elif role in ['Roughness','Opacity']:tx.set_editor_property('srgb',False);e.set_editor_property('sampler_type',u.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
   prop={'BaseColor':u.MaterialProperty.MP_BASE_COLOR,'Normal':u.MaterialProperty.MP_NORMAL,'Roughness':u.MaterialProperty.MP_ROUGHNESS,'Opacity':u.MaterialProperty.MP_OPACITY_MASK}[role];u.MaterialEditingLibrary.connect_material_property(e,'RGB' if role in ['BaseColor','Normal'] else 'R',prop);lib.save_loaded_asset(tx)
   if role=='Opacity':mat.set_editor_property('blend_mode',u.BlendMode.BLEND_MASKED)
  u.MaterialEditingLibrary.recompile_material(mat);lib.save_loaded_asset(mat);m.set_material(slot,mat)
 m.get_editor_property('body_setup').set_editor_property('collision_trace_flag',u.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE);lib.save_loaded_asset(m)
print('HOUSE_IMPORT_COMPLETE')
