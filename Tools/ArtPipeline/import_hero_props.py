"""Lead-run import: detailed original chair/doll presentation assets only, no map edits."""
from pathlib import Path
import unreal as u
ROOT=Path(__file__).resolve().parents[2];E=u.EditorAssetLibrary;AT=u.AssetToolsHelpers.get_asset_tools();M=u.MaterialEditingLibrary
props=['RestraintChair','CollectionDoll'];plans=[]
for prop in props:
 src=ROOT/'Tallow/SourceAssets/Production'/prop;dest='/Game/Presentation/Environment/HeroProps/'+prop
 names=['SM_'+prop,'T_'+prop+'_BaseColor','T_'+prop+'_Roughness','T_'+prop+'_Normal','M_'+prop+'_PBR']
 assert all(not E.does_asset_exist(dest+'/'+n) for n in names),'Hero destination already exists; use explicit versioned update'
 files=['SM_'+prop+'.fbx']+['T_'+prop+'_'+channel+'.png' for channel in ['BaseColor','Roughness','Normal']]
 for f in files:assert (src/f).is_file() and not (src/f).read_bytes()[:80].startswith(b'version https://git-lfs'),f
 plans.append((prop,src,dest,files))
for prop,src,dest,files in plans:
 tasks=[]
 for f in files:
  t=u.AssetImportTask();t.filename=str(src/f);t.destination_path=dest;t.destination_name=Path(f).stem;t.automated=True;t.save=True;t.replace_existing=False
  if f.endswith('.fbx'):
   opt=u.FbxImportUI();opt.import_mesh=True;opt.import_as_skeletal=False;opt.mesh_type_to_import=u.FBXImportType.FBXIT_STATIC_MESH;opt.import_materials=False;opt.import_textures=False;opt.static_mesh_import_data.set_editor_property('combine_meshes',True);opt.static_mesh_import_data.set_editor_property('auto_generate_collision',False);opt.static_mesh_import_data.set_editor_property('normal_import_method',u.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS);t.options=opt
  tasks.append(t)
 AT.import_asset_tasks(tasks);mesh=u.load_asset(dest+'/SM_'+prop);assert mesh
 mat=AT.create_asset('M_'+prop+'_PBR',dest,u.Material,u.MaterialFactoryNew())
 for i,(channel,property_) in enumerate([('BaseColor',u.MaterialProperty.MP_BASE_COLOR),('Roughness',u.MaterialProperty.MP_ROUGHNESS),('Normal',u.MaterialProperty.MP_NORMAL)]):
  tex=u.load_asset(dest+'/T_'+prop+'_'+channel);assert tex;tex.set_editor_property('srgb',channel=='BaseColor')
  if channel=='Normal':tex.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_NORMALMAP);tex.set_editor_property('flip_green_channel',True)
  elif channel=='Roughness':tex.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_MASKS)
  node=M.create_material_expression(mat,u.MaterialExpressionTextureSample,-400,i*250);node.texture=tex
  if channel=='Normal':node.set_editor_property('sampler_type',u.MaterialSamplerType.SAMPLERTYPE_NORMAL)
  elif channel=='Roughness':node.set_editor_property('sampler_type',u.MaterialSamplerType.SAMPLERTYPE_MASKS)
  M.connect_material_property(node,'R' if channel=='Roughness' else 'RGB',property_);E.save_loaded_asset(tex,False)
 M.recompile_material(mat);mesh.set_material(0,mat);E.save_loaded_asset(mat,False);E.save_loaded_asset(mesh,False)
u.log('TALLOW: chair and collection doll imported into isolated HeroProps assets. No map, collision or gameplay changes; verify normal orientation and camera scale before placement.')
