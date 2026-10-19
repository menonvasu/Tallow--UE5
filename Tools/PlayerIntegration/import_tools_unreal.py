"""Fresh scoped original tool import. Run only in editor, never modifies item rules."""
from pathlib import Path
import unreal as u
ROOT=Path(__file__).resolve().parents[2];SOURCE=ROOT/'Tallow/SourceAssets/Production/ToolProps';DEST='/Game/Presentation/ToolProps'
E=u.EditorAssetLibrary;A=u.AssetToolsHelpers.get_asset_tools();M=u.MaterialEditingLibrary
assert not E.list_assets(DEST,recursive=True,include_folder=False),'Refusing existing ToolProps namespace'
def task(path,name,dest,options=None):
 assert path.is_file(),path
 t=u.AssetImportTask();t.filename=str(path);t.destination_path=dest;t.destination_name=name;t.automated=True;t.replace_existing=False;t.save=True
 if options:t.options=options
 A.import_asset_tasks([t]);assert t.imported_object_paths,path
 return [u.load_asset(p) for p in t.imported_object_paths]
def opts(skeleton=None,animation=False,static=False):
 o=u.FbxImportUI()
 for key,value in {'automated_import_should_detect_type':False,'import_mesh':not animation,'import_as_skeletal':not static,'import_animations':animation,'import_materials':False,'import_textures':False}.items():o.set_editor_property(key,value)
 o.set_editor_property('mesh_type_to_import',u.FBXImportType.FBXIT_STATIC_MESH if static else u.FBXImportType.FBXIT_ANIMATION if animation else u.FBXImportType.FBXIT_SKELETAL_MESH)
 if skeleton:o.set_editor_property('skeleton',skeleton)
 if not animation and not static:o.get_editor_property('skeletal_mesh_import_data').set_editor_property('import_morph_targets',False)
 return o
def material(dest,name,source,prefix,metal=False,skinned=False):
 mat=A.create_asset(name,dest,u.Material,u.MaterialFactoryNew());assert mat
 if skinned:mat.set_editor_property('used_with_skeletal_mesh',True)
 labels=['BaseColor','Roughness','NormalGL']+(['Metallic'] if metal else [])
 properties={'BaseColor':u.MaterialProperty.MP_BASE_COLOR,'Roughness':u.MaterialProperty.MP_ROUGHNESS,'NormalGL':u.MaterialProperty.MP_NORMAL,'Metallic':u.MaterialProperty.MP_METALLIC}
 for i,label in enumerate(labels):
  name=prefix+label;tx=next(x for x in task(source/(name+'.png'),name,dest) if isinstance(x,u.Texture2D));tx.set_editor_property('srgb',label=='BaseColor')
  if label=='NormalGL':tx.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_NORMALMAP);tx.set_editor_property('flip_green_channel',True)
  elif label!='BaseColor':tx.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_MASKS)
  assert E.save_loaded_asset(tx,False)
  n=M.create_material_expression(mat,u.MaterialExpressionTextureSample,-400,i*180);n.texture=tx;n.sampler_type=u.MaterialSamplerType.SAMPLERTYPE_NORMAL if label=='NormalGL' else u.MaterialSamplerType.SAMPLERTYPE_COLOR if label=='BaseColor' else u.MaterialSamplerType.SAMPLERTYPE_MASKS
  M.connect_material_property(n,'RGB' if label in ['BaseColor','NormalGL'] else 'R',properties[label])
 M.recompile_material(mat);assert E.save_loaded_asset(mat,False);return mat

for label,height in [('Crowbar',41.35),('Fuse',12),('Match',4.1)]:
 name='SM_ToolV02_'+label
 mesh=next(x for x in task(SOURCE/(name+'.fbx'),name,DEST,opts(static=True)) if isinstance(x,u.StaticMesh))
 bounds=mesh.get_bounding_box();actual=bounds.max.z-bounds.min.z
 assert abs(actual-height)<.5,(name,actual,height)
 mat=material(DEST,'M_ToolV02_'+label,SOURCE,'T_ToolV02_'+label+'_',metal=True)
 mesh.set_material(0,mat);assert E.save_loaded_asset(mesh,False)
assert E.save_directory(DEST,only_if_is_dirty=False,recursive=True)
u.log('ToolProps imported, sizes and materials verified; gameplay connections remain adapter-owned.')
