"""Lead-run NEW V02 architecture import, no world or shared-asset mutation."""
from pathlib import Path
import json
import unreal as u
ROOT=Path(__file__).resolve().parents[2];SRC=ROOT/'Tallow/SourceAssets/Production/HeroRoomV02';DEST='/Game/Presentation/Environment/HeroRoomV02';E=u.EditorAssetLibrary;AT=u.AssetToolsHelpers.get_asset_tools();M=u.MaterialEditingLibrary
assert not E.does_directory_exist(DEST),'V02 exists: refuse overwrite'
rows=json.loads((SRC/'architecture-metrics.json').read_text());names=['AshTimber','BlackenedTimber','WornEdge','LimePlaster','FadedWallpaper'];tasks=[]
for name in names:
 for channel in ['BaseColor','Roughness']:
  file='T_'+name+'_'+channel+'.png';t=u.AssetImportTask();t.set_editor_property('filename',str(SRC/file));t.set_editor_property('destination_path',DEST+'/Textures');t.set_editor_property('destination_name',Path(file).stem);t.set_editor_property('automated',True);t.set_editor_property('save',True);tasks.append(t)
for row in rows:
 name=row['mesh'];t=u.AssetImportTask();t.set_editor_property('filename',str(SRC/(name+'.fbx')));t.set_editor_property('destination_path',DEST+'/Meshes');t.set_editor_property('destination_name',name);t.set_editor_property('automated',True);t.set_editor_property('save',True);opt=u.FbxImportUI()
 for key,value in [('import_mesh',True),('import_as_skeletal',False),('mesh_type_to_import',u.FBXImportType.FBXIT_STATIC_MESH),('import_materials',False),('import_textures',False)]:opt.set_editor_property(key,value)
 data=opt.get_editor_property('static_mesh_import_data')
 for key,value in [('combine_meshes',True),('auto_generate_collision',False),('normal_import_method',u.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS)]:data.set_editor_property(key,value)
 t.set_editor_property('options',opt);tasks.append(t)
AT.import_asset_tasks(tasks);mats={}
for name in names:
 mat=AT.create_asset('M_HeroRoomV02_'+name,DEST+'/Materials',u.Material,u.MaterialFactoryNew());mat.set_editor_property('two_sided',name=='FadedWallpaper')
 for i,(channel,prop) in enumerate([('BaseColor',u.MaterialProperty.MP_BASE_COLOR),('Roughness',u.MaterialProperty.MP_ROUGHNESS)]):
  tex=u.load_asset(DEST+'/Textures/T_'+name+'_'+channel);assert tex;tex.set_editor_property('srgb',channel=='BaseColor')
  if channel=='Roughness':tex.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_MASKS)
  node=M.create_material_expression(mat,u.MaterialExpressionTextureSample,-400,i*220);node.set_editor_property('texture',tex)
  if channel=='Roughness':node.set_editor_property('sampler_type',u.MaterialSamplerType.SAMPLERTYPE_MASKS)
  M.connect_material_property(node,'RGB' if channel=='BaseColor' else 'R',prop);E.save_loaded_asset(tex,False)
 M.recompile_material(mat);E.save_loaded_asset(mat,False);mats['M_HeroRoomV02_'+name]=mat
for row in rows:
 mesh=u.load_asset(DEST+'/Meshes/'+row['mesh']);assert mesh
 for i,material in enumerate(row['materials']):mesh.set_material(i,mats[material])
 E.save_loaded_asset(mesh,False)
u.log('V02 new architecture imported. Directional ProductionUV uses intentional material tiling. No worlds modified.')
